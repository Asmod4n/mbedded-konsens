# Build of a next-configuration node from an old node that recovers

Commit of the model: ba69fc2 (branch reconfiguration).
Module: model/tla/BuildCovers.tla over SequencePaxos.tla.
TLC: tla2tools.jar of the session, 3 workers, -deadlock, -Xmx6g.
Machine: Firecracker VM, kernel 6.18, 4 processors.

## Question

Can a node of the old configuration give its decided prefix to a node
of the next configuration while it recovers after a restart?

The mutation `BuildFromRecovering` drops the guard
`j \in running /\ ~recovering[j]` in `BuildForServer`.
The source must still hold the StopSign decided (`IsReconfigured`).

## Bounds

Nodes {1, 2, 3}, NextConfigNodes {6, 7, 8}, MaxReconfigure 1,
MaxBallot 1, MaxLog 1, MaxCrash 1, MaxRollback 1, MaxWipe 0,
MaxElection 2, Recovery TRUE, RuleR TRUE.

## Runs

| Run | Kind | Result | States |
|---|---|---|---|
| MC_c_build_recovering_cover.sim | simulation, seed 1, depth 80 | CoverBuildFromRecovering violated, as wanted | 251,395 |
| MC_c_build_recovering.sim | simulation, seed 1, depth 80, 15 min | no error in S1-S6, AcceptedIdxIsLogLength, S2Grows | 27,824,829 |
| MC_c_build_recovering | BFS, 15 min | no error to depth 11; did not end | 5,596,235 distinct |

The cover shows that a build from a recovering source is reachable in
these bounds. The BFS does not end in 15 min, so the simulation is the
check, and the BFS is a partial search only.

## Decision

A node of the old configuration that holds the decided StopSign in its
synced store gives its decided prefix without the recovery. It never
votes again. Reason: a decided StopSign ends the configuration, so no
entry of its decided prefix changes after the decide.
