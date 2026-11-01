# Plan: library v0 for a first test

Source: the request of the owner of 2026-10-19 06:58: a library that
the owner can test the same evening. This version applies the review
`doc/review-plan-library-v0.md` (approve with changes).

## 1. Goal

The owner builds the library, starts three nodes in one process,
writes entries, reads the decided entries, stops a node, reopens its
store and sees the data.

## 2. What the model checks for v0

| Part of v0 | Model run | Result |
|---|---|---|
| Sequence Paxos, with max(old, new) for decided_idx | base_monotone (3 nodes, ballot n <= 2, log <= 2, no fault) | complete, 13,489,555 distinct states, no violation; the S7 counter-check breaks |
| One crash, one loss, one duplicate | a-F3, a-F1-loss, a-F1-duplicate (small bounds, Recovery off) | complete, no violation; not run again on the current model |
| Ballot Leader Election | none | not in the model |
| CRC32C, reopen of the store | none | not in the model; `Crash` assumes that the durable state survives |

The proof for the code is a simulation test of 3 nodes with one crash
and reopen, one loss and one duplicate. After each step it checks S1,
S2, S2b, S3, S5 and S7 in the form of the model. A fault in this test
goes to the owner at once.

## 3. Scope of v0

- SequencePaxos on MemoryStorage and on PersistentStorage (a
  synchronous commit). Names after Omni-Paxos.
- BallotLeaderElection with a fixed priority from the configuration.
  It is needed when the owner stops the leader. The code cites the
  Omni-Paxos source at 532ff79 by file and line in the tests; the
  source is not copied into the tree (owner, 2026-10-17).
- CRC32C over index, kind and payload of each entry, software form
  only. The text of RFC 3720 (section 12.1, appendix B.4) goes into
  `refs/` in the same commit.
- The API: a node takes a message or a tick and returns the messages
  to send. The user does the network IO. Messages are C++ types.
- Error paths, as decided by the owner: a failed commit ends
  the process. A full disk is the error value "no space". A CRC
  mismatch is reported with its evidence, and the node stops voting.

Moved to v0.1: recovery after each start, client sessions, the read
index, the CBOR form of the messages (mbedded-cbor needs g++ 16 for
packed). Recovery can wait for this test: the complete crash run a-F3
had Recovery off, and the test causes no rollback.

Not before v1: reconfiguration, stage 1 and stage 2, the witness,
compaction, Execute-Verify, TLS, the binary, the logging sink, the
Fil-C build.

## 4. Layout

| Path | Content |
|---|---|
| `include/konsens/` | Public headers. |
| `src/` | SequencePaxos, BallotLeaderElection, PersistentStorage, MemoryStorage, CRC32C. |
| `test/` | Unit tests, the simulation test, the reopen test of the store. |
| `examples/three_nodes.cpp` | Three nodes in one process. |
| `refs/rfc3720.txt` | The specification of CRC32C. |

## 5. Build

- CMake, g++ 13.3 of Ubuntu 24.04, C++23, every
  warning an error, a debug build with ASan and UBSan. clang++ 18.1
  with libstdc++ 13 has no `std::expected`, so clang comes in v0.1.
- `doctest-dev` from Ubuntu (installed). The former form of this item is in
  the git history at ebd9f42.
- Branch: `claude/project-thread-4xjeai`, the branch of this session.

## 6. Order

1. Agent A: MemoryStorage, PersistentStorage, CRC32C, with tests.
   Agent B at the same time: SequencePaxos and BallotLeaderElection on
   MemoryStorage, with unit tests.
2. The simulation test, first on MemoryStorage, then on
   PersistentStorage with reopen.
3. The example and the README.

Each agent has its own build directory. Each green step is committed
and pushed. At most two agents and four builds run at a time.
