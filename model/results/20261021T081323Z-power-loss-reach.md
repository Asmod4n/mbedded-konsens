# TLC run power-loss-reach, 20261021T081323Z

Reach check of model/plan-power-loss.md section 3 and R7. Config `model/tla/power-loss/MC_p_reach.cfg` (PowerLoss = TRUE, MaxCrash = 3). The invariant NoPowerLossCompletion states that no node completes recovery in power-loss mode while every node recovers. Expected: a violation. Model d92f43a plus the value "PowerLossWithF" in the ASSUME of Mutation (no change of behaviour). Breadth-first search, 1 worker.

## Configuration

```
CONSTANTS
  Nodes = {1, 2, 3}
  Values = {v1, v2}
  MaxBallot = 2
  MaxLog = 1
  MaxCounter = 4
  MaxSession = 2
  Cap = 2
  MaxCrash = 3
  MaxWipe = 0
  MaxLoss = 0
  MaxDuplicate = 0
  MaxTimeout = 1
  MaxElection = 2
  RuleR = TRUE
  DecidedIdxMonotone = TRUE
  MaxRollback = 0
  Recovery = TRUE
  PowerLoss = TRUE
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
  Witness = FALSE
  LeaseLength = 0
SPECIFICATION Spec
CONSTRAINT Bounds
SYMMETRY ValueSymmetry
VIEW View
INVARIANTS NoPowerLossCompletion
```

## Numbers

- States generated: 2856178. Distinct states: 993113. Depth: 8.
- Wall time: 02min 22s (start 20261021T081323Z, UTC).

## Result

```
Error: Invariant NoPowerLossCompletion is violated.
```

The expected violation. In the trace, nodes 3, 2 and 1 crash, all three send Recovery, all three answer as recovering nodes, and node 1 completes in power-loss mode in state 8. Every log is empty in this trace. So the branch fires; a trace with a non-empty log is not shown by this run.

## Trace, state 8 (excerpt)

```
/\ recovering = <<FALSE, TRUE, TRUE>>
/\ power_loss_completed = {1}
/\ phase = <<"Recover", "Recover", "Recover">>
/\ crashes = 3
/\ log = <<<<>>, <<>>, <<>>>>
```

## Tools

- TLC2 Version 2.19 of 08 August 2024 (rev: 5a47802)
- Running breadth-first search Model-Checking with fp 46 and seed -1627877102396042067 with 1 worker on 4 cores with 600MB heap and 64MB offheap memory [pid: 2394] (Linux 6.18.44-fc-v140 amd64, Ubuntu 21.0.12.1 x86_64, MSBDiskFPSet, DiskStateQueue).
