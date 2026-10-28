---------------------------- MODULE MultiPaxosVoting ----------------------------
EXTENDS Integers, Sequences, FiniteSets

CONSTANTS Value, Acceptor, Quorum

ASSUME QA == /\ \A Q \in Quorum : Q \subseteq Acceptor
             /\ \A Q1, Q2 \in Quorum : Q1 \cap Q2 # {}

Ballot == Nat
Log == Seq(Value)

IsPrefix(a, b) == Len(a) <= Len(b) /\ \A k \in 1..Len(a) : a[k] = b[k]
Compatible(a, b) == IsPrefix(a, b) \/ IsPrefix(b, a)

VARIABLES votes, maxBal
vars == <<votes, maxBal>>

AllVotes == UNION {votes[a] : a \in Acceptor}
VotedFor(a, b, l) == <<b, l>> \in votes[a]
VotesAt(a, b) == {p[2] : p \in {q \in votes[a] : q[1] = b}}
DidNotVoteIn(a, b) == VotesAt(a, b) = {}

SafeAtWith(Qs, b, l, extends) ==
  LET SA[bb \in Ballot] == [m \in Log |->
        \/ bb = 0
        \/ \E Q \in Qs :
             /\ \A a \in Q : maxBal[a] >= bb
             /\ \E c \in -1..(bb - 1) :
                  /\ (c # -1) => \E a0 \in Q : \E L \in VotesAt(a0, c) :
                                   /\ SA[c][L]
                                   /\ extends => IsPrefix(L, m)
                                   /\ \A a \in Q : \A w \in VotesAt(a, c) : IsPrefix(w, L)
                  /\ \A d \in (c + 1)..(bb - 1), a \in Q : DidNotVoteIn(a, d)]
  IN SA[b][l]

SafeAt(b, l) == SafeAtWith(Quorum, b, l, TRUE)

Init == /\ votes = [a \in Acceptor |-> {}]
        /\ maxBal = [a \in Acceptor |-> -1]

IncreaseMaxBal(a, b) ==
  /\ b > maxBal[a]
  /\ maxBal' = [maxBal EXCEPT ![a] = b]
  /\ UNCHANGED votes

VoteFor(a, b, l) ==
  /\ maxBal[a] <= b
  /\ \A m \in VotesAt(a, b) : IsPrefix(m, l)
  /\ \A p \in Acceptor \ {a} : \A m \in VotesAt(p, b) : Compatible(m, l)
  /\ SafeAt(b, l)
  /\ votes' = [votes EXCEPT ![a] = votes[a] \cup {<<b, l>>}]
  /\ maxBal' = [maxBal EXCEPT ![a] = b]

Next == \E a \in Acceptor, b \in Ballot :
          \/ IncreaseMaxBal(a, b)
          \/ \E l \in Log : VoteFor(a, b, l)

Spec == Init /\ [][Next]_vars

TypeOK == /\ votes \in [Acceptor -> SUBSET (Ballot \X Log)]
          /\ maxBal \in [Acceptor -> Ballot \cup {-1}]

ChosenIn(b, m) == \E Q \in Quorum : \A a \in Q : \E w \in VotesAt(a, b) : IsPrefix(m, w)
Prefixes == UNION {{SubSeq(p[2], 1, k) : k \in 0..Len(p[2])} : p \in AllVotes}
chosen == {m \in Prefixes : \E p \in AllVotes : ChosenIn(p[1], m)}

ChosenCompatible == \A m1, m2 \in chosen : Compatible(m1, m2)

VInv2 == \A a \in Acceptor : \A p \in votes[a] : SafeAt(p[1], p[2])
VInv3 == \A p1, p2 \in AllVotes : p1[1] = p2[1] => Compatible(p1[2], p2[2])
VInv4 == \A a \in Acceptor : \A p \in votes[a] : p[1] <= maxBal[a]

VInv == TypeOK /\ VInv2 /\ VInv3 /\ VInv4
=================================================================================
