# Model L: runs of 2026-10-28 and the night after

Model: model/tla/BallotLeaderElection.tla at 1bf04e7 (round forms of
plan section 3a from f9468b8; the runs before 14:30 UTC used the model
at f1b9ddb, which equals RoundForm "async" and TrackStable TRUE).
Tool: TLC 2.19, breadth-first search, 1 worker, -Xmx2g, metadir on a
compressing btrfs file system. Machine: session VM, 4 processors,
15 GiB memory, Linux 6.18. At most 3 runs at a time.
Liveness property: EventuallyStableLeader. "Cut" means stopped by the
time limit, with no liveness result.

## Size of the state space (invariants only, no faults, 3 nodes, MaxN 3, MaxEarlyTimeout 1)

| Round form | Result | Distinct states |
|---|---|---|
| async | cut after 15 min, depth 36 | more than 10,556,964 |
| async, one crash | cut after 30 min, depth 34 | more than 21,656,399 |
| states before Stabilize only | complete, depth 18 | 36,936 |
| collect | complete, depth 39 | 721,000 |
| atomic | complete, depth 19 | 43,366 |

The size comes from the order of the heartbeat messages after
Stabilize.

## Liveness runs, round form async (the first form)

| Config | Bounds | Time | Result | Distinct states |
|---|---|---|---|---|
| MC_ble_cover | MaxN 4 | 2 min | cover reached, as wanted | 1,972,215 |
| MC_ble_all_crash | MaxN 4, crash 1 | 1.5 h | cut | 950,924 |
| MC_ble_all_restart | MaxN 4, restart 1 | 1.5 h | cut | 947,916 |
| MC_ble_mut_no_above | MaxN 4 | 1.5 h | cut | 2,452,121 |
| MC_ble_mut_no_timeout | MaxN 4 | 30 min | cut | 341,748 |
| MC_ble_mut_no_quorum | MaxN 4, cut 1 | 30 min | cut | 504,739 |
| MC_ble_all_crash_small | MaxN 3, early 1 | 50 min | cut | 998,941 |
| MC_ble_all_restart_small | MaxN 3, early 1 | 50 min | cut | 1,433,935 |
| MC_ble_all_crash_small, -lncheck final | MaxN 3, early 1 | 1.5 h | cut | 1,018,595 |

## Liveness runs, round form collect (TrackStable FALSE)

| Config | Time | Result | Distinct states |
|---|---|---|---|
| MC_ble_nofault_collect | 2 h | cut | 189,893 |
| MC_ble_all_crash_small_collect | 2 h | cut | 625,205 |
| MC_ble_all_restart_small_collect | 2 h | cut | 971,813 |

## Liveness runs, round form atomic (TrackStable FALSE, MaxN 3, MaxEarlyTimeout 1)

| Config | Faults | Time | Result | Distinct states |
|---|---|---|---|---|
| MC_ble_nofault_atomic | none | 48 min | holds, complete, depth 19 | 43,366 |
| MC_ble_all_crash_small_atomic | 1 crash | 1 h 53 min | holds, complete, depth 21 | 473,462 |
| MC_ble_chain_atomic | chained links | 5 min | holds, complete, depth 15 | 3,743 |
| MC_ble_all_restart_small_atomic | 1 restart | 2 h | cut | 362,838 |
| same, -lncheck final | 1 restart | 2 h | cut | 366,070 |
| MC_ble_cut_atomic | 1 cut link | 2 h | cut | 206,652 |
| same, -lncheck final | 1 cut link | 2 h | cut | 256,346 |
| MC_ble_cover_atomic (TrackStable TRUE) | 1 crash | 11 s | cover reached, as wanted | 97,179 |

## Mutations, round form atomic

| Config | Expected | Result | Distinct states |
|---|---|---|---|
| MC_ble_mut_no_timeout_atomic, -deadlock | liveness fails | liveness violated | 2,283 |
| MC_ble_mut_no_quorum_atomic, MaxCut 2 | NoOverflow fails after Stabilize | NoOverflow violated | 13,775 |
| MC_ble_mut_no_above | - | not run: it cannot break in this model (plan 3a item 11) | - |

## What the runs show

- A stable leader comes and stays with no fault, after one crash, and
  on chained links, in the atomic round form.
- With a restart or a cut link, the runs did not end in 2 h. A run
  with the metadir in memory had the same rate (about 1,700 distinct
  states per minute). So the disk is not the cause. Not checked: the
  share of the 31 weak fairness conditions in the time.
- The model does not answer the question of the owner of 2026-10-21
  (AbovePromise): it does not model complete_recovery.
