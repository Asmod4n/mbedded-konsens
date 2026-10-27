# Plan: a node program for a cluster of three machines

Goal (owner, 2026-10-27 18:07): the cluster runs on the first three
machines, and clients on other machines use it. The management tool
writes the config of each node (owner, 18:06); its `init` command is
step 1 of doc/plan-management-tool.md on branch management-tool-plan.
This plan builds the program that runs one node from that config.

## 1. Facts checked in this session

1. The library has no network code and no program that runs a node.
   `examples/three_nodes.cpp` runs three nodes in one process and
   moves the messages in memory.
2. `konsens::encode(const message&)` gives a `std::string`, and
   `konsens::decode(std::string)` gives a `received`
   (include/konsens/wire.hpp:57-58). The bytes carry no length.
3. The node gives its messages through `take_outgoing_messages()`
   (node.hpp:475). It takes messages through `handle_incoming`
   (node.hpp:238). After a lost connection the program calls
   `reconnected(pid)` (node.hpp:508). The model assumes FIFO for each
   pair of nodes (owner, 2026-10-20 06:22: TCP).
4. A client call (`register_client`, `client_request`,
   `client_query`, `expire_session`) returns a transaction; the reply
   comes later through `take_client_replies()`. `expire_session` works
   on the leader only and gives `retry` elsewhere (node.hpp:446).
5. A start without a clean stop retires the node (plan-clean-stop).
   So the program must call `stop()` on SIGTERM and SIGINT, or every
   restart costs a node.
6. slipstreamIO carries the liburing API on Linux (the kernel) and on
   macOS and the BSDs (its engine: kqueue). The engine has no timeout
   opcode, but it has timed waits (`IORING_FEAT_EXT_ARG`,
   slipstreamIO README).
7. Packages in Ubuntu 26.10 (the build image): g++-16 16.2.0,
   libtomlplusplus-dev 3.4.0, liburing-dev 2.15, libllhttp-dev 9.4.3.
   mruby-webmachine has an HTTP/1.1 parser in plain C++
   (src/http1.hpp, src/http.hpp; it needs ada).
8. The config file is TOML, read with toml++ 3.4, for the program only
   (owner, 2026-10-17 06:54 and 06:58).

## 2. The form

1. Program `konsensd` (provisional; the pattern of `konsensctl`, owner
   2026-10-26 14:40). One process, one thread, one ring.
2. Config (TOML), written by `konsensctl init`:

   ```toml
   [cluster]
   name = "home"
   configuration_id = 1

   [node]
   pid = 1
   data_directory = "/var/lib/konsens"
   peer_address = "[::]:7151"
   client_address = "[::]:7152"

   [[peers]]
   pid = 2
   address = "192.0.2.2:7151"
   ```

   The two port numbers are provisional; 7150/udp is the discovery
   beacon (owner, 2026-10-21). Election and resend ticks and the tick
   length have defaults and may be set in `[node]`.
3. The first start of a node is the flag `--first-start` (owner,
   2026-10-27 15:42: the caller states the first start). Without the
   flag, the library decides as plan-clean-stop says.
4. Between the nodes: TCP. Each node opens one connection to each
   peer and only sends on it; it only reads on the connections that it
   accepts. So each direction is one TCP stream, and FIFO holds for
   each direction. A frame is a 32-bit length in network byte order
   and the bytes of `konsens::encode`. The first frame of a
   connection names the cluster and the pid of the sender; a wrong
   cluster name closes the connection. When a connection to a peer
   opens again, the program calls `reconnected(pid)`.
5. A frame longer than a named bound closes the connection. The bound
   is a config field with a default.
6. Ticks: the ring waits with a timeout equal to the time to the next
   tick (a timed wait, fact 6). The clock is read once for each loop
   and passed down.
7. Clients: HTTP/1.1 (question 1).
   - `PUT /kv/{key}` with the value as body, `GET /kv/{key}`,
     `DELETE /kv/{key}`. Key and value are bytes.
   - Each client connection is one session: the first write registers
     it, each write takes the next request number. When the connection
     closes, the program expires the session; on a follower it sends
     `expire_session` again at later ticks until a leader takes it, so
     the session table does not grow with old connections.
   - Any node takes a call. A follower forwards writes and asks the
     leader for the read index, as the library does now.
   - Status: 200 with the value, 204 after a write or a delete, 404
     for a key that is not there. An error of the library is a
     problem detail of RFC 9457 (`application/problem+json`) with
     status 503 for `retry`, `recovering`, `retired`, `stopped` and
     `reconfiguring`, and 409 for `rejected`.
8. The state machine is a key-value map (question 3). A write is the
   CBOR array `[op, key, value]` (op 1 put, 2 delete); the reply is
   empty. A read is the key; the reply is the value or an empty marker
   for "not there". Put and delete are idempotent, so a write that a
   client sends again after a lost connection changes nothing.
9. SIGTERM and SIGINT: the program ends the open step, commits it,
   calls `stop()`, closes the sockets and exits with 0. A failed sync
   in `stop()` throws `sync_error`; `main` records it and ends with a
   failure.
10. The program refuses root (the library does already).
11. Logs: one line per event on stderr (start, stop, retirement,
    connection to a peer opened or lost, leader change). The record
    format of logging.md comes later.

## 3. What this plan does not do

- TLS, authentication, the sandbox, the management protocol. The VPN
  protects the first tests (owner, 2026-10-26 14:40, question 6.7).
- Stufe 2, compaction, the SQL form for clients.
- The `init` command of `konsensctl`; that is the management thread.
  This plan gives it the config format of 2.2.

## 4. Deployment

One container image per architecture (amd64, arm64), built in podman
from the packages of Ubuntu 26.10 (`containers/konsensd/Containerfile`).
The data directory is a volume. On macOS, podman runs the image in its
Linux machine (question 4).

## 5. Tests

1. Config: each field, a missing field, a wrong type, an unknown key.
2. Frames: a frame round trip; a length above the bound; a frame cut in
   two reads; a wrong cluster name.
3. HTTP mapping: each method and each status of 2.7; each error code
   of `client_errc` gives its status (the whole set).
4. The key-value machine: put, get, delete, get after delete, an empty
   value.
5. Three processes on one machine (ctest, loopback):
   - clients put and get on each node, and read what another node
     wrote;
   - SIGTERM to one node and a start again: it votes again;
   - SIGKILL to one node and a start again: it is retired, and the
     other two serve the clients;
   - SIGTERM to all three and a start again: the data is there.
6. The image: the test of 5 runs inside it.

## 6. The model

The program changes no step of the protocol. It moves the messages
that the library gives, keeps FIFO for each direction, and calls
`reconnected` after a lost connection, which the model assumes.
Proposal: no new model run (question 5).

## 7. Questions to the owner

1. Client protocol: HTTP/1.1 (curl and every language) or the CBOR
   frames of 2.4 with a client library. Proposal: HTTP/1.1.
2. HTTP parser: mruby-webmachine's C++ parser as a submodule (a new
   exception to the package rule) or llhttp from the package.
   Proposal: llhttp (a package, no exception).
3. The first state machine: a key-value map of bytes, or wait for the
   SQL form. Proposal: key-value map.
4. The three machines: which ones, and with which system? The image
   needs amd64 or arm64 Linux, or podman on macOS.
5. No new model run for the program (section 6). Proposal: yes.

## 8. Order

This plan, review 1, the cards of section 7, then 2.2 config, 2.4-2.6
frames and loop, 2.8 machine, 2.7 HTTP, 2.9 stop, the tests of 5.5,
the image.
