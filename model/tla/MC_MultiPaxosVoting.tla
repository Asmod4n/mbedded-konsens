------------------------- MODULE MC_MultiPaxosVoting -------------------------
EXTENDS MultiPaxosVoting, TLC, Randomization

CONSTANTS MaxBallot, MaxLength, Samples

MCBallot == 0..MaxBallot
MCLog == UNION {[1..n -> Value] : n \in 0..MaxLength}
MCQuorum == {Q \in SUBSET Acceptor : 2 * Cardinality(Q) > Cardinality(Acceptor)}

TypeOKRandom ==
  /\ votes \in RandomSubset(Samples, [Acceptor -> SUBSET (MCBallot \X MCLog)])
  /\ maxBal \in [Acceptor -> MCBallot \cup {-1}]

InductiveInit == TypeOKRandom /\ VInv
InductiveSpec == InductiveInit /\ [][Next]_vars
OneStep == TLCGet("level") < 2

VoteForBelowPromise(a, b, l) ==
  /\ \A m \in Log : VotedFor(a, b, m) => IsPrefix(m, l)
  /\ \A p \in Acceptor \ {a} : \A m \in Log : VotedFor(p, b, m) => Compatible(m, l)
  /\ SafeAt(b, l)
  /\ votes' = [votes EXCEPT ![a] = votes[a] \cup {<<b, l>>}]
  /\ maxBal' = [maxBal EXCEPT ![a] = IF b > @ THEN b ELSE @]

SafeAtWith(Qs, b, l, extends) ==
  LET SA[bb \in Ballot] ==
        {m \in Log :
           \/ bb = 0
           \/ \E Q \in Qs :
                /\ \A a \in Q : maxBal[a] >= bb
                /\ \E c \in -1..(bb - 1) :
                     /\ (c # -1) => \E L \in SA[c] :
                                      /\ extends => IsPrefix(L, m)
                                      /\ \A a \in Q : \A w \in Log :
                                           VotedFor(a, c, w) => IsPrefix(w, L)
                     /\ \A d \in (c + 1)..(bb - 1), a \in Q : DidNotVoteIn(a, d)}
  IN l \in SA[b]

SafeAtWithoutExtension(b, l) == SafeAtWith(Quorum, b, l, FALSE)
SafeAtWithOneNode(b, l) == SafeAtWith({{a} : a \in Acceptor}, b, l, TRUE)
=============================================================================
