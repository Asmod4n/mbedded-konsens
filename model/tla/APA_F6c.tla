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
OVERRIDE_PowerLoss == FALSE
OVERRIDE_Mutation == "none"
OVERRIDE_MaxStorageFault == 0
OVERRIDE_StartChecks == TRUE
OVERRIDE_RunningFaults == FALSE
OVERRIDE_StepChecks == TRUE
OVERRIDE_VerifyCrc == FALSE
OVERRIDE_ChainCheck == FALSE
OVERRIDE_MetaRecord == FALSE
OVERRIDE_PeerCheck == FALSE
OVERRIDE_HighWater == FALSE
OVERRIDE_VerifyPromise == FALSE
OVERRIDE_MismatchAction == "keep"
OVERRIDE_StateInLmdb == FALSE
OVERRIDE_MaxInFlight == 0
OVERRIDE_StateRepair == "copy"
OVERRIDE_StartDamageCounts == FALSE
OVERRIDE_RepairFromPeer == FALSE
OVERRIDE_StorageFaultKind == {}
OVERRIDE_ClientSessions == FALSE
OVERRIDE_MaxClientSend == 0
OVERRIDE_MaxRequest == 0
OVERRIDE_ReadIndex == FALSE
OVERRIDE_MaxRead == 0
OVERRIDE_NextConfigNodes == {}
OVERRIDE_MaxReconfigure == 0
OVERRIDE_SingleNode == FALSE
OVERRIDE_MaxForceSingle == 0
OVERRIDE_MaxReadOnly == 0
OVERRIDE_MaxHalt == 0
OVERRIDE_StaleReadNode == FALSE
OVERRIDE_Witness == FALSE
OVERRIDE_LeaseLength == 0
OVERRIDE_ExecuteVerify == FALSE
OVERRIDE_MaxWrong == 0
OVERRIDE_Spread == FALSE
OVERRIDE_Flip == FALSE
OVERRIDE_MaxFlip == 0
OVERRIDE_MaxResultThrow == 0
OVERRIDE_SpreadAction == "throw"

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
  /\ UNCHANGED <<statevars, evvars>>
  /\ Bounds'

(* S2Grows of MC_F6c.cfg as an action invariant. *)
S2GrowsAction == IsPrefix(decided_history, decided_history')
==============================================================================
