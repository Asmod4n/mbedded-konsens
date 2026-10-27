# Plan: a node program for a cluster of three machines

Goal (owner, 2026-10-27 18:07): the cluster runs on the first three
machines, and clients on other machines use it. The first clients are
curl (owner, 18:13). The management tool writes the config of each node
(owner, 18:06); its `init` command is step 1 of
doc/plan-management-tool.md on branch management-tool-plan. This plan
builds the program that runs one node from that config.

## 1. Facts checked in this session

1. The library has no network code and no program that runs a node.
   `examples/three_nodes.cpp` runs three nodes in one process and
   moves the messages in memory.
2. `konsens::encode(const message&)` gives a `std::string`, and
   `konsens::decode(std::string)` gives a `received`
   (include/konsens/wire.hpp:57-58). The bytes carry no length.
3. The node gives its messages through `take_outgoing_messages()`
   (node.hpp:475). It takes messages through `handle_incoming`
   (node.hpp:238), at most `max_batch_size` = 1024 for each commit
   (node.hpp:34, 536-540). Output is released only by `commit`
   (node.hpp:255-283). After a lost connection the program calls
   `reconnected(pid)` (node.hpp:508; counter_stress.cpp:392-396 calls it
   on both nodes). The model assumes FIFO for each pair of nodes
   (owner, 2026-10-20 06:22: TCP).
4. A client call (`register_client`, `client_request`, `client_query`,
   `expire_session`) returns a transaction; the reply comes later
   through `take_client_replies()`. A follower forwards a register and
   a request (`propose_a_request`, node.hpp:892-899). `expire_session`
   works on the leader only and gives `retry` elsewhere (node.hpp:446).
5. A start without a clean stop retires the node (plan-clean-stop).
   `stop()` is a wrong use on a retired node, after an error of the
   store, after a fault and while a step is open (node.hpp:192-196).
6. `accept_sync` carries the whole suffix from the valid index
   (sequence_paxos.hpp:1494-1506), with no bound on its size.
7. slipstreamIO carries the liburing API on Linux (the kernel) and on
   macOS and the BSDs (its engine). The engine answers when the kernel
   refuses io_uring. It has no timeout opcode, but it has timed waits
   (`IORING_FEAT_EXT_ARG`). `slipstream_signal_open` gives a signal as a
   descriptor on every platform (src/slipstream_signal.h).
8. Measured in this session (scratchpad file hyp-node/h.cpp): podman's
   default seccomp profile refuses `io_uring_setup` with -38 (ENOSYS);
   with `seccomp=unconfined` the kernel answers, and
   `io_uring_wait_cqe_timeout` works. toml++ 3.4 parses without
   exceptions under g++ 16.
9. mruby-webmachine (branch integration, 8c76f51) parses HTTP/1.1 in
   plain C++ with no mruby: src/http.hpp, src/http1.hpp
   (`http1::parse_request`, at most 8192 bytes of a request head), and
   writes problem details in src/problem.hpp. http.hpp needs ada
   (Ubuntu package libada-url-dev 3.4.4).
10. Packages in Ubuntu 26.10 (the build image): g++-16 16.2.0,
    libtomlplusplus-dev 3.4.0. The config file is TOML, read with
    toml++, for the program only (owner, 2026-10-17 06:54 and 06:58).

## 2. Decisions of the owner for this plan

1. Clients speak HTTP/1.1 (card, 18:12), as REST, so that every
   client talks to the cluster directly (owner, 18:18). The API
   expresses what the clients speak among themselves, CoAP over TCP
   (owner, 18:16). Everything is plain REST; the one addition is the
   method QUERY (IETF httpbis, "The HTTP QUERY Method"; status not
   checked here), whose body is a read in SQL (owner, 18:19).
   mruby-webmachine knows QUERY (src/http.hpp:230).
2. HTTP comes from mruby-webmachine, which is renamed to
   embedded-webmachine later (owner, 18:13). It is a submodule.
3. io_uring comes from slipstreamIO, a submodule that builds liburing
   from its source with the shim (card, 18:14).

## 3. The form

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
   leader_priority = 0

   [[peers]]
   pid = 2
   address = "192.0.2.2:7151"
   ```

   Optional fields of `[node]` with defaults: `election_ticks`,
   `resend_ticks`, `tick_milliseconds`, `max_frame_bytes`,
   `max_body_bytes`, `build_from` (a list of pids, for a node of a next
   configuration). The two port numbers are provisional; 7150/udp is
   the discovery beacon (owner, 2026-10-21). The nonce of each start
   comes from `std::random_device`.
3. The first start of a node is the flag `--first-start` (owner,
   2026-10-27 15:42). It is never part of the standing command of a
   service: a later start with it on a used store is a wrong use, and
   the program ends.
4. Between the nodes: TCP. Each node opens one connection to each peer
   and only sends on it; it only reads on the connections that it
   accepts. So each direction is one TCP stream, and FIFO holds for
   each direction.
   - A frame is a 32-bit length in network byte order and the bytes of
     `konsens::encode`. The length reserves nothing: the buffer grows
     with the bytes read. A frame longer than `max_frame_bytes` closes
     the connection.
   - The first frame of a connection names the cluster and the pid of
     the sender. A wrong cluster name or an unknown pid closes it. A
     message whose `from` is not that pid, or whose `to` is not the own
     pid, closes it.
   - A new accepted connection of a pid closes the old one before the
     program handles any frame of the new one.
   - The program calls `reconnected(pid)` when its outgoing connection
     to the pid opens again, and when an accepted connection of the pid
     replaces an older one.
5. One step of the loop:
   1. Wait on the ring until a completion comes or the next tick is
      due (a timed wait). Read the clock once.
   2. Take the completions: frames, HTTP requests, accepts, the signal.
   3. Give the frames to `handle_incoming` in batches of at most
      `max_batch_size`; call the client calls; call `tick` when it is
      due. All calls of one step collect into one transaction.
   4. Commit once (group commit: all that waits, no fixed wait).
   5. Take the messages and the replies, and queue the sends.
   An error value of `commit` drops the output of that step; the HTTP
   clients of that step get the error of 3.7.
6. Clients: HTTP/1.1, one request at a time on each connection (no
   pipelining: the program reads the next request after its reply).
   - `PUT /kv/{key}` with the value as body, `GET /kv/{key}`,
     `DELETE /kv/{key}`. Key and value are bytes; the key is
     percent-encoded in the path.
   - A body longer than `max_body_bytes` gets 413. The body buffer
     grows with the bytes read, never with Content-Length.
   - `QUERY /sql` with an SQL statement as body (media type
     `application/sql`, RFC 6922) is a read at the read index. It comes
     after the key-value form, as its own step: the state machine must
     first keep its state in tables that SQL can read (open, see the
     SQL form of the owner, 2026-10-26).
   - Every error that the SQL of a client can cause is refused at once
     with a 4xx status and a problem detail (owner, 18:21): each
     primary result code of SQLite that a statement can give gets its
     status, for example a syntax or name error 400, a write in a
     QUERY 403 (QUERY is safe), a constraint 409, a value too big 413.
     A fault of the machine (I/O, a full disk, a failed sync) is not a
     client error and keeps the rules of the library. The test checks
     every primary result code (the whole set of sqlite3.h).
   - Any node takes a call. A follower forwards writes and asks the
     leader for the read index, as the library does now.
7. Status of each result (every value of `client_errc` and the rest):
   - 200 with the value; 204 after a write or a delete; 404 for a key
     that is not there.
   - Problem details of RFC 9457 (`application/problem+json`):
     503 with `Retry-After` for `retry`, `recovering`, `reconfiguring`
     and `retired`; 409 for `rejected`; 503 of the type "outcome
     unknown" for `stopped`, because the write may still be decided;
     `session_expired` and `stale_request` cannot reach a client (3.8)
     and give 500; any other error code gives 500.
8. Sessions (provisional, the session decides it; owner question 6.2
   asks for the form that lasts):
   - Each client connection is one session. The program registers it
     before the first write of that connection, and gives each write
     the next request number. A write that waits blocks the next
     request of that connection, so the numbers of one session are
     decided in order.
   - When the connection closes, the program expires the session. The
     library forwards `expire_session` from a follower to the leader,
     as it forwards a request (a change of node.hpp; the decided expiry
     of a session that is gone changes nothing).
   - A connection that closes before its register is decided: the
     program expires the session when the register reply comes.
   - The sessions of a node that ended with SIGKILL stay until
     question 6.2 is decided.
   - A write without a reply (the connection is lost, or `stopped`)
     may still be decided later. That is the contract of a call with
     no reply in a linearizable store. The README says so.
9. The state machine is a key-value map. A write is the CBOR array
   `[op, key, value]` (op 1 put, 2 delete); the reply is empty. A read
   is the key; the reply is CBOR: the byte string of the value, or
   `null` when the key is not there.
10. SIGTERM and SIGINT come through `slipstream_signal_open`; the
    program blocks them first. Then:
    - The program ends the step, commits it, and calls `stop()` when
      the node is not retired and has no error. It sends the replies
      that `stop()` gives (`stopped`), closes the sockets and exits
      with 0.
    - A retired node, or a node with an error of the store, exits with
      0 and 1 without `stop()`.
    - A failed sync in `stop()` throws `sync_error`; `main` records it
      and exits with 1.
11. The program refuses root (the library does already).
12. Logs: one line per event on stderr (start and its kind, stop,
    retirement, a connection to a peer opened or lost, a change of
    leader). The record format of logging.md comes later.

## 4. What this plan does not do

- TLS, authentication, the sandbox, the management protocol. The VPN
  protects the first tests (owner, 2026-10-26 14:40, question 6.7).
- Stufe 2, compaction, the SQL form for clients.
- The `init` command and the `repair` button of `konsensctl`; that is
  the management thread. This plan gives it the config of 3.2.
- A bound on the entries of one `accept_sync` (fact 6). Until then,
  `max_frame_bytes` is large (default 1 GiB), and a follower that lags
  by more than that does not catch up. It goes on the list of open
  work.

## 5. Deployment

One container image per architecture (amd64, arm64), built in podman
from the packages of Ubuntu 26.10 (`containers/konsensd/Containerfile`).
The data directory is a volume. The program runs under a podman
quadlet unit with a stop timeout that is longer than one clean stop, so
`podman stop` and a reboot of the host send SIGTERM and wait; a SIGKILL
retires the node. The default seccomp profile stays: slipstreamIO
answers when the kernel refuses io_uring. On macOS, podman runs the
image in its Linux machine.

## 6. Questions to the owner

1. After a SIGKILL or a power loss the node retires for good, and only
   a reconfiguration with a new pid brings a node back (`repair` in
   the management tool, not built yet). A power loss on all three
   machines retires the cluster. For the first use: (a) build a
   `konsensctl repair` for one retired node before the first
   deployment; (b) deploy first, and build it next. Proposal: (b) with
   a UPS on each machine; the data stays on the disks either way.
2. Sessions that stay after a SIGKILL of the node that held their
   connections: (a) the leader expires the sessions registered through
   a pid when a reconfiguration removes that pid; (b) a session
   expires after a decided number of entries without a write. Proposal:
   (a): it follows the owner rule "the end of a session is a log
   entry", and a removed pid holds no connection.
3. The three machines and their systems.

## 7. Tests

1. Config: each field, a missing field, a wrong type, an unknown key.
2. Frames: a round trip; a frame cut in two reads; a length above the
   bound; a wrong cluster name; a message with another `from` or `to`.
3. HTTP mapping: each method; each value of `client_errc` and a code of
   another category give their status (the whole set); a body above
   the bound gives 413; an empty value round trip.
4. The key-value machine: put, get, delete, get after delete, an empty
   value, a key that is not there.
5. Three processes on one machine (ctest, loopback, curl as client):
   - clients put and get on each node, and read what another node
     wrote;
   - SIGTERM to one node and a start again: it votes again;
   - SIGTERM to the leader during writes: the writes that got 204 are
     there, and each write is decided at most once per request number;
   - SIGKILL to one node and a start again: it is retired, it answers
     503, and the other two serve the clients;
   - SIGTERM to a retired node: it exits with 0;
   - SIGTERM to all three and a start again: the data is there;
   - `--first-start` on a used store: the program ends with an error;
   - a loss of each direction of one TCP pair: the node calls
     `reconnected`, and the cluster decides again;
   - 1000 connections through a follower, each with one write: the
     session count of the leader returns to its start value;
   - a follower that is stopped during 10000 writes catches up after
     its start;
   - a burst of more than `max_batch_size` frames in one step.
6. The image: the test of 7.5 runs inside it, with the default seccomp
   profile, so the slipstreamIO engine answers.

## 8. The model

The program changes no step of the protocol. It moves the messages
that the library gives, keeps FIFO for each direction, and calls
`reconnected` after a lost connection, as the model assumes. The
forwarded `expire_session` (3.8) is a client entry like a request; the
model has no client entries. So no new model run (owner card 18:12
"Braucht das Knoten-Programm einen neuen Modelllauf?", open).

## 9. Review 1 and changes

Review by a second agent, 2026-10-27: approve after changes. Taken:
1 (expiry forwarded, question 6.2), 2 (no fix: a write with no reply
may be decided later, 3.8), 3 (3.7), 4 (3.9), 5 (3.10), 6 (3.4),
7 (4, open work), 8 (3.5), 9 (3.10), 10 (3.2), 11 (5), 12 (owner card,
2.3), 13 (3.6, 3.8), 14 (7.5), 15 (question 6.1).

## 10. Order

Each item is a commit, green before the next:
1. Submodules slipstreamIO and mruby-webmachine; the CMake target.
2. Config (3.2) with its tests.
3. Frames and the loop (3.4, 3.5) with three processes that elect.
4. The key-value machine (3.9) and HTTP (3.6, 3.7).
5. Sessions (3.8) and the forwarded expiry in the library.
6. Signals and stop (3.10), then the tests of 7.5.
7. The image and its quadlet unit (5), the test of 7.6.
