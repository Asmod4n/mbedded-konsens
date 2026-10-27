---- MODULE ProofChecks ----
EXTENDS SequencePaxos

LogExtendsOwnVotes ==
  [][\A i \in Servers :
       /\ n_accepted'[i] = n_accepted[i]
       /\ ~recovering[i]
       /\ crashes' = crashes /\ wipes' = wipes /\ rolled_back' = rolled_back
       => IsPrefix(log[i], log'[i])]_vars

RolledBackStaysRecovering == \A k \in rolled_back : recovering[k]

RecoveryCompleteWithOne(rs, i) ==
  LET normal == {r \in rs : ~r.recovering}
  IN normal # {} /\ RecoveryPrimaries(normal) # {}
====
