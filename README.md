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

The repository holds formal models in TLA+ and library v0 in C++.

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

The build needs a C++23 compiler, such as g++ 13. Install the packages:

```sh
sudo apt install g++ cmake pkg-config liblmdb-dev doctest-dev
```

Build in a directory of your choice:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
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

## Layout

| Path | Content |
|---|---|
| `include/konsens/` | The headers of the library. |
| `src/` | The sources of the library. |
| `test/` | The tests. |
| `examples/` | The example with three nodes. |
| `doc/` | The plan of library v0 and its review. |
| `model/plan.md` | The plan of the models: properties, faults, bounds and the decisions of the owner. |
| `model/tla/` | The TLA+ model of Sequence Paxos and the TLC configuration files. |
| `model/results/` | One file for each TLC run, with the bounds, the result and the trace of a violation. |
| `model/results/logs-*/` | The raw logs of a batch of runs. |
| `refs/omnipaxos/` | The repository and the commit of the Omni-Paxos source that the models follow. |
| `refs/papers.md` | References to the papers that the models use. |

## License

Apache License 2.0. See `LICENSE`.
