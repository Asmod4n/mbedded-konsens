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
  stops with `std::logic_error`. A write to the disk that fails throws
  `konsens::write_error`, and an LMDB commit that fails throws
  `konsens::sync_error`. The messages and the replies of that commit
  stay in the node.
- **Group commit.** Each call collects its writes and its messages, and
  returns a `konsens::transaction`. `commit()` takes the transaction,
  writes all collected writes in one synchronous LMDB commit, and only
  then gives the messages and the replies to the program.
- **Runs on your thread.** Each call runs on the thread of the caller.
  `commit()` returns when its LMDB commit is done.

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

Each part of a write, one at a time. Three nodes in one process, each
with its own LMDB environment, each message through `encode` and
`decode`. In the serial form the nodes run one after the other; in the
threaded form each node runs on its own thread. g++ 16 `-O2`, x86-64, a
Firecracker VM with 4 cpus, ext4. ns or µs per operation, median of 7
rounds of 31 samples, payload 1 KiB:

| Operation | Time |
|---|---|
| CRC32C | 34 ns |
| LMDB alone, put and commit | 338 µs |
| LMDB alone, get | 76 ns |
| Write, serial | 1.9 ms |
| Write, threaded | 1.6 ms |
| 64 writes at once, threaded, per write | 49 µs |
| Read, serial | 8.2 µs |
| Read, threaded | 91 µs |

A write makes 6 synchronous LMDB commits, two on each node. A read
reads no LMDB: it answers from the state in memory after one read-index
round.

| Message | Encode | Decode | Handle |
|---|---|---|---|
| AcceptDecide, 64 × 1 KiB | 45 µs | 55 µs | 4.8 µs |
| Decide | 65 ns | 277 ns | 61 ns |

The largest difference between two builds of the same source is 30.3 %,
in the case of 64 writes of 64 KiB at once; in the cases of this table
it is 2.8 % or less.
All cases, the payloads of 64 B and 64 KiB and the context of the run:
[bench/results/2026-10-22-052032Z.txt](bench/results/2026-10-22-052032Z.txt).
`bench/run.sh` runs the bench; the option `-DKONSENS_BENCH=ON` builds it.
CRC32C of Google crc32c against a byte table:
[bench/results/2026-10-22-053257Z.txt](bench/results/2026-10-22-053257Z.txt).
The cost of the checksum against a MAC:
[bench/results/2026-10-21-092557Z.txt](bench/results/2026-10-21-092557Z.txt).

## Requirements

| Part | Version |
|---|---|
| Compiler | g++ 16 |
| System | Linux |
| LMDB | the submodule `deps/lmdb`, LMDB 1.0.2, OpenLDAP Public License |
| Boost | the headers |
| mbedded-cbor | the submodule `deps/mbedded-cbor` |
| crc32c | the submodule `deps/crc32c`, Google crc32c, BSD-3-Clause license |
| doctest | for the tests |

The library runs as a user that is not root.

LMDB 1.0 does not open a data file of LMDB 0.9. The file formats of the
two versions differ. The build makes `mdb_dump-0.9` from LMDB 0.9.36 and
`mdb_load` from LMDB 1.0.2 for the upgrade. Stop the node, then:

```sh
mkdir new
mdb_dump-0.9 -a -f dump old
mdb_load -f dump new
```

Start the node on `new` after it has opened and read the log. Then
`new` replaces `old`. `mdb_load` opens at most two named databases;
the storage of a node has two.

## Installation

Ubuntu 26.10:

```sh
sudo apt install g++-16 cmake doctest-dev libboost-dev git
```

On openSUSE Tumbleweed, Boost comes from `libboost_headers-devel`.

```sh
git clone https://github.com/Asmod4n/mbedded-konsens
cd mbedded-konsens
git submodule update --init --recursive
CC=gcc-16 CXX=g++-16 cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The library is the CMake target `konsens`.

## Usage

### Run a node

The program owns the clock and the network. On each tick of its clock,
it calls `tick()`. It gives the messages that arrived to
`handle_incoming()` as one batch. It adds the two transactions with
`+=`, commits them, and sends each outgoing message.

```cpp
auto t = node.tick().value();

// arrived: the bytes of each message from a peer since the last commit
std::vector<konsens::received> batch;
for (auto& bytes : arrived)
    batch.push_back(konsens::decode(std::move(bytes)).value());
t += node.handle_incoming(std::move(batch)).value();

node.commit(t).value();

for (const auto& m : node.take_outgoing_messages())
    send(konsens::encode(m).value());
```

Each call collects its writes and its messages and returns a
transaction. `commit()` writes the collected writes in one LMDB commit.
`take_outgoing_messages()` and `take_client_replies()` give what a
commit released. The compiler refuses a transaction that the program
drops. A commit of a transaction that is committed, or of a transaction
of another node, throws `std::logic_error`. At most
`konsens::max_batch_size` messages wait for one commit.

The nonce is a new random number for each start of the process.

At start, the program calls `konsens::can_use_crc32c_instruction()`.
When it gives false, the CPU has no CRC32C instruction, each CRC32C
runs without it and is slower, and the program writes a warning.

### Write through a session

```cpp
node.commit(node.register_client(client_id).value()).value();
// the reply of the registration carries the session
node.commit(node.client_request(client_id, session, 1, payload).value()).value();

for (const auto& r : node.take_client_replies())
    use(r.client_id, r.response);
```

Any node takes a call. A follower forwards it to the leader.

### Read

```cpp
node.commit(node.client_query(client_id, payload).value()).value();
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
Election, both stores, the wire form and the clients. `test_simulation`,
`test_simulation_rollback` and `test_simulation_clients` run three nodes
on LMDB for 2000 seeds with message loss, duplicate messages, a crash
and a restart from an earlier copy of the store. They check the safety
properties of the model after each step.
`counter_stress` runs many clients against three nodes on a network
that loses and repeats messages. The tests run with g++ 16, with
AddressSanitizer and UndefinedBehaviorSanitizer.

The tests open their LMDB environments in `TMPDIR`. The cache variable
`KONSENS_TEST_TMPDIR` sets `TMPDIR` for each test, for example to a
tmpfs:

```sh
cmake -S . -B build -DKONSENS_TEST_TMPDIR=/dev/shm
```

All tests pass with a tmpfs of 64 MiB. A build with AddressSanitizer runs
200 seeds of each simulation instead of 2000.

`test_device_faults` runs LMDB on a FUSE file system that fails each
fsync, or the reads of one range of the data file. It needs the packages
libfuse3-dev and fuse3, the device /dev/fuse and the right to mount. The
option turns it on:

```sh
cmake -S . -B build -DKONSENS_FUSE_TESTS=ON
```

In podman, the container needs `--device /dev/fuse --cap-add SYS_ADMIN`.

The TLA+ model of the protocol is checked with TLC for each fault: lost
and duplicate messages, crashes, a disk that loses writes, and a
restart of every node.

## Status

Version 0.0.1, before the first release. The API and the wire form can
still change.

## License

Apache License 2.0. See [LICENSE](LICENSE).
