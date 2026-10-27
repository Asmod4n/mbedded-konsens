---- MODULE BuildCovers ----
EXTENDS SequencePaxos
CoverBuildFromRecovering ==
  ~\E s \in NextConfigNodes : build_source[s] # 0 /\ recovering[build_source[s]]
====
