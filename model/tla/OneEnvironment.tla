---------------------------- MODULE OneEnvironment ----------------------------
(***************************************************************************)
(* The form of doc/plan-one-environment.md: log and state in one LMDB      *)
(* environment; the work runs before it is durable; one sync for each task *)
(* and node, at the done record; the client reply after a majority acked   *)
(* "synced".                                                               *)
(*                                                                         *)
(* A write and its sync are two actions. A commit (Commit) changes the     *)
(* current values of the node. A sync (SyncDone) makes the current values  *)
(* durable and releases the messages that wait on it (the groups "sync,    *)
(* then send" of plan 3.5). A crash puts the node back to its durable      *)
(* values: everything after the last sync is lost. Q1 of plan 3.5: the     *)
(* commit of a step is durable at the next sync; the free interleaving of  *)
(* Commit and SyncDone covers Q1 and Q2.                                   *)
(***************************************************************************)
EXTENDS Naturals, Sequences, FiniteSets, TLC

CONSTANTS Nodes, Requests, MaxBallot, MaxLog, MaxCrash,
          WorkRecords,        \* P0: the work is a record of the commit (a tombstone)
          Expiry,             \* a tombstone expires after this many ticks
          MaxTick,            \* bound of the tick count of a start
          NoNonce,            \* mutation: a tombstone carries no nonce of its start
          DoneBeforeOwnSync,  \* the leader sends "done" while its sync runs (question 4)
          ApplyAtDone,        \* mutation: the done write also applies the state
          ReplyAtLeaderSync,  \* mutation: the client reply after the sync of the leader only
          DoneWithoutSync,    \* mutation: a follower sends "synced" before its sync
          LeaderCountsUnsynced, \* mutation: the leader counts its own done before its sync
          NoRecovery,         \* a restarted node votes at once
          StoreRollback,      \* fault: a start finds the store at the sync before its last one
          RecoveryTakesLog    \* the recovery takes the log of the answer with the highest acc

\* cur[i]: the values of the last commit. dur[i]: the values at the last
\* sync. A record has the fields promise, acc, log, dec, state, applied,
\* wrec (the tombstones of P0: a set of [idx, req, start, tick]).
\* ticks[i]: the ticks of node i since its last start. starts[i]: the
\* nonce of its last start. prevDur[i]: the durable values before the last
\* sync (a damaged WAL frame drops the last synced commit, plan-sqlite-storage.md 7.3).
VARIABLES cur, dur, queue, work, status, lead, answers,
          msgs, chosen, replied, crashes, crashed, decAtCrash, ticks, starts,
          prevDur

vars == <<cur, dur, queue, work, status, lead, answers, msgs, chosen, replied,
          crashes, crashed, decAtCrash, ticks, starts, prevDur>>

N == Cardinality(Nodes)
IsMajority(Q) == 2 * Cardinality(Q) > N
F == (N - 1) \div 2
Ballots == 1..MaxBallot
NodeSeq == CHOOSE s \in [1..N -> Nodes] : \A a, b \in 1..N : a # b => s[a] # s[b]
Owner(b) == NodeSeq[((b - 1) % N) + 1]
Max(a, b) == IF a > b THEN a ELSE b
Min(a, b) == IF a < b THEN a ELSE b
SetMax(S) == CHOOSE x \in S : \A y \in S : x >= y

Entry == [bal : Ballots, req : Requests]

\* The fold of entries: each request takes effect once, at its first
\* entry; a later entry of the same request gives the stored reply.
RECURSIVE FoldSeq(_, _)
FoldSeq(s, acc0) ==
    IF s = <<>> THEN acc0
    ELSE FoldSeq(Tail(s), IF \E k \in 1..Len(acc0) : acc0[k] = Head(s).req
                          THEN acc0 ELSE Append(acc0, Head(s).req))
Fold(s) == FoldSeq(s, <<>>)

ChosenAt(k) == {e \in Entry : <<k, e>> \in chosen}
ChosenPrefixDefined(n) == \A k \in 1..n : ChosenAt(k) # {}
ChosenPrefix(n) == [k \in 1..n |-> CHOOSE e \in ChosenAt(k) : TRUE]

Send(m) == msgs' = msgs \cup {m}
SendAll(S) == msgs' = msgs \cup S
\* A message that waits on the next sync of node i.
Hold(i, m) == queue' = [queue EXCEPT ![i] = @ \cup {m}]

\* A commit with no sync of its own.
Commit(i, r) == cur' = [cur EXCEPT ![i] = r]

NoLead == [b |-> 0, phase |-> "none", promises |-> {}]
Empty == [promise |-> 0, acc |-> 0, log |-> <<>>, dec |-> 0, state |-> <<>>,
          applied |-> 0, wrec |-> {}]

\* A tombstone has expired when it comes from another start of its node,
\* or when it is Expiry ticks old (owner 2026-10-26 15:33).
Expired(i, w) ==
    \/ ~NoNonce /\ w.start # starts[i]
    \/ ticks[i] >= w.tick + Expiry
Live(i) == {w \in cur[i].wrec : ~Expired(i, w)}
Tombstone(i, idx, r) == [idx |-> idx, req |-> r, start |-> starts[i], tick |-> ticks[i]]
\* A done record and a tombstone have the same key, the log index: a write
\* of the log up to n replaces each tombstone at or below n.
Above(W, n) == {w \in W : w.idx > n}

Init ==
    /\ cur = [i \in Nodes |-> Empty]
    /\ dur = [i \in Nodes |-> Empty]
    /\ queue = [i \in Nodes |-> {}]
    /\ work = [i \in Nodes |-> <<>>]
    /\ status = [i \in Nodes |-> "running"]
    /\ lead = [i \in Nodes |-> NoLead]
    /\ answers = [i \in Nodes |-> {}]
    /\ msgs = {}
    /\ chosen = {}
    /\ replied = {}
    /\ crashes = 0
    /\ crashed = [i \in Nodes |-> FALSE]
    /\ decAtCrash = [i \in Nodes |-> 0]
    /\ ticks = [i \in Nodes |-> 0]
    /\ starts = [i \in Nodes |-> 0]
    /\ prevDur = [i \in Nodes |-> Empty]

Ghosts == <<crashes, crashed, decAtCrash, ticks, starts, prevDur>>

----------------------------------------------------------------------------
(* The sync. One fdatasync makes every commit before it durable and       *)
(* releases what waits on it. A sync with nothing new is not taken.       *)

SyncDone(i) ==
    /\ status[i] # "down"
    /\ dur[i] # cur[i] \/ queue[i] # {}
    /\ dur' = [dur EXCEPT ![i] = cur[i]]
    /\ prevDur' = [prevDur EXCEPT ![i] = IF dur[i] # cur[i] THEN dur[i] ELSE @]
    /\ SendAll(queue[i])
    /\ queue' = [queue EXCEPT ![i] = {}]
    /\ UNCHANGED <<cur, work, status, lead, answers, chosen, replied,
                   crashes, crashed, decAtCrash, ticks, starts>>

----------------------------------------------------------------------------
(* Phase 1.                                                               *)

\* The prepare leaves after the sync of the own promise.
Prepare(i, b) ==
    /\ status[i] = "running"
    /\ Owner(b) = i
    /\ b > cur[i].promise
    /\ Commit(i, [cur[i] EXCEPT !.promise = b])
    /\ lead' = [lead EXCEPT ![i] = [b |-> b, phase |-> "prepare",
                  promises |-> {[from |-> i, acc |-> cur[i].acc, log |-> cur[i].log, dec |-> cur[i].dec]}]]
    /\ work' = [work EXCEPT ![i] = <<>>]
    /\ Hold(i, [type |-> "prepare", b |-> b, from |-> i])
    /\ UNCHANGED <<dur, status, answers, msgs, chosen, replied, Ghosts>>

\* The promise leaves after its sync.
HandlePrepare(j, m) ==
    /\ m.type = "prepare"
    /\ status[j] = "running"
    /\ m.b > cur[j].promise
    /\ Commit(j, [cur[j] EXCEPT !.promise = m.b])
    /\ lead' = [lead EXCEPT ![j] = NoLead]
    /\ work' = [work EXCEPT ![j] = <<>>]
    /\ Hold(j, [type |-> "promise", b |-> m.b, from |-> j, to |-> m.from,
                acc |-> cur[j].acc, log |-> cur[j].log, dec |-> cur[j].dec])
    /\ UNCHANGED <<dur, status, answers, msgs, chosen, replied, Ghosts>>

Best(P) == CHOOSE p \in P : \A q \in P : \/ p.acc > q.acc
                                         \/ p.acc = q.acc /\ Len(p.log) >= Len(q.log)

\* The accept sync leaves at once: the leader counts itself only from its
\* durable values (LeaderDecide).
HandlePromise(i, m) ==
    /\ m.type = "promise"
    /\ m.to = i
    /\ status[i] = "running"
    /\ lead[i].b = m.b /\ cur[i].promise = m.b
    /\ LET p == [from |-> m.from, acc |-> m.acc, log |-> m.log, dec |-> m.dec]
       IN
       IF lead[i].phase = "prepare"
       THEN LET P == lead[i].promises \cup {p}
            IN IF IsMajority({q.from : q \in P})
               THEN LET L == Best(P).log
                        d == Min(Len(L), SetMax({q.dec : q \in P} \cup {cur[i].dec}))
                    IN /\ Commit(i, [cur[i] EXCEPT !.log = L, !.acc = m.b, !.dec = d, !.wrec = Above(@, Len(L))])
                       /\ lead' = [lead EXCEPT ![i] = [b |-> m.b, phase |-> "accept", promises |-> P]]
                       /\ SendAll({[type |-> "acceptsync", b |-> m.b, from |-> i, to |-> q.from,
                                    log |-> L, dec |-> d] : q \in {x \in P : x.from # i}})
               ELSE /\ lead' = [lead EXCEPT ![i].promises = P]
                    /\ UNCHANGED <<cur, msgs>>
       ELSE /\ lead[i].phase = "accept"
            /\ m.from \notin {q.from : q \in lead[i].promises}
            /\ lead' = [lead EXCEPT ![i].promises = @ \cup {p}]
            /\ Send([type |-> "acceptsync", b |-> m.b, from |-> i, to |-> m.from,
                     log |-> cur[i].log, dec |-> cur[i].dec])
            /\ UNCHANGED cur
    /\ UNCHANGED <<dur, queue, work, status, answers, chosen, replied, Ghosts>>

\* A follower takes one accept sync in a ballot. "synced" leaves after
\* its sync.
HandleAcceptSync(j, m) ==
    /\ m.type = "acceptsync"
    /\ m.to = j
    /\ status[j] = "running"
    /\ cur[j].promise = m.b
    /\ cur[j].acc # m.b
    /\ LET d == Min(Len(m.log), Max(cur[j].dec, m.dec))
       IN Commit(j, [cur[j] EXCEPT !.log = m.log, !.acc = m.b, !.dec = d, !.wrec = Above(@, Len(m.log))])
    /\ work' = [work EXCEPT ![j] = <<>>]
    /\ Hold(j, [type |-> "synced", b |-> m.b, from |-> j, idx |-> Len(m.log)])
    /\ UNCHANGED <<dur, status, lead, answers, msgs, chosen, replied, Ghosts>>

----------------------------------------------------------------------------
(* Phase 2.                                                               *)

\* The leader takes a request (or a retry) and runs it. Under P1 nothing
\* is written; under P0 the work is a record of a commit with no sync.
Propose(i, r) ==
    /\ status[i] = "running"
    /\ lead[i].phase = "accept"
    /\ Len(cur[i].log) + Len(work[i]) < MaxLog
    /\ work' = [work EXCEPT ![i] = Append(@, r)]
    /\ LET idx == Len(cur[i].log) + Len(work[i]) + 1
       IN /\ IF WorkRecords THEN Commit(i, [cur[i] EXCEPT !.wrec = @ \cup {Tombstone(i, idx, r)}]) ELSE UNCHANGED cur
          /\ Send([type |-> "work", b |-> lead[i].b, idx |-> idx, req |-> r])
    /\ UNCHANGED <<dur, queue, status, lead, answers, chosen, replied, Ghosts>>

\* A follower runs the work in log order and reports at once.
HandleWork(j, m) ==
    /\ m.type = "work"
    /\ status[j] = "running"
    /\ cur[j].promise = m.b /\ cur[j].acc = m.b
    /\ Owner(m.b) # j
    /\ m.idx = Len(cur[j].log) + Len(work[j]) + 1
    /\ work' = [work EXCEPT ![j] = Append(@, m.req)]
    /\ IF WorkRecords THEN Commit(j, [cur[j] EXCEPT !.wrec = @ \cup {Tombstone(j, m.idx, m.req)}]) ELSE UNCHANGED cur
    /\ Send([type |-> "ran", b |-> m.b, from |-> j, idx |-> m.idx, req |-> m.req])
    /\ UNCHANGED <<dur, queue, status, lead, answers, chosen, replied, Ghosts>>

\* A majority ran the work: the leader commits the done record. "done"
\* leaves after its sync, or at once with DoneBeforeOwnSync.
LeaderDone(i) ==
    /\ status[i] = "running"
    /\ lead[i].phase = "accept"
    /\ work[i] # <<>>
    /\ LET b == lead[i].b
           idx == Len(cur[i].log) + 1
           r == Head(work[i])
           e == [bal |-> b, req |-> r]
           Q == {i} \cup {j \in Nodes : [type |-> "ran", b |-> b, from |-> j, idx |-> idx, req |-> r] \in msgs}
           nl == Append(cur[i].log, e)
           dm == [type |-> "done", b |-> b, idx |-> idx, req |-> r]
       IN /\ IsMajority(Q)
          /\ work' = [work EXCEPT ![i] = Tail(@)]
          /\ Commit(i, [cur[i] EXCEPT !.log = nl,
                         !.wrec = Above(@, idx),
                         !.state = IF ApplyAtDone THEN FoldSeq(SubSeq(nl, cur[i].applied + 1, idx), @) ELSE @,
                         !.applied = IF ApplyAtDone THEN idx ELSE @])
          /\ IF DoneBeforeOwnSync
             THEN /\ Send(dm)
                  /\ UNCHANGED queue
             ELSE /\ Hold(i, dm)
                  /\ UNCHANGED msgs
          /\ IF ReplyAtLeaderSync
             THEN replied' = replied \cup {<<idx, e>>}
             ELSE UNCHANGED replied
    /\ UNCHANGED <<dur, status, lead, answers, chosen, Ghosts>>

\* A follower checks the done record, commits it, and sends "synced"
\* after its sync.
HandleDone(j, m) ==
    /\ m.type = "done"
    /\ status[j] = "running"
    /\ cur[j].promise = m.b /\ cur[j].acc = m.b
    /\ Owner(m.b) # j
    /\ m.idx = Len(cur[j].log) + 1
    /\ Commit(j, [cur[j] EXCEPT !.log = Append(@, [bal |-> m.b, req |-> m.req]),
                   !.wrec = Above(@, m.idx)])
    /\ work' = [work EXCEPT ![j] = IF @ # <<>> THEN Tail(@) ELSE @]
    /\ LET sm == [type |-> "synced", b |-> m.b, from |-> j, idx |-> m.idx]
       IN IF DoneWithoutSync
          THEN /\ Send(sm)
               /\ UNCHANGED queue
          ELSE /\ Hold(j, sm)
               /\ UNCHANGED msgs
    /\ UNCHANGED <<dur, status, lead, answers, chosen, replied, Ghosts>>

\* A majority holds the entry durable in this ballot: decided, and the
\* client gets its reply. The leader counts itself from its durable values.
LeaderDecide(i) ==
    /\ status[i] = "running"
    /\ lead[i].phase = "accept"
    /\ cur[i].dec < Len(cur[i].log)
    /\ LET b == lead[i].b
           idx == cur[i].dec + 1
           own == IF LeaderCountsUnsynced THEN cur[i] ELSE dur[i]
           self == IF own.acc = b /\ Len(own.log) >= idx THEN {i} ELSE {}
           Q == self \cup {j \in Nodes : \E m \in msgs : m.type = "synced" /\ m.b = b /\ m.from = j /\ m.idx >= idx}
       IN /\ IsMajority(Q)
          /\ Commit(i, [cur[i] EXCEPT !.dec = idx])
          /\ chosen' = chosen \cup {<<idx, cur[i].log[idx]>>}
          /\ replied' = replied \cup {<<idx, cur[i].log[idx]>>}
          /\ Send([type |-> "decide", b |-> b, idx |-> idx])
    /\ UNCHANGED <<dur, queue, work, status, lead, answers, Ghosts>>

\* The decided index rides in a commit with no sync of its own.
HandleDecide(j, m) ==
    /\ m.type = "decide"
    /\ status[j] = "running"
    /\ cur[j].promise = m.b /\ cur[j].acc = m.b
    /\ cur[j].dec < Min(m.idx, Len(cur[j].log))
    /\ Commit(j, [cur[j] EXCEPT !.dec = Min(m.idx, Len(cur[j].log))])
    /\ UNCHANGED <<dur, queue, work, status, lead, answers, msgs, chosen, replied, Ghosts>>

\* The state takes the decided entries, with the applied index, in one
\* commit of the one environment.
Apply(i) ==
    /\ status[i] = "running"
    /\ cur[i].applied < cur[i].dec
    /\ Commit(i, [cur[i] EXCEPT !.state = FoldSeq(SubSeq(cur[i].log, cur[i].applied + 1, cur[i].dec), @),
                                !.applied = cur[i].dec])
    /\ UNCHANGED <<dur, queue, work, status, lead, answers, msgs, chosen, replied, Ghosts>>

----------------------------------------------------------------------------
(* Faults.                                                                *)

\* A crash or a power loss: the node is back at its last sync. Messages
\* that waited on a sync are lost with it.
Crash(i) ==
    /\ status[i] # "down"
    /\ crashes < MaxCrash
    /\ crashes' = crashes + 1
    /\ crashed' = [crashed EXCEPT ![i] = TRUE]
    /\ decAtCrash' = [decAtCrash EXCEPT ![i] = cur[i].dec]
    /\ status' = [status EXCEPT ![i] = "down"]
    /\ cur' = [cur EXCEPT ![i] = dur[i]]
    /\ queue' = [queue EXCEPT ![i] = {}]
    /\ work' = [work EXCEPT ![i] = <<>>]
    /\ lead' = [lead EXCEPT ![i] = NoLead]
    /\ answers' = [answers EXCEPT ![i] = {}]
    /\ UNCHANGED <<dur, msgs, chosen, replied, ticks, starts, prevDur>>

\* A crash after which the store holds the values of the sync before its
\* last one: SQLite recovery stopped at a damaged frame of the last commit.
RollbackCrash(i) ==
    /\ StoreRollback
    /\ status[i] # "down"
    /\ crashes < MaxCrash
    /\ dur[i] # prevDur[i]
    /\ crashes' = crashes + 1
    /\ crashed' = [crashed EXCEPT ![i] = TRUE]
    /\ decAtCrash' = [decAtCrash EXCEPT ![i] = cur[i].dec]
    /\ status' = [status EXCEPT ![i] = "down"]
    /\ cur' = [cur EXCEPT ![i] = prevDur[i]]
    /\ dur' = [dur EXCEPT ![i] = prevDur[i]]
    /\ queue' = [queue EXCEPT ![i] = {}]
    /\ work' = [work EXCEPT ![i] = <<>>]
    /\ lead' = [lead EXCEPT ![i] = NoLead]
    /\ answers' = [answers EXCEPT ![i] = {}]
    /\ UNCHANGED <<msgs, chosen, replied, ticks, starts, prevDur>>

\* A start takes a new nonce and counts its ticks from zero. It deletes
\* nothing: the tombstones of the earlier start have expired.
Restart(i) ==
    /\ status[i] = "down"
    /\ status' = [status EXCEPT ![i] = IF NoRecovery THEN "running" ELSE "recovering"]
    /\ starts' = [starts EXCEPT ![i] = @ + 1]
    /\ ticks' = [ticks EXCEPT ![i] = 0]
    /\ UNCHANGED <<cur, dur, queue, work, lead, answers, msgs, chosen, replied, crashes, crashed, decAtCrash, prevDur>>

\* The library counts the calls of tick(); it has no clock.
Tick(i) ==
    /\ status[i] # "down"
    /\ ticks[i] < MaxTick
    /\ ticks' = [ticks EXCEPT ![i] = @ + 1]
    /\ UNCHANGED <<cur, dur, queue, work, status, lead, answers, msgs, chosen, replied,
                   crashes, crashed, decAtCrash, starts, prevDur>>

\* A step that sees an expired tombstone leaves it out of its commit.
DropExpired(i) ==
    /\ status[i] # "down"
    /\ Live(i) # cur[i].wrec
    /\ Commit(i, [cur[i] EXCEPT !.wrec = Live(i)])
    /\ UNCHANGED <<dur, queue, work, status, lead, answers, msgs, chosen, replied, Ghosts>>

\* After each start, a node votes only after f + 1 answers (VR Revisited).
RecoveryAnswer(i, j) ==
    /\ status[i] = "recovering"
    /\ status[j] = "running"
    /\ j # i
    /\ answers' = [answers EXCEPT ![i] = @ \cup {<<j, cur[j].promise, cur[j].acc, cur[j].log, cur[j].dec>>}]
    /\ UNCHANGED <<cur, dur, queue, work, status, lead, msgs, chosen, replied, Ghosts>>

RecoveryComplete(i) ==
    /\ status[i] = "recovering"
    /\ Cardinality({a[1] : a \in answers[i]}) >= F + 1
    /\ LET p    == SetMax({a[2] : a \in answers[i]} \cup {cur[i].promise})
           own  == <<i, cur[i].promise, cur[i].acc, cur[i].log, cur[i].dec>>
           best == CHOOSE a \in answers[i] \cup {own} :
                     \A o \in answers[i] \cup {own} :
                        \/ a[3] > o[3]
                        \/ a[3] = o[3] /\ Len(a[4]) >= Len(o[4])
       IN IF RecoveryTakesLog /\ best # own
          THEN Commit(i, [cur[i] EXCEPT !.promise = p, !.acc = best[3], !.log = best[4],
                                        !.dec = Max(@, best[5])])
          ELSE Commit(i, [cur[i] EXCEPT !.promise = p])
    /\ status' = [status EXCEPT ![i] = "running"]
    /\ answers' = [answers EXCEPT ![i] = {}]
    /\ UNCHANGED <<dur, queue, work, lead, msgs, chosen, replied, Ghosts>>

----------------------------------------------------------------------------

Next ==
    \/ \E i \in Nodes : Crash(i) \/ RollbackCrash(i)
    \/ \E i \in Nodes : Restart(i) \/ Tick(i)
    \/ \E i \in Nodes : SyncDone(i)
    \/ /\ UNCHANGED Ghosts
       /\ \/ \E i \in Nodes, b \in Ballots : Prepare(i, b)
          \/ \E i \in Nodes, r \in Requests : Propose(i, r)
          \/ \E i \in Nodes : LeaderDone(i) \/ LeaderDecide(i) \/ Apply(i)
          \/ \E i \in Nodes : RecoveryComplete(i) \/ DropExpired(i)
          \/ \E i, j \in Nodes : RecoveryAnswer(i, j)
          \/ \E m \in msgs :
               \/ \E j \in Nodes : HandlePrepare(j, m)
               \/ HandlePromise(m.to, m)
               \/ HandleAcceptSync(m.to, m)
               \/ \E j \in Nodes : HandleWork(j, m) \/ HandleDone(j, m) \/ HandleDecide(j, m)

Spec == Init /\ [][Next]_vars

----------------------------------------------------------------------------
(* Properties.                                                            *)

\* An index is chosen with one request, and each client reply names that
\* request (no confirmed entry is lost). The ballot of an entry is not part
\* of the value.
ChosenOnce ==
    \A x, y \in chosen \cup replied : x[1] = y[1] => x[2].req = y[2].req

DecidedAgree ==
    \A i, j \in Nodes : \A k \in 1..Min(cur[i].dec, cur[j].dec) : cur[i].log[k].req = cur[j].log[k].req

DecidedIsChosen ==
    \A i \in Nodes : \A k \in 1..cur[i].dec : \E e \in ChosenAt(k) : e.req = cur[i].log[k].req

\* The state of each node is the fold of the chosen log up to its applied
\* index (owner 2026-10-26 07:19). The fold is the same function as the
\* node's own check, so this checks which entries and in which order, not
\* the check of the session table itself (plan 13.4).
StateIsFold ==
    \A i \in Nodes : ChosenPrefixDefined(cur[i].applied) =>
        cur[i].state = Fold(ChosenPrefix(cur[i].applied))

\* Applied and decided index stand in one record: applied <= decided, and
\* every applied entry is chosen.
AppliedIsDecided ==
    \A i \in Nodes : /\ cur[i].applied <= cur[i].dec
                     /\ ChosenPrefixDefined(cur[i].applied)

\* A tombstone that has not expired comes from the last start of its node,
\* and it stands only above the log of its node: no live tombstone names
\* an index that holds a done record.
NoOldWorkRecords ==
    \A i \in Nodes : status[i] # "down" =>
        \A w \in Live(i) : w.start = starts[i] /\ w.idx > Len(cur[i].log)

\* Covers: each must be violated by a trace.
NoForgottenDone ==
    ~ \E i \in Nodes, k \in 1..MaxLog :
        /\ k <= Len(dur[i].log)
        /\ ChosenAt(k) # {}
        /\ \A e \in ChosenAt(k) : e.req # dur[i].log[k].req
NoForgottenDoneAfterCrash ==
    ~ \E i \in Nodes, k \in 1..MaxLog :
        /\ crashed[i]
        /\ k <= Len(dur[i].log)
        /\ ChosenAt(k) # {}
        /\ \A e \in ChosenAt(k) : e.req # dur[i].log[k].req
NoRetryChosen ==
    ~ \E x, y \in chosen : x[1] # y[1] /\ x[2].req = y[2].req
\* A crash lost a decided index that rode on no sync.
NoDecidedLost ==
    ~ \E i \in Nodes : status[i] = "recovering" /\ decAtCrash[i] > cur[i].dec
\* A work record survived a crash (P0) and the start deleted it.
NoWorkRecordSurvives ==
    ~ \E i \in Nodes : status[i] = "down" /\ cur[i].wrec # {}
\* A live tombstone stands while no node has its task in work: the cost of
\* the expiry, a retry gets "still in progress" until the tombstone expires.
InWork(j, idx) == idx > Len(cur[j].log) /\ idx <= Len(cur[j].log) + Len(work[j])
NoOrphanTombstone ==
    ~ \E i \in Nodes : status[i] = "running" /\ \E w \in Live(i) : \A j \in Nodes : ~InWork(j, w.idx)
\* A tombstone of the current start expired by its age.
NoTombstoneExpires ==
    ~ \E i \in Nodes : \E w \in cur[i].wrec : w.start = starts[i] /\ ticks[i] >= w.tick + Expiry

=============================================================================
