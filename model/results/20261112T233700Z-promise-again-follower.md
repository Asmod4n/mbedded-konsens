# Plan promise-again: a follower promises the same ballot again

Model of commit 2b63436 (branch model-promise-again). TLC 2026.11.06 (rev d8fb231), BFS, one worker, 3040 MB heap, Java 21, Linux 6.18.44, 4 cores. Plan: model/plan-promise-again.md, section 3.

## cover NoPromiseAgainReached, MC_a_F1_dup.cfg
```
Error: Invariant NoPromiseAgainReached is violated.
244 states generated, 142 distinct states found, 121 states left on queue.
The depth of the complete state graph search is 4.
Finished in 02s at (2026-11-12 22:40:14)
```

## MC_a_F1_dup.cfg with RecoverIsFollower, -deadlock
```
Running breadth-first search Model-Checking with fp 76 and seed 2804880808756052666 with 1 worker on 4 cores with 3040MB heap and 64MB offheap memory [pid: 13119] (Linux 6.18.44-fc-v378 amd64, Ubuntu 21.0.12.1 64bit, MSBDiskFPSet, DiskStateQueue).
Progress(16) at 2026-11-13 00:28:32: 25,440,972 states generated (563,414 s/min), 7,303,439 distinct states found (147,928 ds/min), 2,248,702 states left on queue.
```

## MC_a_F1_loss.cfg with RecoverIsFollower, -deadlock
```
Running breadth-first search Model-Checking with fp 99 and seed -4797223673322300179 with 1 worker on 4 cores with 3040MB heap and 64MB offheap memory [pid: 13120] (Linux 6.18.44-fc-v378 amd64, Ubuntu 21.0.12.1 64bit, MSBDiskFPSet, DiskStateQueue).
Progress(13) at 2026-11-13 00:28:32: 30,070,827 states generated (631,712 s/min), 8,207,760 distinct states found (156,346 ds/min), 4,151,786 states left on queue.
```

Both runs started at 23:37 UTC with a time limit of one hour. A restart of the container stopped them after 51 min, before the limit. Neither run found a violation of an invariant. Neither run completed: the queue grew in each minute of the run, so the time limit is not long enough for a complete search.

## MC_base, MC_base_monotone, MC_base_s7

These configs do not reach the new path (plan section 3). Each run stopped after 757 678 distinct states with no violation.
