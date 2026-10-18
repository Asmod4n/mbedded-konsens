Review: `model/review-plan-fixes-20261018.md` (approve with changes).
This version applies those changes.

# Plan: fixes after the review of runs c and d (2026-10-18)

Source: the review of the model `SequencePaxos.tla` (scratchpad copy,
commit 4882052) for runs c, d and the read path. Each item names the
finding number of the review.

## 1. Model

1. `deactivated` becomes a durable variable. Wipe and rollback keep it.
   (6)
2. EnterReadOnly requires `SingleNode` (the start flag). MC_d0 sets
   `SingleNode = TRUE`. Provisional: one global constant stands for the
   flag of each node, so stage 1 at only some nodes is not modelled. (7)
3. LeaderCountsOwnPromise and LeaderCountsOwnAccepted require
   `ClusterNodes(i) = {i}`, `~deactivated[i]` and `~read_only[i]`. (8)
4. LeaveReadOnly is disabled at every node once `force_single.node # 0`.
   (9)
5. BuildForServer takes a source only from a node that runs, is not
   recovering and is not deactivated. (10)
6. `stage1_ghost` becomes a function from node to record. (13)
7. The read record keeps the crash count, the rollback count and the
   ballot of node i at the index step. ReadReply requires that all three
   are unchanged. MC_e is measured before and after this change. (15)

## 2. Properties

8. No new warning property: ForceWarning already states the warning
   condition, including a longer log that diverges. In its place, the
   loss case gets its own properties: after stage 2, S1Running, S2 and
   R1 hold among the new configuration and for entries decided after
   stage 2. (11)
8a. The S1Running violation of run 20261018T051843Z-d2-sim is
   explained first. Its trace was cut by a TLC fault. The run is
   repeated with `-dumpTrace json` on the same seed. S1Force does not
   count until that trace shows that the violation is a loss that
   stage 2 accepts, and not a fault.
9. EachReadSeesDecided, ClusterDecidesNothingAfterStage1, S7 and a
   property "a stale read is marked stale" enter the cfgs of d. (12)
10. DeactivatedNodeIsSilent covers LeaderCountsOwnAccepted. (14)

## 3. Configurations

11. Every cfg in the repository declares all constants of the model.
    A script lists the constants of the model and of each cfg and fails
    on a difference. (2)
12. The stage-2 cfgs and their mutations set MaxReadOnly >= 1. (1)
13. MC_d2 and MC_c do not finish breadth-first at their present bounds.
   Each fault gets its own small cfg at MaxLog 1: one for crash, one
   for wipe, one for loss. The run at MaxLog 2 and Cap 2 is a
   simulation run, and its result says so. (3, 4, 5)
14. MC_c: Recovery TRUE in each of its cfgs. (5)
15. MC_e: MaxCrash 1. (15)

## 4. Counter-checks

16. Each new or changed guard gets a mutation that removes it, with a
    cfg: DeactivatedStillVotes, ResumeWithoutRecovery, BuildFromRecovering,
    ForceWithoutStage1, ForceAtTwoNodes, ReadBeforeServing,
    StopSignLost. (16, 17)
17. Before a property counts, its mutation must break it. Each cfg
    with a property is also run once with `-coverage 1`, and every
    action it guards must show a count above zero. Where a negated
    invariant is used, it reads a ghost counter, never ENABLED. No
    liveness formula is used for this.

## 5. Order of the runs

- Regression first: MC_base_monotone, MC_f and MC_e at the old
  bounds, with the counts of states compared. Items 1, 6 and 7 add
  state. With SingleNode FALSE and MaxRead 0, the counts must stay
  equal. Where they differ, the result names the variable that adds
  the states.
- Then the mutations, each with its expected violation.
- Then MC_c and MC_d2. A run that does not finish within 1 h moves to
  simulation, and its result says so.
- At most 3 TLC workers, one heavy job at a time.

## 6. What this plan does not do

- No witness lease. It gets its own plan.
- No change to the protocol beyond the guards above.
