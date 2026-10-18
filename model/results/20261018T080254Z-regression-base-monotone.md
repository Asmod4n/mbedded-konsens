# TLC run regression-base-monotone, 20261018T080254Z

Regression run of plan-fixes-20261018 section 5 after items 1 to 7 and 11, with -deadlock (the run before it stopped on a deadlock at the bounds because the option was missing). Config MC_base_monotone with MaxHalt = 0 and StaleReadNode = FALSE added (item 11). Expected: the counts of run 20261017T121020Z-a-base-monotone-runs-e-f-off (13489555 distinct states, depth 35, no error).

## Configuration

Spec: `model/tla/SequencePaxos.tla`. Config file `model/tla/MC_base_monotone.cfg`:

```
CONSTANTS
  Nodes = {1, 2, 3}
  Values = {v1, v2}
  MaxBallot = 2
  MaxLog = 2
  MaxCounter = 3
  MaxSession = 2
  Cap = 2
  MaxCrash = 0
  MaxWipe = 0
  MaxLoss = 0
  MaxDuplicate = 0
  MaxTimeout = 0
  MaxElection = 2
  RuleR = TRUE
  DecidedIdxMonotone = TRUE
  MaxRollback = 0
  Recovery = FALSE
  Mutation = "none"
  ClientSessions = FALSE
  MaxClientSend = 0
  MaxRequest = 0
  ReadIndex = FALSE
  MaxRead = 0
  NextConfigNodes = {}
  MaxReconfigure = 0
  SingleNode = FALSE
  MaxForceSingle = 0
  MaxReadOnly = 0
  MaxHalt = 0
  StaleReadNode = FALSE
SPECIFICATION Spec
CONSTRAINT Bounds
SYMMETRY ValueSymmetry
VIEW View
INVARIANTS AcceptedIdxIsLogLength S1 S2 S2b S3 S5
PROPERTIES S2Grows S7
```

## Tools

- TLC2 Version 2.19 of 08 August 2024 (rev: 5a47802)
- Running breadth-first search Model-Checking with fp 118 and seed 1518251906964998750 with 3 workers on 4 cores with 9102MB heap and 64MB offheap memory [pid: 680] (Linux 6.18.44-fc-v139 amd64, Ubuntu 21.0.12.1 x86_64, MSBDiskFPSet, DiskStateQueue).
- Java:

```
openjdk version "21.0.12.1" 2026-08-18
OpenJDK Runtime Environment (build 21.0.12.1+1-1-24.04.4-Ubuntu)
OpenJDK 64-Bit Server VM (build 21.0.12.1+1-1-24.04.4-Ubuntu, mixed mode, sharing)
```

## Numbers

- States generated: 46061842
- Distinct states: 13489555
- States left on queue: 0
- Diameter (depth of the state graph search): 35
- Wall time: 31min 54s (start 20261018T080254Z, end 20261018T083449Z, UTC)

## Result

```
Model checking completed. No error has been found.
```

