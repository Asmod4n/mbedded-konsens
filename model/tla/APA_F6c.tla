------------------------------- MODULE APA_F6c -------------------------------
(* Run F6c for Apalache: the constants of MC_F6c.cfg. Apalache has no model   *)
(* value, so each value is an entry record with its own type.                *)
EXTENDS SequencePaxos

OVERRIDE_Nodes == {1, 2, 3}
OVERRIDE_Values == {[type |-> "v1", session |-> 0, request_number |-> 0],
                    [type |-> "v2", session |-> 0, request_number |-> 0]}
OVERRIDE_MaxBallot == 1
OVERRIDE_MaxLog == 1
OVERRIDE_MaxCounter == 2
OVERRIDE_MaxSession == 2
OVERRIDE_Cap == 2
OVERRIDE_MaxCrash == 0
OVERRIDE_MaxWipe == 0
OVERRIDE_MaxLoss == 0
OVERRIDE_MaxDuplicate == 0
OVERRIDE_MaxTimeout == 0
OVERRIDE_MaxElection == 2
OVERRIDE_RuleR == TRUE
OVERRIDE_DecidedIdxMonotone == TRUE
OVERRIDE_MaxRollback == 2
OVERRIDE_Recovery == TRUE
OVERRIDE_Mutation == "none"
OVERRIDE_ClientSessions == FALSE
OVERRIDE_MaxClientSend == 0
OVERRIDE_MaxRequest == 0
OVERRIDE_ReadIndex == FALSE
OVERRIDE_MaxRead == 0

(* The disjuncts of Next that the constants above enable. Each other        *)
(* disjunct of Next needs a crash, a wipe, a loss, a duplicate, a timeout, a *)
(* client session or a read, and the constants above allow none of them.    *)
(* Apalache removes an action with a guard that is FALSE, and Bounds' then  *)
(* reads variables that no action assigns.                                  *)
(* TLC keeps no state that violates the CONSTRAINT Bounds of MC_F6c.cfg and  *)
(* checks no invariant in such a state. ApaNext keeps the same states.       *)
ApaNext ==
  /\ \/ \E p \in Pairs : Receive(p[1], p[2])
     \/ \E i \in Nodes, k \in 1..MaxBallot : HandleLeader(i, k)
     \/ \E i \in Nodes, v \in Values : Propose(i, v)
     \/ \E R \in SUBSET Nodes : Rollback(R)
  /\ Bounds'

(* S2Grows of MC_F6c.cfg as an action invariant. *)
S2GrowsAction == IsPrefix(decided_history, decided_history')
==============================================================================
