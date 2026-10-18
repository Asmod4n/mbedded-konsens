# TLC run mutation-ForceWithoutStage1_all-sim, 20261018T131012Z

Counter-check of plan-fixes-20261018 item 16: mutation ForceWithoutStage1_all, random simulation (-simulate -deadlock -depth 60, seed 1, limit 15 min, SYMMETRY and VIEW removed). Expected: a violation of every safety property of MC_d2. Mode: simulation.

## Configuration

Spec: `model/tla/SequencePaxos.tla`. Config file `model/tla/MC_mut_ForceWithoutStage1_all.sim.cfg`:

```
CONSTANTS
  Nodes = {1, 2, 3}
  Values = {v1, v2}
  MaxBallot = 1
  MaxLog = 1
  MaxCounter = 3
  MaxSession = 2
  Cap = 1
  MaxCrash = 0
  MaxWipe = 0
  MaxLoss = 0
  MaxDuplicate = 0
  MaxTimeout = 0
  MaxElection = 2
  RuleR = TRUE
  DecidedIdxMonotone = TRUE
  MaxRollback = 0
  Recovery = TRUE
  Mutation = "ForceWithoutStage1"
  ClientSessions = FALSE
  MaxClientSend = 0
  MaxRequest = 0
  ReadIndex = FALSE
  MaxRead = 0
  NextConfigNodes = {6}
  MaxReconfigure = 0
  SingleNode = TRUE
  MaxForceSingle = 1
  MaxReadOnly = 1
  MaxHalt = 2
  StaleReadNode = FALSE
SPECIFICATION Spec
CONSTRAINT Bounds
INVARIANTS AcceptedIdxIsLogLength S1Force S2Force S2b S3 S5 R1Force ForceRetiresOldNodes ForceKeepsDecided NoTwoWriters S1New S2New R1New EachReadSeesDecided ClusterDecidesNothingAfterStage1
PROPERTIES S2Grows ReadOnlyWritesNothing DeactivatedNodeIsSilent DeactivatedStays S7
```

## Tools

- TLC2 Version 2.19 of 08 August 2024 (rev: 5a47802)
- Running Random Simulation with seed 1 with 3 workers on 4 cores with 9102MB heap and 64MB offheap memory [pid: 6554] (Linux 6.18.44-fc-v139 amd64, Ubuntu 21.0.12.1 x86_64).
- Java:

```
openjdk version "21.0.12.1" 2026-08-18
OpenJDK Runtime Environment (build 21.0.12.1+1-1-24.04.4-Ubuntu)
OpenJDK 64-Bit Server VM (build 21.0.12.1+1-1-24.04.4-Ubuntu, mixed mode, sharing)
```

## Numbers

- States generated: n/a
- Distinct states: n/a
- States left on queue: n/a
- Diameter (depth of the state graph search): n/a
- Wall time: n/a (start 20261018T131012Z, end 20261018T132513Z, UTC)

## Result

```
No error in the explored states. TLC was stopped by the time limit (exit 124).
```

