# TLC runs after the review of the code plan, 20261021T102805Z

Model 4c357be: the tie rule of `PowerLossSource` (the node itself, then the lowest id) and the check `PowerLossKeptPrefix` (an `Assert` in the power-loss branch), after doc/plan-power-loss-code.md section 7 (R1, R6, R7). TLC 2026.10.20.183001. Each simulation: `-simulate -deadlock -depth 60 -seed 1 -workers 1`, limit 15 min, one after the other, beside the breadth-first run below. A simulation without a violation is not a complete run.

## Simulations

| Run | Config in model/tla/power-loss/ | Expected | Exit | Last progress line |
|---|---|---|---|---|
| mut_start | MC_mut_PowerLossStartAtAccepted.sim.cfg | Assert PowerLossKeptPrefix fails | 255 | 356096 states checked, 6775 traces generated (trace length: mean=31, var(x)=29, sd=5) |
| rollback | MC_p_rollback.sim.cfg | no violation | 124 | 7313624 states checked, 138891 traces generated (trace length: mean=31, var(x)=30, sd=5) |
| p | MC_p.sim.cfg | no violation | 124 | 7439041 states checked, 136150 traces generated (trace length: mean=28, var(x)=39, sd=6) |
| p_dup | MC_p_dup.sim.cfg (MaxDuplicate 1) | no violation | 124 | 7278801 states checked, 130944 traces generated (trace length: mean=30, var(x)=34, sd=6) |
| c_crash | MC_c_crash.sim.cfg (PowerLoss FALSE) | no violation | 124 | 3091094 states checked, 96909 traces generated (trace length: mean=17, var(x)=17, sd=4) |
| c_crash_timeout | MC_c_crash_timeout.sim.cfg (MaxTimeout 1) | no violation | 124 | 3428327 states checked, 96056 traces generated (trace length: mean=21, var(x)=23, sd=5) |
| d2_crash | MC_d2_crash.sim.cfg (PowerLoss FALSE) | no violation | 124 | 5415550 states checked, 146179 traces generated (trace length: mean=25, var(x)=87, sd=9) |

Exit 124 is the time limit, with no violation. Exit 255 of mut_start is the `Assert`:

```
The exception was a tlc2.tool.EvalException
: The first argument of Assert evaluated to FALSE; the second argument was:
"PowerLossKeptPrefix"
```

So the mutation that always starts at `accepted_idx` breaks the prefix that the code keeps, and the rule of the plan (from `decided_idx` when `n_accepted` differs) does not break it in the runs above.

## Breadth-first regression MC_F6b

PowerLoss = FALSE. Breadth-first with `-deadlock`, 3 workers.

- New model (4c357be): 177428893 states generated, 45351136 distinct states found, 0 states left on queue. The depth of the complete state graph search is 37. No error. Finished in 01h 34min at (2026-10-21 12:02:34)
- Old model (b1f15d7, before the power-loss change): 176234869 states generated, 45345778 distinct states found, 0 states left on queue. The depth of the complete state graph search is 37. No error. Finished in 01h 32min (start 20261021T120843Z).
- Both arms hold every property. The new model has 5358 more distinct states. With PowerLoss = FALSE and MaxTimeout = 0, the one change that acts in MC_F6b is that recovery keeps the maximum of its own decided_idx and that of the primary, so the difference is taken to come from that change (inferred, not traced).
- Run 20261017T052813Z-F6b on the model of that day found 45345778 distinct states.

A first pair of runs without `-deadlock` stopped at a deadlock of the bounds (depth 15 and 14), and a second pair stopped when the disk was full. Neither is a result.
