---------------------------- MODULE MultiPaxosVoting ----------------------------
EXTENDS Integers, Sequences, FiniteSets

CONSTANTS Value, Acceptor, Quorum

ASSUME QA == /\ \A Q \in Quorum : Q \subseteq Acceptor
             /\ \A Q1, Q2 \in Quorum : Q1 \cap Q2 # {}

Ballot == Nat
Log == Seq(Value)

IsPrefix(a, b) == Len(a) <= Len(b) /\ SubSeq(b, 1, Len(a)) = a
Compatible(a, b) == IsPrefix(a, b) \/ IsPrefix(b, a)

VARIABLES votes, maxBal
vars == <<votes, maxBal>>

VotedFor(a, b, l) == <<b, l>> \in votes[a]
DidNotVoteIn(a, b) == \A l \in Log : ~VotedFor(a, b, l)

SafeAt(b, l) ==
  LET SA[bb \in Ballot] ==
        {m \in Log :
           \/ bb = 0
           \/ \E Q \in Quorum :
                /\ \A a \in Q : maxBal[a] >= bb
                /\ \E c \in -1..(bb - 1) :
                     /\ (c # -1) => \E L \in SA[c] :
                                      /\ IsPrefix(L, m)
                                      /\ \A a \in Q : \A w \in Log :
                                           VotedFor(a, c, w) => IsPrefix(w, L)
                     /\ \A d \in (c + 1)..(bb - 1), a \in Q : DidNotVoteIn(a, d)}
  IN l \in SA[b]

Init == /\ votes = [a \in Acceptor |-> {}]
        /\ maxBal = [a \in Acceptor |-> -1]

IncreaseMaxBal(a, b) ==
  /\ b > maxBal[a]
  /\ maxBal' = [maxBal EXCEPT ![a] = b]
  /\ UNCHANGED votes

VoteFor(a, b, l) ==
  /\ maxBal[a] <= b
  /\ \A m \in Log : VotedFor(a, b, m) => IsPrefix(m, l)
  /\ \A p \in Acceptor \ {a} : \A m \in Log : VotedFor(p, b, m) => Compatible(m, l)
  /\ SafeAt(b, l)
  /\ votes' = [votes EXCEPT ![a] = votes[a] \cup {<<b, l>>}]
  /\ maxBal' = [maxBal EXCEPT ![a] = b]

Next == \E a \in Acceptor, b \in Ballot :
          \/ IncreaseMaxBal(a, b)
          \/ \E l \in Log : VoteFor(a, b, l)

Spec == Init /\ [][Next]_vars

TypeOK == /\ votes \in [Acceptor -> SUBSET (Ballot \X Log)]
          /\ maxBal \in [Acceptor -> Ballot \cup {-1}]

ChosenIn(b, m) == \E Q \in Quorum : \A a \in Q : \E w \in Log : VotedFor(a, b, w) /\ IsPrefix(m, w)
chosen == {m \in Log : \E b \in Ballot : ChosenIn(b, m)}

ChosenCompatible == \A m1, m2 \in chosen : Compatible(m1, m2)

VInv2 == \A a \in Acceptor, b \in Ballot, l \in Log : VotedFor(a, b, l) => SafeAt(b, l)
VInv3 == \A a1, a2 \in Acceptor, b \in Ballot, l1, l2 \in Log :
           VotedFor(a1, b, l1) /\ VotedFor(a2, b, l2) => Compatible(l1, l2)
VInv4 == \A a \in Acceptor, b \in Ballot : maxBal[a] < b => DidNotVoteIn(a, b)

VInv == TypeOK /\ VInv2 /\ VInv3 /\ VInv4
=================================================================================
