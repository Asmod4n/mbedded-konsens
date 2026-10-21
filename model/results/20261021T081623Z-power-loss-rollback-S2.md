# TLC runs power-loss-rollback, 20261021T081623Z

Run MC_p_rollback of model/plan-power-loss.md section 4 (PowerLoss = TRUE, MaxCrash = 2, MaxRollback = 1). Expected: no violation. Model d92f43a plus the value "PowerLossWithF" in the ASSUME of Mutation.

## Runs

1. TLC 2.19, random simulation of `model/tla/power-loss/MC_p_rollback.sim.cfg` (`-simulate -deadlock -depth 60 -seed 1 -workers 1`), start 20261021T081623Z. TLC found a violation of S2 after 7 min 35 s. It printed states 1 to 12 and then stopped with an ArrayIndexOutOfBoundsException, the known fault of TLC 2.19 in the print of a simulation trace.
2. TLC 2026.10.20.183001 (release v1.8.0 of tla2tools.jar), the same config, seeds 1 to 5, 15 min each: no violation. The random generator differs from TLC 2.19, so seed 1 does not give the same trace.
3. TLC 2026.10.20.183001, breadth-first search, 3 workers, from state 12 of run 1 as the initial state (a wrapper module with `InitT` equal to that state and the constants of MC_p_rollback.cfg). S2 is violated at depth 14 after 5 min 04 s (3214840 distinct states).

## Trace of run 3

| State | Step | Change |
|---|---|---|
| 1 | state 12 of run 1 | node 2 leader of ballot 22 in Accept, log <<v2>>; node 3 leader of ballot 23 in Prepare; node 1 accepted ballot 22, log empty |
| 2-4 | Prepare/Promise/AcceptSync of ballot 23 | nodes 1 and 3 hold <<v1>> at ballot 23 |
| 5-6 | Decide | v1 decided at index 1; decided_idx of nodes 1 and 3 is 1 |
| 7 | node 1 crashes | node 1 recovers |
| 8-10 | Recovery, answer | node 2 answers as primary of ballot 22 with <<v2>> |
| 11 | node 3 rolls back | node 3 recovers with promise 0 and an empty log |
| 12-14 | Recovery, answer | node 3 answers as a recovering node; node 1 completes the normal recovery with the two answers, takes <<v2>> from node 2 and keeps decided_idx 1 |

S2 fails in state 14: the decided prefix of node 1 is <<v2>>, and decided_history is <<v1>>.

## Cause

A fault of the model change, not of the form of the plan. The form says that the normal recovery completes "as today". Before the change, a recovering node did not answer, so the f + 1 answers of the normal recovery came from nodes that do not recover. After the change a recovering node answers, and `RecoveryComplete` counted its answer. Node 3 lost its promise of ballot 23 by the rollback, so ballot 22 looked like the highest ballot.

## Fix

`RecoveryComplete` and the choice of the primary count only the answers whose field `recovering` is FALSE. The response record carries that field. With PowerLoss = FALSE no recovering node answers, so the runs of today do not change.

Liveness, stated and not checked: in the trace above, node 1 then waits for one more answer of a node that does not recover, and node 3 waits as well. Node 2 stays the leader of ballot 22. Liveness is not part of this plan (section 5).
