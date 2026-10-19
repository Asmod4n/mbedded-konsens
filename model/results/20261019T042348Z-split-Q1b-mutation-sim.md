# TLC run split-Q1b-mutation-sim, 20261019T042348Z

Counter-check of plan-split-20261019 (9dfc42d), model d58cce1, 2 workers. Question Q1b with its mutation, random simulation (-simulate -deadlock -depth 60, seed 1, limit 15 min). Expected: a violation.

## Configuration

Spec: `model/tla/SequencePaxos.tla`. Config file `model/tla/Q1b_mut.sim.cfg`:

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
  MaxLoss = 1
  MaxDuplicate = 0
  MaxTimeout = 0
  MaxElection = 2
  RuleR = TRUE
  DecidedIdxMonotone = TRUE
  MaxRollback = 0
  Recovery = FALSE
  Mutation = "SeqNumUnchecked"
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
  Witness = FALSE
  LeaseLength = 0
SPECIFICATION Spec
CONSTRAINT Bounds
INVARIANTS S5
```

## Tools

- TLC2 Version 2.19 of 08 August 2024 (rev: 5a47802)
- Running Random Simulation with seed 1 with 2 workers on 4 cores with 9102MB heap and 64MB offheap memory [pid: 11296] (Linux 6.18.44-fc-v139 amd64, Ubuntu 21.0.12.1 x86_64).
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
- Wall time: n/a (start 20261019T042348Z, end 20261019T043848Z, UTC)

## Result

```
No error in the explored states. TLC was stopped by the time limit (exit 124).
```

