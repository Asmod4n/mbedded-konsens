# Review of plan-fixes-20261018.md

Read: the plan, the model copy (lines 1496-1505, 1579-1601, 1800-1860), MC_d2.cfg, MC_c.cfg, the owner decisions, the results of runs c and d2. TLC was not run.

## Findings, most severe first

1. Items 13 and 14: the bounds are not feasible. MC_c at MaxLog 2 and Cap 2, with no crash and no loss, did not finish in 1 h (run 20261018T035354Z-c: 16.9 M distinct states, level 25, stopped). Item 14 adds crash, Recovery and loss. MC_d2 at MaxLog 1 and Cap 1 also did not finish breadth-first (run 20261018T022450Z-d2, cited in the d2-sim file). Item 13 doubles the log and the channel and adds crash and wipe. With 3 workers, neither run ends in 1 h. Change: say now that both runs are simulation runs, or split each fault into its own small cfg (one cfg for crash, one for wipe, one for loss) at MaxLog 1.

2. Missing: run 20261018T051843Z-d2-sim found a violation of S1Running. Its trace was cut by a TLC fault. The plan does not name this violation. The plan must say which item explains it, or add a step that gets the full trace first (for example `-dumpTrace json`, or a short breadth-first run on the same seed).

3. Item 8 is not well defined. The model has no warning variable. ForceWarning (line 1840) already states "if Len(history) <= Len(log), then no loss". This is the only warning condition in the model. Note that loss can also come from a log that is longer but diverges, and ForceWarning catches that case. NoForceLossReported must either add a ghost field `force_single.warned` and state `~NoForceLoss => warned`, or be dropped because it repeats ForceWarning. Also, the plan does not say what S1, S2 and R1 must still hold after a loss, for example S1Running among the new configuration only. Without that, the loss case stays unchecked.

4. Item 17: reachability is feasible, but not as written. TLC stops at the first violation, so one negated invariant needs one run for each action. A cheaper form is one run with `-coverage 1`: the coverage report shows every action that never fired. Where a negated invariant is used, it reads a ghost counter (for example `halts = 0`), not ENABLED. Do not use `[]<><<A>>_vars`, because liveness checking is too slow at these sizes.

5. Item 7 needs new state. ReadReply cannot see a crash or a rollback after the index step unless the read record keeps the crash count, the rollback count and the ballot at the index step. The plan must name these fields. They add states to MC_e, and item 15 adds MaxCrash 1 there too. Measure MC_e before the change.

6. Item 5 says "running nodes". The mutation BuildFromRecovering in item 16 implies that the guard also excludes nodes that are recovering. Write the full guard: running, not recovering, not deactivated.

7. Item 2 uses the global constant SingleNode. The owner decision says "a flag at the start of the process", which is set at each node. One global constant cannot model stage 1 at only some nodes. That is acceptable as a first form, but mark it as provisional.

8. Section 5 (regression "at the old bounds") is correct, but items 1, 6 and 7 change the state vector. The state counts then differ even when nothing is wrong. Say which difference is expected.

9. Items 1, 3, 4, 9, 10, 11, 12 and 16 fix their findings, and no item contradicts an owner decision. Item 4 matches "stage 2 has no way back". Item 3 matches "a leader counts its own promise and its own accepted" in single-node mode.

Verdict: approve with changes
