# TLC run mutation-DeactivatedStillVotes, 20261018T112228Z

Counter-check of plan-fixes-20261018 item 16: mutation DeactivatedStillVotes. Expected: a violation of DeactivatedNodeIsSilent.

## Configuration

Spec: `model/tla/SequencePaxos.tla`. Config file `model/tla/MC_mut_DeactivatedStillVotes.cfg`:

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
- Running breadth-first search Model-Checking with fp 119 and seed -2761825698389236184 with 3 workers on 4 cores with 9102MB heap and 64MB offheap memory [pid: 3910] (Linux 6.18.44-fc-v139 amd64, Ubuntu 21.0.12.1 x86_64, MSBDiskFPSet, DiskStateQueue).
- Java:

```
openjdk version "21.0.12.1" 2026-08-18
OpenJDK Runtime Environment (build 21.0.12.1+1-1-24.04.4-Ubuntu)
OpenJDK 64-Bit Server VM (build 21.0.12.1+1-1-24.04.4-Ubuntu, mixed mode, sharing)
```

## Numbers

- States generated: 56,454,834
- Distinct states: 21,551,200
- States left on queue: 13,402,984
- Diameter (depth of the state graph search): 12 (level reached when TLC was stopped; not complete)
- Wall time: stopped by the 1 h limit (timeout 3600 s); last progress line at 2026-10-18 11:51:34 (start 20261018T112228Z, end 20261018T115229Z, UTC)

## Result

```
No error in the explored states. TLC was stopped by the time limit (exit 124).
```

