# Review of plan-recovery-delta.md (commit 905eea9)

Reviewer: a second agent of the session, read only, on 2026-10-29.

## Blocking, folded into the plan

- B1. MC_F6a is the counter-check that must break S2
  (results/20261017T044741Z-F6a.md:3, :58). It does not belong in the
  list of runs with no violation.
- B2. Most listed configs never complete a normal recovery. MC_F6a,
  MC_a_F3 and MC_a_F4 set Recovery FALSE. MC_c_wipe and
  MC_c_wipe_n5.sim have no crash, rollback, storage fault, halt or
  read-only step, and only `RestartInPhase` sets recovering
  (SequencePaxos.tla:1727; wipe :1788; build :2411-2428). In MC_F6c both
  nodes roll back in one step, and a recovering node answers no Recovery
  outside power-loss mode (:1556), so no recovery completes. Configs that
  reach the path: MC_F6b, MC_c_crash, MC_d2_crash, MC_ev_loss.sim and
  the power-loss configs.
- B3. No BFS of the list ends within 15 min: MC_F6b took 1 h 34 min
  with 3 workers, MC_a_F3 59 min 50 s; MC_c_crash, MC_c_wipe and
  MC_d2_crash did not end in 1 h (older models).
- B4. The mutation at the bounds of MC_F6b needs about 19 steps. A crash
  in place of a rollback needs about 16. Proposal: a small config with
  one crash, MaxBallot 1, MaxLog 1, MaxElection 2, Cap 2, no
  reconfiguration, as BFS and as simulation.

## The start rule

The rule is sound on the normal path. The new Assert fails only where
an existing invariant fails too: a crashed node with the same
n_accepted holds a prefix of the log of the leader of that ballot; with
another n_accepted its decided prefix is chosen and in the log of the
primary (S2b). A log of a leader shrinks in its own ballot only through
RunningStorageFault with StepChecks off (:2034-2039). A decided_idx
above the log of the primary contradicts S2b.

Not covered by the list: DecidedIdxMonotone FALSE with Recovery TRUE;
a kept decided prefix with a different suffix in a BFS (MaxLog 1).

## The code (for the code plan)

- `new_state` carries the chain CRC of the whole log of the source, not
  of the prefix (sequence_paxos.hpp:1843-1849).
- A start index above the log of the source is a hardware fault
  (:1801, :1839-1841). Omni-Paxos sends nothing then
  (follower.rs:29-36). Error path for the owner.
- The whole copy overwrites a damaged prefix that no check found; the
  delta stops the vote. Error path for the owner.
- Not checked: one transaction for the log and accepted_round at
  AcceptSync.

## Not blocking

Line numbers corrected; PowerLossStartAtAccepted is already in the
ASSUME (:3515); one Assert message for each path; MC_s_powerloss.sim
lies in model/tla/; the worker count of all runs together stays at 4.

## Not checked by the reviewer

Omni-Paxos beyond follower.rs:13-51; Raft and Dynamo; current state
counts; every guard of the hand traces.
