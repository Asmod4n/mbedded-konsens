---------------------------- MODULE OneEnvironment ----------------------------
(***************************************************************************)
(* The form of doc/plan-one-environment.md: log and state in one LMDB      *)
(* environment; the work runs before it is durable; one sync per task and  *)
(* node, at the "done" record; the client reply after a majority synced    *)
(* "done". A crash drops everything after the last sync of the node.       *)
(*                                                                         *)
(* Durable (survive a crash up to the last sync): promise, acc, log,       *)
(* syn (the log length at the last sync), durdec, state, applied.          *)
(* Volatile: dec, work, status, lead, answers.                             *)
(* Ghost: chosen, replied, crashes.                                        *)
(***************************************************************************)
EXTENDS Naturals, Sequences, FiniteSets, TLC

CONSTANTS Nodes, Requests, MaxBallot, MaxLog, MaxCrash,
          ApplyAtDone,        \* mutation: the done write also applies the state
          ReplyAtLeaderSync,  \* mutation: the client reply after the sync of the leader only
          DoneWithoutSync,    \* mutation: a follower acks "synced" before its sync
          NoRecovery          \* mutation: a restarted node votes at once

VARIABLES promise, acc, log, syn, durdec, state, applied,
          dec, work, status, lead, answers,
          msgs, chosen, replied, crashes, crashed

vars == <<promise, acc, log, syn, durdec, state, applied,
          dec, work, status, lead, answers, msgs, chosen, replied, crashes, crashed>>

N == Cardinality(Nodes)
IsMajority(Q) == 2 * Cardinality(Q) > N
F == (N - 1) \div 2
Ballots == 1..MaxBallot
\* Each ballot has one owner, as Omni-Paxos ballots carry the pid.
NodeSeq == CHOOSE s \in [1..N -> Nodes] : \A a, b \in 1..N : a # b => s[a] # s[b]
Owner(b) == NodeSeq[((b - 1) % N) + 1]
Max(a, b) == IF a > b THEN a ELSE b
Min(a, b) == IF a < b THEN a ELSE b

\* An entry of the log is a done record: the ballot in which it was
\* written and the request. The result is a function of the entries
\* before it, so the model does not carry it.
Entry == [bal : Ballots, req : Requests]

\* The fold of a sequence of entries: each request takes effect once,
\* at its first entry; a later entry of the same request gives the
\* stored reply and changes nothing (session and request number).
RECURSIVE FoldSeq(_, _)
FoldSeq(s, acc0) ==
    IF s = <<>> THEN acc0
    ELSE FoldSeq(Tail(s), IF \E k \in 1..Len(acc0) : acc0[k] = Head(s).req
                          THEN acc0 ELSE Append(acc0, Head(s).req))
Fold(s) == FoldSeq(s, <<>>)

ApplySeq(st, s) == FoldSeq(s, st)

ChosenAt(k) == {e \in Entry : <<k, e>> \in chosen}
ChosenPrefixDefined(n) == \A k \in 1..n : ChosenAt(k) # {}
ChosenPrefix(n) == [k \in 1..n |-> CHOOSE e \in ChosenAt(k) : TRUE]

Send(m) == msgs' = msgs \cup {m}
SendAll(S) == msgs' = msgs \cup S

\* A synced commit makes every earlier commit of the environment durable.
Sync(i, newlog, newdec) ==
    /\ syn' = [syn EXCEPT ![i] = Len(newlog)]
    /\ durdec' = [durdec EXCEPT ![i] = newdec]

NoLead == [b |-> 0, phase |-> "none", promises |-> {}]

Init ==
    /\ promise = [i \in Nodes |-> 0]
    /\ acc = [i \in Nodes |-> 0]
    /\ log = [i \in Nodes |-> <<>>]
    /\ syn = [i \in Nodes |-> 0]
    /\ durdec = [i \in Nodes |-> 0]
    /\ state = [i \in Nodes |-> <<>>]
    /\ applied = [i \in Nodes |-> 0]
    /\ dec = [i \in Nodes |-> 0]
    /\ work = [i \in Nodes |-> <<>>]
    /\ status = [i \in Nodes |-> "running"]
    /\ lead = [i \in Nodes |-> NoLead]
    /\ answers = [i \in Nodes |-> {}]
    /\ msgs = {}
    /\ chosen = {}
    /\ replied = {}
    /\ crashes = 0
    /\ crashed = [i \in Nodes |-> FALSE]

----------------------------------------------------------------------------
(* Phase 1. A promise is written with a sync.                             *)

Prepare(i, b) ==
    /\ status[i] = "running"
    /\ Owner(b) = i
    /\ b > promise[i]
    /\ promise' = [promise EXCEPT ![i] = b]
    /\ Sync(i, log[i], dec[i])
    /\ lead' = [lead EXCEPT ![i] = [b |-> b, phase |-> "prepare",
                  promises |-> {[from |-> i, acc |-> acc[i], log |-> log[i], dec |-> dec[i]]}]]
    /\ work' = [work EXCEPT ![i] = <<>>]
    /\ Send([type |-> "prepare", b |-> b, from |-> i])
    /\ UNCHANGED <<acc, log, state, applied, dec, status, answers, chosen, replied, crashes>>

HandlePrepare(j, m) ==
    /\ m.type = "prepare"
    /\ status[j] = "running"
    /\ m.b > promise[j]
    /\ promise' = [promise EXCEPT ![j] = m.b]
    /\ Sync(j, log[j], dec[j])
    /\ lead' = [lead EXCEPT ![j] = NoLead]
    /\ work' = [work EXCEPT ![j] = <<>>]
    /\ Send([type |-> "promise", b |-> m.b, from |-> j, to |-> m.from,
             acc |-> acc[j], log |-> log[j], dec |-> dec[j]])
    /\ UNCHANGED <<acc, log, state, applied, dec, status, answers, chosen, replied, crashes>>

\* The log with the highest pair (accepted ballot, length), as Omni-Paxos.
Best(P) == CHOOSE p \in P : \A q \in P : \/ p.acc > q.acc
                                         \/ p.acc = q.acc /\ Len(p.log) >= Len(q.log)

HandlePromise(i, m) ==
    /\ m.type = "promise"
    /\ m.to = i
    /\ status[i] = "running"
    /\ lead[i].b = m.b /\ promise[i] = m.b
    /\ LET p == [from |-> m.from, acc |-> m.acc, log |-> m.log, dec |-> m.dec]
       IN
       IF lead[i].phase = "prepare"
       THEN LET P == lead[i].promises \cup {p}
            IN IF IsMajority({q.from : q \in P})
               THEN LET L == Best(P).log
                        d == Min(Len(L), Max(dec[i], CHOOSE x \in {q.dec : q \in P} :
                                                     \A y \in {q.dec : q \in P} : x >= y))
                    IN /\ log' = [log EXCEPT ![i] = L]
                       /\ acc' = [acc EXCEPT ![i] = m.b]
                       /\ dec' = [dec EXCEPT ![i] = d]
                       /\ Sync(i, L, d)
                       /\ lead' = [lead EXCEPT ![i] = [b |-> m.b, phase |-> "accept", promises |-> P]]
                       /\ SendAll({[type |-> "acceptsync", b |-> m.b, from |-> i, to |-> q.from,
                                    log |-> L, dec |-> d] : q \in {x \in P : x.from # i}})
               ELSE /\ lead' = [lead EXCEPT ![i].promises = P]
                    /\ UNCHANGED <<log, acc, dec, syn, durdec, msgs>>
       ELSE \* a late promise in the accept phase
            /\ lead[i].phase = "accept"
            /\ m.from \notin {q.from : q \in lead[i].promises}
            /\ lead' = [lead EXCEPT ![i].promises = @ \cup {p}]
            /\ Send([type |-> "acceptsync", b |-> m.b, from |-> i, to |-> m.from,
                     log |-> log[i], dec |-> dec[i]])
            /\ UNCHANGED <<log, acc, dec, syn, durdec>>
    /\ UNCHANGED <<promise, state, applied, work, status, answers, chosen, replied, crashes>>

\* A follower takes one AcceptSync in a ballot (the refusal of a stale
\* AcceptSync, owner 2026-10-19). It is written with a sync.
HandleAcceptSync(j, m) ==
    /\ m.type = "acceptsync"
    /\ m.to = j
    /\ status[j] = "running"
    /\ promise[j] = m.b
    /\ acc[j] # m.b
    /\ log' = [log EXCEPT ![j] = m.log]
    /\ acc' = [acc EXCEPT ![j] = m.b]
    /\ dec' = [dec EXCEPT ![j] = Min(Len(m.log), Max(dec[j], m.dec))]
    /\ Sync(j, m.log, Min(Len(m.log), Max(dec[j], m.dec)))
    /\ work' = [work EXCEPT ![j] = <<>>]
    /\ Send([type |-> "synced", b |-> m.b, from |-> j, idx |-> Len(m.log)])
    /\ UNCHANGED <<promise, state, applied, status, lead, answers, chosen, replied, crashes>>

----------------------------------------------------------------------------
(* Phase 2. The work runs before it is durable.                           *)

\* The leader takes a request from a client (or a retry of one) and runs
\* it. Nothing is written: the work stays in memory (plan 2.3, P1).
Propose(i, r) ==
    /\ status[i] = "running"
    /\ lead[i].phase = "accept"
    /\ Len(log[i]) + Len(work[i]) < MaxLog
    /\ LET idx == Len(log[i]) + Len(work[i]) + 1
       IN /\ work' = [work EXCEPT ![i] = Append(@, r)]
          /\ Send([type |-> "work", b |-> lead[i].b, idx |-> idx, req |-> r])
    /\ UNCHANGED <<promise, acc, log, syn, durdec, state, applied, dec, status, lead, answers,
                   chosen, replied, crashes>>

\* A follower runs the work in log order on its own base and reports.
HandleWork(j, m) ==
    /\ m.type = "work"
    /\ status[j] = "running"
    /\ promise[j] = m.b /\ acc[j] = m.b
    /\ Owner(m.b) # j
    /\ m.idx = Len(log[j]) + Len(work[j]) + 1
    /\ work' = [work EXCEPT ![j] = Append(@, m.req)]
    /\ Send([type |-> "ran", b |-> m.b, from |-> j, idx |-> m.idx, req |-> m.req])
    /\ UNCHANGED <<promise, acc, log, syn, durdec, state, applied, dec, status, lead, answers,
                   chosen, replied, crashes>>

\* The leader has a majority of runs with its own result: it writes the
\* done record and syncs.
LeaderDone(i) ==
    /\ status[i] = "running"
    /\ lead[i].phase = "accept"
    /\ work[i] # <<>>
    /\ LET b == lead[i].b
           idx == Len(log[i]) + 1
           r == Head(work[i])
           e == [bal |-> b, req |-> r]
           Q == {i} \cup {j \in Nodes : [type |-> "ran", b |-> b, from |-> j, idx |-> idx, req |-> r] \in msgs}
           nl == Append(log[i], e)
       IN /\ IsMajority(Q)
          /\ log' = [log EXCEPT ![i] = nl]
          /\ work' = [work EXCEPT ![i] = Tail(@)]
          /\ Sync(i, nl, dec[i])
          /\ Send([type |-> "done", b |-> b, idx |-> idx, req |-> r])
          /\ IF ApplyAtDone
             THEN /\ state' = [state EXCEPT ![i] = ApplySeq(@, SubSeq(nl, applied[i] + 1, idx))]
                  /\ applied' = [applied EXCEPT ![i] = idx]
             ELSE UNCHANGED <<state, applied>>
          /\ IF ReplyAtLeaderSync
             THEN replied' = replied \cup {<<idx, e>>}
             ELSE UNCHANGED replied
    /\ UNCHANGED <<promise, acc, durdec, dec, status, lead, answers, chosen, crashes>>

\* A follower checks the done record (plan-execute-verify 2.7), writes it
\* and syncs, then acks "synced".
HandleDone(j, m) ==
    /\ m.type = "done"
    /\ status[j] = "running"
    /\ promise[j] = m.b /\ acc[j] = m.b
    /\ Owner(m.b) # j
    /\ m.idx = Len(log[j]) + 1
    /\ LET nl == Append(log[j], [bal |-> m.b, req |-> m.req])
       IN /\ log' = [log EXCEPT ![j] = nl]
          /\ IF DoneWithoutSync
             THEN UNCHANGED <<syn, durdec>>
             ELSE Sync(j, nl, dec[j])
    /\ work' = [work EXCEPT ![j] = IF @ # <<>> THEN Tail(@) ELSE @]
    /\ Send([type |-> "synced", b |-> m.b, from |-> j, idx |-> m.idx])
    /\ UNCHANGED <<promise, acc, state, applied, dec, status, lead, answers, chosen, replied, crashes>>

\* A majority synced the entry in this ballot: it is decided, and the
\* client gets its reply (owner 2026-10-26 07:18).
LeaderDecide(i) ==
    /\ status[i] = "running"
    /\ lead[i].phase = "accept"
    /\ dec[i] < Len(log[i])
    /\ LET b == lead[i].b
           idx == dec[i] + 1
           Q == {i} \cup {j \in Nodes : \E m \in msgs : m.type = "synced" /\ m.b = b /\ m.from = j /\ m.idx >= idx}
       IN /\ IsMajority(Q)
          /\ dec' = [dec EXCEPT ![i] = idx]
          /\ chosen' = chosen \cup {<<idx, log[i][idx]>>}
          /\ replied' = replied \cup {<<idx, log[i][idx]>>}
          /\ Send([type |-> "decide", b |-> b, idx |-> idx])
    /\ UNCHANGED <<promise, acc, log, syn, durdec, state, applied, work, status, lead, answers, crashes>>

\* The decided index needs no sync of its own; it is durable with the
\* next sync of the node.
HandleDecide(j, m) ==
    /\ m.type = "decide"
    /\ status[j] = "running"
    /\ promise[j] = m.b /\ acc[j] = m.b
    /\ dec[j] < Min(m.idx, Len(log[j]))
    /\ dec' = [dec EXCEPT ![j] = Min(m.idx, Len(log[j]))]
    /\ UNCHANGED <<promise, acc, log, syn, durdec, state, applied, work, status, lead, answers,
                   msgs, chosen, replied, crashes>>

\* The state takes the decided entries in one commit of the one
\* environment, with the decided index. This commit syncs; in the code it
\* rides in the next sync of the node (plan 2.5).
Apply(i) ==
    /\ status[i] = "running"
    /\ applied[i] < dec[i]
    /\ state' = [state EXCEPT ![i] = ApplySeq(@, SubSeq(log[i], applied[i] + 1, dec[i]))]
    /\ applied' = [applied EXCEPT ![i] = dec[i]]
    /\ Sync(i, log[i], dec[i])
    /\ UNCHANGED <<promise, acc, log, dec, work, status, lead, answers, msgs, chosen, replied, crashes>>

----------------------------------------------------------------------------
(* Faults.                                                                *)

\* A crash or a power loss drops everything after the last sync.
Crash(i) ==
    /\ status[i] # "down"
    /\ crashes < MaxCrash
    /\ crashes' = crashes + 1
    /\ crashed' = [crashed EXCEPT ![i] = TRUE]
    /\ status' = [status EXCEPT ![i] = "down"]
    /\ log' = [log EXCEPT ![i] = SubSeq(@, 1, syn[i])]
    /\ dec' = [dec EXCEPT ![i] = Min(durdec[i], syn[i])]
    /\ work' = [work EXCEPT ![i] = <<>>]
    /\ lead' = [lead EXCEPT ![i] = NoLead]
    /\ answers' = [answers EXCEPT ![i] = {}]
    /\ UNCHANGED <<promise, acc, syn, durdec, state, applied, msgs, chosen, replied>>

\* After each start, a node votes only after f + 1 answers (VR Revisited).
Restart(i) ==
    /\ status[i] = "down"
    /\ status' = [status EXCEPT ![i] = IF NoRecovery THEN "running" ELSE "recovering"]
    /\ UNCHANGED <<promise, acc, log, syn, durdec, state, applied, dec, work, lead, answers,
                   msgs, chosen, replied, crashes>>

RecoveryAnswer(i, j) ==
    /\ status[i] = "recovering"
    /\ status[j] = "running"
    /\ j # i
    /\ answers' = [answers EXCEPT ![i] = @ \cup {<<j, promise[j]>>}]
    /\ UNCHANGED <<promise, acc, log, syn, durdec, state, applied, dec, work, status, lead,
                   msgs, chosen, replied, crashes>>

RecoveryComplete(i) ==
    /\ status[i] = "recovering"
    /\ Cardinality({a[1] : a \in answers[i]}) >= F + 1
    /\ LET p == CHOOSE x \in {a[2] : a \in answers[i]} \cup {promise[i]} :
                  \A y \in {a[2] : a \in answers[i]} \cup {promise[i]} : x >= y
       IN promise' = [promise EXCEPT ![i] = p]
    /\ Sync(i, log[i], dec[i])
    /\ status' = [status EXCEPT ![i] = "running"]
    /\ answers' = [answers EXCEPT ![i] = {}]
    /\ UNCHANGED <<acc, log, state, applied, dec, work, lead, msgs, chosen, replied, crashes>>

----------------------------------------------------------------------------

Next ==
    \/ \E i \in Nodes : Crash(i)
    \/ /\ UNCHANGED crashed
       /\ \/ \E i \in Nodes, b \in Ballots : Prepare(i, b)
          \/ \E i \in Nodes, r \in Requests : Propose(i, r)
          \/ \E i \in Nodes : LeaderDone(i) \/ LeaderDecide(i) \/ Apply(i)
          \/ \E i \in Nodes : Restart(i) \/ RecoveryComplete(i)
          \/ \E i, j \in Nodes : RecoveryAnswer(i, j)
          \/ \E m \in msgs :
               \/ \E j \in Nodes : HandlePrepare(j, m)
               \/ HandlePromise(m.to, m)
               \/ HandleAcceptSync(m.to, m)
               \/ \E j \in Nodes : HandleWork(j, m) \/ HandleDone(j, m) \/ HandleDecide(j, m)

Spec == Init /\ [][Next]_vars

----------------------------------------------------------------------------
(* Properties.                                                            *)

TypeOK ==
    /\ \A i \in Nodes : status[i] = "running" => applied[i] <= dec[i]
    /\ \A i \in Nodes : applied[i] <= durdec[i]
    /\ \A i \in Nodes : syn[i] <= Len(log[i])

\* An index is chosen with one request only, and a client reply names
\* that request (no confirmed entry is lost). The ballot of an entry is
\* not part of the value: a later ballot takes the value over.
ChosenOnce ==
    \A x, y \in chosen \cup replied : x[1] = y[1] => x[2].req = y[2].req

\* The decided prefixes of two nodes agree (S2).
DecidedAgree ==
    \A i, j \in Nodes : \A k \in 1..Min(dec[i], dec[j]) : log[i][k] = log[j][k]

\* The decided prefix of each node is the chosen log.
DecidedIsChosen ==
    \A i \in Nodes : \A k \in 1..dec[i] : <<k, log[i][k]>> \in chosen

\* The state of each node is the fold of the chosen log up to its applied
\* index: each request once, in the order of the chosen log (owner
\* 2026-10-26 07:19).
StateIsFold ==
    \A i \in Nodes : ChosenPrefixDefined(applied[i]) => state[i] = Fold(ChosenPrefix(applied[i]))

\* Applied and decided index stand in one meta record: the applied index
\* is never above the durable decided index after a sync.
AppliedIsDecided ==
    \A i \in Nodes : ChosenPrefixDefined(applied[i])

\* Covers: each must be violated by a trace.
\* A node holds a synced done record that is not chosen at its index while
\* another entry is chosen there (the case of owner 2026-10-26 07:19).
NoForgottenDone ==
    ~ \E i \in Nodes, k \in 1..MaxLog :
        /\ k <= syn[i]
        /\ ChosenAt(k) # {}
        /\ \A e \in ChosenAt(k) : e.req # log[i][k].req
\* The same after a crash: the node that holds the forgotten done record
\* was down once.
NoForgottenDoneAfterCrash ==
    ~ \E i \in Nodes, k \in 1..MaxLog :
        /\ crashed[i]
        /\ k <= syn[i]
        /\ ChosenAt(k) # {}
        /\ \A e \in ChosenAt(k) : e.req # log[i][k].req
\* A retry of a request that the chosen log already holds is chosen.
NoRetryChosen ==
    ~ \E x, y \in chosen : x[1] # y[1] /\ x[2].req = y[2].req
\* After a restart, the decided index of a node is below the chosen log:
\* the decided index that rode on no sync is lost.
NoDecidedLost ==
    ~ \E i \in Nodes : status[i] = "recovering" /\ dec[i] < Cardinality(chosen)

=============================================================================
