---- MODULE Reg ----
EXTENDS SequencePaxos, TLC
LevelBound == TLCGet("level") < 9
====
