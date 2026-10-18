# TLC run mutation-DeactivatedStillVotes-sim, 20261018T115357Z

Counter-check of plan-fixes-20261018 item 16: mutation DeactivatedStillVotes, random simulation (-simulate -deadlock -depth 60, seed 1, limit 15 min, SYMMETRY and VIEW removed). Expected: a violation of DeactivatedNodeIsSilent. Mode: simulation.

## Configuration

Spec: `model/tla/SequencePaxos.tla`. Config file `model/tla/MC_mut_DeactivatedStillVotes.sim.cfg`:

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
  Mutation = "DeactivatedStillVotes"
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
PROPERTIES DeactivatedNodeIsSilent
```

## Tools

- TLC2 Version 2.19 of 08 August 2024 (rev: 5a47802)
- Running Random Simulation with seed 1 with 3 workers on 4 cores with 9102MB heap and 64MB offheap memory [pid: 4504] (Linux 6.18.44-fc-v139 amd64, Ubuntu 21.0.12.1 x86_64).
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
- Wall time: n/a (start 20261018T115357Z, end 20261018T120857Z, UTC)

## Result

```
No error in the explored states. TLC was stopped by the time limit (exit 124).
```

