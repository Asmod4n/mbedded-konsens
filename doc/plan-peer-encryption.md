# Plan: the peers of konsensd talk through a key exchange and AES-256-GCM

Branch peer-encryption, from main ebd9f42. Owner 2026-10-29 21:30:
"Zum Client werden wir TLS sprechen ... Intern will ich keine
Zertifikate haben ... alles was wir von Zertifikaten brauchen, ist eine
Revoke-Liste ... intern ein Key-Exchange-Verfahren ... und wir machen
nur AES."

Marks: "provisional" marks a form or a name that waits on the owner.

## 1. Facts checked in this session

1. Peers of konsensd talk over TCP. A frame is a length of 4 bytes and
   a payload (program/frame.hpp:14, :33-36). The first frame of a
   connection is `hello {cluster_name, pid}` (:26-31). The library
   does no network IO; all of this is in program/.
2. libhydrogen offers `hydro_kx_kk_1`, `hydro_kx_kk_2` and
   `hydro_kx_kk_3` (deps/libhydrogen/hydrogen.h:216-231), the Noise
   pattern KK (impl/kx.h:257, "Noise_KK_hydro1"). Both sides know the
   static public key of the other side before the handshake. Each
   packet has 48 bytes. The result is one key of 32 bytes for each
   direction (`rx`, `tx`, hydrogen.h:188-191).
3. `hydro_kx_kk_1` and `hydro_kx_kk_2` make an ephemeral key pair from
   the random generator of libhydrogen, which needs `hydro_init`.
   `hydro_init` reads /dev/random and can call `abort`.
4. Measured on this machine (bench/results/2026-10-29-*-peer-aead.txt
   on branch chain-hash, 7015b75), median over g++ 15, g++ 16 and
   clang 22: to seal one message, AES-256-GCM of OpenSSL takes 78 ns at
   64 B and 3.9 µs at 64 KiB; hydro_secretbox takes 2.0 µs to 9.9 µs at
   64 B and 0.6 ms to 3.0 ms at 64 KiB.
5. RFC 8446 5.5: with AES-GCM, at most 2^24.5 full-size records are
   protected under one key. RFC 8446 5.3: the nonce of a record is a
   counter of 64 bits per direction, padded to 12 bytes.

## 2. What others do

- WireGuard: one fixed cipher, no negotiation. Each peer is named by its
  static public key in base64 in the configuration. To revoke a peer,
  its key is removed. A key pair comes from `wg genkey`, and the private
  key lives in a file that only its owner reads.
- OpenSSH: `RevokedKeys` names a file of public keys that the server
  refuses, whatever else allows them.
- Noise (revision 34): a protocol name fixes the pattern, the DH, the
  cipher and the hash. A change of any of them is a new protocol name.

## 3. The change

1. Each node has one static key pair of hydro_kx. `konsensd
   --generate-key <file>` writes the secret key and the public key to a
   new file with mode 0600, and prints the public key in base64
   (provisional form). konsensd refuses to start when the key file is
   readable by others.
2. The configuration names the key file (`node.key_file`) and the
   public key of each peer (`peers[].public_key`, base64). A peer
   without a key, or two peers with the same key, is a configuration
   error.
3. The revocation list is a list of public keys,
   `cluster.revoked_public_keys`, in the configuration (provisional
   form, see 6.1). A node refuses a handshake with a revoked key, and
   refuses a configuration in which a peer has a revoked key.
4. The connecting node sends `hello` and packet 1 of KK in one frame.
   The accepting node looks up the public key of `hello.pid`, checks
   the revocation list, and answers with packet 2. A pid with no key,
   a revoked key or a failed packet closes the connection and writes
   one log line with the pid and the address. A wrong pid fails in
   packet 2, because KK checks the static key of the sender.
5. After the handshake, each frame is the length of 4 bytes, then the
   ciphertext and a tag of 16 bytes. The length is the additional data
   of GCM. The nonce is 4 zero bytes and a counter of 64 bits in
   little-endian order, one counter per direction, starting at 0. The
   counter is never sent; a record out of order fails the tag.
6. A failed tag closes the connection. A connection that reaches the
   record limit of fact 1.5 closes, and the next connection makes new keys.
   The limit is one named constant, `max_records_per_key` (provisional
   name), 2^24.
7. Each connection holds one `EVP_CIPHER_CTX` per direction, set up
   once with its key; the session keys are erased with
   `OPENSSL_cleanse` after that. AES-256-GCM is fetched once at start.
   A libcrypto without AES-256-GCM stops the start with an error.
8. `hydro_init` is called once, at the start of konsensd, before any
   connection.
9. No other cipher and no negotiation. A change of the cipher is a new
   version of the peer protocol.
10. The library does not change.

## 4. Tests

- Two nodes with the right keys exchange frames both ways.
- A node with a key that the other side does not know is refused.
- A revoked key is refused at the handshake and in the configuration.
- A flipped byte in a ciphertext, a tag or a length closes the
  connection.
- A record sent twice, or two records swapped, closes the connection.
- The record limit, passed as a value to the function that checks it,
  closes the connection at the limit and not before.
- The nonce of record n equals 4 zero bytes and n in little-endian
  order.
- A key file readable by others stops the start.

## 5. Measurement

The comparison bench of doc/plan-bench-distributed-logs.md, konsensd
rows, before and after, both builds. Also the time of one handshake.
No number about the cost is written before these runs.

## 6. Questions for the owner

1. The revocation list stands in the configuration (the form of
   OpenSSH). A change needs a restart of each node. Another form is an
   entry in the log, which reaches every node at the same index; no
   system of section 2 does that.
2. The key file and its form (3.1).

## 7. What this plan does not do

- No TLS to clients; that is a plan of its own.
- No change of the wire of the library messages.

## 8. Data of this plan

- Build directories under the scratchpad: made again by the build
  command.
- Result files under bench/results: recorded in the state file.
