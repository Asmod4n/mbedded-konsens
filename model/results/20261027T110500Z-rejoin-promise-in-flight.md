# The rejoin loses a promise that was sent before the crash

What: model/tla/RecoveryRollback.tla, Recovery = "newleader" (the form
RecoveryWaitsForLeader of plan-sqlite-storage.md 7.3), with five nodes.
The module RecoveryRollbackInFlight.tla fixes one trace of 22 steps.
Config: MC_rr_inflight_n5.cfg (Nodes n1 to n5, MaxBallot 2, MaxLog 2,
MaxCrash 1).

Result: TLC (tools/tla2tools.jar of the session) reports "Invariant
ChosenOnce is violated" at state 23, 32 states.

The trace:
1. The owner of ballot 2 prepares. Node R promises ballot 2.
2. R loses that promise in a rollback of its store.
3. R gets its f + 1 = 3 answers from nodes that have promised nothing
   yet, so the highest promise of its answers is 0.
4. The owner of ballot 1 prepares and gets a majority without R.
5. R takes the first accept above 0, of ballot 1. v1 is chosen at index
   1 by the owner of ballot 1, R and one more node.
6. The owner of ballot 2 counts the old promise of R and one more
   promise, takes an empty log, and chooses v2 at index 1.

Why the earlier runs did not find it: they have three nodes. With three
nodes, f + 1 answers of the two peers always hold the node that got the
old promise.

The same fault in the literature: Michael, Ports, Sharma, Szekeres,
"Recovering Shared Objects Without Stable Storage", DISC 2017, technical
report, appendix B.2. It shows the fault for the rule of Paxos Made Live
5.1, which is the rule of this form.
