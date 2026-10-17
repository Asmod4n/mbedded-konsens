# TLC run a-base-monotone-runs-e-f-off, 20261017T121020Z

Regression run after the client sessions of run (f) and the read index of run (e) were added to the model. Config MC_base_monotone with ClientSessions = FALSE, MaxClientSend = 0, MaxRequest = 0, ReadIndex = FALSE, MaxRead = 0 and Mutation = "none". Expected: the same numbers as run 20261017T093049Z-a-base-monotone-mutation-none (46061842 states generated, 13489555 distinct states, depth 35, no error).

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
SPECIFICATION Spec
CONSTRAINT Bounds
SYMMETRY ValueSymmetry
VIEW View
INVARIANTS AcceptedIdxIsLogLength S1 S2 S2b S3 S5
PROPERTIES S2Grows S7
```

## Tools

- TLC2 Version 2.19 of 08 August 2024 (rev: 5a47802)
- Running breadth-first search Model-Checking with fp 108 and seed -774197811507925039 with 3 workers on 4 cores with 5461MB heap and 64MB offheap memory [pid: 22332] (Linux 6.18.44-fc-v139 amd64, Ubuntu 21.0.12.1 x86_64, MSBDiskFPSet, DiskStateQueue).
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
- Wall time: 24min 14s (start 20261017T121020Z, end 20261017T123438Z, UTC)

## Result

```
Model checking completed. No error has been found.
```


## Comparison

The numbers of states generated, distinct states and the depth are equal to run 20261017T093049Z-a-base-monotone-mutation-none. With the new constants off, the model keeps its behaviour in this configuration. The wall time is not compared: the two runs are no measurement of speed.

All other config files were started for 15 s each with the new model, one worker each. Each computed its initial states. MC_mut_R1 found its violation of R1 in that time. The others were stopped by the time limit and were not run to the end. MC_f and MC_mut_E1 were not run again with the read index in the model.
