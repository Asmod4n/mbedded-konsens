# Plan: library v0 for a first test

Source: the request of the owner of 2026-10-19 06:58: a library that
the owner can test the same evening, with the proof that it needs.
This plan gets one review by a fresh agent before the build.

## 1. Goal

The owner builds the library, starts three nodes in one process,
writes entries, reads them, stops a node, starts it again and sees the
data.

## 2. The proof that v0 needs

v0 contains only parts that the model already checks:

| Part of v0 | Model run | Result |
|---|---|---|
| Sequence Paxos, with max(old, new) for decided_idx | base_monotone, S7 | no violation, each mutation breaks its property |
| Recovery after each start (f+1 answers) | F6b | complete, no violation |
| Client sessions | f | complete, no violation of E1 |
| Read index | e | complete, no violation of R1 |

v0 contains no part that the stopped runs check (stage 1, stage 2,
the witness, reconfiguration). So no model run is needed before v0.

The proof for the code is a test that replays the model properties on
the C++ code: a simulation of 3 and 5 nodes with loss, duplicates, a
changed order of messages and crashes with a reopen of the store.
After each step it checks S1 to S7, E1 and R1 in the form of the
model. A fault in this test stops the build of later parts.

## 3. Scope

In v0:

- SequencePaxos: Prepare, Promise, AcceptSync, AcceptDecide, Accepted,
  Decide, with the form of the model.
- BallotLeaderElection: heartbeats on `tick()`, ballot `(n, pid)`.
  Priority: fixed by configuration in v0.
- Storage with two forms after Omni-Paxos: PersistentStorage (LMDB,
  synchronous commit) and MemoryStorage (tests only).
- CRC32C over index, kind and payload of each entry. The hardware
  instruction where it exists, the software form elsewhere.
- Recovery after each start.
- Client sessions and the read index.
- The API: a node takes a message or a tick and returns the messages
  to send. The user does the network IO.
- Messages as C++ types, with encode and decode as packed CBOR through
  mbedded-cbor.

Not in v0: reconfiguration (StopSign), stage 1 and stage 2, the
witness, compaction, Execute-Verify, TLS, the binary, the logging
sink, the Fil-C build. Compaction is needed by the memory rule; it
comes in v1, before a release.

## 4. Layout

| Path | Content |
|---|---|
| `include/konsens/` | Public headers: `node.hpp`, `config.hpp`, `message.hpp`, `entry.hpp`, `storage.hpp`. |
| `src/` | SequencePaxos, BallotLeaderElection, PersistentStorage, MemoryStorage, CRC32C, CBOR form of the messages. |
| `test/` | Unit tests per handler, the simulation test, a test of LMDB reopen. |
| `examples/three_nodes.cpp` | Three nodes in one process, for the owner. |
| `CMakeLists.txt` | One target `konsens`, the tests, the example. |

Names come from Omni-Paxos (SequencePaxos, BallotLeaderElection,
Storage, PersistentStorage, MemoryStorage, Ballot, Entry) and from the
model.

## 5. Build

- CMake, g++ 13 and clang++ 18 of Ubuntu 24.04, C++23, every warning
  an error, a debug build with ASan and UBSan.
- LMDB from the package `liblmdb-dev`.
- mbedded-cbor at its current main, as a CMake dependency.
- The test framework: the one that mbedded-cbor uses.

## 6. Order

1. Storage and CRC32C, with tests. In parallel: SequencePaxos and
   BallotLeaderElection on MemoryStorage, with unit tests.
2. Recovery, client sessions, read index.
3. The simulation test on MemoryStorage, then on PersistentStorage
   with reopen.
4. CBOR form of the messages, the example, the README.

Each green step is committed and pushed. At most two build agents at a
time, so that the load stays within 4 processors.

## 7. Risks

- mbedded-cbor may need features that g++ 13 does not have. Then the
  messages stay C++ types in v0, and CBOR comes in v1.
- The simulation test can find a fault that the model does not have,
  because the code differs from the model. Such a fault goes to the
  owner at once.
