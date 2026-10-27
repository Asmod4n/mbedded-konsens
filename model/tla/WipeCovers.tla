---- MODULE WipeCovers ----
EXTENDS SequencePaxos
CoverWipedPromiseCounted ==
  ~\E i, w \in Nodes : /\ wiped[w] /\ w # i
                       /\ role[i] = "Leader"
                       /\ leader_state[i].promises_meta[w].state = "Promised"
CoverNewConfigDecides ==
  ~(wipes > 0 /\ \E j \in NextConfigNodes : decided_idx[j] > 0)
====
