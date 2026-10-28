# One environment: runs of the tombstone expiry

Model: model/tla/OneEnvironment.tla at b136871 (the expiry of plan
5.1 item 6 and the fix that a log write of a new leader replaces the
tombstones at or below its length). Plan: doc/plan-one-environment.md,
sections 13.1 and 13.2. TLC 2.19, OpenJDK 21.0.12.1, 4 processors, one
worker, the command of 20261026T095100Z-one-environment-runs-1.md.
Each config: Expiry 1, MaxTick 2, NoNonce FALSE, except where named.

| config | exit | wall | result |
|---|---|---|---|
| MC_oe_mut_NoNonce (NoNonce TRUE) | 12 | 2 s | NoOldWorkRecords violated, as the mutation expects |
| MC_oe_cover_NoTombstoneExpires | 12 | 2 s | cover reached: a tombstone expires |
| MC_oe_cover_NoOrphanTombstone | 12 | 2 s | cover reached: a tombstone without its done record |
| MC_oe_cover_NoWorkRecordSurvives | 12 | 1 s | cover reached: a tombstone survives a step |
| MC_oe_p0_l1 (BFS) | 124 | 6601 s | no violation; depth 25, 101,485,586 distinct states, 35,450,285 states left on the queue at the limit |

The safe form p0_l1 did not complete. Its queue grew in the last hour,
so a longer limit at these bounds does not end it.

Round 1 of these runs (before b136871) found that p0_l1 violated
NoOldWorkRecords, because a log write of a new leader did not replace
the tombstones. b136871 fixed this.
