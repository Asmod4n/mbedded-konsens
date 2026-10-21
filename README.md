# mbedded-konsens

A C++ library that replicates a log over a cluster of nodes. A majority
of the nodes decides each entry, and an entry counts as decided only
after a majority wrote it to disk with a synchronous LMDB commit. Each
node applies the decided entries in order to a state machine that the
program gives.

```cpp
struct counter {
    std::uint64_t value;

    std::vector<std::byte> apply(std::span<const std::byte>) { return bytes_of(++value); }
    std::vector<std::byte> query(std::span<const std::byte>) const { return bytes_of(value); }
};

auto storage = konsens::persistent_storage::open("node-1", map_size, geteuid()).value();
auto node = konsens::node<konsens::persistent_storage, counter>::with(
    {.configuration_id = 1, .pid = 1, .peers = {2, 3}, .leader_priority = 0,
     .election_tick_timeout = 5, .resend_message_tick_timeout = 3},
    std::move(storage), counter{}, nonce).value();
```

It follows Omni-Paxos (Sequence Paxos and Ballot Leader Election), the
recovery and the client table of Viewstamped Replication Revisited, and
the read index of the Raft dissertation. The messages between the nodes
are CBOR (RFC 8949). The checksum of each stored entry is CRC32C
(RFC 7143).

## Features

- **Durable decisions.** A node answers Accepted only after the
  synchronous commit of the entry.
- **Recovery after each start.** A node votes again only after answers
  of its peers. After a restart of every node, the nodes compare their
  last entries and take the highest log.
- **Linearizable reads.** A read goes through a read index: one round
  of heartbeats to a majority, and the answer comes from the applied
  state at that index.
- **Client sessions.** The state machine executes each request number
  of a session once. A request that comes again gets the kept reply.
- **Checksums.** Each log entry carries a CRC32C over its index and its
  bytes. Each meta value carries a CRC32C over its bytes.
- **Your network.** The node gives its outgoing messages to the
  program, and the program gives it the messages that arrive.
  `konsens::encode` and `konsens::decode` turn them into bytes.
- **Errors as values.** Each call returns `std::expected`. A wrong use
  stops with `std::logic_error`.
- **Runs on your thread.** Each call runs on the thread of the caller
  and returns when its commit is done.

## How a write works

<img src="doc/img/write.svg" alt="the leader commits the entry, sends AcceptDecide, decides with the Accepted of one more node, applies it and replies" width="100%">

The leader commits the entry to its own disk and sends it to its
followers. When one more node has committed it, two of three nodes hold
it, and the leader decides it. The leader sends Decide to its
followers, applies the entry and replies. Each follower applies the
entry when its Decide arrives.

## How a node starts

<img src="doc/img/start.svg" alt="a node that starts asks every peer, takes the highest log, writes it in one commit and votes again" width="100%">

A node that starts with a stored promise sends Recovery to every peer.
When the leader answers, the node takes the state of the leader. When
every peer answers and no answer comes from a leader, the node takes
the node with the highest round and index as the source and gets the
entries after its own from that source.

## Performance

Each part of a write, one at a time. Three nodes in one process, the
network a queue, each message through `encode` and `decode`. g++ 16
`-O2`, x86-64, a Firecracker VM with 4 cpus, ext4. ns or µs per
operation, median of 7 rounds of 31 samples, payload 1 KiB:

| Operation | Time |
|---|---|
| CRC32C | 2.1 µs |
| LMDB commit of one entry | 349 µs |
| Write, memory | 9.1 µs |
| Write, LMDB | 2.0 ms |
| 64 writes at once, LMDB, per write | 2.0 ms |
| Read, LMDB | 5.9 µs |

| Message | Encode | Decode | Handle |
|---|---|---|---|
| AcceptDecide, 64 × 1 KiB | 45 µs | 57 µs | 750 ns |
| Decide | 66 ns | 265 ns | 26 ns |

The largest difference between two builds of the same source is 6.6 %.
All cases, the payloads of 64 B and 64 KiB and the context of the run:
[bench/results/2026-10-21-144527Z.txt](bench/results/2026-10-21-144527Z.txt).
`bench/run.sh` runs the bench; the option `-DKONSENS_BENCH=ON` builds it.
The cost of the checksum against a MAC:
[bench/results/2026-10-21-092557Z.txt](bench/results/2026-10-21-092557Z.txt).

## Requirements

| Part | Version |
|---|---|
| Compiler | g++ 16 |
| System | Linux |
| LMDB | `liblmdb` with pkg-config |
| Boost | the headers |
| mbedded-cbor | the submodule `deps/mbedded-cbor` |
| doctest | for the tests |

The library runs as a user that is not root.

## Installation

Ubuntu 26.10:

```sh
sudo apt install g++-16 cmake pkg-config liblmdb-dev doctest-dev libboost-dev git
```

On openSUSE Tumbleweed, Boost comes from `libboost_headers-devel`.

```sh
git clone https://github.com/Asmod4n/mbedded-konsens
cd mbedded-konsens
git submodule update --init deps/mbedded-cbor
CXX=g++-16 cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The library is the CMake target `konsens`.

## Usage

### Run a node

The program owns the clock and the network. On each tick of its clock,
it calls `tick()`. It sends each outgoing message, and it gives each
message that arrives to `handle_incoming()`.

```cpp
node.tick();

for (const auto& m : node.take_outgoing_messages())
    send(konsens::encode(m).value());

// bytes: one message from a peer
node.handle_incoming(konsens::decode(std::move(bytes)).value());
```

The nonce is a new random number for each start of the process.

### Write through a session

```cpp
node.register_client(client_id);
// the reply of the registration carries the session
node.client_request(client_id, session, 1, payload);

for (const auto& r : node.take_client_replies())
    use(r.client_id, r.response);
```

Any node takes a call. A follower forwards it to the leader.

### Read

```cpp
node.client_query(client_id, payload);
```

The reply comes through `take_client_replies()` when the applied state
of the node reaches the read index.

### Error values

| Error value | Case |
|---|---|
| `recovering` | The node recovers and takes no call. |
| `session_expired` | The session has expired. Register again. |
| `stale_request` | The request number is below the last request number of the session. |
| `retry` | The node knows no leader, or its promise changed. Send the call again. |

## Documentation

- [doc/](doc/): the plan of each part of the library and its review.
- [model/plan.md](model/plan.md): the TLA+ model, its properties and faults.
- [model/results/](model/results/): one file for each run of the model checker.
- [refs/](refs/): the specifications and papers that the code follows.

## Testing

The unit tests cover each message of Sequence Paxos and Ballot Leader
Election, both stores, the wire form and the clients. `test_simulation`
runs three nodes in memory for 2000 seeds with message loss, duplicate
messages, a crash and a restart from an earlier copy of the store, and
checks the safety properties of the model after each step.
`test_simulation_lmdb` runs the same on LMDB. `counter_stress` runs
many clients against three nodes on a network that loses and repeats
messages. The tests run with g++ 16, with AddressSanitizer and
UndefinedBehaviorSanitizer.

The TLA+ model of the protocol is checked with TLC for each fault: lost
and duplicate messages, crashes, a disk that loses writes, and a
restart of every node.

## Status

Version 0.0.1, before the first release. The API and the wire form can
still change.

## License

Apache License 2.0. See [LICENSE](LICENSE).
