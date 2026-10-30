# Fault matrix of the durable storage contract

Source: the owner, 2026-10-24. The task is 0e63c799 in Claude-state. This
document is a plan. It changes no code and no protocol. One review is
in section 6.

Each fault has one table with five parts: the fault, the required
behavior, the assumptions, the existing checks in the tests and in the
model, and the gap. The form follows a failure mode and effects table.

The code is at 06a52bf. The model is `model/tla/SequencePaxos.tla` at
4c357be. Section 3 lists each model run with its result file.

## 1. The contract

An entry is acknowledged when the leader has decided it. The leader
decides an entry when a majority of the nodes has written it with a
synchronous commit. A node sends a message or a reply only after
the commit that the message depends on (`node.hpp:153-166`).

The target: the contract holds while at most f of the 2f + 1 nodes
have a fault at the same time. A fault is a crash, a rollback, a wipe,
a damaged entry or damaged metadata. When more than f disks lose the
same decided entry, no copy is left. The contract promises nothing
then.

The current code does not reach the target in four rows. One node is
enough to break it there: 2.6 (a missing entry), 2.8 (a missing meta
value), 2.9 (a wipe) and 2.10 (a clone).

The safety properties are S1, S2, S2b, S3, S5 and S7 of the model
(`SequencePaxos.tla:2132-2164`, `:2312`). The simulation tests and
counter_stress check the same properties on the storage.

A node stops when it cannot keep the contract:

- Each error of `write_atomically` throws `write_error`. A failed sync
  throws `sync_error`. A full store throws `write_error` with
  `no_space_on_device`. The node sends nothing of that batch and takes
  no further commit (`sequence_paxos.hpp:576-588`,
  `persistent_storage.cpp:125-134`).
- A read that fails, or a CRC that does not agree, stops the voting of
  the node (`stop_voting`, `sequence_paxos.hpp:556`). The node stays
  silent until an operator repairs it (PAR, owner 2026-10-17).

The former form of this section is in the git history at ebd9f42.

The consensus and the read index use no clock. Only the lease of the
witness measures a local duration (owner, 2026-10-19). Clock faults
are therefore outside this matrix.

## 2. Faults

### 2.1 A node crashes and restarts

| Part | Content |
|---|---|
| Fault | The process ends. The disk keeps the last commit. |
| Required | After the start, a node with a stored promise does not vote. It sends Recovery to every peer. It votes again after f + 1 answers, one from the primary of the highest ballot (VR Revisited 4.3). |
| Assumptions | The disk is good. At most f nodes recover at the same time. |
| Tests | test_sequence_paxos: "a node with a stored promise restarts in recover and sends recovery" and 20 recovery cases. test_node: "a restarted leader answers no heartbeat while it recovers". test_group_commit: "a crash between the batch and the commit sends nothing". test_simulation: one crash per seed, 2000 seeds (200 under ASan). counter_stress: restarts, seeds 1 to 300. |
| Model | `Crash` (`:1320`) restarts through `Restart`, as `Rollback` does (`:1323`, `:1341`). MC_F6b is complete on 4c357be with recovery. That run checks the restart into recovery with an older state. MC_a_F3 is complete on the model of 2026-10-16, with no recovery and no S7. |
| Gap | No complete run of `Crash` with recovery on the current model. The model writes and sends in one step. It has no crash between the commit and the send. The code test for that case exists (test_group_commit). |

### 2.2 The whole cluster restarts

| Part | Content |
|---|---|
| Fault | Every node ends, for example at a power loss, and starts again. |
| Required | When no primary answers, a node waits for an answer from every peer. It takes the log of the node with the highest (accepted round, accepted index). It takes the highest promise (owner, 2026-10-21; `sequence_paxos.hpp:918-960`). |
| Assumptions | Every node starts again. A node that does not start stops the recovery of all. The operator then uses stage 1 or stage 2. |
| Tests | test_sequence_paxos: "when every node restarts, each node takes the log of the highest accepted round", "one node restarts while no peer is a primary, and the power-loss path recovers it". counter_stress: `--power-loss 50`, seeds 1 to 300. |
| Model | No action of its own: a sequence of `Crash`. MC_p_reach reaches the case at depth 8. MC_p and MC_p_dup are simulations with no violation. |
| Gap | No complete run. The model does not count how often the power-loss branch fired. A cluster that restarts with one node lost waits for ever. The README does not say so. |

### 2.3 The leader changes

| Part | Content |
|---|---|
| Fault | The leader crashes, the leader is cut off, or a higher ballot comes. |
| Required | A new leader holds each decided entry (S2b). A decided entry is never lost or changed (S1, S2). |
| Assumptions | Each pair of nodes has a FIFO channel (owner, 2026-10-20). |
| Tests | test_node: "three nodes elect, decide and survive the loss of the leader". test_client: "a read after a change of leader sees a request that the old leader decided". test_simulation and counter_stress check S2b after each step or round. |
| Model | `HandleLeader` (`:1216`), with no BLE. Complete: MC_base, MC_base_monotone, MC_e. Each complete fault run has MaxElection 2. |
| Gap | No model has BLE. The model does not check the election timer and the priority. MC_f (client sessions) has no leader change. |

### 2.4 A connection is lost, or a message is lost, duplicated or reordered

| Part | Content |
|---|---|
| Fault | A message is lost or arrives twice. A connection ends and comes back. |
| Required | A gap in the sequence numbers starts a resync (`sequence_paxos.hpp:658`). A duplicate changes nothing. A timeout sends a lost message again. |
| Assumptions | TCP gives FIFO order on each connection. Reorder is out of scope (owner, 2026-10-20). |
| Tests | test_sequence_paxos: "a gap in the sequence numbers starts recovery", "a duplicate accept_decide is ignored". test_node: three tests of a lost Decide or AcceptDecide. test_simulation: one drop and one duplicate per seed. counter_stress: loss and duplicates. The reorder simulation is skipped. |
| Model | `Lose` (`:1444`), `Duplicate` (`:1455`). Complete: MC_a_F1_loss and MC_a_F1_dup, both on the model of 2026-10-16. A full channel drops a message too. |
| Gap | No complete run on the current model. The model has single losses and no partition. MC_mut_S5 breaks S5 at MaxBallot 2 and MaxLog 2. At MaxBallot 1 and MaxLog 1, the mutation SeqNumUnchecked did not break S5 in a simulation of 15 min. |

### 2.5 A node rolls back

| Part | Content |
|---|---|
| Fault | The disk of one node returns to an older valid state: a lost fsync, a volatile disk cache, a restore from a backup. |
| Required | The node recovers as after a crash. The recovery gives it back each entry that it lost. A decided entry stays on f + 1 nodes. |
| Assumptions | At most f nodes roll back at the same time. The disk keeps a whole earlier commit. This is not checked. A disk that drops one page write and keeps the next meta page is row 2.6. |
| Tests | test_simulation_rollback and test_simulation_clients: one rollback from a copy that `persistent_storage::copy` made. test_sequence_paxos: "recovery after a rollback keeps S1". |
| Model | `Rollback` (`:1333`). MC_F6b is complete with one rollback, on 4c357be. MC_F6c (two rollbacks) has a simulation only. MC_p_rollback2 (more than f rollbacks) breaks S2b. |
| Gap | No complete run of F6c. counter_stress has no rollback. The code cannot detect a rollback. Only the recovery after each start covers it. |

### 2.6 An entry is missing, or a write is lost or torn

| Part | Content |
|---|---|
| Fault | The disk lacks an entry that the node acknowledged, and it holds later data. Causes: a damaged B-tree, or a disk that drops a page write and keeps a later one. |
| Required | The node detects it and stops voting. |
| Assumptions | The storage writes a commit as a whole or not at all, while the disk writes each page that the storage syncs. |
| Tests | None. |
| Model | Not modeled. A rollback restores a whole earlier record only. |
| Gap | `read_entries` returns a shorter vector with no indexes (`persistent_storage.cpp:468-486`). Its callers place entry k + 1 at index k: `create_log_sync` (`sequence_paxos.hpp:609-617`), `handle_recovery` (`:891-896`), `handle_get_state` (`:987-991`) and `apply_decided_entries` (`node.hpp:392-403`). This shifts the log with no error, and it can break S1. `get_log_len` counts the entries and does not read the last index (`persistent_storage.cpp:199`). Some detection exists. `get_last_entry_crc` reads the true last key (`:550-566`). `find_hardware_fault` can then flag a peer with the same (round, accepted index) (`sequence_paxos.hpp:962-975`). A lost page write that leaves an old entry with a valid CRC is not detected. The CRC is in the entry and not in a reference to it (state file, section 1). |

The former form of this section is in the git history at ebd9f42.

### 2.7 An entry is damaged

| Part | Content |
|---|---|
| Fault | The bytes of a stored entry change. |
| Required | The CRC32C of the record does not agree. The node stops voting. It reports the index, the stored CRC and the computed CRC. At recovery, two nodes with the same (round, accepted index) and a different last CRC are a hardware fault with evidence. |
| Assumptions | CRC32C detects the change. |
| Tests | test_persistent_storage: "a flipped byte in a stored entry is a CRC mismatch at its index". test_sequence_paxos: "a different CRC32C of one last entry stops voting with the evidence", and the flipped last entry under recovery. test_device_faults: a failed read gives SIGBUS. |
| Model | Not modeled. plan.md counts it as a `Wipe` of the node. |
| Gap | The check runs only when an entry is read. An entry that no one reads stays damaged with no report until a scrub (not built). The B-tree pages have no checksum (section 5, item 4). |

The former form of this section is in the git history at ebd9f42.

### 2.8 Metadata is damaged or missing

| Part | Content |
|---|---|
| Fault | The promise, the accepted round or the decided index changes or is missing. |
| Required | A changed value gives `crc_mismatch`, and the node does not start. A missing value with a non-empty log is a fault. |
| Assumptions | As 2.7. |
| Tests | None. No test damages a meta value. The CRC tests of test_persistent_storage damage an entry only (`:155-191`). |
| Model | Not modeled. A detected fault counts as a `Wipe`. |
| Gap | A missing value reads as a default: `ballot{}` (`persistent_storage.cpp:327-329`) and 0 (`:523-525`). A missing promise with a non-empty log starts the node as new, with no recovery (`sequence_paxos.hpp:268`). That node then promises to a lower ballot (`:706`). The start takes the four values with no comparison between them (`:262-266`). |

### 2.9 A node is wiped

| Part | Content |
|---|---|
| Fault | The disk of a node is empty: a new disk, a deleted directory. |
| Required | Rule R of the model: a node with empty storage does not vote. It votes again only after a decided configuration adds it under a new incarnation (`model/plan.md:64-71`). Rule R is an open question of the owner (state file, section 4). |
| Assumptions | The start of a cluster writes the storage of the first nodes in a step of its own (`model/plan.md:69-70`). |
| Tests | None. |
| Model | `Wipe` (`:1347`) with rule R. MC_a_F4 is complete on the model of 2026-10-16. MC_G breaks S2 without rule R. |
| Gap | The code has no rule R, and no place refuses a wiped node. `open` creates the databases (`persistent_storage.cpp:386-425`). `node::with` checks only the ids and the timeouts (`node.hpp:106-109`). Recovery starts only with a stored promise (`sequence_paxos.hpp:268-272`). The wiped node promises to any higher ballot (`:706`). It also answers Recovery as a normal node (`:881-900`). So it counts in the recovery majority (`:1050-1055`) and in the wait for every peer (`:921-923`). MC_G shows that this can lose a decided entry. |

### 2.10 A node is cloned

| Part | Content |
|---|---|
| Fault | A copy of the disk of a node runs as a second node with the same pid: a copied VM, or a clone of a disk that the operator starts by mistake. The guide for the operator tells the operator to clone a disk with GNU ddrescue before stage 1 or 2. |
| Required | Owner, 2026-10-17: a hidden identity, made at the first start. When the cluster sees one identity twice, it reports it and refuses all input of both (`/mnt/project-files/actor-raft/fehlerquellen.md:92`, `:105`). |
| Assumptions | The clone runs with network to the cluster. A clone without network is allowed (owner: scrub on a clone). |
| Tests | None. |
| Model | Not modeled. |
| Gap | Not built. `handle` checks only that the sender is a peer (`sequence_paxos.hpp:313-316`). Two voters with one pid break the intersection of quorums with one fault. |

### 2.11 A disk fails

| Part | Content |
|---|---|
| Fault | A write fails, a sync fails, the disk or the map is full, or a read fails. |
| Required | Each failed write and each failed sync throws and ends the node. The program never repeats the sync. A full map throws `write_error` too. A failed read ends the process with SIGBUS. |
| Assumptions | A failed sync leaves the last commit on the disk. |
| Tests | test_device_faults on FUSE: fsync, write and read. test_persistent_storage: a full map, a failed commit. test_sequence_paxos: a failed write inside a batch and at the commit. |
| Model | Not modeled. A node that ends is a crash. |
| Gap | A full disk at the start: `open` commits a transaction (`persistent_storage.cpp:421`) and can throw `sync_error`. No test covers this. A full map ends the node, also for its reads. The owner has not decided whether a node keeps serving reads then. |

The former form of this section is in the git history at ebd9f42.

### 2.12 Memory changes a value before its CRC

| Part | Content |
|---|---|
| Fault | A bit flips in RAM before `encode_record` computes the CRC (`persistent_storage.cpp:141-146`). |
| Required | Execute-Verify compares the result of each node (owner, 2026-10-17). A machine with no ECC gives a warning at the start (owner, 2026-10-22). |
| Assumptions | The leader and a follower do not flip the same bit. |
| Tests | None. |
| Model | Not modeled. |
| Gap | The CRC is valid, and the wrong value replicates. Execute-Verify is not built. The warning for missing ECC is not built. |

### 2.13 The mark "deactivated" is lost

| Part | Content |
|---|---|
| Fault | A node that stored "deactivated by the operator" after stage 2 rolls back or is wiped. |
| Required | The node stays silent and does not vote (owner, 2026-10-17 21:18). |
| Assumptions | Stage 2 and the witness lease are not in the code yet. |
| Tests | None. |
| Model | MC_d2 checks DeactivatedKeepsDurable and DeactivatedNodeIsSilent. A rollback of the mark is not modeled. |
| Gap | A rollback before the mark, or a wipe, gives a node that votes again in the old cluster. The witness epoch (owner, 2026-10-19) limits the loss to one lease. This is not checked with a rollback. |

## 3. Model runs named above

The result files are in `model/results/`.

| Run | Result file | Mode |
|---|---|---|
| MC_a_F3 | 20261016T225703Z-a-F3.md | complete, 55.0 M states |
| MC_c_crash | 20261018T201633Z-c-crash.md | stopped 1 h |
| MC_d2_crash | 20261018T135444Z-d2-crash.md | stopped 1 h |
| MC_p_reach | 20261021T081323Z-power-loss-reach.md | violated, as expected |
| MC_p_rollback2 | 20261021T091016Z-power-loss-runs.md | S2b violated, as expected |
| MC_p, MC_p_dup, MC_c_crash, MC_d2_crash on 4c357be | 20261021T102805Z-power-loss-review-runs.md | simulation 15 min |
| MC_F6b on 4c357be | 20261021T102805Z-power-loss-review-runs.md | complete |
| MC_base_monotone | 20261018T080254Z-regression-base-monotone.md | complete, 13.5 M states |
| MC_e (MaxCrash 0) | 20261018T085320Z-regression-e.md | complete, 13.4 M states |
| MC_G | 20261016T182454Z-G.md | S2 violated, as expected |
| MC_a_F1_loss | 20261017T011745Z-a-F1-loss.md | complete, 86.4 M states |
| MC_a_F1_dup | 20261017T004507Z-a-F1-duplicate.md | complete, 29.3 M states |
| MC_a_F4 | 20261016T205534Z-a-F4.md | complete, 54.6 M states |
| MC_mut_S5 | 20261017T092744Z-mutation-S5.md | S5 violated, as expected |
| SeqNumUnchecked, split | 20261019T042348Z-split-Q1b-mutation-sim.md | simulation, no violation |

## 4. Related decisions and open proposals

- Log comparison at recovery: the open work of the state file proposes
  a chain over the CRC of each entry, 64 bits, or a running sum of all
  CRCs. A binary search then finds the first index that differs. A
  chain would also detect 2.6, because each entry depends on the one
  before. The owner decides the form.
- SHA-256: the state file holds no decision on SHA-256 batches. The
  branch bench/raw-sha256-vs-crc32c of the owner holds a measurement.
  This document does not read it.
- libhydrogen does all cryptography (owner, 2026-10-20). Its hash
  (hydro_hash) is the hash of the file pieces (owner, 2026-10-17).
  CRC32C stays the check of each record (owner, 2026-10-22).
- Self-healing: the study comes before any design of a repair (owner,
  2026-10-21). Until then, a damaged follower stays silent.
- Replicas without voting rights are task 339ae7d0. This matrix does
  not cover them.

## 5. Proposals for the gaps

These are proposals. The owner decides each one.

1. Rule R in the code (2.9). A wipe needs no act of the operator. An
   identity on the disk is lost with the disk. So the refusal needs a
   mark that the empty disk does not have: the start of a cluster, or
   the join of a node, writes a member record in a step of its own. A
   node with no member record does not vote and does not answer
   Recovery. A new node that gets a copy of the store (`copy`) gets no
   member record with it. The decided configuration that adds it
   writes the record. The hidden identity of 2.10 can be part of the
   same record.
2. A check at the start (2.6, 2.8). The node does not start when one
   of these fails: the last index equals the count of entries minus
   one; a non-empty log has a promise; the decided index is at most
   the log length; the accepted round is at most the promise. The cost
   is one cursor read at the start. It must be measured.
3. A check of continuity in `read_entries` (2.6): each index equals the
   one before plus one. Else the read gives `malformed_record`.
4. Page checksums (2.7): a checksum on each page with SQLite is an
   open point (plan-sqlite-storage.md section 9). The former form of this section is in the git history at ebd9f42.
5. A chain of CRCs (section 4) also for 2.6.
6. A complete run of each row with "no complete run", on the current
   model, after the owner chooses the bounds.

## 6. Review

One review by a second Claude agent, 2026-10-24, on 01eb4fc. Verdict:
approve with changes, 17 findings. Two were blocking: a full map ends
the node (it was described as an error value), and the clone was
missing. This version applies all 17. The clone decision has its
source in fehlerquellen.md, not in the state file.

## 7. Next test plan

After the owner decides section 5, one plan covers:

- a test for each check of proposals 2 and 3, with a store file that
  the test damages;
- a test that a wiped node does not vote, if rule R enters the code;
- a rollback in counter_stress;
- a full disk at the start.
