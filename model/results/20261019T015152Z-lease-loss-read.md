# TLC run lease-loss-read, 20261019T015152Z

Loss check of plan-witness-lease (8b7cafd), model d58cce1. EachReadSeesDecided or StaleReadIsMarked is expected to break by the same loss. Breadth-first, limit 30 min.

## Configuration

Spec: `model/tla/SequencePaxos.tla`. Config file `model/tla/MC_w_loss_read.cfg`:

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
INVARIANTS EachReadSeesDecided StaleReadIsMarked
```

## Tools

- TLC2 Version 2.19 of 08 August 2024 (rev: 5a47802)
- Running breadth-first search Model-Checking with fp 130 and seed -7660716413618750265 with 3 workers on 4 cores with 9102MB heap and 64MB offheap memory [pid: 6225] (Linux 6.18.44-fc-v139 amd64, Ubuntu 21.0.12.1 x86_64, MSBDiskFPSet, DiskStateQueue).
- Java:

```
openjdk version "21.0.12.1" 2026-08-18
OpenJDK Runtime Environment (build 21.0.12.1+1-1-24.04.4-Ubuntu)
OpenJDK 64-Bit Server VM (build 21.0.12.1+1-1-24.04.4-Ubuntu, mixed mode, sharing)
```

## Numbers

- States generated: 110,160,840
- Distinct states: 31,188,629
- States left on queue: 20,632,001
- Diameter (depth of the state graph search): 12 (level reached when TLC was stopped; not complete)
- Wall time: stopped by the 1 h limit (timeout 3600 s); last progress line at 2026-10-19 02:20:56 (start 20261019T015152Z, end 20261019T022153Z, UTC)

## Result

```
No error in the explored states. TLC was stopped by the time limit (exit 124).
```

