# Plan: check form B of the rejoin, a new identity after a rollback

The owner chose form B on 2026-10-27. After a start whose store may
have lost its last sync, the node does not vote again under its old
identity. It deletes its store and joins as a new node through a
StopSign. Source of the form: Raft dissertation 3.8 and PAR 2.3.

## 1. What the plan shows

- The trace of results/20261027T110500Z-rejoin-promise-in-flight.md
  does not break ChosenOnce under form B.
- With five nodes, no other trace of the small model breaks ChosenOnce
  under form B.
- A model with the old rejoin and the same bounds finds the fault. So
  the bounds are large enough to see it.
- The protocol model with a rollback mapped onto a wipe keeps S1 to S6
  with five nodes.

## 2. Small model, RecoveryRollback.tla

1. A new value of Recovery: "newid". Under "newid", RollbackCrash sets
   the status of the node to "retired". A retired node takes no action
   and answers no recovery request. Its messages that are already sent
   stay in msgs.
2. Nodes is the old configuration, so a retired node still counts in N.
   The join of the new identity is not in the small model. Section 3
   checks it.
3. Configs, all with five nodes, MaxBallot 2 and SYMMETRY over Values
   (symmetry over Nodes is not sound, because Owner orders the nodes):
   - MC_rr_newleader_n5_l1.cfg: Recovery "newleader", MaxLog 1, MaxCrash
     1. TLC must report ChosenOnce violated. This shows that the bounds
     reach the fault.
   - MC_rr_newid_n5_l1.cfg, MC_rr_newid_n5.cfg (MaxLog 2) and
     MC_rr_newid_n5_c2.cfg (MaxLog 2, MaxCrash 2): Recovery "newid",
     BFS, invariant ChosenOnce. A run that does not end is not a check.
   - MC_rr_cover_retired_n5_l1.cfg: CoverRetired says that no leader in
     its accept phase counts a promise of a retired node. TLC must
     report it violated, so the scene of the fault is reachable.
4. RecoveryRollbackInFlightNewId.tla: the trace of the fault without
   the recovery steps. Under "newid" the leader of ballot 2 still counts
   the promise of R from before the crash. After the fixed steps, the
   module continues with Next.
   - MC_rr_inflight_n5_newid_Reached.cfg: TLC must report Reached
     violated, so the trace reaches its end.
   - MC_rr_inflight_n5_newid_ChosenOnce.cfg: no state after the scene
     breaks ChosenOnce. The deadlock check is off.

## 3. Protocol model, SequencePaxos.tla

1. Form B deletes the store of the old identity. So a rollback under B
   leaves the same state as Wipe(i): the old identity has an empty
   store and, with RuleR, sends no Promise and no Accepted. No new
   action is added. The review checks this mapping.
2. The new identity joins through the existing StopSign path:
   NextConfigNodes, MaxReconfigure, BuildForServer with Votes(src) /\
   ~recovering[src].
3. Configs:
   - MC_c_wipe_n5.sim.cfg: MC_c_wipe.cfg with Nodes {1..5},
     NextConfigNodes {6, 7, 8}, MaxWipe 2, MaxElection 3, simulation,
     15 min. MaxElection 3 leaves an election to the new configuration.
   - A cover in the same bounds: a promise of a wiped node is counted,
     then BuildForServer runs, then the new configuration decides an
     entry. TLC must report the cover violated.
   - MC_G_n5.sim.cfg: MC_G.cfg (RuleR FALSE, which gave S2 violated
     with three nodes in results/20261016T182454Z-G.md) with five
     nodes. Expected: a violation. If none is found, a fixed trace
     shows that the bounds reach the fault.
4. Known difference: in SequencePaxos a wiped node can still become
   leader and propose (HandleLeader and Propose have no Votes guard).
   Under B the old identity does nothing. The model allows more than B,
   which is acceptable for safety.

## 4. What the plan does not do

- No change to the C++ code. The build of the rejoin follows in plan
  14 of doc/plan-sqlite-storage.md after these runs.
- No liveness check. A restart of a majority at the same time needs
  the operator under form B.
- The model retires a node only after a rollback (st # prev). The
  C++ node cannot see that. Assumption: every start after which the
  last sync may be lost retires the identity. Plan 14 shows that the
  code holds this assumption.
- No crash vectors (form A) and no double promise (form C).

## 5. Runs

BFS runs where they end, simulation of 15 min otherwise. Each run
writes one result file in model/results/. TLC runs one at a time, with
the work directory on the compressing file system.
