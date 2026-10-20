# mbedded-konsens

mbedded-konsens is a consensus library in C++. It follows the form of
Omni-Paxos. It has three parts:

- Sequence Paxos replicates the log.
- Ballot Leader Election elects the leader.
- StopSign changes the configuration.

The library does no network IO. A separate binary does the IO. The
library keeps its state in an internal store.

## Design

- The library takes C++ types only. It has no C API and starts no
  threads.
- An entry counts as stored only after a synchronous commit to disk.
- After each start, a node votes only after f+1 answers.
- Reads go through a read index. An operator can enable a stale read
  for clients.
- A single-node mode starts only by a flag at the start of the process.
  Stage 1 reads only. Stage 2 reads and writes at one node.
- A wrong use of the public API does not compile. What the compiler
  cannot check is checked at run time. A use that neither can check is
  not offered.
- Stored memory grows with the active state, not with the number of
  changes.

## Current state

The repository holds formal models in TLA+ and library v0.1 in C++.

- TLC checks safety under message loss, duplicate messages, a crash
  and a disk rollback, at lowered bounds.
- The model found that the Omni-Paxos source lets `decided_idx` fall
  after a late Promise. The fix keeps the higher value. It is reported
  upstream.
- Each property has a mutation of the model that makes TLC find a
  violation.
- Reconfiguration and single-node mode are checked by simulation only.

## Library v0

### What it contains

- `konsens::node<S>` joins Sequence Paxos and Ballot Leader Election.
  It does no IO. The caller calls `tick()`, gives each message to
  `handle()`, sends what `take_outgoing_messages()` returns, and reads
  `read_decided_suffix()`.
- The concept `konsens::storage` states what a store must do.
- `memory_storage` keeps the state in memory. The tests use it.
- `persistent_storage` keeps the state in LMDB. Each write is one
  transaction with a synchronous commit. Each log entry carries a
  CRC32C. `open` refuses the root user.

### What it does not contain

- No recovery after a start. A node that starts again does not wait
  for f+1 answers before it votes.
- No client sessions.
- No read index.
- No CBOR encoding of messages.
- No reconfiguration. StopSign is a kind of entry only.
- The channels between nodes must be FIFO. The library does not hold
  its properties when a channel reorders messages.

### Build

The build needs g++ 16 or clang 22 in C++23 mode. mbedded-cbor does
not compile with g++ 13 or g++ 14. Where the distribution has no such
compiler, the build runs in a container, for example Ubuntu 26.10 in
podman. Install the packages (Ubuntu 26.10):

```sh
sudo apt install g++-16 cmake pkg-config liblmdb-dev doctest-dev libboost-dev git
```

On openSUSE Tumbleweed, Boost comes from `libboost_headers-devel`.
mbedded-cbor is the submodule `deps/mbedded-cbor`. Check it out, then
build in a directory of your choice:

```sh
git submodule update --init deps/mbedded-cbor
CXX=g++-16 cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

### Run the tests

```sh
ctest --test-dir build
```

`test_simulation` runs three nodes on `memory_storage` for 2000 seeds.
`test_simulation_lmdb` runs three nodes on `persistent_storage` for 50
seeds. A crash in it closes the node and opens LMDB again from disk.
Both check the safety properties of the model in each state.

### Run the example

`three_nodes` starts three nodes in one process on LMDB. It appends
entries, stops one node, appends with two nodes, opens the stopped
node again and lets it catch up. It prints each step.

```sh
./build/three_nodes
./build/three_nodes dir-1 dir-2 dir-3
```

Without directories, the example uses new temporary directories and
removes them at the end. Each given directory must exist. The exit
code is 0 when all steps succeed.

The library refuses the root user. As root, the example stops with
exit code 2. The flag `--allow-root-for-test` gives the library a user
id that is not root. This flag is for the test machine only.

## Library v0.1

Library v0.1 adds recovery after each start, client sessions and the
read index to library v0.

### What the node API offers

- `konsens::node<S, M>` takes a storage `S` and a state machine `M`.
  `M` is a template parameter, so no call to it is indirect.
- The concept `konsens::state_machine` states what `M` must do.
  `apply(payload)` executes a request and returns its reply.
  `query(payload)` reads the state and changes nothing. The node keeps
  the client table. The state machine gets only the payloads.
- `node::with(config, storage, machine, nonce)` starts a node. The
  caller gives a new nonce at each start. The node applies the decided
  log from index 0 at each start.
- `register_client(id)` appends a RegisterClient entry. The session id
  is the log index of that entry.
- `client_request(id, session, request_number, payload)` appends a
  request. The first request number is 1. The state machine executes
  each request number of a session once. When the same request number
  comes again, the node gives the reply that it keeps.
- `client_query(id, payload)` reads through the read index. The leader
  takes its accepted index and confirms its leadership with one round
  of heartbeats to a majority. A follower asks the leader for this
  index. The node answers when its applied state reaches the index.
- `expire_session(id, session)` appends an ExpireSession entry. Only
  the leader takes this call. The caller decides when a session
  expires.
- `take_client_replies()` gives the replies. Each reply carries the
  `id` of its call. The caller gives this id, and the caller can drop
  a call that has no reply.

### Error values

Each call returns `std::expected`. A reply can also hold an error.

| Error value | Case |
|---|---|
| `recovering` | The node recovers and takes no call. |
| `session_expired` | The session has expired. Register again. |
| `stale_request` | The request number is below the last request number of the session. |
| `retry` | The node knows no leader, or its promise changed. Send the call again. |

### Recovery after each start

After each start with a stored promise, a node does not vote. It sends
Recovery with the nonce to all peers. It waits for f+1 answers, and
one answer must come from the primary. It then takes the durable
state of the primary and votes again. This is the recovery of
Viewstamped Replication Revisited.

When more than f nodes restart at the same time, no primary answers.
The recovery then waits. The operator must act. This rule is
provisional.

### What it does not contain

- No reconfiguration. StopSign is a kind of entry only.
- No compaction of the log.
- No single-node mode and no witness.
- The channels between nodes must be FIFO.
- When a connection to a peer drops and comes back, the caller calls
  `reconnected(pid)` on the node, as in Omni-Paxos. The node then
  syncs with that peer again. At each resend timeout, a leader in the
  accept phase sends a Decide with its decided index to each follower;
  a follower that lost the last message sees the gap and syncs again.

### Tests of v0.1

`test_simulation` runs three cases of 2000 seeds each on
`memory_storage`. The case with clients has two clients. They
register, send requests with request numbers, send a call again after
a timeout, read and expire sessions. One message is lost, one message
is duplicated, one node crashes and one node restarts from an earlier
copy of its storage. After each step, the test checks S1, S2, S2b, S3,
S5, S7, E1 and R1 of the TLA+ model:

- E1: each pair of session and request number is executed at most
  once. The test also compares the state of each node with the
  requests that the model executes.
- R1: the answer to a read contains each request that was decided
  before the read started.

`test_simulation_lmdb` runs the same clients for 50 seeds on
`persistent_storage`.

### The example

At the end, `three_nodes` runs a client. The client registers at a
follower and writes one value there. It then reads the value at the
other follower through the read index.

`counter` runs three nodes in memory with a state machine that counts.
A client registers once and sends five increments, each at another
node. A follower forwards its increment to the leader. The fifth
increment is sent again; the counter does not change, because the
session keeps the reply of each request number. The client then reads
the counter at each node through the read index. The example stops the
leader, waits for a new leader and increments once more.

```
./build/counter
```

### The stress form of the counter

`counter_stress` runs three nodes in memory with the same counter.
Many clients increment the counter. The network loses messages and
sends messages twice. Each channel stays FIFO. A lost message counts
as a dropped connection: the program calls `reconnected` at both ends
of the channel, unless `--no-reconnected` is given. Nodes restart from a
copy of their storage. A restart starts only while no node recovers.
After each tenth increment, a client reads the counter.

```
./build/counter_stress [--clients C] [--increments N] [--seed S] [--loss P] [--duplicate P] [--restart R] [--no-reconnected] [--cbor]
```

| Flag | Meaning | Default |
|---|---|---|
| `--clients` | The number of clients. | 4 |
| `--increments` | The increments of each client. | 1000 |
| `--seed` | The seed of the random choices. With the same C++ library, the same seed gives the same run. | 1 |
| `--loss` | The chance in permille that a message is lost. | 10 |
| `--duplicate` | The chance in permille that a message arrives twice. | 10 |
| `--restart` | The chance in permille in each round that one node restarts. | 5 |
| `--no-reconnected` | A lost message does not call `reconnected`. The flag takes no value. | off |
| `--cbor` | Each message goes through the network as CBOR bytes: `send` encodes, the delivery decodes and calls `handle(const received&)`. A duplicate is a copy of the bytes. The program prints the count of the messages of each kind. The flag takes no value. The random choices do not depend on it. | off |

The program checks four properties. Checks 1 and 4 run at each reply.
When all clients are done, the faults stop. Checks 1, 2 and 3 then run
on the end state.

1. The replies of the increments are the numbers 1 to C*N, each once.
2. A read through the read index at each node gives C*N.
3. The decided logs of the three nodes are equal.
4. Each read during the faults gives at least the count of increments
   that were confirmed before the read started.

The program prints how many requests the decided log holds more than
once. When each flag other than `--seed` has its default, this count
must be above 0. The exit code is 0 when all checks pass, 1 when a
check fails or a phase reaches its round limit, and 2 for a wrong
flag. A failure prints the seed.
`ctest` runs the program with `--clients 2 --increments 50`, once
without and once with `--cbor`.

## The messages between the nodes as CBOR

`include/konsens/wire.hpp` encodes and decodes a `konsens::message`.
The library still does no network IO. The form is provisional, and so
are the names that no specification gives.

### The form on the wire

One message is a CBOR sequence (RFC 8742) of two items: the header and
the body. Each item is a map with text keys in the order of RFC 8949
section 4.2.1. No length prefix and no CBOR tag stand in front of a
message.

| Key of the header | Type | Meaning |
|---|---|---|
| `method` | unsigned integer | The CoAP method code of RFC 7252 section 12.1.1. Each message between the nodes has 2, POST. |
| `kind` | unsigned integer | The kind of the body, from the table below. |
| `from` | unsigned integer | The pid of the sender. |
| `to` | unsigned integer | The pid of the receiver. |

The body holds one entry for each member of the struct of its kind in
`message.hpp`, with the name of the member as key. An empty
`std::optional` has no entry. A `std::vector<std::byte>` is the RFC 8746
typed array with tag 64, and a `std::vector<std::uint64_t>` is the
typed array with tag 71.

| Kind | Struct | Kind | Struct |
|---|---|---|---|
| 0 | `prepare_req` | 9 | `recovery` |
| 1 | `prepare` | 10 | `recovery_response` |
| 2 | `promise` | 11 | `read_index_request` |
| 3 | `accept_sync` | 12 | `heartbeat` |
| 4 | `accept_decide` | 13 | `heartbeat_ack` |
| 5 | `accepted` | 14 | `read_index_reply` |
| 6 | `decide` | 15 | `heartbeat_request` |
| 7 | `not_accepted` | 16 | `heartbeat_reply` |
| 8 | `proposal_forward` | | |

The kinds are the alternatives of `paxos_msg`, then those of
`heartbeat_msg`. The header alone names the kind. A body with the keys
of another kind decodes as the kind of the header when it has every
key of that kind: `heartbeat` and `heartbeat_ack` have the same keys,
and so have `prepare_req` and `not_accepted`.

### The API

- `encode(const message&)` returns the bytes.
- `decode(std::string bytes)` takes the bytes and keeps them in one
  shared owner. It returns a `received`: `from`, `to` and a
  `std::variant` with one `cbor::ref<T>` for each kind, in the order of
  the kinds. Each `cbor::ref` holds the body that mbedded-cbor decoded,
  and decode copies no body.
- `node::handle(const received&)` gives the body to the node. It
  copies the body into a `message`, because the handling of Sequence
  Paxos takes a message by value. `handle(message)` stays for callers in
  one process.
- `kind_of` gives the kind of a `message` or of a `received`.

### Errors

`decode` checks, in this order: two items and nothing after them, the
header, the method, the kind, the body, and each entry in the body. An
error of mbedded-cbor comes back unchanged, in the category `cbor`. The
checks of konsens give a value of `wire_errc`:

| Error value | Case |
|---|---|
| `extraneous_data` | Bytes follow the body. |
| `method_not_allowed` | The method is not POST. |
| `unknown_kind` | The kind is 17 or more. |
| `unknown_entry_kind` | An entry has a kind that is not a value of `entry_kind`. |
| `entry_field_of_another_kind` | An entry has a field that its kind does not carry. |

An empty input and an input that ends inside an item give
`too_little_data` of mbedded-cbor.

### What it does not contain

- No Packed CBOR, no CBOR tag on a message and no length prefix.
- No socket, no TLS and no client calls over the wire.
- Ballot Leader Election does not check `from` and `to` of a message.

## Layout

| Path | Content |
|---|---|
| `include/konsens/` | The headers of the library. |
| `src/` | The sources of the library. |
| `test/` | The tests. |
| `examples/` | The example with three nodes. |
| `deps/mbedded-cbor/` | The submodule mbedded-cbor, the CBOR library. |
| `doc/` | The plan of library v0 and its review. |
| `model/plan.md` | The plan of the models: properties, faults, bounds and the decisions of the owner. |
| `model/tla/` | The TLA+ model of Sequence Paxos and the TLC configuration files. |
| `model/results/` | One file for each TLC run, with the bounds, the result and the trace of a violation. |
| `model/results/logs-*/` | The raw logs of a batch of runs. |
| `refs/omnipaxos/` | The repository and the commit of the Omni-Paxos source that the models follow. |
| `refs/papers.md` | References to the papers that the models use. |
| `refs/rfc*.txt` | The specifications that the code implements: CRC32C (RFC 7143), CBOR (RFC 8949), CBOR sequences (RFC 8742) and typed arrays (RFC 8746). |

## License

Apache License 2.0. See `LICENSE`.
