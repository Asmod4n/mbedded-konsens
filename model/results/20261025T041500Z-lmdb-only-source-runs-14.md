# LMDB as the only source: simulation runs, round 14

Model b15ed8d. New: `StartDamageCounts` (owner, 2026-10-25 03:54; plan
2.4 item 8). A damage that the start checks find makes the node repair
itself from a peer while its count allows it, and the repair counts.
Configs `MC_st_start*`: MC_st_fault of round 13 with
`StartDamageCounts = TRUE` and more fault kinds. TLC simulation, seed 7,
depth 60, one worker.

| Config | Faults | Limit | States | Result |
|---|---|---|---|---|
| MC_st_start | 2 of missing_promise, missing_entry, stale_meta, rollback | 15 min | 3892983 | no violation |
| MC_st_start_cover | as MC_st_start | 5 min | 259 | NoRepairLimitReached violated, as a cover must be |
| MC_st_start_only_cover | 2 of missing_promise, missing_entry, at the start only | 5 min | 21132 | NoRepairLimitReached violated, as a cover must be |

Invariants of MC_st_start: S1 S2 S3 S4 HighWaterHolds
AcceptedIdxIsLogLen EntryRunsOnOnePrefix StateIsDecided OpenRunsOnLog.

- With faults at the start only, a node reaches the limit and is
  refused. `repairs` grows only through a repair, so the first damage
  at the start was repaired and counted.
- With the repair at the start counted, S1 to S4 and the state
  invariants hold.
