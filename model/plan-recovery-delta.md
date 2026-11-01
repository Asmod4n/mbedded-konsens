# Plan: recovery copies only the entries after a start index

Branch recovery-delta, from main 473d975. Owner card 2026-10-29: the
model and the proofs come first; the recovery delta is one item of
them.

Note, 2026-10-29: the code has no recovery since branch remove-recovery
(doc/plan-remove-recovery.md). The facts about the code in section 1
describe the code before that change. The model keeps the recovery.

## 1. Facts checked in this session

- The model takes the whole log of the source when a recovery ends
  (`HandleRecoveryResponse`, model/tla/SequencePaxos.tla:1337-1378).
- In power-loss mode the model states that the code keeps the entries
  before a start index and takes the rest from the source
  (`PowerLossKeptPrefix`, SequencePaxos.tla:788-797). The start index is
  the accepted_idx of the node when its n_accepted equals that of the
  source, else its decided_idx. The comment there names the rule of
  Omni-Paxos, follower.rs:13-51. The assertion runs only on that path
  (SequencePaxos.tla:1355). The value is already in the ASSUME of
  `Mutation` (SequencePaxos.tla:3515).
- The mutation PowerLossStartAtAccepted breaks the assertion
  (model/results/20261021T102805Z-power-loss-review-runs.md:9).
- The code copies the whole log of the primary when a recovery ends
  with a primary: `complete_recovery(0, l.log, ...)`
  (include/konsens/sequence_paxos.hpp:1726). In power-loss mode it picks
  the start index (sequence_paxos.hpp:1767), asks for the entries from it
  with `get_state` (:1768, :1789-1811), extends its own prefix chain CRC
  with the entries and compares the result with the chain CRC of the
  whole log of the source (:1813-1859).
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
   for the power-loss path. The new value goes into the ASSUME of
   `Mutation`. Each path has its own Assert message, so that a trace
   names the path.
4. The model keeps taking the whole log. The assertion states that the
   whole log and the kept prefix with the suffix are the same log.
5. No other action changes. No new variable.

## 4. Runs

Each with 1 worker, one at a time, beside the two runs of the mapping
(3 workers), so 4 workers in all. A simulation has depth 60 and seed 1.
The owner allowed long runs by day for this work (2026-10-28).

Only a node that restarts in a phase recovers
(`RestartInPhase`, SequencePaxos.tla:1727). A wipe and a build do not.
A recovering node answers no Recovery outside power-loss mode
(:1556), so two rollbacks in one step never complete a recovery
(MC_F6c). These configs reach the normal path: MC_F6b (rollback),
MC_c_crash, MC_d2_crash and MC_ev_loss.sim (crash), and the power-loss
configs. A complete BFS of MC_F6b took 1 h 34 min with 3 workers
(results/20261021T102805Z-power-loss-review-runs.md:31), so the old
configs run as simulations.

| Config | Kind | Expected |
|---|---|---|
| MC_delta_crash (new: MC_c_crash with MaxCounter 2, Cap 2, no reconfiguration) | BFS, 2 h | no violation |
| MC_mut_PrimaryStartAtAccepted (MC_delta_crash with the mutation) | simulation, then BFS | Assert of the primary path fails |
| MC_F6b, MC_c_crash, MC_d2_crash, MC_ev_loss.sim | simulation, 15 min | no violation |
| MC_s_powerloss.sim, power-loss/MC_p_rollback | simulation, 15 min | no violation |
| power-loss/MC_mut_PowerLossStartAtAccepted.sim | simulation | Assert of the power-loss path fails, as before |

If the assertion fails on a config without a mutation, the work stops
and the trace goes to the owner. A config with RunningFaults TRUE and
StepChecks FALSE can shorten the log of a leader in its own ballot
(:2034-2039); there a failure is expected. No such config is in the
list.

Not covered: DecidedIdxMonotone FALSE together with Recovery TRUE
(no config has both), and a kept decided prefix with a different
suffix in a BFS (every BFS config has MaxLog 1; only MC_ev_loss.sim
has MaxLog 4).

## 5. Code, after the model holds and the owner says go

- The Recovery message carries n_accepted, accepted_idx and
  decided_idx of the node. The primary answers with the entries from
  the start index and the chain CRC of its whole log, as `new_state`
  does.
- The node checks the chain CRC of its own prefix, as
  `handle_new_state` does (sequence_paxos.hpp:1843-1849).
- The wire changes. The wire is stable only from 1.0 on.
- Two error paths go to the owner in that plan. A start index above the
  log of the source is a hardware fault today (sequence_paxos.hpp:1801,
  :1839-1841); Omni-Paxos sends nothing when the follower is up to date
  (follower.rs:29-36). The whole copy of today overwrites a damaged
  prefix that no check found; with the delta that damage stops the vote.
- Not checked: whether the code writes the log and accepted_round in
  one transaction at AcceptSync. The start rule needs it.
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
