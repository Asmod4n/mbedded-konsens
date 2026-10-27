---- MODULE RecoveryRollbackInFlightNewId ----
EXTENDS RecoveryRollback
VARIABLE k
L == Owner(2)
B == Owner(1)
Rest == Nodes \ {L, B}
R == CHOOSE x \in Rest : TRUE
Sn == CHOOSE x \in Rest \ {R} : TRUE
Y == CHOOSE x \in Rest \ {R, Sn} : TRUE
Msg(t, b) == {m \in msgs : m.type = t /\ m.b = b}
Prom(b, f) == {m \in msgs : m.type = "promise" /\ m.b = b /\ m.from = f}
Acc(b, lg) == {m \in msgs : m.type = "accept" /\ m.b = b /\ m.log = lg}
Step ==
  CASE k = 0 -> Prepare(L, 2)
    [] k = 1 -> \E m \in Msg("prepare", 2) : HandlePrepare(R, m)
    [] k = 2 -> RollbackCrash(R)
    [] k = 3 -> Prepare(B, 1)
    [] k = 4 -> \E m \in Msg("prepare", 1) : HandlePrepare(Sn, m)
    [] k = 5 -> \E m \in Msg("prepare", 1) : HandlePrepare(Y, m)
    [] k = 6 -> \E m \in Prom(1, Sn) : HandlePromise(B, m)
    [] k = 7 -> \E m \in Prom(1, Y) : HandlePromise(B, m)
    [] k = 8 -> \E m \in Msg("prepare", 2) : HandlePrepare(Sn, m)
    [] k = 9 -> \E m \in Prom(2, R) : HandlePromise(L, m)
    [] k = 10 -> \E m \in Prom(2, Sn) : HandlePromise(L, m)
    [] k = 11 -> Propose(B, "v1")
    [] k = 12 -> \E m \in Acc(1, <<"v1">>) : HandleAccept(Y, m)
    [] OTHER -> FALSE
End == 13
TInit == Init /\ k = 0
TNext == IF k < End THEN Step /\ k' = k + 1 ELSE Next /\ k' = k
Reached == k < End
====
