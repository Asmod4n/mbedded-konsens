# OneEnvironment: a store that falls back to the sync before its last one

Model: `tla/OneEnvironment.tla` at commit 12fd21e (RollbackCrash).
TLC 1 worker each, `-deadlock`, limit 1800 s, 3 runs at a time.

| config | exit | result |
|---|---|---|
| MC_oe_rollback_l1 | 12 | AppliedIsDecided violated, depth 29, 25,445,888 distinct states, 28 min 55 s |
| MC_oe_rollback_parallel_l1 | 124 | limit, depth 29, 26,241,288 distinct states, no violation |
| MC_oe_base_l1 | 124 | limit, depth 31, 24,178,054 distinct states, no violation |

The trace (`20261026T185400Z-rollback-l1-trace.out`):

1. n1 leads ballot 1. n1 and n2 accept r1 at index 1 and sync. r1 is
   chosen, and n1 decides and applies it.
2. n2 restarts with the store of its sync before the last one: its log
   is empty and its accepted ballot is 0.
3. n2 recovers. The recovery takes only the highest promise of f + 1
   answers, not the log.
4. n2 prepares ballot 2 and gets promises from n2 and n3. Both have an
   empty log. n2 sends an empty log in accept-sync, and n1 takes it.
5. n1 has applied index 1 with decided index 0.

So a node that loses a synced entry and then votes can lose a decided
entry. The fix in the model: the recovery takes the log, the accepted
ballot and the decided index of the answer with the highest accepted
ballot and the longest log (`RecoveryTakesLog`).

## Run 2: the recovery takes the log (model 0c9ca02)

| config | exit | result |
|---|---|---|
| MC_oe_rollback_takeslog_l1 | 124 | limit, depth 26, 29,637,292 distinct states, no violation |
| MC_oe_rollback_takeslog_parallel_l1 | 124 | limit, depth 26, 30,009,168 distinct states, no violation |

The violation of run 1 lies at depth 29. These runs end at depth 26,
so they do not show that the fix holds. Run 3 takes one request
(`*_r1.cfg`) so that the search can end.
