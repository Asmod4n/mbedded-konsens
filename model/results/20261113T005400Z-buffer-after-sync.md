# Plan buffer-after-sync: the buffer of a new leader after the AcceptSync

Model of commit 41678fe (branch model-buffer-after-sync). Plan:
model/plan-buffer-after-sync.md, section 3. TLC 2026.11.06 (rev d8fb231),
BFS, one worker, OpenJDK 21.0.12.1, Linux 6.18.44, 4 cores.

## Trace check, seeds 1 to 10

The trace spec and the traces of the spike of 2026-11-13 (branch
trace-validation, model/results/20261113T004300Z-trace-spike-seed1-net.md),
code 654c088. One change to the config of the trace spec:
`DecidedIdxMonotone = TRUE`. The code keeps max(old, new) for the
decided index of a follower; the config of the spike had the form of
Omni-Paxos. Seed 5 was refused at record 274 for that reason, before
the change.

| seed | records | end | first record not matched |
|---|---|---|---|
| 1 | 887 | whole trace accepted | none |
| 2 | 807 | whole trace accepted | none |
| 3 | 558 | refused | 26: crash, not clean |
| 4 | 616 | refused | 44: crash, not clean |
| 5 | 839 | whole trace accepted | none |
| 6 | 796 | whole trace accepted | none |
| 7 | 570 | refused | 56: crash, not clean |
| 8 | 594 | refused | 58: crash, not clean |
| 9 | 602 | refused | 149: crash, not clean |
| 10 | 872 | whole trace accepted | none |

Each run took 4 to 11 s. A crash that is not clean retires the node,
and F6 of plan-trace-validation ends the trace there. So every record
of phase 1 in the ten traces is accepted.

## Counter-checks

| run | first record not matched |
|---|---|
| `Mutation = "BufferInSync"`, seed 2 / 5 / 6 | 21 / 160 / 27 (the old refusals) |
| AcceptDecide with the counter of the AcceptSync, seed 2 / 5 / 6 | 29 / 217 / 126 |
| AcceptDecide with `decided_idx = 0`, seed 2 / 5 / 6 | not refused |
| seed 1, decided index of node 1 plus 1 at record 50 | 50 |
| seed 1, first `decide` from 2 to 1 out of `net` at record 605 | 605 |
| seed 1, the old form of `HandlePrepare` | 55 |

The trace spec does not see `decided_idx = 0`. In these traces the
decided index of the new leader at that step is not above the decided
index of the follower, and the follower keeps the larger one. So the
mutation changes no state. The trace check compares the types of the
messages, not their fields.

## Action property NoSyncAndDecideInOneStep, MC_a_F1_dup.cfg, -deadlock

New form:
```
Error: Action property NoSyncAndDecideInOneStep is violated.
920 states generated, 432 distinct states found, 357 states left on queue.
The depth of the complete state graph search is 5.
Finished in 01s at (2026-11-13 00:54:50)
```

Old form (`Mutation = "BufferInSync"`), time limit 15 min:
```
Progress(13) at 2026-11-13 01:08:52: 6,793,152 states generated (460,676 s/min), 2,109,013 distinct states found (133,905 ds/min), 921,125 states left on queue.
```

No violation under the old form. The search did not complete.

## MC_a_F1_dup.cfg and MC_a_F1_loss.cfg, -deadlock, one hour each

```
Progress(16) at 2026-11-13 02:04:12: 23,104,521 states generated (400,553 s/min), 6,479,181 distinct states found (108,923 ds/min), 1,947,078 states left on queue.
Progress(13) at 2026-11-13 02:04:11: 26,716,075 states generated (440,881 s/min), 7,130,046 distinct states found (116,910 ds/min), 3,586,099 states left on queue.
```

Both runs ended at the time limit of one hour with no violation of an invariant. Neither run searched the whole state space: the queue still held 1 947 078 and 3 586 099 states.
