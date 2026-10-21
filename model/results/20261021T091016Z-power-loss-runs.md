# TLC runs of the power-loss mode, 20261021T091016Z

The runs of model/plan-power-loss.md section 4 on model 57044f2 (the fix of results/20261021T081623Z-power-loss-rollback-S2.md). Configs in `model/tla/power-loss/`. The `.sim.cfg` files are the cfgs without SYMMETRY and VIEW. TLC 2026.10.20.183001 (release v1.8.0 of tla2tools.jar), except run rb_old. Each run has 1 worker; four runs at a time on 4 cpus. A simulation is `-simulate -deadlock -depth 60 -seed 1`, limit 15 min. A simulation without a violation is not a complete run.

| Run | Config | Mode | Expected | Result | Numbers |
|---|---|---|---|---|---|
| p | MC_p.sim.cfg (MaxCrash 3) | simulation | no violation | no violation, stopped by the limit | Progress: 7325116 states checked, 134207 traces generated (trace length: mean=28, var(x)=39, sd=6) |
| p_rollback (rb_old) | MC_p_rollback.sim.cfg (MaxCrash 2, MaxRollback 1) | simulation, TLC 2.19, the seed of the run that found S2 | no violation | no violation, stopped by the limit | Progress: 22946365 states checked. |
| p_rollback from state 12 (ts) | wrapper TraceStart.tla, constants of MC_p_rollback.cfg | breadth-first | no violation | none up to depth 14, stopped by the limit; the old model failed at depth 14 | 3207279 distinct states at depth 14 |
| p_reach | MC_p_reach.cfg | breadth-first | NoPowerLossCompletion violated | violated at depth 8 | 993761 distinct states, 03 min 47 s |
| mut_PowerLossWithF | MC_mut_PowerLossWithF.sim.cfg | simulation | S1 or S2 violated | S2b violated after 29 s | trace of 37 states |
| p_rollback2 | MC_p_rollback2.sim.cfg (MaxRollback 2) | simulation | S2 violated (R1) | S2b violated after 29 s | trace of 37 states |

S2b is the form of S2 for a leader: a leader in Accept holds every decided entry.

## Traces of the violations

mut_PowerLossWithF: v1 is decided at ballot 11 by nodes 1 and 3. Node 1 rolls back and loses v1. Node 1 completes recovery with one answer (f), from node 2, which holds nothing. Node 2 becomes leader with node 1 and accepts v2 at index 1. So the completion with f answers loses a decided entry, and the completion with every peer is needed.

p_rollback2: v2 is decided at ballot 22 by nodes 1 and 2. Both roll back and lose v2. No node holds v2 any more, and node 3 accepts v1 at index 1 as leader of ballot 23. This is the limit of R1: more than f disks that lose writes at the same time.

## Not shown

How often the power-loss branch fired in the simulations is not counted; p_reach shows that it fires. Liveness is not checked.
