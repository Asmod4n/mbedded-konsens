# Plan: checks of the stored state at the start and at each read

Source: doc/storage-fault-matrix.md on branch storage-fault-matrix
(658c93b), rows 2.6 and 2.8, proposals 2 and 3. The matrix proposed
`malformed_record` for proposal 3; this plan proposes a new code. The
owner decided the open points of reviews 3 and 4 on 2026-10-24; the
names stay provisional (section 6).
The owner decided on 2026-10-24 that a full map ends the node and that
rule R comes with the hidden identity; both are outside this plan.

Order of the work: this plan, then the model of section 3 and its TLC
runs, then one review of plan and model by a second agent, then the
go of the owner, then the code of section 2. Reviews 1 to 4
(doc/review-plan-start-checks-*.md) are applied below; section 8 lists
where.

## 1. Facts checked in this session

Line numbers are at commit 4c57d44.

- `read_entries` reads by key, with MDB_SET_RANGE from `from`, and
  stops at the first key at or above `to`
  (`src/persistent_storage.cpp:449-488`). A missing key is skipped. A
  caller reads up to the count of entries, so after a missing key k
  below the last key, a range read returns one entry less, and the last
  key is never read. A read of one index other than k returns the true
  entry.
- After a missing key below the last key, the next append with
  MDB_APPEND at the count meets the existing last key and fails with
  MDB_KEYEXIST (`put_entry` `:207`; `lmdb.h:1572-1575`).
- The meta values are read once, in `with` (`sequence_paxos.hpp:262-266`),
  and then kept in memory. A meta value that is lost while the node runs
  is read only at the next start.
- `write_atomically` appends at the count that it reads
  (`read_log_len`, `:436`), not at the accepted index in memory. A last
  key that is lost while the node runs is not found: the next append
  succeeds at the old last index, and the stored log differs from the
  log in memory. Item 2.5 is the answer (owner, 2026-10-24 12:03).
- A node that reads `ballot{}` as its promise starts as a new node,
  (follower, none), with no recovery (`sequence_paxos.hpp:268-271`).
  So a lost promise with an empty log is found by no check of the
  start. Item 2.6 is the answer (owner, 2026-10-24 12:04).
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
   `counter_stress.cpp:611,644`) keep their behaviour. When the cursor
   stops at a key at or above `to`, `from + entries.size()` must equal
   `to`. A key above `to` that follows a gap is not an error of this
   read.
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
5. `persistent_storage::write_atomically` compares the count of stored
   entries with the accepted index of the caller (owner, 2026-10-24
   12:03). `append_entries` and `append_on_prefix` (`storage.hpp:87-94`)
   each carry the length of the log before that op, as the caller holds
   it. `write_atomically` reads the count once (`:436`) and then keeps
   the length that each op returns (`:438-441`). Each append op compares
   its length with that running length: the count for the first op, the
   length that the op before returned for each later op. It never
   compares with `from_idx`, so `append_on_prefix` with `from_idx`
   above the count stays valid (the `std::min` at `:289` stays; test
   `test_persistent_storage.cpp:128` passes with the length 1 in the
   op). When the lengths differ, `write_atomically` returns
   `missing_entry` and commits nothing. No caller in sequence_paxos
   holds a length other than the stored one (`:684`, `:764`, `:810`,
   `:836`, `:1069`, review 3). Storages other than persistent_storage
   (for example `counting_storage` in the bench) ignore the field.
   `write_waiting_ops_or_throw` (`sequence_paxos.hpp:577-589`) throws
   `missing_entry` as `write_error`, as every other write error: errors
   that the file system reports are exceptions (owner, 2026-10-24
   14:06). No call site changes. The process ends. At the next start,
   the start checks refuse the store when the decided index is above
   the log length (item 2.4); else the recovery repairs it.
   `handle_decide` (`:853-856`) writes `set_decided_index` with no
   append op in the batch. So `write_atomically` also compares each
   `set_decided_index` with the running length: a decided index above
   it is `missing_entry`, thrown as `write_error`, and nothing commits
   (owner, 2026-10-24 14:39). The decided index on disk then stays at
   or below the log length, and the recovery repairs the store at the
   next start.
6. `sequence_paxos::with` starts the recovery also when the promise
   reads as `ballot{}` (owner, 2026-10-24 12:04: a node with an empty
   log gets a full copy and then runs the recovery). Only a seed node
   at the first start of a new cluster starts with no recovery (owner,
   12:05). A seed node starts with no recovery only when the config
   says seed and the store is new: no meta key and no hidden identity
   (owner, 14:01). The hidden identity does not exist yet at 4c57d44.
   Until rule R builds it, "new" means "no meta key"; this form is
   provisional. A seed that gets a new disk loses the seed field
   first. A cluster starts only with a majority of its nodes as seeds,
   f+1 (owner, 14:01); the check of that count belongs to the config
   tool and is outside this plan. The name of the config field is
   provisional. The full copy itself is outside this plan; until it
   exists, the recovery copies the log as it does now. The model of
   that form is 6276400, with the runs of
   `model/results/*-start-checks-runs-7.md` (section 3.6).
7. Each read up to the accepted index or the decided index in
   sequence_paxos compares the count of entries it got with `to - from`
   (owner, 2026-10-24 14:03). When they differ, the node stops voting
   with `missing_entry`. The reads and the place that stops the vote:
   - `create_log_sync` (`:612`), `handle_recovery` (`:891`),
     `handle_get_state` (`:983`) and the read of one entry at `:1033`:
     in sequence_paxos, where the read is. At `:1033` the read always
     returns one entry in a correct state, so the branch for an empty
     result (`:1037-1042`) goes, and a short result is `missing_entry`.
   - `get_last_entry_crc` (`:884`, `:929`, `:981`): see the
     provisional form below.
   - `read_decided_suffix` (`:498-503`) is `const`. It returns
     `missing_entry` to `node::apply_decided_entries`
     (`node.hpp:394-397`) and to the public `node::read_decided_suffix`
     (`node.hpp:255-258`). Both call one non-const method of
     sequence_paxos that stops the vote. Provisional form.
   - `log_or_buffered_proposals_contain` (`:518-529`) returns the error
     to `node.hpp:238`, which calls the same method. Provisional form.
   A correct state always gives the full range, because the waiting ops
   are written before each read.
   `get_last_entry_crc` (`persistent_storage.cpp:535-566`) reads the
   key at MDB_LAST and does not compare it with `accepted_idx_ - 1`.
   After a lost last key, a recovery answer that is not the primary
   (`:890`) reads no log and sends the CRC of the entry before.
   `find_hardware_fault` (`:962-976`) then names a healthy node.
   The node reads `get_entries(accepted_idx_ - 1, accepted_idx_)`
   through the check of this item and computes the CRC; with
   `accepted_idx_` 0 there is no entry. The storage concept
   (`storage.hpp:128`) does not change (2026-10-24 14:40: the owner
   left the choice to the session; the recommended form is taken).
8. The recovery checks that the log it receives from the primary has
   the length of the accepted index of the primary (`l->accepted_idx`,
   `complete_recovery` at `:922`). When it differs, the node keeps that
   answer with primary = false and no log, as `:1011-1019` does for a
   changed state. Else `find_recovery_primary` (`:1050-1062`) finds the
   same answer again and the path through all peers stays closed
   (`:924-926`).
9. Every new error branch is `[[unlikely]]`.
10. No change to the protocol, to the wire or to the file format.

## 3. Model

The model shows what a node with this damage does to the cluster, with
and without the checks. It is a change of
`model/tla/SequencePaxos.tla` on this branch.

### 3.1 New constants and variables

- `MaxStorageFault` (Int): the number of storage faults in one run.
- `StartChecks` (Bool): TRUE models the checks of section 2.
- `StorageFaultKind` (Set of strings): the kinds a run can damage.
- `storage_faults` (Int) and `refused` (set of nodes), both in `env`.
- `NoFalseRefusal`: an invariant. A node that is not refused passes the
  checks with its durable state.

### 3.2 New action `StorageFault(i, kind)`

A node of `Nodes` restarts after a crash and reads a damaged durable
state. A node of the next configuration gets no storage fault. The
kinds:

- `"missing_entry"`: the key of one position k is missing. Without the
  checks, the node reads the log with entry k removed (`LogWithout`).
  This is an approximation of the code (section 1), and only the
  mutation uses it. With the checks, each k below the last position is
  found.
- `"missing_accepted"`: the log is not empty and the accepted round
  reads as `DefaultBallot`.
- `"missing_promise"`: the promise reads as `DefaultBallot`, with or
  without entries.
- `"missing_decided"`: the decided index reads as 0.

A damage that the checks find makes the node leave `running`. It keeps
its state and takes no further step, as `Halt` does. This is what
`stop_voting` does in the code. Every damage that the checks do not
find restarts the node through `Restart` with the recovery. When the
promise reads as `DefaultBallot`, the node first takes a full copy of
the durable state of a node that votes and does not recover, and then
runs the recovery (item 2.6). The start as a new node in phase "None",
with no recovery and no message (`RestartInPhase`), is only the
mutation StartAsNewNode. A node with an empty log, an accepted round
and no promise is refused by check 2.4.2 and gets no copy.

The model has no damage of a running node. With items 2.5 and 2.7
such damage reduces to cases that the model has:
- A lost meta value is read only at the next start: one of the kinds
  above.
- A key lost below the last key is found by the next read that covers
  it (items 2.2 and 2.7), or by the next append (MDB_KEYEXIST), or at
  the next start.
- A lost last key is found by the next read up to the accepted index
  (item 2.7) or by the next append (item 2.5), whichever comes first.
  - `handle_prepare` reads only when it sends a sync (`:710-722`). A
    promise without a sync carries `accepted_idx_` from memory and
    reads nothing. The next AcceptSync or AcceptDecide carries that
    length, and item 2.5 fails the write.
  - A recovery answer of the primary reads the log (`:891`). A recovery
    answer that is not the primary reads only the last CRC; item 2.7
    covers that read.
  If the node crashes first, it starts with a missing last entry:
  `missing_entry` at the last position, which the recovery covers.
- A short log that a node receives in the recovery is refused by item
  2.8.
The model has no damage at run time. These code checks carry the
argument alone.
A read error of LMDB that ends the process is a crash in the model,
and MC_s_crash covers it (section 7).
Without item 2.5, the next append writes at the old last index, and at
the next start the node reads a log with no gap and a wrong entry. The
checks at the start cannot find that; it is the case of the mutation
run MC_s_nocheck_missing_entry_safety, which breaks S2.

### 3.3 Mutation

`Mutation = "StartCheckMissing"` turns the checks off. Each kind runs
alone. The model shows a need for a check when the kind breaks S2 or
S2b without that check. With the recovery, no simulation found a
violation for a missing accepted round or promise without the checks.
So checks 2.4.1 and 2.4.2 are defence in depth while the recovery
runs; they give an error with evidence in place of a silent repair.

`Mutation = "StartAsNewNode"` is the start of the code at 4c57d44: a
node that reads no promise starts as a new node, (follower, none), with
no recovery. Its target is S2b.

### 3.4 Configurations and runs

All runs: Nodes {1, 2, 3}, Values {v1, v2}, MaxBallot 2, MaxLog 2,
`MaxStorageFault = 1`, invariants
`AcceptedIdxIsLogLength S1 S2 S2b S3 S4 S5 S6`, properties `S2Grows`
`BuildOnlyFromRunning`. No reconfiguration (NextConfigNodes {}), so S6
and `BuildOnlyFromRunning` hold with no content. MaxLoss, MaxTimeout,
MaxDuplicate and MaxHalt are 0. Every config with the checks adds
`NoFalseRefusal`.

| Config | Change from MC_s | Kinds |
|---|---|---|
| MC_s | Recovery TRUE, checks | all four |
| MC_s_entry | as MC_s | entry |
| MC_s_promise | as MC_s | promise |
| MC_s_promise_asnew | mutation StartAsNewNode | promise |
| MC_s_crash | MaxCrash 1 | all four |
| MC_s_powerloss | power-loss/MC_p.sim.cfg with the checks | entry, accepted, promise |
| MC_s_rollback | MaxRollback 1 | all four |
| MC_s_norec | Recovery FALSE | entry, accepted, decided |
| MC_s_nocheck_missing_* | mutation, one kind each | one |
| MC_s_nocheck_*_norec | mutation, Recovery FALSE | accepted or promise |

- MC_s_powerloss has no `missing_decided`: a lower decided index
  breaks S7 by its definition. It takes its other bounds from
  power-loss/MC_p.sim.cfg: MaxLog 1, MaxCrash 3, MaxTimeout 1. With
  MaxLog 1 the continuity check never runs; MC_s_powerloss_log2 is the
  same config with MaxLog 2.
- In MC_s_crash the crash can also hit the damaged node.
- Every node is a seed in Init. A start where some nodes are not seeds
  is not in the model.
- Each run is a simulation of 15 min. Breadth-first only when the state
  count of the simulation shows that it can finish.
- At most three TLC runs at a time, one worker and one GC thread each,
  so the load stays below the 4 processors. No run in parallel with a
  build.
- Results go to `model/results/<time>-start-checks-runs-<n>.md` and are
  pushed after each round.

### 3.5 Results

All runs are simulations of 15 min:
`model/results/*-start-checks-runs-1.md` to `-4.md`, `-7.md` and
`-9.md`.

- In the model (`LogWithout`, an approximation of the code), a missing
  key below the last key breaks S5 and S2 without the checks. With the checks, no simulation found a violation.
- A missing accepted round or promise breaks S2b without the recovery
  after the start. With the recovery, no simulation found a violation.
- A lost promise with an empty log breaks S2b when the node starts as
  a new node (mutation StartAsNewNode, the code at 4c57d44). With the
  recovery at that start (item 2.6), no simulation found a violation.
- A missing last entry passes the checks when the decided index is
  below the log length. Without the recovery it breaks S2b; with the
  recovery no simulation found a violation.
- With a crash (of any node, the damaged one too), with power loss and
  with a rollback, no simulation found a violation.
- NoFalseRefusal held in every run with the checks.
- No breadth-first run: about 5 M states in 15 min, and the search did
  not end.

### 3.6 What the model does not show

- That the code reads the damage as the model says. The tests of
  section 4 show it.
- A lost write at the end of the log. The continuity check cannot find
  it (row 2.6 of the matrix). Only the recovery after each start
  protects against it.
- The code of this plan, at the full copy. HEAD (80d8829) models only
  the full copy. The form of this plan, with the recovery that copies
  the log, is model 6276400 with the runs of
  `model/results/*-start-checks-runs-7.md`.
- The copy branch exists only when a source votes and does not recover.
  With no such source the fault is not enabled, so "no promise and no
  source" is not explored.
- A damaged source. In the model a node with a fault at the start
  recovers before it is a source, and there is no damage at run time.
  In the code, a source with a lost last key votes until its next read
  or append, and `persistent_storage::copy`
  (`persistent_storage.cpp:569-575`) would copy it. That is two faults.
  The plan of the full copy reads the log of the source through a read
  that item 2.7 covers, and the receiver checks the length as item 2.8
  does.
- The first start of a seed node other than Init.
- A build of a new node: `BuildForServer` and `ForceSingle` give the
  node a log with no accepted round. No code path does this yet. The
  future build path must set the accepted round, or check 2.4 refuses
  the node.

## 4. Tests

The tests damage a closed LMDB directory with LMDB itself, in a forked
child (`lmdb.h:104-107`). test_persistent_storage links `lmdb` for
this, as the bench does.

- Before the checks: the helper at `test_sequence_paxos.cpp:221-231`
  writes a promise and an accepted round. Every harness checks
  `with(...).has_value()` with REQUIRE before it uses the value, so
  that the simulations and counter_stress show each refusal.
- With item 2.6, every start with an empty store recovers unless it is
  marked as the first start of a seed. The harnesses that start nodes
  mark their first start: simulation.hpp, node_cluster.hpp,
  test_node.cpp and counter_stress.
- test_persistent_storage:
  - "a missing entry in the middle of the log is missing_entry at
    open";
  - "a missing first entry is missing_entry at open";
  - "read_entries of a range with a missing entry gives missing_entry
    at its index" (the store is open; the child removes the entry);
  - "read_entries from a missing index gives missing_entry at that
    index";
  - "an append after a lost last key is missing_entry and commits
    nothing" (the store is open; the child removes the last key);
  - "an append with an accepted index other than the count is
    missing_entry".
- test_sequence_paxos, with `write_atomically` through `storage_of`
  (`:158-166`), no raw LMDB and no CRC code:
  - "a log with no accepted round is inconsistent_storage";
  - "an accepted round above the promise is inconsistent_storage";
  - "a decided index above the log is inconsistent_storage";
  - "a node that reads no promise starts the recovery";
  - "a Prepare after a lost last key makes the node stop voting"
    (item 2.7; the child removes the last key while the store is open);
  - "a read that ends at a missing key gives missing_entry" (item 2.2);
  - "a recovery answer with a log shorter than the accepted index of
    the primary does not complete the recovery with it, and the node
    completes the recovery later" (item 2.8);
  - "a recovery answer that is not the primary after a lost last key
    makes the node stop voting" (item 2.7, `get_last_entry_crc`);
  - "a read at `:1033` that returns no entry gives missing_entry"
    (item 2.7);
  - "a decided suffix after a lost key makes the node stop voting"
    (item 2.7, through node);
  - "a batch of append_on_prefix and append_entries on a whole log
    commits" (item 2.5, `:764`);
  - "a complete read with a gap above `to` returns its entries"
    (item 2.2);
  - "an append after a lost last key throws write_error" (item 2.5);
  - "a decided index above the stored log length throws write_error
    and commits nothing" (item 2.5);
  - "a read error of LMDB throws and is not a stop of the vote"
    (section 7);
  - "a seed node at the first start starts with no recovery";
  - "a seed node with a used store starts the recovery".
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
`StartChecks`, `MaxStorageFault`, `StartCheckMissing`,
`StartAsNewNode`, `NoFalseRefusal`, `RestartInPhase`, the field of
item 2.6 and the accepted index in the append ops of item 2.5. No
specification names these faults. The owner decides the names.

## 7. Error rule

- At a write, `missing_entry` is thrown as `write_error`, derived
  from `std::system_error` (`storage.hpp:110`), as every error that the
  file system reports (owner, 2026-10-24 14:06).
- At a read, `missing_entry` is a value in `read_failure` with the
  index, and sequence_paxos stops the vote. sequence_paxos finds that
  fault, not the file system.
- A read error that LMDB reports is thrown as a type derived from
  `std::system_error` and ends the process, as a write error does
  (owner, 2026-10-24 14:39). The calls of `stop_voting` for such
  errors (`:713`, `:719`, `:886`, `:893` and the others of section 1)
  become throws.
- `inconsistent_storage` is a value of `with`, as `crc_mismatch` and
  `malformed_record` are.
This agrees with "An error goes to whoever can repair it".

## 8. Findings of the reviews and where they are

Review 1:

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

Reviews 2 and 3: doc/review-plan-start-checks-2.md and -3.md; the
status of each finding of review 3 is in review 4.

Review 4:

1. `get_last_entry_crc`: section 2, item 7 (provisional form).
2. Read at `:1033`: section 2, item 7; section 4.
3. Place that stops the vote: section 2, item 7.
4. Key where the cursor stops: section 2, item 2.
5. Short recovery answer: section 2, item 8; section 4.
6. Next start after a lost last key: section 2, item 5.
7. Error rule: section 7.
8. Model of the code: section 2, item 6; section 3.6.
9. Copy branch and damaged source: section 3.6; comment in the model.
10. Hidden identity: section 2, item 6.
11. Prepare and recovery answer: section 3.2.
12. Tests: section 4.
13. Copy column: results of rounds 9 and 10.
14. Text: header, section 3.5.
