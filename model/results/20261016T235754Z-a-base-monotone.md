# TLC run a-base-monotone, 20261016T235754Z

Variant run, owner task of 2026-10-16 23:54: DecidedIdxMonotone = TRUE, so the follower keeps max(old, new) decided_idx in handle_acceptsync. Bounds of MC_base: rule R, no fault, two calls of handle_leader, ballot n up to 2, log up to 2, counter up to 3, session up to 2. S7 is checked again, together with AcceptedIdxIsLogLength, S1, S2, S2b, S3, S5 and S2Grows.

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
SPECIFICATION Spec
CONSTRAINT Bounds
SYMMETRY ValueSymmetry
VIEW View
INVARIANTS AcceptedIdxIsLogLength S1 S2 S2b S3 S5
PROPERTIES S2Grows S7
```

## Tools

- TLC2 Version 2.19 of 08 August 2024 (rev: 5a47802)
- Running breadth-first search Model-Checking with fp 89 and seed -6270110849283730012 with 3 workers on 4 cores with 5461MB heap and 64MB offheap memory [pid: 6937] (Linux 6.18.44-fc-v139 amd64, Ubuntu 21.0.12.1 x86_64, MSBDiskFPSet, DiskStateQueue).
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
- Wall time: 14min 36s (start 20261016T235754Z, end 20261017T001232Z, UTC)

## Result

```
Model checking completed. No error has been found.
```


## Leader side: can sync_log in handle_majority_promises lower decided_idx?

Checked in the source; the variant is not applied there.

- handle_majority_promises passes decided_idx = leader_state.get_max_decided_idx() to sync_log (leader.rs:259-262). That is the maximum over all promises in state Promised (util.rs:168-177).
- handle_leader puts the leader's own promise, with its current decided_idx, into leader_state before any Prepare is sent (leader.rs:30-40).
- The own entry is never removed: reset_promise and lost_promise are called only with the pid of a peer that sent PrepareReq or NotAccepted (leader.rs:66-74, leader.rs:365-369).
- In (Leader, Prepare) the leader's decided_idx does not change: handle_acceptsync and handle_acceptdecide need Role::Follower (follower.rs:54, follower.rs:90), handle_decide needs Phase::Accept (follower.rs:130), handle_accepted needs Phase::Accept (leader.rs:326).

So the maximum is at least the leader's own decided_idx, and the leader's sync_log cannot lower it in the source. In the model one case differs: under rule R a wiped leader does not register its own promise (Votes(i) is FALSE). Its decided_idx can then fall in this step. A wiped node is exempt from S7, so the variant leaves the leader side as in the source.
