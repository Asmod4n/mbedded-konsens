# LMDB as the only source: simulation runs, round 9

Model ce4801f. New in the model: the durable count `repairs` of
automatic repairs with the limit `AutoRepairs = 1`, the action
`OperatorAllowsRepair`, and `VerifyPromise` (plan 2.5 item 4: the leader
takes the decided index of a promise only when the prefix of the promise
equals its synced log). Tool and flags as round 8 (`-simulate -depth 60
-seed 7`, one worker, 15 min limit, three runs at a time on 4
processors). The `MC_hw_*` configs are the configs of the full model
with `HighWater = TRUE`, `VerifyPromise = TRUE`, `RunningFaults = FALSE`,
and `HighWaterHolds` added to their invariants.

| Config | Faults | States | Result |
|---|---|---|---|
| MC_r_repair_cover | stale_meta, rollback; 2 faults | 344 | NoRepairLimitReached violated, as required |
| MC_r_repair_two | stale_meta, rollback; 2 faults | 3697803 | no violation, stopped by the limit |
| MC_r_rollback_vp | rollback; VerifyPromise | 3611741 | no violation, stopped by the limit |
| MC_hw_c_crash | crash, reconfiguration | 2028795 | no violation, stopped by the limit |
| MC_hw_c_wipe | wipe, reconfiguration | 2274345 | no violation, stopped by the limit |
| MC_hw_c_loss | loss, reconfiguration | 2007035 | no violation, stopped by the limit |
| MC_hw_d2 | crash, wipe, loss, forced single node, read index | 3272163 | no violation, stopped by the limit |
| MC_hw_f | client sessions | 3014904 | no violation, stopped by the limit |
| MC_hw_a_F1_dup | duplicate; DecidedIdxMonotone FALSE | 2401 | HighWaterHolds violated |
| MC_hw_a_F1_loss | loss, resend; DecidedIdxMonotone FALSE | 6405 | HighWaterHolds violated |

- The two violations come from `DecidedIdxMonotone = FALSE`, the form of
  Omni-Paxos in which an `AcceptSync` lowers the decided index of a
  follower (S7). In the trace of MC_hw_a_F1_dup, node 2 has decided
  index 1, and an `AcceptSync` of ballot 13 sets it to 0 (state 20). The
  mark then reports a damage that is none. The code keeps
  `max(old, new)`, so these two configs run again with
  `DecidedIdxMonotone = TRUE` in round 10.
- With two faults, a node that used its automatic repair and finds a
  second damage leaves running (the cover). With the repair from a
  source that may hold an undetected damage, no violation.
- `VerifyPromise` changed no result.
- A simulation without a violation is not a complete run.
