# Fault matrix of the durable storage contract

Source: the owner, 2026-10-24, card "Fehlermatrix". The task is
0e63c799 in Claude-state, tasks/org-migration. This document is a plan.
It changes no code and no protocol. It gets one review before the
questions go to the owner.

Each fault has one table with five parts: the fault, the required
behavior, the assumptions, the existing checks in the tests and in the
model, and the gap. The form follows a failure mode and effects table.

The code is at 06a52bf. The model is `model/tla/SequencePaxos.tla` at
4c357be. Each model run named here is listed with its result file in
section 3.

## 1. The contract

An entry is acknowledged when the leader has decided it. The leader
decides an entry when a majority of the nodes has written it with a
synchronous LMDB commit. A node sends a message or a reply only after
the commit that the message depends on (`node.hpp:153-166`).

The contract holds while at most f of the 2f + 1 nodes have a fault at
the same time. A fault is a crash, a rollback, a wipe, a damaged entry
or damaged metadata. When more than f disks lose the same decided
entry, no copy is left. The contract promises nothing then.

The safety properties are S1, S2, S2b, S3, S5 and S7 of the model
(`SequencePaxos.tla:2132-2164`, `:2312`). The simulation tests and
counter_stress check the same properties on LMDB.

A node stops when it cannot keep the contract:

- A failed write or sync throws `write_error` or `sync_error`. The node
  sends nothing of that batch and takes no further commit
  (`sequence_paxos.hpp:578-590`, `persistent_storage.cpp:125-134`).
- A full LMDB map gives the error value `no_space_on_device`. The node
  sends nothing of that batch (`node.hpp:157-160`).
- A read that fails, or a CRC that does not agree, stops the voting of
  the node (`stop_voting`, `sequence_paxos.hpp:556`). The node stays
  silent until an operator repairs it (PAR, owner 2026-10-17).
- A read error under the LMDB map gives SIGBUS, and the process ends.

## 2. Faults

### 2.1 A node crashes and restarts

| Part | Content |
|---|---|
| Fault | The process ends. The disk keeps the last commit. |
| Required | After the start, a node with a stored promise does not vote. It sends Recovery to every peer and votes again after f + 1 answers, one from the primary of the highest ballot (VR Revisited 4.3). |
| Assumptions | The disk is good. At most f nodes recover at the same time. |
| Tests | test_sequence_paxos: "a node with a stored promise restarts in recover and sends recovery" and 20 recovery cases. test_node: "a restarted leader answers no heartbeat while it recovers". test_group_commit: "a crash between the batch and the commit sends nothing". test_simulation: one crash per seed, 2000 seeds (200 under ASan). counter_stress: restarts, seeds 1 to 300. |
| Model | `Crash` (`:1320`). One complete run, MC_a_F3, on the model of 2026-10-16 with no recovery and no S7. Every run with `Recovery = TRUE` and a crash stopped at 1 h or is a simulation, with no violation. |
| Gap | No complete run of a crash with recovery. The model writes and sends in one step, so it has no crash between the commit and the send. The code test for that case exists (test_group_commit). |

### 2.2 The whole cluster restarts

| Part | Content |
|---|---|
| Fault | Every node ends, for example at a power loss, and starts again. |
| Required | When no primary answers, a node waits for an answer from every peer. It takes the log of the node with the highest (accepted round, accepted index) and the highest promise (owner, 2026-10-21; `sequence_paxos.hpp:918-960`). |
| Assumptions | Every node starts again. A node that does not start stops the recovery of all. The operator then uses stage 1 or stage 2. |
| Tests | test_sequence_paxos: "when every node restarts, each node takes the log of the highest accepted round", "one node restarts while no peer is a primary, and the power-loss path recovers it". counter_stress: `--power-loss 50`, seeds 1 to 300. |
| Model | No action of its own: a sequence of `Crash`. MC_p_reach reaches the case at depth 8. MC_p and MC_p_dup are simulations with no violation. |
| Gap | No complete run. The model does not count how often the power-loss branch fired. A cluster that restarts with one node lost waits for ever; the README does not say so. |

### 2.3 The leader changes

| Part | Content |
|---|---|
| Fault | The leader crashes, is cut off, or a higher ballot comes. |
| Required | A new leader holds each decided entry (S2b). A decided entry is never lost or changed (S1, S2). |
| Assumptions | FIFO channels between each pair of nodes (owner, 2026-10-20). |
| Tests | test_node: "three nodes elect, decide and survive the loss of the leader". test_client: "a read after a change of leader sees a request that the old leader decided". test_simulation and counter_stress check S2b after each step or round. |
| Model | `HandleLeader` (`:1216`), with no BLE. Complete: MC_base, MC_base_monotone, MC_e. Each complete fault run has MaxElection 2. |
| Gap | No model has BLE, so the election timer and the priority are not checked. MC_f (client sessions) has no leader change. |

### 2.4 A connection is lost, or a message is lost, duplicated or reordered

| Part | Content |
|---|---|
| Fault | A message is lost or arrives twice. A connection ends and comes back. |
| Required | A gap in the sequence numbers starts a resync (`sequence_paxos.hpp:658`). A duplicate changes nothing. A lost message is sent again after a timeout. |
| Assumptions | TCP gives FIFO order on each connection. Reorder is out of scope (owner, 2026-10-20). |
| Tests | test_sequence_paxos: "a gap in the sequence numbers starts recovery", "a duplicate accept_decide is ignored". test_node: three tests of a lost Decide or AcceptDecide. test_simulation: one drop and one duplicate per seed. counter_stress: loss and duplicates. The reorder simulation is skipped. |
| Model | `Lose` (`:1444`), `Duplicate` (`:1455`). Complete: MC_a_F1_loss, MC_a_F1_dup, both on the model of 2026-10-16. A full channel drops a message too. |
| Gap | No complete run on the current model. A partition is not modeled, only single losses. The mutation SeqNumUnchecked did not break S5 at MaxBallot 1 and MaxLog 1 in 15 min, so S5 has no counter-check at those bounds. |

### 2.5 A node rolls back

| Part | Content |
|---|---|
| Fault | The disk of one node returns to an older valid state: a lost fsync, a volatile disk cache, a restore from a backup. |
| Required | The node recovers as after a crash. The recovery gives it back each entry that it lost. A decided entry stays on f + 1 nodes. |
| Assumptions | At most f nodes roll back at the same time. A disk that loses a write keeps a whole earlier commit, as LMDB does with its two meta pages. |
| Tests | test_simulation_rollback and test_simulation_clients: one rollback from a copy made with `persistent_storage::copy`. test_sequence_paxos: "recovery after a rollback keeps S1". |
| Model | `Rollback` (`:1333`). MC_F6b is complete with one rollback, also on 4c357be. MC_F6c (two rollbacks) is a simulation only. MC_p_rollback2 breaks S2b, as expected with more than f rollbacks. |
| Gap | No complete run of F6c. counter_stress has no rollback. The code cannot detect a rollback; only the recovery after each start covers it. |

### 2.6 An entry is missing

| Part | Content |
|---|---|
| Fault | An entry that the node acknowledged is not on the disk, but later data is. For example a hole in the log, or new metadata with an old log. |
| Required | The node detects it and stops voting. |
| Assumptions | LMDB writes a commit as a whole or not at all. A hole can come only from a damaged B-tree. |
| Tests | None. |
| Model | Not modeled. A rollback restores a whole earlier record only. |
| Gap | `read_entries` skips a missing index with no error (`persistent_storage.cpp:468-484`). `get_log_len` counts the entries and does not read the last index (`:199`). The CRC of an entry covers its own index, not the existence of the entry before it. Proposals in section 4. |

### 2.7 An entry is damaged

| Part | Content |
|---|---|
| Fault | The bytes of a stored entry change. |
| Required | The CRC32C of the record does not agree. The node stops voting and reports the index, the stored and the computed CRC. At recovery, two nodes with the same (round, accepted index) and a different last CRC are a hardware fault with evidence. |
| Assumptions | CRC32C detects the change. A damage in a page that LMDB reads for its B-tree is outside this check. |
| Tests | test_persistent_storage: "a flipped byte in a stored entry is a CRC mismatch at its index". test_sequence_paxos: "a different CRC32C of one last entry stops voting with the evidence", and the flipped last entry under recovery. test_device_faults: a failed read gives SIGBUS. |
| Model | Not modeled. plan.md counts it as a `Wipe` of the node. |
| Gap | The check is done only when an entry is read. An entry that no one reads stays damaged with no report until a scrub (not built). The B-tree pages have no check. LMDB 1.0.2 has page checksums (`mdb_env_set_checksum`, lmdb.h:1110); their use is not checked. |

### 2.8 Metadata is damaged

| Part | Content |
|---|---|
| Fault | The promise, the accepted round or the decided index changes or is missing. |
| Required | A changed value gives `crc_mismatch`, and the node does not start. A missing value with a non-empty log is a fault. |
| Assumptions | As 2.7. |
| Tests | test_persistent_storage: the CRC of each meta value. No test of a missing meta value. |
| Model | Not modeled. A detected fault counts as a `Wipe`. |
| Gap | A missing meta value reads as 0 (`persistent_storage.cpp:262`). A missing promise with a non-empty log starts the node as new, with no recovery (`sequence_paxos.hpp:268`). |

### 2.9 A node is wiped

| Part | Content |
|---|---|
| Fault | The disk of a node is empty: a new disk, a deleted directory. |
| Required | Rule R of the model: a wiped node does not vote until it joins a new configuration. |
| Assumptions | The operator replaces a node only by a reconfiguration. |
| Tests | None. |
| Model | `Wipe` (`:1347`) with rule R. MC_a_F4 is complete on the model of 2026-10-16. MC_G breaks S2 without rule R. |
| Gap | The code has no rule R. A node with an empty disk starts as a new node and votes at once (`sequence_paxos.hpp:268`). It forgets its promise and its accepted entries. MC_G shows that this can lose a decided entry. Rule R is an open question of the owner. |

### 2.10 A disk fails

| Part | Content |
|---|---|
| Fault | A write fails, a sync fails, the disk is full, or a read fails. |
| Required | A failed write or sync throws and ends the node; the program never repeats the sync. A full map gives an error value, and the node keeps serving reads. A failed read ends the process with SIGBUS. |
| Assumptions | A failed sync leaves the last commit on the disk (LMDB writes the meta page after the data pages). |
| Tests | test_device_faults on FUSE: fsync, write and read. test_persistent_storage: full map, failed commit. test_sequence_paxos: full map at a commit. |
| Model | Not modeled. A node that ends is a crash. |
| Gap | None found. |

## 3. Model runs named above

The result files are in `model/results/`.

| Run | Result file | Mode |
|---|---|---|
| MC_a_F3 | 20261016T225703Z-a-F3.md | complete, 55.0 M states |
| MC_c_crash | 20261018T201633Z-c-crash.md | stopped 1 h |
| MC_d2_crash | 20261018T135444Z-d2-crash.md | stopped 1 h |
| MC_p_reach | 20261021T081323Z-power-loss-reach.md | violated, as expected |
| MC_p, MC_p_dup | 20261021T102805Z-power-loss-review-runs.md | simulation |
| MC_base_monotone | 20261018T080254Z-regression-base-monotone.md | complete, 13.5 M states |
| MC_e | 20261018T085320Z-regression-e.md | complete, 13.4 M states |
| MC_G | 20261016T182454Z-G.md | S2 violated, as expected |
| MC_a_F1_loss | 20261017T011745Z-a-F1-loss.md | complete, 86.4 M states |
| MC_a_F1_dup | 20261017T004507Z-a-F1-duplicate.md | complete, 29.3 M states |
| MC_F6b | 20261021T102805Z-power-loss-review-runs.md | complete, on 4c357be |
| MC_a_F4 | 20261016T205534Z-a-F4.md | complete, 54.6 M states |

## 4. Proposals for the gaps

These are proposals. The owner decides each one.

1. Rule R in the code (2.9). A node that has never been a member must
   not vote. Forms: the first start writes a member identity that the
   configuration names, and a node with an empty disk and a known
   identity refuses to vote; or the operator command adds a node only
   through a reconfiguration. The hidden identity against clones
   (owner, 2026-10-17) can be the same value.
2. A check of the log at the start (2.6, 2.8): the last index equals
   the count of entries minus one, and a non-empty log has a promise.
   Else the node does not start. Cost: one cursor read at the start.
3. A check of continuity in `read_entries` (2.6): each index equals
   the one before plus one. Else `malformed_record`.
4. LMDB page checksums (2.7) with CRC32C. It covers the B-tree. It must
   be measured first.
5. A complete run of each row with "no complete run", on the current
   model, after the owner chooses the bounds.

## 5. Next test plan

After the owner decides section 4, one plan covers:

- a test for each check of proposal 2 and 3, with a LMDB file that a
  test damages on purpose;
- a test that a wiped node does not vote, if rule R enters the code;
- a rollback in counter_stress.
