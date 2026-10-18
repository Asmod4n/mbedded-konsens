# Plan: fixes after the review of runs c and d (2026-10-18)

Source: the review of the model `SequencePaxos.tla` (scratchpad copy,
commit 4882052) for runs c, d and the read path. Each item names the
finding number of the review.

## 1. Model

1. `deactivated` becomes a durable variable. Wipe and rollback keep it.
   (6)
2. EnterReadOnly requires `SingleNode` (the start flag). MC_d0 sets
   `SingleNode = TRUE`. (7)
3. LeaderCountsOwnPromise and LeaderCountsOwnAccepted require
   `ClusterNodes(i) = {i}`, `~deactivated[i]` and `~read_only[i]`. (8)
4. LeaveReadOnly is disabled at every node once `force_single.node # 0`.
   (9)
5. BuildForServer takes a source only from the running nodes. (10)
6. `stage1_ghost` becomes a function from node to record. (13)
7. ReadReply requires that node i kept its ballot and did not crash or
   roll back since the index step. (15)

## 2. Properties

8. New property NoForceLossReported: when loss occurs, the warning
   names it. It replaces the vacuous part of R1Force, S1Force and
   S2Force. (11)
9. EachReadSeesDecided, ClusterDecidesNothingAfterStage1, S7 and a
   property "a stale read is marked stale" enter the cfgs of d. (12)
10. DeactivatedNodeIsSilent covers LeaderCountsOwnAccepted. (14)

## 3. Configurations

11. Every cfg in the repository declares all constants of the model.
    A script lists the constants of the model and of each cfg and fails
    on a difference. (2)
12. The stage-2 cfgs and their mutations set MaxReadOnly >= 1. (1)
13. MC_d2: MaxLog 2, Cap 2, MaxCrash 1, MaxWipe 1. (3, 4)
14. MC_c: MaxCrash 1, Recovery TRUE, MaxLoss 1. (5)
15. MC_e: MaxCrash 1. (15)

## 4. Counter-checks

16. Each new or changed guard gets a mutation that removes it, with a
    cfg: DeactivatedStillVotes, ResumeWithoutRecovery, BuildFromRecovering,
    ForceWithoutStage1, ForceAtTwoNodes, ReadBeforeServing,
    StopSignLost. (16, 17)
17. Before a property counts, its mutation must break it. Each cfg
    with a property also proves by a reachability check that the
    action it guards fires at least once.

## 5. Order of the runs

- Regression first: MC_base_monotone, MC_f and MC_e at the old
  bounds, with the counts of states compared.
- Then the mutations, each with its expected violation.
- Then MC_c and MC_d2. A run that does not finish within 1 h moves to
  simulation, and its result says so.
- At most 3 TLC workers, one heavy job at a time.

## 6. What this plan does not do

- No witness lease. It gets its own plan.
- No change to the protocol beyond the guards above.
