# Start checks: simulation runs, round 2

Model: da39406, configs d6e1939. Tool and settings as in round 1
(model/results/*-start-checks-runs-1.md): TLC v1.8.0, one worker,
`-simulate -deadlock -depth 60 -seed 1`, 15 min limit, four runs at a
time.

| Run | Config | StorageFault steps | States | Result |
|---|---|---|---|---|
| missing_entry, no check, no S5 | MC_s_nocheck_missing_entry_noS5 | 27 | 702 | AcceptedIdxIsLogLength violated |
| missing_accepted, no check, no recovery | MC_s_nocheck_missing_accepted_norec | 1368 | 40941 | S2b violated |
| missing_promise, no check, no recovery | MC_s_nocheck_missing_promise_norec | 552 | 17252 | S2b violated |
| checks, no recovery | MC_s_norec | 1015 | 8592 | S2b violated |

## What each trace shows

- missing_entry, no check: node 3 has log <<v1>> with decided index 1
  and loses its only entry. The decided index is now above the log
  length. This is the case that the check "decided index above the log
  length" finds.
- missing_accepted, no check, no recovery: node 2 holds <<v2>> at
  accepted round 11 and loses the accepted round. Without the recovery
  after the start, S2b breaks.
- missing_promise, no check, no recovery: node 2 loses its promise 23
  and keeps <<v2>> at accepted round 11. S2b breaks.
- checks, no recovery: node 3 loses its last entry, the only one. The
  log is then empty, and the continuity check cannot find a missing
  last key. This is a lost write at the end of the log (row 2.6 of the
  matrix). Without the recovery after the start, S2b breaks.

## Result so far

- The recovery after each start (Recovery = TRUE) is what protects
  against a lost last entry. The checks do not find it, as plan section
  3.5 says.
- Without the recovery, missing_accepted and missing_promise break
  S2b. With the recovery, round 1 found no violation in 15 min for
  either. Not checked: whether the recovery covers them in every
  behaviour. Round 3 runs them with a second seed.
- Round 3 runs missing_entry without the checks with only the safety
  invariants (S1 S2 S2b S3 S4 S6), to see whether the decided logs
  break.
