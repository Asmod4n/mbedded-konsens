---- MODULE Compaction ----
EXTENDS Naturals, Sequences, FiniteSets, TLC

CONSTANTS Nodes, Keys, Vals, Absent, MaxIdx, WithMarks, CheckHorizon, MarkHorizon

Ops == [k : Keys, v : Vals \cup {Absent}]
Row == [v : Vals \cup {Absent}, i : 0..MaxIdx]
NoRow == [v |-> Absent, i |-> 0]
Majorities == {Q \in SUBSET Nodes : 2 * Cardinality(Q) > Cardinality(Nodes)}
Max(a, b) == IF a >= b THEN a ELSE b

VARIABLES dlog, st, applied, known, cut, hz
vars == <<dlog, st, applied, known, cut, hz>>

Live(s) == [k \in Keys |-> s[k].v]

Fold(l) ==
  LET F[j \in 0..Len(l)] ==
        IF j = 0 THEN [k \in Keys |-> Absent]
        ELSE [F[j-1] EXCEPT ![l[j].k] = l[j].v]
  IN F[Len(l)]

ApplyOp(s, op, j) ==
  IF op.v = Absent /\ ~WithMarks
  THEN [s EXCEPT ![op.k] = NoRow]
  ELSE [s EXCEPT ![op.k] = [v |-> op.v, i |-> j]]

Init ==
  /\ dlog = <<>>
  /\ st = [n \in Nodes |-> [k \in Keys |-> NoRow]]
  /\ applied = [n \in Nodes |-> 0]
  /\ known = [n \in Nodes |-> 0]
  /\ cut = [n \in Nodes |-> 0]
  /\ hz = [n \in Nodes |-> 0]

Decide(op, Q) ==
  /\ Len(dlog) < MaxIdx
  /\ \A q \in Q : known[q] = Len(dlog)
  /\ dlog' = Append(dlog, op)
  /\ known' = [n \in Nodes |-> IF n \in Q THEN Len(dlog) + 1 ELSE known[n]]
  /\ UNCHANGED <<st, applied, cut, hz>>

Fetch(m, n) ==
  /\ known[m] < known[n]
  /\ cut[n] <= known[m]
  /\ known' = [known EXCEPT ![m] = known[n]]
  /\ UNCHANGED <<dlog, st, applied, cut, hz>>

Apply(n) ==
  /\ applied[n] < known[n]
  /\ LET j == applied[n] + 1 IN
     /\ st' = [st EXCEPT ![n] = ApplyOp(st[n], dlog[j], j)]
     /\ applied' = [applied EXCEPT ![n] = j]
  /\ UNCHANGED <<dlog, known, cut, hz>>

Cut(n, j) ==
  /\ cut[n] < j /\ j <= applied[n]
  /\ cut' = [cut EXCEPT ![n] = j]
  /\ UNCHANGED <<dlog, st, applied, known, hz>>

Passed(h) ==
  IF MarkHorizon = "all" THEN \A n \in Nodes : applied[n] >= h
  ELSE \E Q \in Majorities : \A q \in Q : applied[q] >= h

DropMarks(n, h) ==
  /\ hz[n] < h /\ h <= applied[n]
  /\ Passed(h)
  /\ st' = [st EXCEPT ![n] = [k \in Keys |->
             IF st[n][k].v = Absent /\ st[n][k].i <= h THEN NoRow ELSE st[n][k]]]
  /\ hz' = [hz EXCEPT ![n] = h]
  /\ UNCHANGED <<dlog, applied, known, cut>>

DeltaSnapshot(m, n) ==
  /\ applied[m] < applied[n]
  /\ CheckHorizon => applied[m] >= hz[n]
  /\ st' = [st EXCEPT ![m] = [k \in Keys |->
             IF st[n][k].i > applied[m] THEN st[n][k] ELSE st[m][k]]]
  /\ applied' = [applied EXCEPT ![m] = applied[n]]
  /\ known' = [known EXCEPT ![m] = Max(known[m], applied[n])]
  /\ cut' = [cut EXCEPT ![m] = Max(cut[m], applied[n])]
  /\ hz' = [hz EXCEPT ![m] = Max(hz[m], hz[n])]
  /\ UNCHANGED dlog

FullSnapshot(m, n) ==
  /\ applied[m] < applied[n]
  /\ st' = [st EXCEPT ![m] = st[n]]
  /\ applied' = [applied EXCEPT ![m] = applied[n]]
  /\ known' = [known EXCEPT ![m] = Max(known[m], applied[n])]
  /\ cut' = [cut EXCEPT ![m] = applied[n]]
  /\ hz' = [hz EXCEPT ![m] = hz[n]]
  /\ UNCHANGED dlog

Next ==
  \/ \E op \in Ops, Q \in Majorities : Decide(op, Q)
  \/ \E m, n \in Nodes : Fetch(m, n) \/ DeltaSnapshot(m, n) \/ FullSnapshot(m, n)
  \/ \E n \in Nodes : Apply(n)
  \/ \E n \in Nodes, j \in 1..MaxIdx : Cut(n, j) \/ DropMarks(n, j)

Sym == Permutations(Nodes) \cup Permutations(Keys)

Spec == Init /\ [][Next]_vars

TypeOK ==
  /\ dlog \in Seq(Ops) /\ Len(dlog) <= MaxIdx
  /\ st \in [Nodes -> [Keys -> Row]]
  /\ \A n \in Nodes : cut[n] <= applied[n] /\ applied[n] <= known[n]
                      /\ known[n] <= Len(dlog) /\ hz[n] <= applied[n]

StateMatchesLog ==
  \A n \in Nodes : Live(st[n]) = Fold(SubSeq(dlog, 1, applied[n]))

DecidedAtMajority ==
  \A j \in 1..Len(dlog) : \E Q \in Majorities : \A q \in Q : known[q] >= j

CanCatchUp ==
  \A m \in Nodes : applied[m] < Len(dlog) =>
    \/ applied[m] < known[m]
    \/ \E n \in Nodes : applied[n] > applied[m]
    \/ \E n \in Nodes : cut[n] <= known[m] /\ known[m] < known[n]

AllCaughtUpCover == ~(Len(dlog) = MaxIdx /\ \A n \in Nodes : cut[n] = MaxIdx /\ hz[n] > 0)
====
