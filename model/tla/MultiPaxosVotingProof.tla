------------------------- MODULE MultiPaxosVotingProof -------------------------
EXTENDS MultiPaxosVoting, NaturalsInduction, SequenceTheorems, TLAPS

LEMMA QuorumNonEmpty == \A Q \in Quorum : Q # {}
BY QA

LEMMA LenNat == \A s \in Log : Len(s) \in Nat
BY LenProperties DEF Log

LEMMA PrefixRefl == \A s \in Log : IsPrefix(s, s)
BY LenNat DEF IsPrefix

LEMMA PrefixTrans ==
  ASSUME NEW a \in Log, NEW b \in Log, NEW c \in Log,
         IsPrefix(a, b), IsPrefix(b, c)
  PROVE  IsPrefix(a, c)
BY LenNat DEF IsPrefix

LEMMA PrefixesCompatible ==
  ASSUME NEW a \in Log, NEW b \in Log, NEW c \in Log,
         IsPrefix(a, c), IsPrefix(b, c)
  PROVE  Compatible(a, b)
<1>1. Len(a) \in Nat /\ Len(b) \in Nat /\ Len(c) \in Nat
  BY LenNat
<1>2. CASE Len(a) <= Len(b)
  <2>1. \A k \in 1..Len(a) : a[k] = b[k]
    BY <1>1, <1>2 DEF IsPrefix
  <2>2. QED
    BY <1>2, <2>1 DEF Compatible, IsPrefix
<1>3. CASE Len(b) <= Len(a)
  <2>1. \A k \in 1..Len(b) : b[k] = a[k]
    BY <1>1, <1>3 DEF IsPrefix
  <2>2. QED
    BY <1>3, <2>1 DEF Compatible, IsPrefix
<1>4. QED
  BY <1>1, <1>2, <1>3

LEMMA CompatibleBelow ==
  ASSUME NEW m1 \in Log, NEW m2 \in Log, NEW w1 \in Log, NEW w2 \in Log,
         Compatible(w1, w2), IsPrefix(m1, w1), IsPrefix(m2, w2)
  PROVE  Compatible(m1, m2)
<1>1. CASE IsPrefix(w1, w2)
  BY <1>1, PrefixTrans, PrefixesCompatible
<1>2. CASE IsPrefix(w2, w1)
  BY <1>2, PrefixTrans, PrefixesCompatible
<1>3. QED
  BY <1>1, <1>2 DEF Compatible

LEMMA VotesAtType ==
  TypeOK => \A a \in Acceptor, c \in Int : VotesAt(a, c) \subseteq Log
BY DEF TypeOK, VotesAt

LEMMA VotesAtPair ==
  TypeOK => \A a \in Acceptor : \A c, w : (w \in VotesAt(a, c)) <=> (<<c, w>> \in votes[a])
<1> SUFFICES ASSUME TypeOK, NEW a \in Acceptor, NEW c, NEW w
             PROVE  (w \in VotesAt(a, c)) <=> (<<c, w>> \in votes[a])
  OBVIOUS
<1>1. votes[a] \subseteq Ballot \X Log
  BY DEF TypeOK
<1>2. ASSUME w \in VotesAt(a, c) PROVE <<c, w>> \in votes[a]
  <2>1. PICK p \in votes[a] : p[1] = c /\ p[2] = w
    BY <1>2 DEF VotesAt
  <2>2. p = <<p[1], p[2]>>
    BY <1>1
  <2>3. QED
    BY <2>1, <2>2
<1>3. ASSUME <<c, w>> \in votes[a] PROVE w \in VotesAt(a, c)
  BY <1>3 DEF VotesAt
<1>4. QED
  BY <1>2, <1>3

-----------------------------------------------------------------------------

THEOREM SafeAtProp ==
  \A b \in Ballot, l \in Log :
    SafeAt(b, l) <=>
      \/ b = 0
      \/ \E Q \in Quorum :
           /\ \A a \in Q : maxBal[a] >= b
           /\ \E c \in -1..(b - 1) :
                /\ (c # -1) => \E a0 \in Q : \E L \in VotesAt(a0, c) :
                                 /\ SafeAt(c, L)
                                 /\ IsPrefix(L, l)
                                 /\ \A a \in Q : \A w \in VotesAt(a, c) : IsPrefix(w, L)
                /\ \A d \in (c + 1)..(b - 1), a \in Q : DidNotVoteIn(a, d)
<1> USE DEF Ballot
<1> DEFINE Def(SA, bb) ==
      {m \in Log :
        \/ bb = 0
        \/ \E Q \in Quorum :
             /\ \A a \in Q : maxBal[a] >= bb
             /\ \E c \in -1..(bb - 1) :
                  /\ (c # -1) => \E a0 \in Q : \E L \in VotesAt(a0, c) :
                                   /\ L \in SA[c]
                                   /\ TRUE => IsPrefix(L, m)
                                   /\ \A a \in Q : \A w \in VotesAt(a, c) : IsPrefix(w, L)
                  /\ \A d \in (c + 1)..(bb - 1), a \in Q : DidNotVoteIn(a, d)}
     SA[bb \in Ballot] == Def(SA, bb)
<1>1. \A b, l : SafeAt(b, l) = (l \in SA[b])
  BY Zenon DEF SafeAt, SafeAtWith
<1>2. ASSUME NEW n \in Nat, NEW g, NEW h,
             \A i \in 0..(n - 1) : g[i] = h[i]
      PROVE  Def(g, n) = Def(h, n)
  <2>1. \A c \in -1..(n - 1) : (c # -1) => g[c] = h[c]
    BY <1>2
  <2>2. ASSUME NEW m PROVE m \in Def(g, n) <=> m \in Def(h, n)
    BY <2>1
  <2>3. QED
    BY <2>2
<1>3. SA = [b \in Ballot |-> Def(SA, b)]
  <2> HIDE DEF Def
  <2> QED
    BY <1>2, RecursiveFcnOfNat, Isa
<1>4. \A b \in Ballot : SA[b] = Def(SA, b)
  <2> HIDE DEF Def
  <2> QED
    BY <1>3, Zenon
<1>5. \A c, L : (L \in SA[c]) <=> SafeAt(c, L)
  BY <1>1
<1>6. ASSUME NEW b \in Ballot, NEW l \in Log
      PROVE  SafeAt(b, l) <=> l \in Def(SA, b)
  BY <1>1, <1>4
<1>7. QED
  BY <1>5, <1>6, Zenon

THEOREM SafeAtPropPrime ==
  \A b \in Ballot, l \in Log :
    SafeAt(b, l)' <=>
      \/ b = 0
      \/ \E Q \in Quorum :
           /\ \A a \in Q : maxBal'[a] >= b
           /\ \E c \in -1..(b - 1) :
                /\ (c # -1) => \E a0 \in Q : \E L \in VotesAt(a0, c)' :
                                 /\ SafeAt(c, L)'
                                 /\ IsPrefix(L, l)
                                 /\ \A a \in Q : \A w \in VotesAt(a, c)' : IsPrefix(w, L)
                /\ \A d \in (c + 1)..(b - 1), a \in Q : DidNotVoteIn(a, d)'
<1>1. SafeAtProp'
  BY SafeAtProp, PTL
<1>2. QED
  BY <1>1

-----------------------------------------------------------------------------

LEMMA SafeLemma ==
  TypeOK =>
    \A b \in Ballot : \A l \in Log :
      SafeAt(b, l) =>
        \A c \in 0..(b - 1) :
          \E Q \in Quorum : \A a \in Q : \A w \in VotesAt(a, c) : IsPrefix(w, l)
<1> SUFFICES ASSUME TypeOK PROVE SafeLemma!2
  OBVIOUS
<1> USE DEF Ballot
<1> DEFINE R(bb) == \A l \in Log :
                      SafeAt(bb, l) =>
                        \A c \in 0..(bb - 1) :
                          \E Q \in Quorum : \A a \in Q : \A w \in VotesAt(a, c) : IsPrefix(w, l)
           P(b) == \A bb \in 0..b : R(bb)
<1>1. P(0)
  OBVIOUS
<1>2. ASSUME NEW b \in Nat, P(b) PROVE P(b + 1)
  <2>1. SUFFICES ASSUME NEW l \in Log, SafeAt(b + 1, l), NEW c \in 0..b
                 PROVE  \E Q \in Quorum : \A a \in Q : \A w \in VotesAt(a, c) : IsPrefix(w, l)
    BY <1>2
  <2>2. PICK Q \in Quorum :
          /\ \A a \in Q : maxBal[a] >= b + 1
          /\ \E cc \in -1..b :
               /\ (cc # -1) => \E a0 \in Q : \E L \in VotesAt(a0, cc) :
                                  /\ SafeAt(cc, L)
                                  /\ IsPrefix(L, l)
                                  /\ \A a \in Q : \A w \in VotesAt(a, cc) : IsPrefix(w, L)
               /\ \A d \in (cc + 1)..b, a \in Q : DidNotVoteIn(a, d)
    <3>1. b + 1 \in Ballot /\ b + 1 # 0 /\ (b + 1) - 1 = b
      OBVIOUS
    <3>2. QED
      BY <2>1, <3>1, SafeAtProp, Zenon
  <2>3. PICK cc \in -1..b :
          /\ (cc # -1) => \E a0 \in Q : \E L \in VotesAt(a0, cc) :
                             /\ SafeAt(cc, L)
                             /\ IsPrefix(L, l)
                             /\ \A a \in Q : \A w \in VotesAt(a, cc) : IsPrefix(w, L)
          /\ \A d \in (cc + 1)..b, a \in Q : DidNotVoteIn(a, d)
    BY <2>2
  <2>4. Q \subseteq Acceptor
    BY QA
  <2>5. CASE c > cc
    <3>1. \A a \in Q : VotesAt(a, c) = {}
      BY <2>3, <2>5 DEF DidNotVoteIn
    <3>2. QED
      BY <3>1
  <2>6. CASE c = cc
    <3>1. PICK a0 \in Q, L \in UNION {VotesAt(x, cc) : x \in Q} :
            /\ L \in VotesAt(a0, cc)
            /\ IsPrefix(L, l)
            /\ \A a \in Q : \A w \in VotesAt(a, cc) : IsPrefix(w, L)
      BY <2>3, <2>6
    <3>2. L \in Log
      BY <2>4, VotesAtType
    <3>3. \A a \in Q : \A w \in VotesAt(a, c) : IsPrefix(w, l)
      BY <3>1, <3>2, <2>4, <2>6, VotesAtType, PrefixTrans
    <3>4. QED
      BY <3>3
  <2>7. CASE c < cc
    <3>1. cc \in 0..b
      BY <2>7
    <3>2. PICK a0 \in Q, L \in UNION {VotesAt(x, cc) : x \in Q} :
            /\ L \in VotesAt(a0, cc)
            /\ SafeAt(cc, L)
            /\ IsPrefix(L, l)
      BY <2>3, <3>1
    <3>3. L \in Log
      BY <2>4, VotesAtType
    <3>4. c \in 0..(cc - 1)
      BY <2>7
    <3>5. PICK Q2 \in Quorum : \A a \in Q2 : \A w \in VotesAt(a, c) : IsPrefix(w, L)
      BY <1>2, <3>1, <3>2, <3>3, <3>4
    <3>6. Q2 \subseteq Acceptor
      BY QA
    <3>7. \A a \in Q2 : \A w \in VotesAt(a, c) : IsPrefix(w, l)
      BY <3>2, <3>3, <3>5, <3>6, VotesAtType, PrefixTrans
    <3>8. QED
      BY <3>7
  <2>8. QED
    BY <2>5, <2>6, <2>7
<1>3. \A b \in Nat : P(b)
  BY <1>1, <1>2, NatInduction, Isa
<1>4. QED
  BY <1>3

LEMMA VT0 ==
  TypeOK =>
    \A l, m \in Log, b, c \in Ballot :
      (b > c) /\ SafeAt(b, l) /\ ChosenIn(c, m) => IsPrefix(m, l)
<1> SUFFICES ASSUME TypeOK, NEW l \in Log, NEW m \in Log, NEW b \in Ballot, NEW c \in Ballot,
                    b > c, SafeAt(b, l), ChosenIn(c, m)
             PROVE  IsPrefix(m, l)
  OBVIOUS
<1>1. c \in 0..(b - 1)
  BY DEF Ballot
<1>2. PICK Q1 \in Quorum : \A a \in Q1 : \A w \in VotesAt(a, c) : IsPrefix(w, l)
  BY <1>1, SafeLemma
<1>3. PICK Q2 \in Quorum : \A a \in Q2 : \E w \in VotesAt(a, c) : IsPrefix(m, w)
  BY DEF ChosenIn
<1>4. PICK a \in Q1 \cap Q2 : TRUE
  BY QA
<1>5. a \in Acceptor
  BY QA
<1>6. PICK w \in VotesAt(a, c) : IsPrefix(m, w)
  BY <1>3, <1>4
<1>7. w \in Log
  BY <1>5, VotesAtType DEF Ballot
<1>8. QED
  BY <1>2, <1>4, <1>6, <1>7, PrefixTrans

LEMMA ChosenIsLog ==
  TypeOK => \A m \in chosen : m \in Log /\ \E b \in Ballot : ChosenIn(b, m)
<1> SUFFICES ASSUME TypeOK, NEW m \in chosen
             PROVE  m \in Log /\ \E b \in Ballot : ChosenIn(b, m)
  OBVIOUS
<1>1. PICK p \in AllVotes : ChosenIn(p[1], m)
  BY DEF chosen
<1>2. p[1] \in Ballot
  BY DEF AllVotes, TypeOK
<1>3. PICK q \in AllVotes, k \in Nat : k <= Len(q[2]) /\ m = SubSeq(q[2], 1, k)
  BY DEF chosen, Prefixes
<1>4. q[2] \in Log
  BY DEF AllVotes, TypeOK
<1>5. Len(q[2]) \in Nat /\ \A i \in 1..Len(q[2]) : q[2][i] \in Value
  BY <1>4, LenProperties DEF Log
<1>6. \A i \in 1..k : q[2][i] \in Value
  BY <1>3, <1>5
<1>7. SubSeq(q[2], 1, k) \in Seq(Value)
  BY <1>6, SubSeqProperties
<1>8. QED
  BY <1>1, <1>2, <1>3, <1>7 DEF Log

THEOREM VT1 ==
  TypeOK /\ VInv2 /\ VInv3 => ChosenCompatible
<1> SUFFICES ASSUME TypeOK, VInv2, VInv3, NEW m1 \in chosen, NEW m2 \in chosen
             PROVE  Compatible(m1, m2)
  BY DEF ChosenCompatible
<1>1. m1 \in Log /\ m2 \in Log
  BY ChosenIsLog
<1>2. PICK b1 \in Ballot : ChosenIn(b1, m1)
  BY ChosenIsLog
<1>3. PICK b2 \in Ballot : ChosenIn(b2, m2)
  BY ChosenIsLog
<1>4. ASSUME NEW c1 \in Ballot, NEW c2 \in Ballot, NEW n1 \in Log, NEW n2 \in Log,
             c1 < c2, ChosenIn(c1, n1), ChosenIn(c2, n2)
      PROVE  Compatible(n1, n2)
  <2>1. PICK Q \in Quorum : \A a \in Q : \E w \in VotesAt(a, c2) : IsPrefix(n2, w)
    BY <1>4 DEF ChosenIn
  <2>2. PICK a \in Q : TRUE
    BY QuorumNonEmpty
  <2>3. a \in Acceptor
    BY QA
  <2>4. PICK w \in VotesAt(a, c2) : IsPrefix(n2, w)
    BY <2>1, <2>2
  <2>5. w \in Log /\ <<c2, w>> \in votes[a]
    BY <2>3, VotesAtType, VotesAtPair DEF Ballot
  <2>6. SafeAt(c2, w)
    BY <2>3, <2>5 DEF VInv2
  <2>7. IsPrefix(n1, w)
    BY <1>4, <2>5, <2>6, VT0
  <2>8. QED
    BY <2>4, <2>5, <2>7, PrefixesCompatible
<1>5. CASE b1 = b2
  <2>1. PICK Q1 \in Quorum : \A a \in Q1 : \E w \in VotesAt(a, b1) : IsPrefix(m1, w)
    BY <1>2 DEF ChosenIn
  <2>2. PICK Q2 \in Quorum : \A a \in Q2 : \E w \in VotesAt(a, b1) : IsPrefix(m2, w)
    BY <1>3, <1>5 DEF ChosenIn
  <2>3. PICK a \in Q1 \cap Q2 : TRUE
    BY QA
  <2>4. a \in Acceptor
    BY QA
  <2>5. PICK w1 \in VotesAt(a, b1) : IsPrefix(m1, w1)
    BY <2>1, <2>3
  <2>6. PICK w2 \in VotesAt(a, b1) : IsPrefix(m2, w2)
    BY <2>2, <2>3
  <2>7. /\ w1 \in Log /\ <<b1, w1>> \in votes[a]
        /\ w2 \in Log /\ <<b1, w2>> \in votes[a]
    BY <2>4, VotesAtType, VotesAtPair DEF Ballot
  <2>8. Compatible(w1, w2)
    <3>1. <<b1, w1>> \in AllVotes /\ <<b1, w2>> \in AllVotes
      BY <2>4, <2>7 DEF AllVotes
    <3>2. QED
      BY <3>1 DEF VInv3
  <2>9. QED
    BY <1>1, <2>5, <2>6, <2>7, <2>8, CompatibleBelow
<1>6. CASE b1 < b2
  BY <1>1, <1>2, <1>3, <1>4, <1>6
<1>7. CASE b2 < b1
  <2>1. Compatible(m2, m1)
    BY <1>1, <1>2, <1>3, <1>4, <1>7
  <2>2. QED
    BY <2>1 DEF Compatible
<1>8. QED
  BY <1>5, <1>6, <1>7 DEF Ballot

-----------------------------------------------------------------------------

Grows ==
  /\ \A a \in Acceptor : votes[a] \subseteq votes'[a]
  /\ \A a \in Acceptor : maxBal'[a] >= maxBal[a]
  /\ \A a \in Acceptor : \A p \in votes'[a] \ votes[a] : p[1] >= maxBal[a]

LEMMA SafeAtStable ==
  ASSUME TypeOK, TypeOK', Grows
  PROVE  \A c \in Ballot, w \in Log : SafeAt(c, w) => SafeAt(c, w)'
<1> USE DEF Ballot
<1> DEFINE R(bb) == \A w \in Log : SafeAt(bb, w) => SafeAt(bb, w)'
           P(i) == \A j \in 0..i : R(j)
<1>00. VotesAtPair'
  BY VotesAtPair, PTL
<1>0. ASSUME NEW a \in Acceptor, NEW c \in Int, NEW x,
             x \in VotesAt(a, c)', x \notin VotesAt(a, c)
      PROVE  c >= maxBal[a]
  <2>1. <<c, x>> \in votes'[a] /\ <<c, x>> \notin votes[a]
    BY <1>0, <1>00, VotesAtPair
  <2>2. QED
    BY <2>1 DEF Grows
<1>1. P(0)
  <2>1. ASSUME NEW w \in Log PROVE SafeAt(0, w)'
    BY 0 \in Ballot, SafeAtPropPrime, Zenon
  <2>2. QED
    BY <2>1
<1>2. ASSUME NEW d \in Nat, P(d) PROVE P(d + 1)
  <2>1. SUFFICES ASSUME NEW e \in 0..(d + 1), NEW w \in Log, SafeAt(e, w)
                 PROVE  SafeAt(e, w)'
    OBVIOUS
  <2>2. CASE e \in 0..d
    BY <1>2, <2>1, <2>2
  <2>3. CASE e = d + 1
    <3>1. PICK Q \in Quorum :
            /\ \A a \in Q : maxBal[a] >= e
            /\ \E cc \in -1..(e - 1) :
                 /\ (cc # -1) => \E a0 \in Q : \E L \in VotesAt(a0, cc) :
                                    /\ SafeAt(cc, L)
                                    /\ IsPrefix(L, w)
                                    /\ \A a \in Q : \A x \in VotesAt(a, cc) : IsPrefix(x, L)
                 /\ \A dd \in (cc + 1)..(e - 1), a \in Q : DidNotVoteIn(a, dd)
      <4>1. e \in Ballot /\ e # 0
        BY <2>3
      <4>2. QED
        BY <2>1, <4>1, SafeAtProp, Zenon
    <3>2. PICK cc \in -1..(e - 1) :
            /\ (cc # -1) => \E a0 \in Q : \E L \in VotesAt(a0, cc) :
                               /\ SafeAt(cc, L)
                               /\ IsPrefix(L, w)
                               /\ \A a \in Q : \A x \in VotesAt(a, cc) : IsPrefix(x, L)
            /\ \A dd \in (cc + 1)..(e - 1), a \in Q : DidNotVoteIn(a, dd)
      BY <3>1
    <3>3. Q \subseteq Acceptor
      BY QA
    <3>4. \A a \in Q : maxBal'[a] >= e
      BY <3>1, <3>3 DEF Grows, TypeOK
    <3>5. ASSUME NEW a \in Q, NEW c \in -1..(e - 1), NEW x, x \in VotesAt(a, c)'
          PROVE  x \in VotesAt(a, c)
      <4>1. SUFFICES ASSUME x \notin VotesAt(a, c) PROVE FALSE
        OBVIOUS
      <4>2. c >= maxBal[a]
        BY <3>3, <4>1, <3>5, <1>0
      <4>3. QED
        BY <3>1, <4>2, <3>3 DEF TypeOK
    <3>6. ASSUME cc # -1
          PROVE  \E a0 \in Q : \E L \in VotesAt(a0, cc)' :
                   /\ SafeAt(cc, L)'
                   /\ IsPrefix(L, w)
                   /\ \A a \in Q : \A x \in VotesAt(a, cc)' : IsPrefix(x, L)
      <4>1. PICK a0 \in Q, L \in UNION {VotesAt(x, cc) : x \in Q} :
              /\ L \in VotesAt(a0, cc)
              /\ SafeAt(cc, L)
              /\ IsPrefix(L, w)
              /\ \A a \in Q : \A x \in VotesAt(a, cc) : IsPrefix(x, L)
        BY <3>2, <3>6
      <4>2. L \in Log
        BY <3>3, VotesAtType
      <4>3. L \in VotesAt(a0, cc)'
        <5>0. a0 \in Acceptor /\ L \in VotesAt(a0, cc)
          BY <4>1, <3>3
        <5>1. <<cc, L>> \in votes[a0]
          BY <5>0, VotesAtPair
        <5>2. <<cc, L>> \in votes'[a0]
          BY <5>1, <3>3 DEF Grows
        <5>3. QED
          BY <5>0, <5>2, <1>00
      <4>4. cc \in 0..d
        BY <3>6, <2>3
      <4>5. SafeAt(cc, L)'
        BY <4>1, <4>2, <4>4, <1>2
      <4>6. \A a \in Q : \A x \in VotesAt(a, cc)' : IsPrefix(x, L)
        BY <4>1, <3>5
      <4>7. QED
        BY <4>1, <4>3, <4>5, <4>6
    <3>7. \A dd \in (cc + 1)..(e - 1), a \in Q : DidNotVoteIn(a, dd)'
      BY <3>2, <3>5 DEF DidNotVoteIn
    <3>8. e \in Ballot /\ e # 0
      BY <2>3
    <3>9. QED
      BY <2>1, <3>8, <3>4, <3>6, <3>7, SafeAtPropPrime, Zenon
  <2>4. QED
    BY <2>2, <2>3
<1>3. \A d \in Nat : P(d)
  BY <1>1, <1>2, NatInduction, Isa
<1>4. QED
  BY <1>3

-----------------------------------------------------------------------------

THEOREM InitImpliesInv == Init => VInv
BY DEF Init, VInv, TypeOK, VInv2, VInv3, VInv4, AllVotes

THEOREM InductiveInvariance == VInv /\ [Next]_vars => VInv'
<1> SUFFICES ASSUME VInv, [Next]_vars PROVE VInv'
  OBVIOUS
<1> USE DEF Ballot
<1>1. TypeOK
  BY DEF VInv
<1>2. CASE vars' = vars
  <2>1. TypeOK' /\ Grows
    BY <1>1, <1>2 DEF vars, TypeOK, Grows
  <2>2. VInv2'
    <3>1. SUFFICES ASSUME NEW a \in Acceptor, NEW p \in votes'[a]
                   PROVE  SafeAt(p[1], p[2])'
      BY DEF VInv2
    <3>2. p \in votes[a] /\ p[1] \in Ballot /\ p[2] \in Log
      BY <1>1, <1>2 DEF vars, TypeOK
    <3>3. QED
      BY <1>1, <2>1, <3>2, SafeAtStable DEF VInv, VInv2
  <2>3. QED
    BY <1>2, <2>1, <2>2 DEF vars, VInv, VInv3, VInv4, AllVotes
<1>3. ASSUME NEW s \in Acceptor, NEW b \in Ballot, IncreaseMaxBal(s, b)
      PROVE  VInv'
  <2>1. TypeOK' /\ Grows
    BY <1>1, <1>3 DEF IncreaseMaxBal, TypeOK, Grows
  <2>2. VInv2'
    <3>1. SUFFICES ASSUME NEW a \in Acceptor, NEW p \in votes'[a]
                   PROVE  SafeAt(p[1], p[2])'
      BY DEF VInv2
    <3>2. p \in votes[a] /\ p[1] \in Ballot /\ p[2] \in Log
      BY <1>1, <1>3 DEF IncreaseMaxBal, TypeOK
    <3>3. QED
      BY <1>1, <2>1, <3>2, SafeAtStable DEF VInv, VInv2
  <2>3. VInv3'
    BY <1>3 DEF IncreaseMaxBal, VInv, VInv3, AllVotes
  <2>4. VInv4'
    BY <1>1, <1>3 DEF IncreaseMaxBal, VInv, VInv4, TypeOK
  <2>5. QED
    BY <2>1, <2>2, <2>3, <2>4 DEF VInv
<1>4. ASSUME NEW s \in Acceptor, NEW b \in Ballot, NEW l \in Log, VoteFor(s, b, l)
      PROVE  VInv'
  <2>0. /\ votes'[s] = votes[s] \cup {<<b, l>>}
        /\ \A a \in Acceptor \ {s} : votes'[a] = votes[a]
        /\ maxBal'[s] = b
        /\ \A a \in Acceptor \ {s} : maxBal'[a] = maxBal[a]
        /\ maxBal[s] <= b
    BY <1>1, <1>4 DEF VoteFor, TypeOK
  <2>1. TypeOK'
    BY <1>1, <2>0, <1>4 DEF VoteFor, TypeOK
  <2>2. Grows
    BY <1>1, <2>0 DEF Grows, TypeOK
  <2>3. VInv2'
    <3>1. SUFFICES ASSUME NEW a \in Acceptor, NEW p \in votes'[a]
                   PROVE  SafeAt(p[1], p[2])'
      BY DEF VInv2
    <3>2. CASE p \in votes[a]
      <4>1. p[1] \in Ballot /\ p[2] \in Log
        BY <1>1, <3>2 DEF TypeOK
      <4>2. QED
        BY <1>1, <2>1, <2>2, <3>2, <4>1, SafeAtStable DEF VInv, VInv2
    <3>3. CASE p \notin votes[a]
      <4>1. a = s /\ p = <<b, l>>
        BY <2>0, <3>3
      <4>2. SafeAt(b, l)
        BY <1>4 DEF VoteFor
      <4>3. QED
        BY <1>1, <2>1, <2>2, <4>1, <4>2, SafeAtStable
    <3>4. QED
      BY <3>2, <3>3
  <2>4. VInv3'
    <3>1. SUFFICES ASSUME NEW p1 \in AllVotes', NEW p2 \in AllVotes', p1[1] = p2[1]
                   PROVE  Compatible(p1[2], p2[2])
      BY DEF VInv3
    <3>2. ASSUME NEW q \in AllVotes', q \notin AllVotes PROVE q = <<b, l>>
      BY <2>0, <3>2 DEF AllVotes
    <3>3. ASSUME NEW q \in AllVotes, q[1] = b
          PROVE  Compatible(q[2], l) /\ Compatible(l, q[2])
      <4>1. PICK a \in Acceptor : q \in votes[a]
        BY DEF AllVotes
      <4>2. q = <<b, q[2]>> /\ q[2] \in Log
        BY <1>1, <3>3, <4>1 DEF TypeOK
      <4>3. q[2] \in VotesAt(a, b)
        BY <1>1, <4>1, <4>2, VotesAtPair
      <4>4. CASE a = s
        <5>1. IsPrefix(q[2], l)
          BY <1>4, <4>3, <4>4 DEF VoteFor
        <5>2. QED
          BY <5>1 DEF Compatible
      <4>5. CASE a # s
        <5>1. Compatible(q[2], l)
          BY <1>4, <4>3, <4>5 DEF VoteFor
        <5>2. QED
          BY <5>1 DEF Compatible
      <4>6. QED
        BY <4>4, <4>5
    <3>4. CASE p1 \in AllVotes /\ p2 \in AllVotes
      BY <3>1, <3>4 DEF VInv, VInv3
    <3>5. CASE p1 \notin AllVotes /\ p2 \in AllVotes
      BY <3>1, <3>2, <3>3, <3>5
    <3>6. CASE p1 \in AllVotes /\ p2 \notin AllVotes
      BY <3>1, <3>2, <3>3, <3>6
    <3>7. CASE p1 \notin AllVotes /\ p2 \notin AllVotes
      BY <3>2, <3>7, PrefixRefl DEF Compatible
    <3>8. QED
      BY <3>4, <3>5, <3>6, <3>7
  <2>5. VInv4'
    <3>1. SUFFICES ASSUME NEW a \in Acceptor, NEW p \in votes'[a]
                   PROVE  p[1] <= maxBal'[a]
      BY DEF VInv4
    <3>2. CASE p \in votes[a]
      <4>1. p[1] <= maxBal[a]
        BY <3>2 DEF VInv, VInv4
      <4>2. maxBal'[a] >= maxBal[a]
        BY <2>2 DEF Grows
      <4>3. p[1] \in Int /\ maxBal[a] \in Int /\ maxBal'[a] \in Int
        BY <1>1, <2>1, <3>2 DEF TypeOK
      <4>4. QED
        BY <4>1, <4>2, <4>3
    <3>3. CASE p \notin votes[a]
      BY <2>0, <3>3
    <3>4. QED
      BY <3>2, <3>3
  <2>6. QED
    BY <2>1, <2>3, <2>4, <2>5 DEF VInv
<1>5. QED
  BY <1>2, <1>3, <1>4 DEF Next

THEOREM Invariance == Spec => []VInv
BY InitImpliesInv, InductiveInvariance, PTL DEF Spec

THEOREM Safety == Spec => []ChosenCompatible
<1>1. VInv => ChosenCompatible
  BY VT1 DEF VInv
<1>2. QED
  BY <1>1, Invariance, PTL
=================================================================================
