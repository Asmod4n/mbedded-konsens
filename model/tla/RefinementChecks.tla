---- MODULE RefinementChecks ----
EXTENDS SequencePaxos

VARIABLES hist_votes, hist_maxbal

hist == <<hist_votes, hist_maxbal>>

A == INSTANCE MultiPaxosVoting
       WITH Value <- Values,
            Acceptor <- Nodes,
            Quorum <- {Q \in SUBSET Nodes : 2 * Cardinality(Q) > Cardinality(Nodes)},
            votes <- hist_votes,
            maxBal <- hist_maxbal

HInit ==
  /\ Init
  /\ hist_votes = [a \in Nodes |-> {}]
  /\ hist_maxbal = [a \in Nodes |-> promise[a]]

HNext ==
  /\ Next
  /\ hist_votes' =
       [a \in Nodes |->
          IF /\ crashes' = crashes /\ wipes' = wipes /\ rolled_back' = rolled_back
             /\ n_accepted'[a] # DefaultBallot
             /\ <<n_accepted'[a], log'[a]>> # <<n_accepted[a], log[a]>>
          THEN hist_votes[a] \cup {<<n_accepted'[a], log'[a]>>}
          ELSE hist_votes[a]]
  /\ hist_maxbal' = [a \in Nodes |-> Max(hist_maxbal[a], promise'[a])]

HSpec == HInit /\ [][HNext]_<<vars, hist>>

HView == <<View, hist>>

AbstractStep ==
  \E a \in Nodes :
    LET new == hist_votes'[a] \ hist_votes[a] IN
    \/ /\ new = {}
       /\ A!IncreaseMaxBal(a, hist_maxbal'[a])
    \/ \E p \in new : new = {p} /\ A!VoteFor(a, p[1], p[2])

Refines == [][AbstractStep]_hist

NoVoteBelowPromise ==
  [][\A a \in Nodes : \A p \in hist_votes'[a] \ hist_votes[a] : p[1] >= hist_maxbal[a]]_hist

AbstractInv == A!VInv
ChosenCompatible == A!ChosenCompatible
DecidedIsChosen ==
  \A i \in Nodes : decided_idx[i] > 0 => SubSeq(log[i], 1, decided_idx[i]) \in A!chosen
====
