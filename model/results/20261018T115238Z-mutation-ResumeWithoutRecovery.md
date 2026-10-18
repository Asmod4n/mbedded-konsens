# TLC run mutation-ResumeWithoutRecovery, 20261018T115238Z

Counter-check of plan-fixes-20261018 item 16: mutation ResumeWithoutRecovery. Expected: a violation of NoTwoWriters.

## Configuration

Spec: `model/tla/SequencePaxos.tla`. Config file `model/tla/MC_mut_ResumeWithoutRecovery.cfg`:

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
  Mutation = "ResumeWithoutRecovery"
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
INVARIANTS NoTwoWriters
```

## Tools

- TLC2 Version 2.19 of 08 August 2024 (rev: 5a47802)
- Running breadth-first search Model-Checking with fp 68 and seed -7453061525139314740 with 3 workers on 4 cores with 9102MB heap and 64MB offheap memory [pid: 4352] (Linux 6.18.44-fc-v139 amd64, Ubuntu 21.0.12.1 x86_64, MSBDiskFPSet, DiskStateQueue).
- Java:

```
openjdk version "21.0.12.1" 2026-08-18
OpenJDK Runtime Environment (build 21.0.12.1+1-1-24.04.4-Ubuntu)
OpenJDK 64-Bit Server VM (build 21.0.12.1+1-1-24.04.4-Ubuntu, mixed mode, sharing)
```

## Numbers

- States generated: 44,920
- Distinct states: 21,203
- States left on queue: 17,341
- Diameter (depth of the state graph search): 7 (level reached when TLC was stopped; not complete)
- Wall time: stopped by the 1 h limit (timeout 3600 s); last progress line at 2026-10-18 11:52:43 (start 20261018T115238Z, end 20261018T115336Z, UTC)

## Result

```
see output
```


The session stopped this breadth-first run after about 1 min (TLC killed by its PID, exit 143). The mutation moves to simulation.
