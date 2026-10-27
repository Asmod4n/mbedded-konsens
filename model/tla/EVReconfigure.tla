---- MODULE EVReconfigure ----
EXTENDS SequencePaxos
StopSignCoversRequests ==
  \A i \in Nodes :
    (i \in running /\ IsReconfigured(i)) =>
      \A k \in 1..Len(log[i]) : IsRequest(log[i][k]) => k <= V(DecidedPrefix(i)).p
CoverReconfigured == ~\E i \in Nodes : i \in running /\ IsReconfigured(i)
====
