# Plan: recovery copies only the entries after a start index

Branch recovery-delta, from main 473d975. Owner card 2026-10-29: the
model and the proofs come first; the recovery delta is one item of
them.

## 1. Facts checked in this session

- The model takes the whole log of the source when a recovery ends
  (`HandleRecoveryResponse`, model/tla/SequencePaxos.tla:1338-1378).
- In power-loss mode the model states that the code keeps the entries
  before a start index and takes the rest from the source
  (`PowerLossKeptPrefix`, SequencePaxos.tla:788-797). The start index is
  the accepted_idx of the node when its n_accepted equals that of the
  source, else its decided_idx. The comment there names the rule of
  Omni-Paxos, follower.rs:13-51. The assertion runs only on that path
  (SequencePaxos.tla:1355).
- The mutation PowerLossStartAtAccepted breaks the assertion
  (model/results/20261021T102805Z-power-loss-review-runs.md:9).
- The code copies the whole log of the primary when a recovery ends
  with a primary: `complete_recovery(0, l.log, ...)`
  (include/konsens/sequence_paxos.hpp:1727). In power-loss mode it asks
  for the entries from a start index with `get_state` and checks the
  chain CRC of the prefix (sequence_paxos.hpp:1795-1858).
- The state file proposed a hash chain over the log and a binary search
  for the first index that differs (Claude-state mbedded-konsens.md,
  section 4, "Recovery copies the whole log").

## 2. What others do

- Omni-Paxos: the Prepare carries decided_idx, accepted_round and
  accepted_idx. The follower takes the suffix from the accepted_idx
  when the rounds are equal, else from the decided_idx
  (follower.rs:13-51, cited by the model; not read again in this
  session).
- Raft: AppendEntries carries prevLogIndex and prevLogTerm, and the
  leader moves nextIndex back until the term matches (Ongaro,
  dissertation 2014, section 3.5; from memory, not read in this
  session).
- Neither does a binary search over hashes. A search over hashes is the
  form of anti-entropy for replicas that have no common order of
  writes (Merkle trees in Dynamo, SOSP 2007, section 4.7; from memory).
- The common solution for an ordered log is to match by ballot and
  index. For us it gives the start index with no extra round and no
  new field on the disk. The chain CRC of the prefix at the start index
  still finds a damaged prefix, as the power-loss path does now. So the
  binary search of the state file is not built.

## 3. Change of the model

1. `PowerLossKeptPrefix` becomes `RecoveryKeptPrefix(i, l, mutated)`
   with the same start rule. `mutated` is TRUE for the mutation of the
   path that calls it.
2. The normal path asserts `RecoveryKeptPrefix` too. The power-loss
   path keeps its assertion.
3. A new value of `Mutation`, "PrimaryStartAtAccepted", starts the
   normal path at accepted_idx always. "PowerLossStartAtAccepted" stays
   for the power-loss path. Both values go into the ASSUME of
   `Mutation`.
4. The model keeps taking the whole log. The assertion states that the
   whole log and the kept prefix with the suffix are the same log.
5. No other action changes. No new variable.

## 4. Runs

Each with 1 worker, one at a time, beside the two runs of the mapping
(3 workers). By day each run ends within 15 min; a BFS that ends is
preferred to a simulation (owner 2026-10-25).

| Config | Kind | Expected |
|---|---|---|
| MC_F6a, MC_F6b, MC_a_F3, MC_a_F4, MC_c_crash, MC_c_wipe, MC_d2_crash | BFS, 15 min | no violation |
| MC_F6c | simulation, 15 min | no violation |
| MC_c_wipe_n5.sim, MC_ev_loss.sim | simulation, 15 min | no violation |
| power-loss/MC_p_rollback, MC_s_powerloss.sim | as before | no violation |
| MC_mut_PrimaryStartAtAccepted (new, constants of MC_F6b) | BFS | Assert RecoveryKeptPrefix fails |
| power-loss/MC_mut_PowerLossStartAtAccepted.sim | simulation | Assert fails, as before |

If the assertion fails on a config without a mutation, the work stops
and the trace goes to the owner.

## 5. Code, after the model holds and the owner says go

- The Recovery message carries n_accepted, accepted_idx and
  decided_idx of the node. The primary answers with the entries from
  the start index and the chain CRC of the prefix, as `new_state` does.
- The node checks the chain CRC of its own prefix, as
  `handle_new_state` does (sequence_paxos.hpp:1843-1849).
- The wire changes. The wire is stable only from 1.0 on.
- This is a plan of its own, not part of this plan.

## 6. What this plan does not do

- No change of the build of a new node (`build_request`). A new node
  has an empty log, so it copies the whole log.
- No snapshot and no compaction.
- No proof without bounds.

## 7. Data of this plan

- Result file model/results/<ts>-recovery-delta.md: recorded in the
  state file.
- TLC metadirs under /mnt/zrun/proofs/delta-*: deleted after each run.
