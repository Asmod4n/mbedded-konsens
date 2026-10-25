# LMDB as the only source: simulation runs, round 13

Model 872cec4 (`StateInLmdb`, plan 2.6 and 2.4 item 10, review 3
findings 1 to 4). Configs `MC_st_*`: the configs `MC_pipe_*` of round 12
with `StateInLmdb = TRUE` and `MaxInFlight = 2`. TLC simulation, seed
7, depth 60, one worker, at most three runs at a time. The covers and
the mutation ran for 25 s; the others ran to the limit of 15 min.

| Config | Change against MC_pipe_* | States | Result |
|---|---|---|---|
| MC_st_cover_two | no crash | 213 | NoTwoInFlight violated, as a cover must be |
| MC_st_cover_partial | no crash | 235 | NoPartialDecide violated, as a cover must be |
| MC_st_cover_cut | no crash | 2866 | NoCutOfOpen violated, as a cover must be |
| MC_st_noabort | Mutation NoAbortOnCut | 3138 | StateIsDecided violated, as it must be |
| MC_st_crash | - | 4096639 | no violation |
| MC_st_net | 1 loss, 1 duplicate | 4068191 | no violation |
| MC_st_fault | stale_meta, rollback, repair | 3875768 | no violation |
| MC_st_keep_fault | as fault, StateRepair "keep" | 4153003 | no violation |
| MC_st_sessions | ClientSessions, MaxTimeout 1 | 3906036 | no violation |

Invariants of the runs with no violation: S1 S2 S3 S4 HighWaterHolds
AcceptedIdxIsLogLen StateIsDecided OpenRunsOnLog, and
EntryRunsOnOnePrefix except in MC_st_sessions.

- With batches in flight, a partial decide and a cut of the open
  transaction all reachable, the committed state holds only decided
  entries in every run.
- Without the abort on a cut, the committed state takes an entry that is
  not decided. So the rule of 2.6 item 7 is needed.
- "keep" held too: in these runs no repair met a state that disagreed
  with the decided prefix of the repaired log, because the committed
  state holds only decided entries. The copy of 2.4 item 10 stays for
  the case that the model does not have: a fault of the state
  environment.
- Not checked: a cover that a resend of a client request reached the
  log twice in MC_st_sessions.
- Found on the way: APA_F6c.tla defined OVERRIDE_MaxHalt and
  OVERRIDE_StaleReadNode twice (since 6c338d0); SANY refused it. Fixed in
  872cec4.
