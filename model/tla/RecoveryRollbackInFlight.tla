---- MODULE RecoveryRollbackInFlight ----
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
    [] k = 3 -> RecoveryAnswer(R, B)
    [] k = 4 -> RecoveryAnswer(R, Sn)
    [] k = 5 -> RecoveryAnswer(R, Y)
    [] k = 6 -> Prepare(B, 1)
    [] k = 7 -> \E m \in Msg("prepare", 1) : HandlePrepare(Sn, m)
    [] k = 8 -> \E m \in Msg("prepare", 1) : HandlePrepare(Y, m)
    [] k = 9 -> \E m \in Prom(1, Sn) : HandlePromise(B, m)
    [] k = 10 -> \E m \in Prom(1, Y) : HandlePromise(B, m)
    [] k = 11 -> \E m \in Msg("prepare", 2) : HandlePrepare(Sn, m)
    [] k = 12 -> \E m \in Prom(2, R) : HandlePromise(L, m)
    [] k = 13 -> \E m \in Prom(2, Sn) : HandlePromise(L, m)
    [] k = 14 -> Propose(B, "v1")
    [] k = 15 -> RecoveryComplete(R)
    [] k = 16 -> \E m \in Acc(1, <<"v1">>) : HandleAccept(Y, m)
    [] k = 17 -> Propose(B, "v2")
    [] k = 18 -> \E m \in Acc(1, <<"v1", "v2">>) : HandleAccept(R, m)
    [] k = 19 -> Propose(L, "v2")
    [] k = 20 -> \E m \in Acc(2, <<"v2">>) : HandleAccept(Sn, m)
    [] k = 21 -> \E m \in Acc(2, <<"v2">>) : HandleAccept(R, m)
    [] OTHER -> FALSE
TInit == Init /\ k = 0
TNext == Step /\ k' = k + 1
TSpec == TInit /\ [][TNext]_<<vars, k>>
Reached == k < 22
====
