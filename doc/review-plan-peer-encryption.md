# Review: plan for the key exchange and AES-256-GCM between the peers

Plan: `doc/plan-peer-encryption.md`, branch peer-encryption, d99fc54.
Source: `program/` at d99fc54. libhydrogen: 264bf42.

The submodule `deps/libhydrogen` is not checked out in this worktree.
The directory is empty. The reviewer read libhydrogen at the same
commit 264bf42 in `/home/claude/mbedded-konsens/deps/libhydrogen`.

The tree holds no `refs/rfc8446.txt` and no Noise specification. The
reviewer did not download them. Each claim below about RFC 8446, RFC
9001 and Noise comes from the memory of the reviewer and is marked
"not checked". The reviewer did not build or run anything.

## Verdict

Reject.

The cryptographic core is sound: KK of libhydrogen gives mutual
authentication, and the rx/tx directions fit. Three faults change the
design, so the plan must be written again before the build starts:

- Finding 1: the peer connections of serve.cpp are one-way. The plan
  sends packet 2 on a connection that closes on any receive.
- Finding 2: the record limit counts records, but a frame can hold
  2^30 bytes. The bound of RFC 8446 5.5 is for records of 2^14 bytes.
- Finding 3: a replayed `hello` with packet 1 passes `hydro_kx_kk_2`
  and closes the live connection of that peer.

## Findings

### 1. Blocking: a peer connection carries data in one direction only

Place: plan 3.4. `program/serve.cpp:521-524`, `:610-614`, `:410-414`,
`:489-500`.

Fault: an outgoing peer connection closes on any receive
(`c.kind == link::outgoing_peer` at serve.cpp:521). An incoming peer
connection never sends. Each pair of nodes uses two TCP connections,
one for each direction. Packet 2 of the plan goes from the accepting
node to the connecting node, so the connecting node closes the
connection when packet 2 arrives. Fact 1.1 does not state this.

Also, the connecting node sets `c.open = true` and calls
`l.node.reconnected` when the TCP connect ends (serve.cpp:610-613).
`release_the_output` sends on every open outgoing connection
(serve.cpp:414). With the plan, a message goes out before
`hydro_kx_kk_3` gives the keys.

Fix: the plan states the new states of a peer connection:
connected, packet 1 sent, keys ready. The outgoing side reads exactly
one frame (packet 2) and closes on any later receive. The incoming
side sends exactly one frame (packet 2). `c.open` becomes true and
`reconnected` is called only after `hydro_kx_kk_3` succeeds. Add fact
1.6: "a peer connection is one-way" with serve.cpp:521.

### 2. Blocking: the record limit counts records, but a record can hold 2^30 bytes

Place: plan 1.5, 3.6. `program/config.hpp:17`
(`default_max_frame_bytes = 1 << 30`).

Fault: RFC 8446 5.5 gives 2^24.5 records for "full-size" records,
that is records of 2^14 bytes of plaintext, with a margin of about
2^-57 (not checked). The bound comes from the number of AES blocks
under one key. A frame of konsensd holds up to `max_frame_bytes`.
With 2^24 records of 2^30 bytes, one key protects 2^54 bytes. The
bound of RFC 8446 is about 2^38.5 bytes (2^24.5 x 2^14). A long
connection that replicates a log reaches 2^38 bytes (256 GiB).

Fix: count the bytes of plaintext (or the 16-byte blocks) under one
key, and close at 2^24.5 x 2^14 bytes, or at a lower power of two such
as 2^38 bytes. The limit is one named constant. Its name comes from
RFC 9001 6.6 and the CFRG draft on AEAD limits: "confidentiality
limit" (not checked). Fact 1.5 states the unit "full-size record =
2^14 bytes". The test passes the byte limit as a value.

### 3. Blocking: a replayed hello with packet 1 closes the live connection of a peer

Place: plan 3.4. `program/serve.cpp:489-500`. libhydrogen
`impl/kx.h:273-305`.

Fault: packet 1 of KK depends only on the ephemeral key of the
initiator and on the two static keys (es and ss, kx.h:262-266).
Packet 1 carries no value from the responder. A recorded `hello` with
packet 1 from an earlier connection of node A passes
`hydro_kx_kk_2` at node B again. B cannot know that the packet is
old. The session keys stay safe, because they also hold ee and se
with the new ephemeral key of B (kx.h:295-301). But serve.cpp closes
the old incoming connection of the same pid and calls `reconnected`
as soon as the `hello` is read (serve.cpp:495-500). An attacker that
replays one recorded first frame closes the A-to-B connection, again
and again. This is a known property of KK: the responder knows that
the initiator is alive only after the first transport message.

Fix: the initiator sends one record at once after
`hydro_kx_kk_3`. That record holds the `hello` again: cluster name,
pid and the protocol version (finding 5). The responder decrypts that
record and compares it with the plaintext `hello`. Only then does it
close the old incoming connection of that pid, put the new one in
`l.incoming` and call `reconnected`. This also binds the cluster name
and the version to the keys, because the `hello` itself is not in the
hash of the handshake (libhydrogen KK has no prologue,
hydrogen.h:219-230).

### 4. Should fix: the nonce form has no source and contradicts both sources

Place: plan 1.5, 3.5, test "the nonce of record n".

Fault: the plan takes 4 zero bytes and the counter in little-endian
order. RFC 8446 5.3 encodes the sequence number in network byte order
(big-endian), pads it on the left to 12 bytes, and XORs it with a
static write IV (not checked). Noise revision 34, section 12, for
AESGCM: 32 bits of zeros, then the counter in big-endian order. The
little-endian form is the Noise form for ChaChaPoly (not checked). Fact
1.5 omits the byte order and the XOR. Each session key is new, so no
IV is needed for uniqueness, and the Noise AESGCM form fits.

Fix: take the Noise AESGCM form: 4 zero bytes, then the counter of 64
bits in big-endian order. Fact 1.5 cites Noise section 12 for the
form and RFC 8446 5.3 only for the counter per direction. The test
writes the expectation from that section.

### 5. Should fix: the hello has no protocol version

Place: plan 3.9. `program/frame.hpp:26-31`.

Fault: 3.9 says that a change of the cipher is a new version of the
peer protocol. No field carries a version. A node of the old binary
sends a plaintext `hello` without packet 1. The plan does not say what
the new node does with it.

Fix: add a version to the `hello`, or a protocol name in the form of
Noise. A node refuses a `hello` with another version, and refuses a
`hello` without packet 1. There is no plaintext mode. The plan states
that a rolling upgrade from the plaintext protocol is not possible.

### 6. Should fix: fact 1.3 is wrong in two points

Place: plan 1.3, 3.8. libhydrogen `impl/core.h:1-6`,
`impl/random.h:1-6`, `:49-60`, `impl/random/unix.h:17-33`, `:58-85`,
`impl/common.h:38-46`.

Fault:

- `hydro_init` always returns 0. A failure of the random source calls
  `abort` in `hydro_random_ensure_initialized` (random.h:53-54).
- It polls `/dev/random` until the pool is ready, and reads
  `/dev/urandom`. It does not read `/dev/random`.
- The state of the generator is thread-local (`static TLS`,
  random.h:1; `TLS` is `__thread`, common.h:42). `hydro_init` in one
  thread does not set up another thread. serve.cpp runs one loop in the
  main thread, so this holds today. A handshake in another thread
  sets up the generator at run time and can call `abort` there.
- `hydro_kx_kk_*` returns -1 when an X25519 result is zero (a key of
  low order). This is the "failed packet" path, and it is an input
  error.

Fix: correct fact 1.3. 3.8 says that `hydro_init` is called in the
thread that runs the handshakes, before the first connection. The plan
names `abort` at start as a deviation from the error rule (a machine
failure is raised as `std::system_error`) and gives it to the owner.
konsensd does not fork, so a copied generator state is not a risk
today; the plan states this as a fact.

### 7. Should fix: the frame length and `max_frame_bytes` do not agree with the tag

Place: plan 3.5. `program/frame.cpp:90-110`,
`program/serve.cpp:418`.

Fault: the plan does not say whether the length counts the ciphertext
alone or the ciphertext with the tag. The sender checks
`bytes->size() > max_frame_bytes` on the plaintext (serve.cpp:418).
If the length holds the tag, a message of exactly `max_frame_bytes`
is refused by the receiver (frame.cpp:99). A length below 16 bytes has
no tag.

Fix: the length counts ciphertext and tag, as in the TLSCiphertext
record of RFC 8446 5.2 (not checked). The receiver allows
`max_frame_bytes` plus the tag size. A length below the tag size
closes the connection. The source of the AAD is RFC 8446 5.2, where
the additional data is the record header with its length (not
checked). Decrypt in place in the frame string, so that no second copy
is made.

### 8. Should fix: the form of the first frame is not defined

Place: plan 3.4. `program/frame.cpp:121-133`.

Fault: "`hello` and packet 1 in one frame" has no layout.
`decode_hello` refuses a payload that has bytes after the CBOR item
(frame.cpp:125).

Fix: packet 1 becomes a field of `hello` (a byte string of 48 bytes)
in the CBOR struct, or the plan states a layout with an explicit
length. The decoder refuses a packet of another size.

### 9. Should fix: the first frame has no own size limit and no deadline

Place: plan 3.4. `program/serve.cpp:478-489`, `:527-529`.

Fault: before the handshake, the peer is unknown. It can send a length
up to `max_frame_bytes` (2^30 bytes by default), and the node buffers
it. A connection that sends nothing stays open with no end. Each
`hello` with a known pid costs two X25519 operations before the MAC
check (kx.h:290-291). Each failure writes one log line (plan 3.4), so
an attacker controls the size of the log. This is not measured.

Fix: the first frame has its own limit, the size of the largest valid
`hello`. A connection that has no keys after a fixed number of ticks
closes. The log line for a failed handshake is limited in rate, or is
a counter. The handshake time of section 5 also measures the cost of
a refused packet 1.

### 10. Should fix: key erasure is incomplete

Place: plan 3.7.

Fault: the plan erases the session keys only. The initiator keeps the
ephemeral secret key in `hydro_kx_state` (hydrogen.h:193-196), and
libhydrogen does not erase it after `hydro_kx_kk_3` (kx.h:308-326).
`hydro_kx_kk_2` keeps its state on the stack and does not erase it
(kx.h:280-305). The caller cannot erase that state. A connection
is one-way (finding 1), so one of `rx` and `tx` is not used.

Fix: `hydro_memzero` on the `hydro_kx_state` after `hydro_kx_kk_3`
and when a connection closes before it. The key that is not used is
erased at once. The buffer that held the key file is erased after the
read. The plan states that the stack state of `hydro_kx_kk_2` is not
erased. A connection holds one `EVP_CIPHER_CTX`, not two. Whether
`EVP_CIPHER_CTX_free` erases the key schedule is not checked in the
OpenSSL source; the plan checks it.

### 11. Should fix: the error paths are not listed

Place: plan 3.1-3.7. `program/config.hpp:48-55`,
`program/main.cpp:20-37`.

Fault: the owner decides each error path. The plan does not list
them. Open: a key file that is missing, too short or with wrong mode;
a public key that is not valid base64 or not 32 bytes; a duplicate
public key; a revoked key in the peers; an EVP call that returns 0 at
run time; `--generate-key` on a file that exists.

Fix: list each path with its form. Proposal: a configuration error is
a new `config_errc` value (for example `duplicate_public_key`,
`revoked_public_key`), and `name_of` in main.cpp names each. A failed
tag or a failed handshake closes the connection (input from outside).
An EVP call that fails during a seal or an open is raised (a fault of
the machine or of ours). `--generate-key` opens with `O_EXCL` and
refuses a file that exists.

### 12. Should fix: the check of the key file mode is too weak

Place: plan 3.1.

Fault: "readable by others" leaves the group open. OpenSSH refuses a
private key file with any bit in `0077` (not checked in its source).

Fix: konsensd refuses a key file whose mode has any bit of `0077`, or
whose owner is not the user of the process. The test also covers mode
0640.

### 13. Should fix: the specifications are not in the tree

Place: plan 1.5, 2, 3.5. `refs/`.

Fault: the rule "A grammar is read, not remembered" puts each
specification verbatim in the tree, in the same commit as the first
function that reads it. `refs/` has no RFC 8446, no Noise
specification and no RFC 4648. Facts 1.5 and 2 were not checked
against a text in the tree.

Fix: the plan adds `refs/rfc8446.txt`, the Noise specification
revision 34, and `refs/rfc4648.txt` (base64, section 4) to the commit
of the first function that reads each. Facts 1.5 and section 2 are
checked there before the build.

### 14. Should fix: the revocation check at the handshake cannot be reached

Place: plan 3.3, test "a revoked key is refused at the handshake".

Fault: a node refuses a configuration in which a peer has a revoked
key. A handshake takes the key from the peer list only. So a revoked
key never reaches the handshake check, and the test cannot be written
as it stands.

Fix: keep the check in the configuration only, and remove the
handshake check and its test. The plan states that the revocation list
keeps a replaced key from coming back into the peer list. Question 6.1
stays with the owner.

### 15. Should fix: names with no source

Place: plan 3.6, 3.1, 3.2.

Fault: `max_records_per_key` has no source.

Fix: "confidentiality limit" from RFC 9001 6.6 (finding 2). The
option `--generate-key` follows `wg genkey`; `revoked_public_keys`
follows `RevokedKeys` of OpenSSH. The plan writes the source next to
each provisional name.

### 16. Should fix: the build change is not in the plan

Place: plan 3.7. `CMakeLists.txt`.

Fault: konsensd does not link libcrypto today. "Fetched once at start"
is `EVP_CIPHER_fetch`, which exists from OpenSSL 3.0.

Fix: the plan names the distribution package of OpenSSL 3 libcrypto,
the minimum version and the CMake target. `deps/libhydrogen` is
checked out before the build.

### 17. Note: the facts that hold

- 1.1: frame.hpp:14 (4 bytes), :26-31 (`hello`), :33-36 hold. The
  length is big-endian (frame.cpp:62-74). The first frame is the
  `hello` (serve.cpp:612, :489).
- 1.2: hydrogen.h:216-230, :188-191, kx.h:257 hold. The pattern is KK
  with the primitives of libhydrogen (Gimli), not a Noise protocol name
  with standard DH, cipher and hash.
- 1.3 holds for `hydro_kx_kk_2` too: it makes an ephemeral key pair
  (kx.h:295).
- 1.4: the numbers match
  `bench/results/2026-10-29-212437Z-peer-aead.txt` at 7015b75. They are
  medians per compiler, not one median over the three. AES-256-GCM
  seal at 64 B is 77.2 ns (clang 22) to 80.1 ns (g++ 15). Write the
  range. libhydrogen was built for baseline x86-64 in that run.

### 18. Note: the handshake and the directions are right

- KK gives mutual authentication. The initiator authenticates the
  responder by the MAC of packet 2, which depends on its own fresh
  ephemeral key (kx.h:315-322). The responder authenticates the static
  key of the initiator by the MAC of packet 1 (kx.h:288-292), without
  freshness (finding 3).
- Key compromise of the responder lets an attacker forge packet 1, but
  not the session keys, because se needs the key of the initiator. The
  payload of packet 1 is empty, so nothing leaks.
- The responder derives `(tx, rx)` (kx.h:301). The initiator derives
  `(rx, tx)` (kx.h:323). So the `tx` of one side is the `rx` of the
  other. The plan uses `tx` to seal and `rx` to open.
- Plan 3.4 says "a wrong pid fails in packet 2". The failure is in
  `hydro_kx_kk_2` on the accepting side, at the check of packet 1.
  Correct the words.
- The keys are new for each connection, so a counter that starts at 0
  gives a unique nonce across reconnects and restarts, as long as the
  generator gives new ephemeral keys.
- TCP keeps the order. A record that is repeated, missing or moved
  fails the tag, and the connection closes. This is correct.

### 19. Note: base64

`EVP_DecodeBlock` of OpenSSL counts the padding as output bytes. The
decoder checks that the result is exactly 32 bytes after the padding
is removed (not checked in the OpenSSL source).

### 20. Should fix: missing tests

Add these tests:

- A recorded first frame, sent again, does not close the live
  connection and does not call `reconnected` (finding 3).
- A connecting node sends no record before `hydro_kx_kk_3` succeeds
  (finding 1).
- A packet 2 with a flipped byte, or from a node with another key at
  the address, closes the connection at the connecting node.
- A `hello` with the pid of node B and packet 1 made with the key of
  node C is refused.
- A `hello` without packet 1, with a packet of another size, or with
  another version is refused (findings 5, 8).
- The encrypted `hello` in the first record differs from the
  plaintext `hello`: the connection closes (finding 3).
- `tx` of the initiator equals `rx` of the responder, read from
  `hydro_kx_kk_2` and `hydro_kx_kk_3` directly.
- A known-answer test of the seal: key, nonce, AAD and plaintext from
  a published AES-256-GCM test vector give its ciphertext and tag.
- A frame of `max_frame_bytes` passes, one byte more is refused. A
  length of 0 to 15 bytes closes the connection (finding 7).
- The byte limit closes at the limit and not before (finding 2).
- A first frame larger than the `hello` limit, and a connection that
  sends nothing until the deadline, close (finding 9).
- Configuration: a peer without `public_key`, a key that is not
  base64, a key of 31 or 33 bytes, two peers with one key, a peer with
  a revoked key. Each gives its `config_errc`, and `name_of` names
  each value.
- Key file: missing, of a wrong size, mode 0640, mode 0604, another
  owner. `--generate-key` on a file that exists is refused.
- `program/test/three_nodes.sh` runs with keys.
