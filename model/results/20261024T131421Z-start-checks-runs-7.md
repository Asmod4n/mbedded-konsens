# Start checks: simulation runs, rounds 7 and 8

Model 6276400: a node that reads no promise restarts through the
recovery (owner, 2026-10-24 12:04); the start of the code at 4c57d44
is the mutation StartAsNewNode. Settings as in rounds 4 to 6
(model/results/*-start-checks-runs-4.md). Every config with the checks
has NoFalseRefusal.

| Config | Seed | Kinds | Refused | Restarted | States | Result |
|---|---|---|---|---|---|---|
| MC_s | 1 | all four | 109528 | 207793 | 4917436 | no violation |
| MC_s_promise | 1 | promise | 61802 | 174764 | 4875231 | no violation |
| MC_s_promise_asnew | 1 | promise, mutation | 298 | 771 | 23487 | S2b violated |
| MC_s_crash | 1 | all four | 86098 | 236489 | 5324181 | no violation |
| MC_s_powerloss | 1 | entry, accepted, promise | 32787 | 157224 | 5262412 | no violation |
| MC_s_rollback | 1 | all four | 64520 | 196658 | 5173979 | no violation |

- With the recovery at a start with no promise, MC_s_promise found no
  violation in 15 min. Round 4 found S2b after 23654 states with the
  old start; the mutation StartAsNewNode finds it again after 23487.
- No run with the checks broke NoFalseRefusal.
- A simulation without a violation is not a complete run.
