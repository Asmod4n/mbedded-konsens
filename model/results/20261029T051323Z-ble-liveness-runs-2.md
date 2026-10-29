# Model L: long runs of 2026-10-29

Model: model/tla/BallotLeaderElection.tla at 10e7fdd.
Tool: TLC 2.19, breadth-first search, -Xmx4g, -deadlock, metadir on a
compressing btrfs file system. Machine: session VM, 4 processors,
15 GiB memory, Linux 6.18. Both runs at the same time, time limit 8 h.
Liveness property: EventuallyStableLeader. Invariants: TypeOK,
NoOverflow. Round form atomic, TrackStable FALSE, 3 nodes, MaxN 3,
MaxEarlyTimeout 1.

These two configurations were cut after 2 h in
20261029T010607Z-ble-liveness-runs-1.md.

| Config | Faults | Workers | Time | Result | Distinct states | States generated | Depth |
|---|---|---|---|---|---|---|---|
| MC_ble_cut_atomic | 1 cut link | 1 | 3 h 07 min | holds, complete | 304,506 | 1,724,595 | 20 |
| MC_ble_all_restart_small_atomic | 1 restart | 2 | 3 h 26 min | holds, complete | 801,923 | 4,349,261 | 22 |

Fingerprint collision probability, based on the actual fingerprints:
cut 3.7E-8, restart 1.6E-8.

With these runs, the atomic form holds the liveness property for each
fault class of the model: none, 1 crash, 1 restart, 1 cut link and
chained links.
