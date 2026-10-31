--------------------------- MODULE AcceptWithResult ---------------------------
(***************************************************************************)
(* The form of doc/plan-accept-carries-the-result.md. The leader runs a    *)
(* request and proposes the entry with its own result. A follower runs the *)
(* entry on its own log and accepts it only when its result agrees. A new  *)
(* leader selects the result of a majority for each entry above the        *)
(* highest decided index of its promises before it sends the accept sync.  *)
(*                                                                         *)
(* Commit, sync, hold, crash, restart and recovery are the actions of      *)
(* OneEnvironment.tla. The tombstones, the ticks and the store rollback of *)
(* that module are left out.                                               *)
(***************************************************************************)
EXTENDS Naturals, Sequences, FiniteSets, TLC

CONSTANTS Nodes, Requests, MaxBallot, MaxLog, MaxCrash,
          MaxReset,             \* prepare requests that a leader takes and promises of the same ballot, in one run
          MaxFaulty,            \* pairs <<node, request>> whose run gives a wrong result
          AcceptBeforeOwnSync,  \* the accept leaves while the sync of the leader runs
          NoCheckAtAccept,      \* mutation: a follower accepts without its own run
          NoCheckAtSync,        \* mutation: a follower takes an accept sync without its own run
          NoSelectionAtSync,    \* mutation: a new leader syncs the log of its promises as it is
          NoRepairBound,        \* mutation: the kept prefix of a sync is not bounded by a repair
          LeaderCountsUnsynced, \* mutation: the leader counts its own entry before its sync
          NoLeaderCheck,        \* mutation: the leader counts a taken entry without its own run
          NoRecovery,           \* a restarted node votes at once
          RecoveryWaitsForLeader,
          WorkWithoutBase,      \* the work carries no entries: a node answers only with its own log up to d
          NoCheckAtDecide,      \* mutation: a node applies a decided entry without its own run
          NoPromiseAgain,       \* a node ignores a prepare of the ballot it promised (the old form)
          NoKeepOnSameBallot    \* an accept sync of the accepted ballot can shorten the log (the old form)

\* cur[i]: the values of the last commit. dur[i]: the values at the last
\* sync. A record has the fields promise, acc, log, dec. An entry is
\* [req, res]. faulty: the pairs <<i, r>> for which the run of node i
\* gives a wrong result. badAt: the pairs <<idx, r>> at which a leader
\* proposed r with a wrong result of its own. repaired: the pairs <<idx, r>>
\* at which a selection took a result other than the result of the taken entry.
\* voted: the durable votes <<b, idx, entry, node>>; a sync makes the log of a
\* node durable with its accepted ballot.
VARIABLES cur, dur, queue, status, lead, answers, msgs, chosen, crashes, faulty, badAt, repaired, voted, resets

vars == <<cur, dur, queue, status, lead, answers, msgs, chosen, crashes, faulty, badAt, repaired, voted, resets>>

N == Cardinality(Nodes)
IsMajority(Q) == 2 * Cardinality(Q) > N
F == (N - 1) \div 2
Ballots == 1..MaxBallot
NodeSeq == CHOOSE s \in [1..N -> Nodes] : \A a, b \in 1..N : a # b => s[a] # s[b]
Owner(b) == NodeSeq[((b - 1) % N) + 1]
Max(a, b) == IF a > b THEN a ELSE b
Min(a, b) == IF a < b THEN a ELSE b
SetMax(S) == CHOOSE x \in S : \A y \in S : x >= y

\* The run of request r on node i with base B. A node that works gives
\* a result that depends on the request and on the results of its base,
\* so two nodes that work give the same result on the same base.
Results(B) == [k \in 1..Len(B) |-> B[k].res]
Good(r, B) == [ok |-> TRUE, who |-> {}, req |-> r, base |-> Results(B)]
Run(i, r, B) == IF <<i, r>> \in faulty THEN [ok |-> FALSE, who |-> {i}, req |-> r, base |-> Results(B)]
                ELSE Good(r, B)
Agrees(i, B, e) == Run(i, e.req, B) = e.res

\* Fewer than a majority of the nodes give a wrong result for a request.
FaultSets == {S \in SUBSET (Nodes \X Requests) :
                /\ Cardinality(S) <= MaxFaulty
                /\ \A r \in Requests : ~IsMajority({i \in Nodes : <<i, r>> \in S})}

ChosenAt(k) == {x[2] : x \in {y \in chosen : y[1] = k}}
ChosenPrefixDefined(n) == \A k \in 1..n : ChosenAt(k) # {}
ChosenPrefix(n) == [k \in 1..n |-> CHOOSE e \in ChosenAt(k) : TRUE]

Send(m) == msgs' = msgs \cup {m}
SendAll(S) == msgs' = msgs \cup S
Hold(i, m) == queue' = [queue EXCEPT ![i] = @ \cup {m}]
Commit(i, r) == cur' = [cur EXCEPT ![i] = r]

NoLead == [b |-> 0, phase |-> "none", promises |-> {}, taken |-> <<>>, d |-> 0, sel |-> <<>>,
           mmacc |-> 0, mmlen |-> 0, fc |-> 0]
Empty == [promise |-> 0, acc |-> 0, log |-> <<>>, dec |-> 0]

Init ==
    /\ cur = [i \in Nodes |-> Empty]
    /\ dur = [i \in Nodes |-> Empty]
    /\ queue = [i \in Nodes |-> {}]
    /\ status = [i \in Nodes |-> "running"]
    /\ lead = [i \in Nodes |-> NoLead]
    /\ answers = [i \in Nodes |-> {}]
    /\ msgs = {}
    /\ chosen = {}
    /\ crashes = 0
    /\ faulty \in FaultSets
    /\ badAt = {}
    /\ repaired = {}
    /\ voted = {}
    /\ resets = 0

Ghosts == <<crashes, faulty, repaired, voted, resets>>

----------------------------------------------------------------------------
(* The sync.                                                              *)

SyncDone(i) ==
    /\ status[i] # "down"
    /\ dur[i] # cur[i] \/ queue[i] # {}
    /\ dur' = [dur EXCEPT ![i] = cur[i]]
    /\ SendAll(queue[i])
    /\ queue' = [queue EXCEPT ![i] = {}]
    /\ voted' = voted \cup IF cur[i].acc > 0
                           THEN {<<cur[i].acc, k, cur[i].log[k], i>> : k \in 1..Len(cur[i].log)} ELSE {}
    /\ UNCHANGED <<cur, status, lead, answers, chosen, crashes, faulty, repaired, resets, badAt>>

----------------------------------------------------------------------------
(* Phase 1.                                                               *)

Prepare(i, b) ==
    /\ status[i] = "running"
    /\ Owner(b) = i
    /\ b > cur[i].promise
    /\ Commit(i, [cur[i] EXCEPT !.promise = b])
    /\ lead' = [lead EXCEPT ![i] = [NoLead EXCEPT !.b = b, !.phase = "prepare",
                  !.promises = {[from |-> i, acc |-> cur[i].acc, log |-> cur[i].log, dec |-> cur[i].dec]}]]
    /\ Hold(i, [type |-> "prepare", b |-> b, from |-> i])
    /\ UNCHANGED <<dur, status, answers, msgs, chosen, Ghosts, badAt>>

\* A node promises a higher ballot. It promises the ballot it promised
\* again when it is not the leader of that ballot: the leader sends a
\* prepare again after a prepare request (handle_preparereq,
\* sequence_paxos.hpp). Until the accept sync, the node takes no accept.
HandlePrepare(j, m) ==
    /\ m.type = "prepare"
    /\ status[j] \in {"running", "rejoining"}
    /\ \/ /\ m.b > cur[j].promise
          /\ UNCHANGED resets
       \/ /\ ~NoPromiseAgain
          /\ m.b = cur[j].promise
          /\ Owner(m.b) # j
          /\ resets < MaxReset
          /\ resets' = resets + 1
    /\ Commit(j, [cur[j] EXCEPT !.promise = m.b])
    /\ lead' = [lead EXCEPT ![j] = [NoLead EXCEPT !.phase = "promised"]]
    /\ Hold(j, [type |-> "promise", b |-> m.b, from |-> j, to |-> m.from,
                acc |-> cur[j].acc, log |-> cur[j].log, dec |-> cur[j].dec,
                rejoin |-> status[j] = "rejoining"])
    /\ UNCHANGED <<dur, status, answers, msgs, chosen, crashes, faulty, repaired, voted, badAt>>

Best(P) == CHOOSE p \in P : \A q \in P : \/ p.acc > q.acc
                                         \/ p.acc = q.acc /\ Len(p.log) >= Len(q.log)

\* The prefix of its own log that follower p keeps: the rule of Omni-Paxos
\* (send_accsync, sequence_paxos.hpp), bounded below the first index whose
\* entry the selection changed (fc).
Keep(p, ma, ml, fc) ==
    LET v == IF p.acc = ma THEN Min(Len(p.log), ml) ELSE p.dec
    IN IF NoRepairBound THEN v ELSE Min(v, fc - 1)

SyncMessage(i, b, p, L, d, ma, ml, fc) ==
    [type |-> "acceptsync", b |-> b, from |-> i, to |-> p.from, keep |-> Keep(p, ma, ml, fc), log |-> L, dec |-> d]

SyncMessages(i, b, P, L, d, ma, ml, fc) ==
    {SyncMessage(i, b, q, L, d, ma, ml, fc) : q \in {x \in P : x.from # i}}

\* The first index above d whose selected entry differs from the taken one.
FirstChanged(T, d, S) ==
    IF \E k \in 1..Len(S) : S[k] # T[d + k]
    THEN d + (CHOOSE k \in 1..Len(S) : S[k] # T[d + k] /\ \A x \in 1..(k - 1) : S[x] = T[d + x])
    ELSE Len(T) + 1

\* A majority of promises: the leader takes the log of the best promise
\* and the highest decided index d. With the selection (plan 3.5) it
\* first selects a result for each entry above d; without it, it syncs at
\* once (the rule of Omni-Paxos).
\* The leader, in any phase, takes a prepare request of node j: it forgets the promise of
\* j and sends its prepare again (handle_preparereq). The prepare of the
\* ballot is in msgs already.
ResetPromise(i, j) ==
    /\ status[i] = "running"
    /\ lead[i].phase \in {"prepare", "verify", "accept"}
    /\ j # i
    /\ j \in {q.from : q \in lead[i].promises}
    /\ resets < MaxReset
    /\ resets' = resets + 1
    /\ lead' = [lead EXCEPT ![i].promises = {q \in @ : q.from # j}]
    /\ UNCHANGED <<cur, dur, queue, status, answers, msgs, chosen, crashes, faulty, repaired, voted, badAt>>

HandlePromise(i, m) ==
    /\ m.type = "promise"
    /\ m.to = i
    /\ status[i] = "running"
    /\ lead[i].b = m.b /\ cur[i].promise = m.b
    /\ m.rejoin => lead[i].phase = "accept"
    /\ LET p == [from |-> m.from, acc |-> m.acc, log |-> m.log, dec |-> m.dec]
       IN
       IF lead[i].phase = "prepare"
       THEN LET P == lead[i].promises \cup {p}
            IN IF IsMajority({q.from : q \in P})
               THEN LET L == Best(P).log
                        ma == Best(P).acc
                        d == Min(Len(L), SetMax({q.dec : q \in P} \cup {cur[i].dec}))
                    IN IF NoSelectionAtSync
                       THEN /\ Commit(i, [cur[i] EXCEPT !.log = L, !.acc = m.b, !.dec = d])
                            /\ lead' = [lead EXCEPT ![i] = [@ EXCEPT !.phase = "accept", !.promises = P,
                                                             !.mmacc = ma, !.mmlen = Len(L), !.fc = Len(L) + 1, !.d = d]]
                            /\ SendAll(SyncMessages(i, m.b, P, L, d, ma, Len(L), Len(L) + 1))
                       ELSE /\ lead' = [lead EXCEPT ![i] = [@ EXCEPT !.phase = "verify", !.promises = P,
                                                             !.taken = L, !.d = d, !.sel = <<>>,
                                                             !.mmacc = ma, !.mmlen = Len(L)]]
                            /\ UNCHANGED <<cur, msgs>>
               ELSE /\ lead' = [lead EXCEPT ![i].promises = P]
                    /\ UNCHANGED <<cur, msgs>>
       ELSE IF lead[i].phase = "verify"
       THEN /\ m.from \notin {q.from : q \in lead[i].promises}
            /\ lead' = [lead EXCEPT ![i].promises = @ \cup {p}]
            /\ UNCHANGED <<cur, msgs>>
       ELSE /\ lead[i].phase = "accept"
            /\ m.from \notin {q.from : q \in lead[i].promises}
            /\ lead' = [lead EXCEPT ![i].promises = @ \cup {p}]
            /\ Send(SyncMessage(i, m.b, p, cur[i].log, cur[i].dec, lead[i].mmacc, lead[i].mmlen, lead[i].fc))
            /\ UNCHANGED cur
    /\ UNCHANGED <<dur, queue, status, answers, chosen, Ghosts, badAt>>

\* The index that the selection of leader i works on, and its base.
VIdx(i) == lead[i].d + Len(lead[i].sel) + 1
VBase(i) == SubSeq(lead[i].taken, 1, lead[i].d) \o lead[i].sel

\* A node of the ballot runs the work of the selection and answers. The
\* work message is not in the model: the answer is enabled while the
\* leader works on the index. The work carries the entries of the taken
\* log from the decided index of the node up to d (keep_the_tasks,
\* node.hpp), so the own log of the node must agree up to that index only.
AnswerWork(j, i) ==
    /\ status[j] = "running"
    /\ status[i] = "running"
    /\ lead[i].phase = "verify"
    /\ cur[j].promise = lead[i].b
    /\ VIdx(i) <= Len(lead[i].taken)
    /\ LET f == IF WorkWithoutBase THEN lead[i].d ELSE cur[j].dec
       IN /\ f <= lead[i].d
          /\ Len(cur[j].log) >= f
          /\ SubSeq(cur[j].log, 1, f) = SubSeq(lead[i].taken, 1, f)
    /\ Send([type |-> "ran", b |-> lead[i].b, from |-> j, idx |-> VIdx(i),
             res |-> Run(j, lead[i].taken[VIdx(i)].req, VBase(i))])
    /\ UNCHANGED <<cur, dur, queue, status, lead, answers, chosen, Ghosts, badAt>>

\* A majority of answers has the same result: it is selected. The bytes
\* of the taken entry are the bytes of that result (plan 3.6).
Select(i) ==
    /\ status[i] = "running"
    /\ lead[i].phase = "verify"
    /\ VIdx(i) <= Len(lead[i].taken)
    /\ \E res \in {m.res : m \in {x \in msgs : x.type = "ran" /\ x.b = lead[i].b /\ x.idx = VIdx(i)}} :
         /\ IsMajority({j \in Nodes : [type |-> "ran", b |-> lead[i].b, from |-> j, idx |-> VIdx(i), res |-> res] \in msgs})
         /\ lead' = [lead EXCEPT ![i].sel = Append(@, [req |-> lead[i].taken[VIdx(i)].req, res |-> res])]
         /\ repaired' = IF res # lead[i].taken[VIdx(i)].res
                        THEN repaired \cup {<<VIdx(i), lead[i].taken[VIdx(i)].req>>} ELSE repaired
    /\ UNCHANGED <<cur, dur, queue, status, answers, msgs, chosen, crashes, faulty, badAt, voted, resets>>

\* Every entry above d is selected: the leader syncs. The leader keeps its
\* own prefix by the rule of the followers (append_on_prefix in
\* handle_majority_promises, sequence_paxos.hpp) and takes the rest.
FinishSelection(i) ==
    /\ status[i] = "running"
    /\ lead[i].phase = "verify"
    /\ VIdx(i) > Len(lead[i].taken)
    /\ LET L == VBase(i)
           fc == FirstChanged(lead[i].taken, lead[i].d, lead[i].sel)
           self == CHOOSE q \in lead[i].promises : q.from = i
           k == Min(Keep(self, lead[i].mmacc, lead[i].mmlen, fc), Len(cur[i].log))
           own == SubSeq(cur[i].log, 1, k) \o SubSeq(L, k + 1, Len(L))
       IN /\ Commit(i, [cur[i] EXCEPT !.log = own, !.acc = lead[i].b, !.dec = lead[i].d])
          /\ lead' = [lead EXCEPT ![i].phase = "accept", ![i].fc = fc]
          /\ SendAll(SyncMessages(i, lead[i].b, lead[i].promises, L, lead[i].d,
                                   lead[i].mmacc, lead[i].mmlen, fc))
    /\ UNCHANGED <<dur, queue, status, answers, chosen, Ghosts, badAt>>

\* The longest length a >= dec such that node j agrees with every entry of
\* L in dec + 1 .. a (plan 3.7).
AgreedLen(j, L, dec) ==
    IF NoCheckAtSync THEN Len(L)
    ELSE SetMax({a \in dec..Len(L) : \A k \in (dec + 1)..a : Agrees(j, SubSeq(L, 1, k - 1), L[k])})

\* An accept sync of the ballot that the node accepted already never
\* shortens its log: the log of one leader only grows, so the sync is a
\* prefix of the own log (Raft, Figure 2, AppendEntries step 3).
HandleAcceptSync(j, m) ==
    /\ m.type = "acceptsync"
    /\ m.to = j
    /\ status[j] \in {"running", "rejoining"}
    /\ cur[j].promise = m.b
    /\ cur[j].acc # m.b \/ lead[j].phase = "promised"
    /\ status[j] = "rejoining" => m.b > SetMax({a[2] : a \in answers[j]})
    /\ m.keep <= Len(cur[j].log)
    /\ LET M == SubSeq(cur[j].log, 1, m.keep) \o SubSeq(m.log, m.keep + 1, Len(m.log))
           a == AgreedLen(j, M, m.dec)
           L == SubSeq(M, 1, a)
           d == Min(a, Max(cur[j].dec, m.dec))
           own == Len(cur[j].log)
           same == ~NoKeepOnSameBallot /\ cur[j].acc = m.b /\ own >= Len(M) /\ SubSeq(cur[j].log, 1, Len(M)) = M
       IN /\ lead' = [lead EXCEPT ![j] = NoLead]
          /\ IF same
             THEN /\ Commit(j, [cur[j] EXCEPT !.dec = Max(@, Min(m.dec, own))])
                  /\ Hold(j, [type |-> "synced", b |-> m.b, from |-> j, idx |-> own])
                  /\ UNCHANGED msgs
             ELSE /\ Commit(j, [cur[j] EXCEPT !.log = L, !.acc = m.b, !.dec = d])
                  /\ Hold(j, [type |-> "synced", b |-> m.b, from |-> j, idx |-> a])
                  /\ IF a < Len(M)
                     THEN Send([type |-> "refused", b |-> m.b, from |-> j, idx |-> a + 1])
                     ELSE UNCHANGED msgs
    /\ status' = [status EXCEPT ![j] = "running"]
    /\ answers' = [answers EXCEPT ![j] = {}]
    /\ UNCHANGED <<dur, chosen, Ghosts, badAt>>

----------------------------------------------------------------------------
(* Phase 2.                                                               *)

\* The leader runs the request on its own log and proposes the entry
\* with its result (plan 3.1).
Propose(i, r) ==
    /\ status[i] = "running"
    /\ lead[i].phase = "accept"
    /\ Len(cur[i].log) < MaxLog
    /\ LET idx == Len(cur[i].log) + 1
           e == [req |-> r, res |-> Run(i, r, cur[i].log)]
           am == [type |-> "accept", b |-> lead[i].b, idx |-> idx, e |-> e]
       IN /\ Commit(i, [cur[i] EXCEPT !.log = Append(@, e)])
          /\ IF AcceptBeforeOwnSync
             THEN /\ Send(am)
                  /\ UNCHANGED queue
             ELSE /\ Hold(i, am)
                  /\ UNCHANGED msgs
          /\ badAt' = IF e.res # Good(r, cur[i].log) THEN badAt \cup {<<idx, r>>} ELSE badAt
    /\ UNCHANGED <<dur, status, lead, answers, chosen, Ghosts>>

\* A follower runs the entry on its own log. It accepts the entry when
\* its result agrees, and refuses it otherwise (plan 3.2).
HandleAccept(j, m) ==
    /\ m.type = "accept"
    /\ status[j] = "running"
    /\ cur[j].promise = m.b /\ cur[j].acc = m.b /\ lead[j].phase # "promised"
    /\ Owner(m.b) # j
    /\ m.idx = Len(cur[j].log) + 1
    /\ IF NoCheckAtAccept \/ Agrees(j, cur[j].log, m.e)
       THEN /\ Commit(j, [cur[j] EXCEPT !.log = Append(@, m.e)])
            /\ Hold(j, [type |-> "synced", b |-> m.b, from |-> j, idx |-> m.idx])
            /\ UNCHANGED msgs
       ELSE /\ Send([type |-> "refused", b |-> m.b, from |-> j, idx |-> m.idx])
            /\ UNCHANGED <<cur, queue>>
    /\ UNCHANGED <<dur, status, lead, answers, chosen, Ghosts, badAt>>

LeaderDecide(i) ==
    /\ status[i] = "running"
    /\ lead[i].phase = "accept"
    /\ cur[i].dec < Len(cur[i].log)
    /\ LET b == lead[i].b
           idx == cur[i].dec + 1
           own == IF LeaderCountsUnsynced THEN cur[i] ELSE dur[i]
           checked == NoLeaderCheck \/
                      \A k \in (lead[i].d + 1)..idx : Agrees(i, SubSeq(own.log, 1, k - 1), own.log[k])
           self == IF own.acc = b /\ Len(own.log) >= idx /\ checked THEN {i} ELSE {}
           Q == self \cup {j \in Nodes : \E m \in msgs : m.type = "synced" /\ m.b = b /\ m.from = j /\ m.idx >= idx}
       IN /\ IsMajority(Q)
          /\ Commit(i, [cur[i] EXCEPT !.dec = idx])
          /\ chosen' = chosen \cup {<<idx, cur[i].log[idx]>>}
          /\ Send([type |-> "decide", b |-> b, idx |-> idx])
    /\ UNCHANGED <<dur, queue, status, lead, answers, Ghosts, badAt>>

\* A node runs every decided entry above its decided index again. When
\* its result differs from the decided result, it records the proof and
\* throws (plan 3.8): the node is down and does not apply the entry.
HandleDecide(j, m) ==
    /\ m.type = "decide"
    /\ status[j] = "running"
    /\ cur[j].promise = m.b /\ cur[j].acc = m.b /\ lead[j].phase # "promised"
    /\ cur[j].dec < Min(m.idx, Len(cur[j].log))
    /\ LET d == Min(m.idx, Len(cur[j].log))
           agrees == NoCheckAtDecide \/
                     \A k \in (cur[j].dec + 1)..d : Agrees(j, SubSeq(cur[j].log, 1, k - 1), cur[j].log[k])
       IN IF agrees
          THEN /\ Commit(j, [cur[j] EXCEPT !.dec = d])
               /\ UNCHANGED <<queue, status, lead, answers>>
          ELSE /\ status' = [status EXCEPT ![j] = "down"]
               /\ cur' = [cur EXCEPT ![j] = dur[j]]
               /\ queue' = [queue EXCEPT ![j] = {}]
               /\ lead' = [lead EXCEPT ![j] = NoLead]
               /\ answers' = [answers EXCEPT ![j] = {}]
    /\ UNCHANGED <<dur, msgs, chosen, Ghosts, badAt>>

----------------------------------------------------------------------------
(* Faults.                                                                *)

Crash(i) ==
    /\ status[i] # "down"
    /\ crashes < MaxCrash
    /\ crashes' = crashes + 1
    /\ status' = [status EXCEPT ![i] = "down"]
    /\ cur' = [cur EXCEPT ![i] = dur[i]]
    /\ queue' = [queue EXCEPT ![i] = {}]
    /\ lead' = [lead EXCEPT ![i] = NoLead]
    /\ answers' = [answers EXCEPT ![i] = {}]
    /\ UNCHANGED <<dur, msgs, chosen, faulty, repaired, voted, resets, badAt>>

Restart(i) ==
    /\ status[i] = "down"
    /\ status' = [status EXCEPT ![i] = IF NoRecovery THEN "running" ELSE "recovering"]
    /\ UNCHANGED <<cur, dur, queue, lead, answers, msgs, chosen, Ghosts, badAt>>

RecoveryAnswer(i, j) ==
    /\ status[i] = "recovering"
    /\ status[j] = "running"
    /\ j # i
    /\ answers' = [answers EXCEPT ![i] = @ \cup {<<j, cur[j].promise>>}]
    /\ UNCHANGED <<cur, dur, queue, status, lead, msgs, chosen, Ghosts, badAt>>

RecoveryComplete(i) ==
    /\ status[i] = "recovering"
    /\ Cardinality({a[1] : a \in answers[i]}) >= F + 1
    /\ Commit(i, [cur[i] EXCEPT !.promise = SetMax({a[2] : a \in answers[i]} \cup {@})])
    /\ status' = [status EXCEPT ![i] = IF RecoveryWaitsForLeader THEN "rejoining" ELSE "running"]
    /\ answers' = [answers EXCEPT ![i] = IF RecoveryWaitsForLeader
                                         THEN {<<i, SetMax({a[2] : a \in answers[i]} \cup {cur[i].promise})>>}
                                         ELSE {}]
    /\ UNCHANGED <<dur, queue, lead, msgs, chosen, Ghosts, badAt>>

----------------------------------------------------------------------------

Next ==
    \/ \E i \in Nodes : Crash(i) \/ Restart(i) \/ SyncDone(i)
    \/ \E i \in Nodes, b \in Ballots : Prepare(i, b)
    \/ \E i \in Nodes, r \in Requests : Propose(i, r)
    \/ \E i \in Nodes : Select(i) \/ FinishSelection(i) \/ LeaderDecide(i) \/ RecoveryComplete(i)
    \/ \E i, j \in Nodes : AnswerWork(j, i) \/ RecoveryAnswer(i, j) \/ ResetPromise(i, j)
    \/ \E m \in msgs :
         \/ \E j \in Nodes : HandlePrepare(j, m) \/ HandleAccept(j, m) \/ HandleDecide(j, m)
         \/ HandlePromise(m.to, m)
         \/ HandleAcceptSync(m.to, m)

Spec == Init /\ [][Next]_vars

----------------------------------------------------------------------------
(* Properties.                                                            *)

\* An index is chosen with one request and one result.
ChosenOnce ==
    \A x, y \in chosen : x[1] = y[1] => x[2] = y[2]

DecidedAgree ==
    \A i, j \in Nodes : \A k \in 1..Min(cur[i].dec, cur[j].dec) : cur[i].log[k] = cur[j].log[k]

DecidedIsChosen ==
    \A i \in Nodes : \A k \in 1..cur[i].dec : cur[i].log[k] \in ChosenAt(k)

\* Each chosen entry has the result of the nodes that work, on the chosen
\* prefix below it.
ChosenIsTrue ==
    \A x \in chosen : ChosenPrefixDefined(x[1] - 1) =>
        x[2].res = Good(x[2].req, ChosenPrefix(x[1] - 1))

\* A value that a majority holds durable in one ballot is the only such
\* value at its index (plan 4.3).
MajorityVoted(k, e) == \E b \in Ballots : IsMajority({j \in Nodes : <<b, k, e, j>> \in voted})
MajorityVotedOnce ==
    \A v, w \in voted : v[2] = w[2] /\ MajorityVoted(v[2], v[3]) /\ MajorityVoted(w[2], w[3]) => v[3] = w[3]

\* Covers: each must be violated by a trace.
NoWrongProposalRepaired ==
    ~ \E p \in repaired \cap badAt : \E x \in chosen : x[1] = p[1] /\ x[2].req = p[2]
NoRefusal ==
    ~ \E m \in msgs : m.type = "refused"

\* A cover, not a property: a run must violate it, so that the branch of
\* a promise of the same ballot is reached.
NoPromiseAgainReached ==
    \A j \in Nodes : lead[j].phase # "promised"

=============================================================================
