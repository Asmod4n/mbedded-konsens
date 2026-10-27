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

## Run 3: one request

| config | exit | result |
|---|---|---|
| MC_oe_rollback_r1 | 124 | limit, depth 28, 29,123,804 distinct states, no violation |
| MC_oe_rollback_takeslog_r1 | 124 | limit, depth 28, 27,158,327 distinct states, no violation |
| MC_oe_rollback_takeslog_parallel_r1 | 124 | limit, depth 28, 27,976,384 distinct states, no violation |

One request does not make the search smaller enough: the run without
the fix does not reach the violation at depth 29 either. Run 4 uses
the simulation of TLC, with the configuration without the fix as the
check that the simulation finds the violation.

## Run 4: simulation, `-simulate -depth 60`, 900 s each

| config | exit | result |
|---|---|---|
| MC_oe_rollback_l1 | 124 | 60,510,906 states checked, no violation |
| MC_oe_rollback_takeslog_l1 | 124 | 59,801,815 states checked, no violation |
| MC_oe_rollback_takeslog_parallel_l1 | 124 | 60,003,826 states checked, no violation |

The simulation does not find the known violation in the configuration
without the fix. So the simulation says nothing about the fix.

## Run 5: the small model `tla/RecoveryRollback.tla`

The model holds only the ballots, the logs, the store rollback and the
recovery. Each write is synced. The invariant `ChosenOnce` says that
one index has one chosen value. TLC 1 worker, `-deadlock`.

| config | Recovery | exit | result |
|---|---|---|---|
| MC_rr_promise_b2 | promise | 12 | ChosenOnce violated, 354,275 distinct states |
| MC_rr_answerlog_b2 | answerlog | 12 | ChosenOnce violated, 362,232 distinct states |
| MC_rr_newleader_b2 | newleader | 0 | complete, 1,228,309 distinct states, no violation |

The form "answerlog" (`RecoveryTakesLog` of OneEnvironment) is wrong
(`20261027T041800Z-rr-answerlog-b2-trace.out`):

1. n1 leads ballot 1. It accepts v1 and sends the accept.
2. n1 falls back to its store before v1, and starts its recovery.
3. n2 and n3 answer with an empty log.
4. n3 then accepts v1 from the old message. v1 is chosen by n1 and n3.
5. n1 completes the recovery with an empty log.
6. n2 leads ballot 2 with the promises of n2 and n1. Both logs are
   empty, and v2 is chosen.

The answers of the recovery can be older than an accept that is still
on the way. The form "newleader" waits: the node votes again only at
the first accept of a ballot above each promise of f + 1 answers, and
takes the log of that accept. A leader of such a ballot completed its
prepare without the node. This is the rule of "Paxos Made Live"
(Chandra, Griesemer, Redstone, PODC 2007, section 5.1): a replica with
a lost disk does not vote until it has seen one instance that started
after its rebuild. Viewstamped Replication Revisited (section 4.3) has
the same form: the recovering replica takes its state from the primary
of the latest view.
