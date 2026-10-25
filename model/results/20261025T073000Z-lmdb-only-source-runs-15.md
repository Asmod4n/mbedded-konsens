# LMDB as the only source: simulation runs, round 15

Model at cc51978. New configs: `MC_st_start_peer` (MC_st_start of
round 14 with `PeerCheck = TRUE`) and `MC_mm_damage_stale_peer`
(MC_mm_damage_stale with `PeerCheck = TRUE`). They check the repair
from a peer together with the peer check of plan 2.3 item 6 (review of
doc/plan-repair-from-peer.md, finding 9). TLC simulation, seed 7,
depth 60, one worker each, two runs at the same time, limit 15 min.

| Config | Faults | Limit | States | Result |
|---|---|---|---|---|
| MC_st_start_peer | 2 of missing_promise, missing_entry, stale_meta, rollback | 15 min | 8300179 (last progress line) | no violation |
| MC_mm_damage_stale_peer | 1 stale_meta while the node runs | 15 min | 7195159 (last progress line) | no violation |

Invariants of MC_st_start_peer: S1 S2 S3 S4 HighWaterHolds
AcceptedIdxIsLogLen EntryRunsOnOnePrefix StateIsDecided OpenRunsOnLog.
Invariants of MC_mm_damage_stale_peer: S1 S2 S3 S4 HighWaterHolds
AcceptedIdxIsLogLen MismatchBlamesFollower; properties S2Grows
BuildOnlyFromRunning.

Both runs ended at the limit (exit 124 of timeout), not at a violation.
