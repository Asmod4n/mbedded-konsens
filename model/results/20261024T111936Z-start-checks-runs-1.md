# Start checks: simulation runs, round 1

Plan: doc/plan-start-checks.md section 3 (05b6930). Model: da39406.
Tool: TLC from release v1.8.0 of tla2tools.jar (2026.10.20), Java
ParallelGC, -Xmx2g. Each run: `-simulate -deadlock -depth 60 -seed 1
-workers 1 -coverage 14`, the .sim.cfg of the run (no SYMMETRY, no
VIEW), time limit 15 min. Four runs at a time on 4 processors. A
simulation without a violation is not a complete run.

| Run | Config | StorageFault steps | States | Result |
|---|---|---|---|---|
| s2, missing_entry | MC_s_nocheck_missing_entry | 6 | 346 | S5 violated after 4 s |
| s2, missing_accepted | MC_s_nocheck_missing_accepted | 173075 | 5287747 | no violation in 15 min |
| s2, missing_promise | MC_s_nocheck_missing_promise | 160512 | 4906377 | no violation in 15 min |
| s2, missing_decided | MC_s_nocheck_missing_decided | 125372 | 4669415 | no violation in 15 min |
| s1 | MC_s | 643645 | 5453738 | no violation in 15 min |
| s3 | MC_s_rollback | 371244 | 5133843 | no violation in 15 min |

## missing_entry without the checks

The violation comes in the step of StorageFault itself (state 23 of
the trace). The node keeps its accepted round, and its log moves one
position down. Another node with the same accepted round holds the
log before the damage. S5 says that two logs with the same accepted
round are compatible, so S5 breaks at once. S2 is not reached in this
trace, because S5 stops the run first. Round 2 runs without S5.

## missing_accepted and missing_promise without the checks

The plan expected S2 or S2b to break. No violation came in 15 min.
Not checked: whether the recovery after each start (VR Revisited,
Recovery = TRUE) repairs the damaged value before the node votes.
Round 2 runs both with Recovery = FALSE.

## missing_decided

As expected: the node decides again what it decided before.

## Breadth-first search

Not started. Each simulation reached about 5 M states in 15 min, and
the state space did not end. A breadth-first search at these bounds
cannot finish on this machine.
