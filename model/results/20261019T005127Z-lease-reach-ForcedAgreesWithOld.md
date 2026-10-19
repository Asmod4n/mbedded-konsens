# TLC run lease-reach-ForcedAgreesWithOld, 20261019T005127Z

Reach check of plan-witness-lease (8b7cafd), model d58cce1. The invariant ForcedAgreesWithOld must break; it shows that MC_w reaches this case. Breadth-first, limit 30 min.

## Configuration

Spec: `model/tla/SequencePaxos.tla`. Config file `model/tla/MC_w_reach_ForcedAgreesWithOld.cfg`:

```
CONSTANTS
  Nodes = {1, 2, 3}
  Values = {v1, v2}
  MaxBallot = 1
  MaxLog = 1
  MaxCounter = 3
  MaxSession = 2
  Cap = 2
  MaxCrash = 0
  MaxWipe = 0
  MaxLoss = 0
  MaxDuplicate = 0
  MaxTimeout = 0
  MaxElection = 3
  RuleR = TRUE
  DecidedIdxMonotone = TRUE
  MaxRollback = 0
  Recovery = TRUE
  Mutation = "none"
  ClientSessions = FALSE
  MaxClientSend = 0
  MaxRequest = 0
  ReadIndex = TRUE
  MaxRead = 1
  NextConfigNodes = {6}
  MaxReconfigure = 0
  SingleNode = TRUE
  MaxForceSingle = 1
  MaxReadOnly = 1
  MaxHalt = 0
  StaleReadNode = FALSE
  Witness = TRUE
  LeaseLength = 2
SPECIFICATION Spec
CONSTRAINT Bounds
VIEW View
INVARIANTS ForcedAgreesWithOld
```

## Tools

- TLC2 Version 2.19 of 08 August 2024 (rev: 5a47802)
- Running breadth-first search Model-Checking with fp 66 and seed 4618681828230298461 with 3 workers on 4 cores with 9102MB heap and 64MB offheap memory [pid: 5206] (Linux 6.18.44-fc-v139 amd64, Ubuntu 21.0.12.1 x86_64, MSBDiskFPSet, DiskStateQueue).
- Java:

```
openjdk version "21.0.12.1" 2026-08-18
OpenJDK Runtime Environment (build 21.0.12.1+1-1-24.04.4-Ubuntu)
OpenJDK 64-Bit Server VM (build 21.0.12.1+1-1-24.04.4-Ubuntu, mixed mode, sharing)
```

## Numbers

- States generated: 108,737,149
- Distinct states: 30,850,094
- States left on queue: 20,445,010
- Diameter (depth of the state graph search): 12 (level reached when TLC was stopped; not complete)
- Wall time: stopped by the 1 h limit (timeout 3600 s); last progress line at 2026-10-19 01:20:30 (start 20261019T005127Z, end 20261019T012127Z, UTC)

## Result

```
No error in the explored states. TLC was stopped by the time limit (exit 124).
```

