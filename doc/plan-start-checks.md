# Plan: checks of the stored state at the start and at each read

Source: doc/storage-fault-matrix.md on branch storage-fault-matrix
(658c93b), rows 2.6 and 2.8, proposals 2 and 3. The matrix proposed
`malformed_record` for proposal 3; this plan proposes a new code. The
owner has not decided these proposals yet. This form is provisional.
The owner decided on 2026-10-24 that a full map ends the node and that
rule R comes with the hidden identity; both are outside this plan.

Order of the work: this plan, then the model of section 3 and its TLC
runs, then one review of plan and model by a second agent, then the
go of the owner, then the code of section 2. The first review of this
plan (2026-10-24, 13 findings) is applied below; section 8 lists where.

## 1. Facts checked in this session

Line numbers are at commit 4c57d44.

- `read_entries` reads with MDB_SET_RANGE from `from` and stops at the
  first key at or above `to` (`src/persistent_storage.cpp:449-488`). A
  missing index is skipped, and the entries after it move down by one
  position.
- `get_log_len` (`:496-504`) calls `read_log_len` (`:192-200`), which
  returns the count of entries. It does not read the last index.
- A missing meta value reads as `ballot{}` or 0 (`:262`, `:327-329`,
  `:523-525`).
- `sequence_paxos::with` takes promise, accepted round, decided index
  and log length with no comparison
  (`include/konsens/sequence_paxos.hpp:241-275`).
- A read error during the run makes the node stop voting:
  `stop_voting` (`:556-561`), called at `:713`, `:719`, `:790`, `:886`,
  `:893`, `:931`, `:983`.
- Each write path keeps these facts:
  - A node writes `set_promise` before or with each accepted entry
    (`:384` and `:723` guard every entry write; `check_valid_ballot`
    `:643-646` guards `:800`). So a non-empty log has a promise.
  - Every write path that adds entries sets the accepted round, or runs
    after it is set (`:763-765`, `:810-811`, `:1069-1070`; `:684` and
    `:836` only in the Accept phase). So a non-empty log has an
    accepted round other than `ballot{}`.
  - The accepted round is never above the promise (`:765`, `:811`;
    `:922`, `:954` and `:1047` take a promise at least as high as the
    promise of the source).
- The fact "the decided index is never above the log length" has only
  the model as evidence: `AcceptedIdxIsLogLength`
  (`model/tla/SequencePaxos.tla:2131-2132`) is about model variables.
  In the code:
  - `update_decided` (`:226-233`) caps the value at the log length.
  - `read_decided_suffix` (`:501`) takes min(decided, accepted).
  - `complete_recovery` (`:1066-1067`) takes max(own, source) with no
    cap. It depends on S2b.
  With power loss and rollback, the invariant ran as a simulation only
  (model/results/20261021T091016Z-power-loss-runs.md, rows p and
  p_rollback). MC_p_rollback2 (more than one rollback) breaks S2b. So
  more rollbacks than the fault model allows can make a node refuse to
  start. No counterexample was found with one rollback.
- A missing decided index reads as 0. The check cannot find it. It is
  harmless for safety: the node decides again what it decided before.
- An existing test starts a node with a log and no promise
  (`test/test_sequence_paxos.cpp:221-231`, used at `:639-645`). The
  harnesses dereference the result of `with` with no check:
  `test/simulation.hpp:510,532,881`, `test/node_cluster.hpp:139`,
  `test/test_node.cpp:74,140,152,229`, `test/test_client.cpp:139,170`,
  `test/test_read_index.cpp:59`, `test/test_group_commit.cpp:181`.
- LMDB forbids a second environment of the same files in one process
  (`deps/lmdb/libraries/liblmdb/lmdb.h:104-107`): when one closes, the
  fcntl locks of the other break. `test/fuse/test_device_faults.cpp:166`
  damages a store in a forked child.
- No bench case opens a store (`bench/konsens_bench.cpp:1705-1721`).

## 2. Change of the code

1. One function states the continuity of the log: the key at position
   p is p. The open check and the read check both call it (rule "A
   check for safety exists once").
2. `persistent_storage::read_entries`: every key found below `to` must
   equal `from + entries.size()`. The first index is 0, so a key above
   `from` is always a missing index, also at `from` itself. Else
   `read_entries` returns `read_failure` with the new code
   `storage_errc::missing_entry` and the index that is missing. A read
   past the end still returns no entries, so the callers
   (`sequence_paxos.hpp:502,524,612,891,987,1033`,
   `counter_stress.cpp:611,644`) keep their behaviour.
3. `persistent_storage::open`: with a non-empty log, the first key must
   be 0 and the last key must be the count minus one. Else `open`
   returns `missing_entry`. One cursor read of the first and of the
   last key, with no `data` argument. `open` returns `std::error_code`,
   so it does not report which index is missing.
4. `sequence_paxos::with` returns the new code
   `paxos_errc::inconsistent_storage` when one of these holds:
   - the log is not empty and the accepted round is `ballot{}`;
   - the accepted round is above the promise;
   - the decided index is above the log length.
   The first two together also find a log with no promise.
5. Every new error branch is `[[unlikely]]`.
6. No change to the protocol, to the wire or to the file format.

## 3. Model

The model shows what a node with this damage does to the cluster, with
and without the checks. It is a change of
`model/tla/SequencePaxos.tla` on this branch.

### 3.1 New constants and variables

- `MaxStorageFault` (Int): the number of storage faults in one run.
- `StartChecks` (Bool): TRUE models the checks of section 2.
- `storage_faults` (Int): a counter, part of `env`.

### 3.2 New action `StorageFault(i, kind)`

A node restarts after a crash and reads a damaged durable state. The
kinds:

- `"missing_entry"`: one index k of the log is missing. Without the
  checks, the node reads the log with entry k removed and the later
  entries one position lower, as `read_entries` does now.
- `"missing_accepted"`: the log is not empty and the accepted round
  reads as `DefaultBallot`.
- `"missing_promise"`: the log is not empty and the promise reads as
  `DefaultBallot`.
- `"missing_decided"`: the decided index reads as 0.

With `StartChecks`, the node finds each kind except `missing_decided`.
It leaves `running` and does not vote again, as `stop_voting` does in
the code. With `missing_decided`, and without `StartChecks`, the node
restarts through `Restart` with the damaged state.

The damage of a running node between two reads is the same action:
`read_entries` finds it at the next read, and the node stops voting.

### 3.3 Mutation

`Mutation = "StartCheckMissing"` sets `StartChecks` to FALSE in effect.
Its target is S2 (the decided logs agree). Each kind except
`missing_decided` must break S2 or S2b alone; the plan names the run
that shows each.

### 3.4 Configurations and runs

All runs: Nodes {1, 2, 3}, `Recovery = TRUE`, `MaxStorageFault = 1`,
the invariants of `MC_c_crash.cfg`
(`AcceptedIdxIsLogLength S1 S2 S2b S3 S4 S5 S6`, properties `S2Grows`
`BuildOnlyFromRunning`).

| Run | Config | Bounds | Expected |
|---|---|---|---|
| s1 | MC_s.cfg | MaxBallot 2, MaxLog 2 | no violation |
| s2 | MC_s_nocheck.cfg | as s1, mutation | S2 or S2b breaks |
| s3 | MC_s_rollback.cfg | as s1, MaxRollback 1 | no violation |

- s2 runs once for each kind (`StorageFaultKind` restricts the kind),
  so each kind shows its own counterexample.
- Each run: a simulation of 15 min first. Breadth-first only when the
  state count of the simulation shows that it can finish.
- One TLC at a time, at most 4 workers. No run in parallel with a
  build.
- Results go to `model/results/<time>-start-checks-<run>.md` and are
  pushed after each run.

### 3.5 What the model does not show

- That the code reads the damage as the model says. The tests of
  section 4 show it.
- A lost write at the end of the log. The continuity check cannot find
  it (row 2.6 of the matrix).

## 4. Tests

The tests damage a closed LMDB directory with LMDB itself, in a forked
child (`lmdb.h:104-107`). test_persistent_storage links `lmdb` for
this, as the bench does.

- Before the checks: the helper at `test_sequence_paxos.cpp:221-231`
  writes a promise and an accepted round. Every harness checks
  `with(...).has_value()` with REQUIRE before it uses the value, so
  that the simulations and counter_stress show each refusal.
- test_persistent_storage:
  - "a missing entry in the middle of the log is missing_entry at
    open";
  - "a missing first entry is missing_entry at open";
  - "read_entries of a range with a missing entry gives missing_entry
    at its index" (the store is open; the child removes the entry);
  - "read_entries from a missing index gives missing_entry at that
    index".
- test_sequence_paxos, with `write_atomically` through `storage_of`
  (`:158-166`), no raw LMDB and no CRC code:
  - "a log with no accepted round is inconsistent_storage";
  - "an accepted round above the promise is inconsistent_storage";
  - "a decided index above the log is inconsistent_storage".
- Mutation, run once: each check removed. Each test fails.

## 5. Checks of the build

- g++ 16, image localhost/cxx-matrix-fuse, Release, all tests,
  KONSENS_FUSE_TESTS=ON, KONSENS_TEST_TMPDIR=/dev/shm.
- counter_stress seeds 1 to 300 with `--cbor --power-loss 50`.
- ASan and UBSan once before the merge.
- The cost of the read check: bench cases `get_entries_lmdb` and
  `read_lmdb`, before and after, in every build, by the rules of a
  measurement. No number is written before the run.

## 6. Provisional names

`missing_entry`, `inconsistent_storage`, `StorageFault`,
`StartChecks`, `MaxStorageFault`, `StartCheckMissing`. No
specification names these faults. The owner decides the names.

## 7. Error rule

`missing_entry` and `inconsistent_storage` are values, as
`crc_mismatch` and `malformed_record` are. `read_failure` carries the
index. This agrees with "An error goes to whoever can repair it".

## 8. Findings of the first review and where they are

1. Test with no promise and harnesses with no check: section 1, 4.
2. A missing index at `from`: section 2, item 2.
3. A missing accepted round: section 2, item 4.
4. Evidence for decided at most log length: section 1.
5. Second environment in one process: section 1, 4.
6. `write_atomically` through `storage_of`: section 4.
7. `open` cannot report the index: section 2, item 3; test names.
8. One continuity function: section 2, item 1.
9. Bench case: section 5.
10. No `data` argument: section 2, item 3.
11. Line numbers, matrix branch, `[[unlikely]]`: header, section 1, 2.
12. Error rule: section 7.
13. Writing rules: no change needed.
