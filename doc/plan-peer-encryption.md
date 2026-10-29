# Plan: the peers of konsensd talk through a key exchange and AES-256-GCM

Branch peer-encryption, from main ebd9f42. Owner 2026-10-29 21:30:
"Zum Client werden wir TLS sprechen ... Intern will ich keine
Zertifikate haben ... alles was wir von Zertifikaten brauchen, ist eine
Revoke-Liste ... intern ein Key-Exchange-Verfahren ... und wir machen
nur AES."

Marks: "provisional" marks a form or a name that waits on the owner.
Review: doc/review-plan-peer-encryption.md (first version rejected, 3
blocking findings); all its findings are folded in here.

## 1. Facts checked in this session

1. Peers of konsensd talk over TCP. A frame is a big-endian length of
   4 bytes and a payload (program/frame.hpp:14, frame.cpp:62-74). The
   first frame of a connection is `hello {cluster_name, pid}`
   (frame.hpp:26-31, serve.cpp:612, :489). `decode_hello` refuses
   bytes after the CBOR item (frame.cpp:125). The library does no
   network IO; all of this is in program/.
2. A peer connection is one-way. Each pair of nodes uses two TCP
   connections. An outgoing peer connection closes on any receive
   (serve.cpp:521). An incoming one never sends. The outgoing side is
   marked open and calls `reconnected` when TCP connects
   (serve.cpp:610-613), and `release_the_output` sends on each open
   outgoing connection (:414). The incoming side closes the old
   connection of the same pid and calls `reconnected` when it reads the
   `hello` (:495-500).
3. libhydrogen 264bf42 offers `hydro_kx_kk_1`, `_2` and `_3`
   (hydrogen.h:216-231), the Noise pattern KK with the primitives of
   libhydrogen ("Noise_KK_hydro1", impl/kx.h:257). Both sides know the
   static public key of the other side before the handshake. Each
   packet has 48 bytes. The responder derives `(tx, rx)` (kx.h:301),
   the initiator `(rx, tx)` (kx.h:323), so `tx` of one side is `rx` of
   the other. A call returns -1 when a packet fails or an X25519 result
   is zero. Packet 1 carries no value of the responder, so the
   responder cannot know that it is fresh (kx.h:262-266). KK of
   libhydrogen takes no prologue (hydrogen.h:219-230).
4. `hydro_kx_kk_1` and `hydro_kx_kk_2` make an ephemeral key pair from
   the generator of libhydrogen. `hydro_init` always returns 0; a
   failure of the random source calls `abort`
   (impl/random.h:49-60). It waits on /dev/random until the pool is
   ready and reads /dev/urandom (impl/random/unix.h:17-33, :58-85).
   The state of the generator is per thread (random.h:1,
   common.h:42). libhydrogen does not erase `hydro_kx_state` after
   `hydro_kx_kk_3` (kx.h:308-326), and `hydro_kx_kk_2` keeps its state
   on its own stack (kx.h:280-305).
5. RFC 8446 5.3 (read in the text of the Ubuntu package
   doc-rfc-std-proposed 20230121-1build1; it goes to refs/ by 3.4.3): one sequence number of 64 bits per direction,
   zero at the start of a key; the nonce is the sequence number in
   network byte order, padded to the left with zeros to 12 bytes, and
   XORed with a static IV. 5.5: "For AES-GCM, up to 2^24.5 full-size
   records ... may be encrypted"; a full-size record holds 2^14 bytes
   (5.1). RFC 9001 6.6 calls this bound the confidentiality limit.
6. Measured on this machine (bench/results/2026-10-29-212437Z-peer-aead.txt
   on branch chain-hash, 7015b75), medians per compiler for g++ 15,
   g++ 16 and clang 22: to seal one message, AES-256-GCM of OpenSSL
   takes 77 ns to 80 ns at 64 B and 3.9 µs at 64 KiB; hydro_secretbox
   takes 2.0 µs to 9.9 µs at 64 B and 0.6 ms to 3.0 ms at 64 KiB.
   libhydrogen was built for baseline x86-64.
7. konsensd does not link libcrypto (CMakeLists.txt). The Ubuntu
   package libssl-dev 4.0.3 gives OpenSSL 4.0; `EVP_CIPHER_fetch` exists
   from OpenSSL 3.0.

## 2. What others do

- WireGuard: one fixed cipher, no negotiation. Each peer is named by its
  static public key in base64 in the configuration. A key pair comes
  from `wg genkey`; the private key lives in a file that only its
  owner reads. To revoke a peer, its key is removed.
- OpenSSH: `RevokedKeys` names public keys that the server refuses,
  whatever else allows them. A private key file with any bit of 0077
  is refused (not checked in the OpenSSH source).
- Noise (revision 34, section 12, not checked: noiseprotocol.org is
  refused by the proxy of this session): the nonce of AESGCM is 32 zero
  bits and the counter in big-endian order. That equals RFC 8446 5.3
  with an IV of zeros.
- TLS 1.3 closes a connection after any record that fails its tag
  (RFC 9001 6.6).

## 3. The change

### 3.1 Keys and configuration

1. Each node has one static key pair of hydro_kx. `konsensd
   --generate-key <file>` (form of `wg genkey`) opens the file with
   `O_CREAT | O_EXCL`, mode 0600, writes the secret and the public key,
   and prints the public key in base64 (RFC 4648 section 4). The file
   form is the 64 raw bytes, secret key first (provisional form).
2. konsensd refuses a key file with any bit of 0077 in its mode, a file
   whose owner is not the user of the process, and a file that is not
   64 bytes long. The buffer that held the file is erased with
   `hydro_memzero` after the read.
3. The configuration names the key file (`node.key_file`) and the
   public key of each peer (`peers[].public_key`, base64, 32 bytes after
   decoding).
4. The revocation list is `cluster.revoked_public_keys` (form of
   `RevokedKeys`), a list of base64 keys (provisional form, question
   6.1). A configuration in which a peer has a revoked key is refused.
   The list keeps a replaced key from coming back into `peers`. The
   handshake takes the key of a pid from `peers` only, so the handshake
   needs no second check.
5. `cluster.name` is at most 255 bytes, the bound of a domain name in
   RFC 1035 2.3.4 (provisional bound).

### 3.2 Handshake

1. The `hello` gets two fields: `version` (provisional name, value 1)
   and `kx` (packet 1 of KK, a byte string of exactly 48 bytes). The
   decoder refuses another size. A `hello` without `kx`, or with
   another version, is refused. There is no plaintext mode, so a
   rolling upgrade from the plaintext protocol is not possible.
2. A connection has three states: `connected`, `kx_sent` (outgoing) or
   `kx_answered` (incoming), and `ready`.
3. Outgoing side: after the TCP connect, it sends the `hello` with
   packet 1 and stays not open. It reads exactly one frame: packet 2,
   48 bytes. `hydro_kx_kk_3` gives the keys. Then it sends, as its
   first encrypted record, the same `hello` without `kx`. Only then is
   the connection open and `reconnected` called. Any later receive
   closes the connection, as now. `hydro_kx_state` is erased with
   `hydro_memzero` after `hydro_kx_kk_3` and when the connection closes
   before it.
4. Incoming side: it looks up the public key of `hello.pid` in
   `peers`, calls `hydro_kx_kk_2`, and sends packet 2. This is the one
   frame that an incoming connection sends. The old connection of that
   pid stays. When the first encrypted record decrypts and its `hello`
   equals the plaintext `hello` without `kx`, the old connection of
   that pid closes, the new one goes into `l.incoming`, and
   `reconnected` is called. So a replayed first frame does not close
   the live connection, and the encrypted `hello` binds the cluster
   name, the pid and the version to the keys.
5. Each side keeps only the key of its direction: the outgoing side
   `tx`, the incoming side `rx`. The other key is erased at once.
6. Before `ready`, the receive limit is `max_hello_bytes`, the encoded
   size of a `hello` with a cluster name of 255 bytes and `kx`
   (provisional name). A connection that is not `ready` after
   `handshake_ticks` ticks closes (provisional name, value 2 x
   `election_ticks`). A failed handshake increments a counter of the
   node and writes at most one log line per tick (provisional form).

### 3.3 Records

1. After the handshake, each frame is the 4-byte length, then the
   ciphertext and the tag of 16 bytes. The length counts ciphertext and
   tag, as TLSCiphertext does (RFC 8446 5.2). The 4 bytes of the length
   are the additional data of GCM. The receiver allows
   `max_frame_bytes` plus the tag; a length below the tag size closes
   the connection. It decrypts in place in the frame string.
2. The nonce is RFC 8446 5.3 with an IV of zeros: 4 zero bytes, then
   the sequence number of 64 bits in network byte order. It starts at 0
   for each key and is never sent. TCP keeps the order, so a record
   that is repeated, missing or moved fails its tag.
3. A failed tag closes the connection.
4. The confidentiality limit counts the full-size records of fact 1.5:
   each frame counts as the number of 2^14-byte pieces of its
   plaintext, rounded up. At `confidentiality_limit` (RFC 9001 6.6),
   2^24 such records, the connection closes, and the next connection
   makes new keys. The test passes the limit as a value.
5. Each connection holds one `EVP_CIPHER_CTX`, set up once with its
   key; the key bytes are erased with `OPENSSL_cleanse` after that.
   Whether `EVP_CIPHER_CTX_free` erases the key schedule is checked in
   the OpenSSL source before the build. AES-256-GCM is fetched once at
   start with `EVP_CIPHER_fetch`.
6. No other cipher and no negotiation. A change of the cipher is a new
   `version`.

### 3.4 Start and build

1. `hydro_init` is called once in the thread of the loop, before the
   first connection. konsensd does not fork, so a copied state of the
   generator is not a risk today.
2. konsensd links `OpenSSL::Crypto` of the Ubuntu package libssl-dev,
   OpenSSL 3.0 or later. The library `konsens` does not change.
3. RFC 8446, RFC 9001 and RFC 4648 go to refs/ from the Ubuntu package
   doc-rfc-std-proposed, in the commit of the first function that
   reads each. refs/rfc-origin.md names the package and its SHA-256.

## 4. Error paths (provisional, question 6.3)

| Case | Form |
|---|---|
| key file missing, wrong size, wrong mode, other owner | start refused, one message |
| `--generate-key` on a file that exists | refused, one message |
| public key not base64 or not 32 bytes | `config_errc::wrong_type` |
| two peers with one key | new `config_errc::duplicate_public_key` |
| a peer with a revoked key | new `config_errc::revoked_public_key` |
| no AES-256-GCM in libcrypto | start refused, one message |
| `hydro_init` fails | `abort` inside libhydrogen; differs from the rule that a machine failure is raised as `std::system_error` |
| failed handshake, failed tag, wrong length | the connection closes (input from outside) |
| an EVP call fails during a seal or an open | raised; nothing catches it (fault of the machine or of ours) |

`name_of` in main.cpp names each new `config_errc` value.

## 5. Tests

- Two nodes with the right keys exchange frames both ways.
- The outgoing side sends no record before `hydro_kx_kk_3` succeeds.
- A recorded first frame, sent again, does not close the live
  connection and does not call `reconnected`.
- A `hello` with the pid of node B and packet 1 made with the key of
  node C is refused.
- A packet 2 with a flipped byte, or from a node with another key at
  the address, closes the connection at the outgoing side.
- A `hello` without `kx`, with a `kx` of 47 or 49 bytes, or with
  another version is refused.
- An encrypted `hello` that differs from the plaintext one closes the
  connection.
- `tx` of the initiator equals `rx` of the responder, read from
  `hydro_kx_kk_2` and `hydro_kx_kk_3` directly.
- A known-answer test of the seal with a published AES-256-GCM test
  vector (key, nonce, AAD, plaintext, ciphertext and tag).
- The nonce of record n equals the form of RFC 8446 5.3 with an IV of
  zeros, for n = 0, 1, 2^8 and 2^64 - 1.
- A flipped byte in a ciphertext, a tag or a length closes the
  connection. A record sent twice, or two records swapped, closes it.
- A frame of `max_frame_bytes` passes; one byte more is refused. A
  length of 0 to 15 bytes closes the connection.
- The confidentiality limit closes at the limit and not before, for a
  frame of 1 byte, of 2^14 bytes and of 2^14 + 1 bytes.
- A first frame larger than `max_hello_bytes`, and a connection that
  sends nothing for `handshake_ticks`, close.
- Configuration: a peer without `public_key`, a key that is not base64,
  a key of 31 or 33 bytes, two peers with one key, a peer with a
  revoked key, a cluster name of 256 bytes. Each gives its error, and
  `name_of` names each value.
- Key file: missing, 63 bytes, mode 0640, mode 0604, another owner.
  `--generate-key` on a file that exists is refused.
- program/test/three_nodes.sh runs with keys.

## 6. Questions for the owner

1. The revocation list stands in the configuration (the form of
   OpenSSH); a change needs a restart of each node. Another form is an
   entry in the log, which reaches every node at the same index; no
   system of section 2 does that.
2. The key file form of 3.1.1.
3. The error paths of section 4, above all `abort` in `hydro_init`.

## 7. Measurement

The comparison bench of doc/plan-bench-distributed-logs.md, konsensd
rows, before and after, both builds. Also the time of one handshake and
of one refused packet 1. No number about the cost is written before
these runs.

## 8. What this plan does not do

- No TLS to clients; that is a plan of its own.
- No change of the wire of the library messages.

## 9. Data of this plan

- Build directories under the scratchpad: made again by the build
  command.
- The download of doc-rfc-std-proposed in the scratchpad: deleted after
  the RFCs stand in refs/.
- Result files under bench/results: recorded in the state file.
