# Start checks: simulation runs, round 3

Model: da39406, configs f6c0d2f. Tool and settings as in round 1,
with the seed in the table.

| Run | Config | Seed | StorageFault steps | States | Result |
|---|---|---|---|---|---|
| missing_entry, no check, safety only | MC_s_nocheck_missing_entry_safety | 1 | 54 | 1313 | S2 violated |
| missing_accepted, no check | MC_s_nocheck_missing_accepted | 2 | 166509 | 5092501 | no violation in 15 min |
| missing_promise, no check | MC_s_nocheck_missing_promise | 2 | 162683 | 4975179 | no violation in 15 min |
| checks | MC_s | 2 | 612770 | 5202966 | no violation in 15 min |

## missing_entry without the checks

All three nodes hold <<v2, v1>> at accepted round 23, and node 3 has
decided index 2. Node 3 loses the key of entry 1 and reads <<v1>>.
Its decided prefix is then <<v1>>, which is not a prefix of the
decided history <<v2, v1>>. S2 breaks in the step of the fault. The
node holds a decided entry that the cluster did not decide at that
index.

## Result of rounds 1 to 3

| Kind | No check, recovery | No check, no recovery | Checks, recovery |
|---|---|---|---|
| missing_entry (not last) | S5, then S2 at once | not run | no violation, 2 seeds |
| missing last entry | not found by the checks | S2b (MC_s_norec) | no violation, 2 seeds |
| missing_accepted | no violation, 2 seeds | S2b | no violation, 2 seeds |
| missing_promise | no violation, 2 seeds | S2b | no violation, 2 seeds |
| missing_decided | no violation, 1 seed | not run | no violation, 2 seeds |

- A missing key below the last key breaks S2 without the checks. The
  continuity check is needed.
- A missing accepted round or promise breaks S2b without the recovery
  after the start. With that recovery, no simulation found a violation.
  Not proven: a simulation is not a complete run.
- A missing last entry is found by no check. Only the recovery after
  each start protects against it.
