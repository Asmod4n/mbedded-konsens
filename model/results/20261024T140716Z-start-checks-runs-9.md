# Start checks: simulation runs, rounds 9 and 10

Model 80d8829: a node that reads no promise takes the durable state of
a node that votes and does not recover, then runs the recovery (plan
item 2.6, review 3 finding 9). Settings as in rounds 4 to 6. "Copy" is
the coverage count of that branch.

| Config | Seed | Kinds | Copy | Refused | States | Result |
|---|---|---|---|---|---|---|
| MC_s | 1 | all four | 354302 | 112137 | 5265830 | no violation |
| MC_s_promise | 1 | promise | 340486 | 60405 | 4972260 | no violation |
| MC_s_nocheck_missing_promise | 1 | promise, no checks | 489576 | 0 | 5477654 | no violation |
| MC_s_crash | 1 | all four | 400946 | 81946 | 5207621 | no violation |
| MC_s_powerloss_log2 | 1 | entry, accepted, promise; MaxLog 2 | 404248 | 38732 | 5211801 | no violation |
| MC_s_rollback | 1 | all four | 348704 | 64281 | 5240140 | no violation |

- MC_s_nocheck_missing_promise is the run at the current model that
  review 3 finding 8 asked for.
- MC_s_powerloss_log2 runs the continuity check under power loss.
- A simulation without a violation is not a complete run.
