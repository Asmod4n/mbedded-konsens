# TLC runs of form B, the rejoin under a new identity

Plan: model/plan-rejoin-new-identity.md. Models and configs at commit
cff4ccf. TLC2 Version 2.19 of 08 August 2024 (rev: 5a47802), 3
workers on 4 cores, Linux 6.18.44, OpenJDK 21.0.12.1. Simulation runs:
-simulate -depth 60 -seed 1 -deadlock, limit 900 s.

## Small model, RecoveryRollback.tla, Recovery "newid"

| Config | Mode | Expected | Result |
|---|---|---|---|
| MC_rr_inflight_n5_newid_Reached | BFS | Reached violated | violated at state 14 |
| MC_rr_inflight_n5_newid_ChosenOnce | BFS, no deadlock check | no error | no error, 43,693 distinct states, depth 29, search ended |
| MC_rr_cover_retired_n5_l1 | BFS | CoverRetired violated | violated, depth 7 |
| MC_rr_newid_n3_b2 | BFS, -deadlock | no error | no error, 240,041 distinct states, depth 22, search ended |
| MC_rr_newid_n3_b3 | BFS, -deadlock | no error | no error, 18,324,885 distinct states, depth 30, search ended |
| MC_rr_newid_n5 | simulation | no error | no error, 167,102,833 states checked, stopped by the limit |
| MC_rr_newid_n5_c2 | simulation | no error | no error, 227,027,524 states checked, stopped by the limit |

The BFS of MC_rr_newid_n5_l1 and MC_rr_newid_n5 did not end: after
280 s each had more than 10,000,000 distinct states and a growing
queue. These runs are not a check.

MC_rr_newleader_n5_l1 (the old rejoin, MaxLog 1) found no error in
600 s and did not end. With MaxLog 1 the fault is not reachable: the
recovery writes the accepted log without an Accepted message, so the
node needs a second entry to send one. The bounds of the green runs
(five nodes, MaxBallot 2, MaxLog 2, MaxCrash 1) hold the trace of
results/20261027T110500Z-rejoin-promise-in-flight.md, which breaks
ChosenOnce under "newleader".

## Protocol model, SequencePaxos.tla, wipe with RuleR

Five nodes, NextConfigNodes {6, 7, 8}, MaxWipe 2, MaxElection 3,
MaxLog 2.

| Config | Expected | Result |
|---|---|---|
| MC_c_wipe_n5.sim | no error | no error, 18,018,454 states checked, stopped by the limit |
| MC_c_wipe_n5_cover_promise.sim (WipeCovers.tla) | violated | CoverWipedPromiseCounted violated |
| MC_c_wipe_n5_cover_decides.sim (WipeCovers.tla) | violated | CoverNewConfigDecides violated |
| MC_c_wipe_n5_noR.sim (RuleR FALSE) | violated | S2b violated |

## Result

No run found a fault of form B. The fault scene and a decision of the
new configuration are reachable in the same bounds. Without rule R
the protocol model breaks S2b. With five nodes the small model and
the protocol model were sampled, not searched to the end.
