------------------------- MODULE MC_MultiPaxosVoting -------------------------
EXTENDS MultiPaxosVoting, TLC, Randomization

CONSTANTS MaxBallot, MaxLength, Samples

MCBallot == 0..MaxBallot
MCLog == UNION {[1..n -> Value] : n \in 0..MaxLength}
MCQuorum == {Q \in SUBSET Acceptor : 2 * Cardinality(Q) > Cardinality(Acceptor)}

MCPair == MCBallot \X MCLog
SmallVotes == {{}} \cup {{p} : p \in MCPair} \cup {{p, q} : p, q \in MCPair}

TypeOKRandom ==
  /\ votes \in RandomSubset(Samples, [Acceptor -> SmallVotes])
  /\ maxBal \in [Acceptor -> MCBallot \cup {-1}]

InductiveInit == TypeOKRandom /\ VInv
InductiveSpec == InductiveInit /\ [][Next]_vars
OneStep == TLCGet("level") < 2

VoteForBelowPromise(a, b, l) ==
  /\ \A m \in VotesAt(a, b) : IsPrefix(m, l)
  /\ \A p \in Acceptor \ {a} : \A m \in VotesAt(p, b) : Compatible(m, l)
  /\ SafeAt(b, l)
  /\ votes' = [votes EXCEPT ![a] = votes[a] \cup {<<b, l>>}]
  /\ maxBal' = [maxBal EXCEPT ![a] = IF b > @ THEN b ELSE @]

SafeAtWithoutExtension(b, l) == SafeAtWith(Quorum, b, l, FALSE)
SafeAtWithOneNode(b, l) == SafeAtWith({{a} : a \in Acceptor}, b, l, TRUE)
=============================================================================
