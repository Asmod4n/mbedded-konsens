----------------------------- MODULE ClusterStop -----------------------------
(***************************************************************************)
(* A small model of the clean stop, the repair in place and the stop of    *)
(* the whole cluster (doc/plan-cluster-stop.md section 3).                 *)
(* Each write is synced. The leader sends its whole log in each accept     *)
(* message. A node reports its log only up to its verified index, in a     *)
(* promise and in an acknowledgement (sequence_paxos.hpp bound_by_the_     *)
(* verified_idx). A crash retires the node (form B, doc/plan-clean-stop).  *)
(* A clean stop writes a mark with the verified index; a clean start takes *)
(* it back and runs no recovery.                                           *)
(***************************************************************************)
EXTENDS Naturals, Sequences, FiniteSets, TLC

CONSTANTS Nodes, MaxBallot, MaxLog, MaxCrash, MaxDamage, MaxStop,
          AutoRepairs,
          Mutation            \* "none";
                              \* "MarkBelowDamage": a clean stop in a repair
                              \* bounds the verified index of the mark at the damage;
                              \* "LaterLeaderSends": a leader sends cluster_stop for a
                              \* stop entry of an earlier ballot

VARIABLES st,       \* durable: [prom, acc, log, dec]
          ver,      \* volatile verified index
          mark,     \* durable clean-stop mark: the verified index, or -1
          status,   \* "running", "repair", "waitop", "stopped", "retired"
          damaged,  \* index of the damaged entry, 0 for none
          count,    \* durable count of repairs
          lead,     \* volatile leader state
          msgs,
          crashes, damages, stops,
          chosen,   \* ghost: the longest chosen log
          stopped_by \* ghost: the ballots of the cluster_stop messages that stopped a node

vars == <<st, ver, mark, status, damaged, count, lead, msgs, crashes, damages, stops,
          chosen, stopped_by>>

N == Cardinality(Nodes)
IsMajority(Q) == 2 * Cardinality(Q) > N
Ballots == 1..MaxBallot
NodeSeq == CHOOSE s \in [1..N -> Nodes] : \A a, b \in 1..N : a # b => s[a] # s[b]
Owner(b) == NodeSeq[((b - 1) % N) + 1]
Min(a, b) == IF a < b THEN a ELSE b
Max(a, b) == IF a > b THEN a ELSE b
SetMax(S) == CHOOSE x \in S : \A y \in S : x >= y

\* An entry is a value and the ballot that appended it.
Write(v, b) == [v |-> v, b |-> b]
Stop(b) == [v |-> "stop", b |-> b]
IsStop(e) == e.v = "stop"
HasStop(l) == \E k \in 1..Len(l) : IsStop(l[k])

Empty == [prom |-> 0, acc |-> 0, log |-> <<>>, dec |-> 0]
NoLead == [b |-> 0, phase |-> "none", promises |-> {}, log |-> <<>>, acks |-> [n \in Nodes |-> 0],
           sent_stop |-> FALSE]

Prefix(a, b) == Len(a) <= Len(b) /\ SubSeq(b, 1, Len(a)) = a
Reported(n) == SubSeq(st[n].log, 1, Min(ver[n], Len(st[n].log)))
Votes(n) == status[n] = "running"

Best(S) == CHOOSE a \in S : \A o \in S :
               \/ a.acc > o.acc
               \/ a.acc = o.acc /\ Len(a.log) >= Len(o.log)

Init ==
    /\ st = [n \in Nodes |-> Empty]
    /\ ver = [n \in Nodes |-> 0]
    /\ mark = [n \in Nodes |-> -1]
    /\ status = [n \in Nodes |-> "running"]
    /\ damaged = [n \in Nodes |-> 0]
    /\ count = [n \in Nodes |-> 0]
    /\ lead = [n \in Nodes |-> NoLead]
    /\ msgs = {}
    /\ crashes = 0 /\ damages = 0 /\ stops = 0
    /\ chosen = <<>>
    /\ stopped_by = {}

Prepare(l, b) ==
    /\ Votes(l)
    /\ Owner(b) = l
    /\ b > st[l].prom
    /\ st' = [st EXCEPT ![l].prom = b]
    /\ lead' = [lead EXCEPT ![l] = [NoLead EXCEPT !.b = b, !.phase = "prepare",
                    !.promises = {[from |-> l, acc |-> st[l].acc, log |-> Reported(l)]}]]
    /\ msgs' = msgs \cup {[type |-> "prepare", b |-> b]}
    /\ UNCHANGED <<ver, mark, status, damaged, count, crashes, damages, stops, chosen, stopped_by>>

HandlePrepare(n, m) ==
    /\ Votes(n)
    /\ m.type = "prepare" /\ m.b > st[n].prom
    /\ st' = [st EXCEPT ![n].prom = m.b]
    /\ lead' = [lead EXCEPT ![n] = NoLead]
    /\ msgs' = msgs \cup {[type |-> "promise", b |-> m.b, from |-> n, acc |-> st[n].acc, log |-> Reported(n)]}
    /\ UNCHANGED <<ver, mark, status, damaged, count, crashes, damages, stops, chosen, stopped_by>>

HandlePromise(l, m) ==
    /\ Votes(l)
    /\ m.type = "promise" /\ lead[l].phase = "prepare" /\ m.b = lead[l].b /\ st[l].prom = m.b
    /\ LET ps == lead[l].promises \cup {[from |-> m.from, acc |-> m.acc, log |-> m.log]}
       IN IF IsMajority({p.from : p \in ps})
          THEN LET lg == Best(ps).log
               IN /\ st' = [st EXCEPT ![l].acc = m.b, ![l].log = lg]
                  /\ ver' = [ver EXCEPT ![l] = Len(lg)]
                  /\ lead' = [lead EXCEPT ![l].promises = ps, ![l].phase = "accept", ![l].log = lg,
                                          ![l].acks = [n \in Nodes |-> IF n = l THEN Len(lg) ELSE 0]]
                  /\ msgs' = msgs \cup {[type |-> "accept", b |-> m.b, log |-> lg]}
          ELSE /\ lead' = [lead EXCEPT ![l].promises = ps]
               /\ UNCHANGED <<st, ver, msgs>>
    /\ UNCHANGED <<mark, status, damaged, count, crashes, damages, stops, chosen, stopped_by>>

\* The leader appends a value. After a stop entry it appends nothing else.
Propose(l, v) ==
    /\ Votes(l)
    /\ lead[l].phase = "accept" /\ st[l].prom = lead[l].b
    /\ Len(lead[l].log) < MaxLog
    /\ ~HasStop(lead[l].log)
    /\ v = "stop" => stops < MaxStop
    /\ stops' = IF v = "stop" THEN stops + 1 ELSE stops
    /\ LET lg == Append(lead[l].log, Write(v, lead[l].b))
       IN /\ st' = [st EXCEPT ![l].acc = lead[l].b, ![l].log = lg]
          /\ lead' = [lead EXCEPT ![l].log = lg, ![l].acks[l] = Min(ver[l], Len(lg))]
          /\ msgs' = msgs \cup {[type |-> "accept", b |-> lead[l].b, log |-> lg]}
    /\ UNCHANGED <<ver, mark, status, damaged, count, crashes, damages, chosen, stopped_by>>

HandleAccept(n, m) ==
    /\ Votes(n)
    /\ m.type = "accept" /\ m.b >= st[n].prom
    /\ ~(st[n].acc = m.b /\ Len(st[n].log) >= Len(m.log))
    /\ st' = [st EXCEPT ![n].prom = m.b, ![n].acc = m.b, ![n].log = m.log]
    /\ ver' = [ver EXCEPT ![n] = Min(ver[n], LET k == CHOOSE k \in 0..Len(m.log) :
                                                  /\ k <= Len(st[n].log)
                                                  /\ SubSeq(m.log, 1, k) = SubSeq(st[n].log, 1, k)
                                                  /\ \A j \in 0..Len(m.log) :
                                                       (j <= Len(st[n].log)
                                                        /\ SubSeq(m.log, 1, j) = SubSeq(st[n].log, 1, j)) => j <= k
                                              IN k)]
    /\ lead' = [lead EXCEPT ![n] = IF lead[n].b = m.b THEN lead[n] ELSE NoLead]
    /\ UNCHANGED <<mark, status, damaged, count, msgs, crashes, damages, stops, chosen, stopped_by>>

\* Execute-Verify passes the entries of the log.
Verify(n) ==
    /\ Votes(n)
    /\ ver[n] < Len(st[n].log)
    /\ ver' = [ver EXCEPT ![n] = Len(st[n].log)]
    /\ UNCHANGED <<st, mark, status, damaged, count, lead, msgs, crashes, damages, stops, chosen, stopped_by>>

\* A node acknowledges its log up to its verified index.
Ack(n) ==
    /\ Votes(n)
    /\ st[n].acc = st[n].prom /\ st[n].acc > 0
    /\ LET a == [type |-> "ack", b |-> st[n].acc, from |-> n, idx |-> Min(ver[n], Len(st[n].log))]
       IN /\ a \notin msgs
          /\ msgs' = msgs \cup {a}
    /\ UNCHANGED <<st, ver, mark, status, damaged, count, lead, crashes, damages, stops, chosen, stopped_by>>

HandleAck(l, m) ==
    /\ Votes(l)
    /\ m.type = "ack" /\ lead[l].phase = "accept" /\ m.b = lead[l].b /\ st[l].prom = m.b
    /\ m.idx > lead[l].acks[m.from]
    /\ LET acks == [lead[l].acks EXCEPT ![m.from] = m.idx,
                                         ![l] = Min(ver[l], Len(st[l].log))]
           ok(i) == IsMajority({n \in Nodes : acks[n] >= i})
           d == SetMax({i \in 0..Len(lead[l].log) : i = 0 \/ ok(i)})
       IN /\ lead' = [lead EXCEPT ![l].acks = acks]
          /\ IF d > st[l].dec
             THEN /\ st' = [st EXCEPT ![l].dec = d]
                  /\ chosen' = IF Len(chosen) < d THEN SubSeq(lead[l].log, 1, d) ELSE chosen
                  /\ msgs' = msgs \cup {[type |-> "decide", b |-> m.b, idx |-> d]}
             ELSE UNCHANGED <<st, chosen, msgs>>
    /\ UNCHANGED <<ver, mark, status, damaged, count, crashes, damages, stops, stopped_by>>

HandleDecide(n, m) ==
    /\ Votes(n)
    /\ m.type = "decide" /\ m.b = st[n].acc /\ m.idx > st[n].dec /\ m.idx <= Len(st[n].log)
    /\ st' = [st EXCEPT ![n].dec = m.idx]
    /\ UNCHANGED <<ver, mark, status, damaged, count, lead, msgs, crashes, damages, stops, chosen, stopped_by>>

\* Plan 2.3: the leader that decided its own stop entry sends cluster_stop
\* to every peer. Mutation LaterLeaderSends: a leader sends it for a stop
\* entry of an earlier ballot.
SendClusterStop(l) ==
    /\ Votes(l)
    /\ lead[l].phase = "accept" /\ st[l].prom = lead[l].b /\ ~lead[l].sent_stop
    /\ \E k \in 1..st[l].dec :
         /\ IsStop(st[l].log[k])
         /\ st[l].log[k].b = lead[l].b \/ Mutation = "LaterLeaderSends"
    /\ lead' = [lead EXCEPT ![l].sent_stop = TRUE]
    /\ msgs' = msgs \cup {[type |-> "cluster_stop", b |-> lead[l].b]}
    /\ UNCHANGED <<st, ver, mark, status, damaged, count, crashes, damages, stops, chosen, stopped_by>>

\* Plan 2.8: the mark keeps the verified index. Mutation MarkBelowDamage
\* bounds it at the damage.
MarkOf(n) ==
    IF Mutation = "MarkBelowDamage" /\ damaged[n] > 0 THEN Min(ver[n], damaged[n] - 1) ELSE ver[n]

CleanStopOf(n) ==
    /\ status' = [status EXCEPT ![n] = "stopped"]
    /\ mark' = [mark EXCEPT ![n] = MarkOf(n)]
    /\ lead' = [lead EXCEPT ![n] = NoLead]

\* An operator stops one node (SIGTERM, question 2 of the plan: also in a repair).
CleanStop(n) ==
    /\ status[n] \in {"running", "repair", "waitop"}
    /\ CleanStopOf(n)
    /\ UNCHANGED <<st, ver, damaged, count, msgs, crashes, damages, stops, chosen, stopped_by>>

\* Plan 2.5: a voting node takes cluster_stop under its promise; a node in
\* a repair takes it when the ballot is not below its stored promise. The
\* leader stops after it sent the message.
TakeClusterStop(n, m) ==
    /\ m.type = "cluster_stop"
    /\ \/ Votes(n) /\ m.b = st[n].prom
       \/ status[n] \in {"repair", "waitop"} /\ m.b >= st[n].prom
    /\ CleanStopOf(n)
    /\ stopped_by' = stopped_by \cup {m.b}
    /\ UNCHANGED <<st, ver, damaged, count, msgs, crashes, damages, stops, chosen>>

\* A clean start takes the verified index of the mark and runs no
\* recovery. The scan of the whole log finds a damage again.
CleanStart(n) ==
    /\ status[n] = "stopped"
    /\ mark' = [mark EXCEPT ![n] = -1]
    /\ ver' = [ver EXCEPT ![n] = Min(mark[n], Len(st[n].log))]
    /\ status' = [status EXCEPT ![n] = IF damaged[n] = 0 THEN "running"
                                       ELSE IF count[n] < AutoRepairs THEN "repair" ELSE "waitop"]
    /\ UNCHANGED <<st, damaged, count, lead, msgs, crashes, damages, stops, chosen, stopped_by>>

\* A crash retires the node: it never votes again under its identity.
Crash(n) ==
    /\ crashes < MaxCrash
    /\ status[n] # "retired"
    /\ crashes' = crashes + 1
    /\ status' = [status EXCEPT ![n] = "retired"]
    /\ lead' = [lead EXCEPT ![n] = NoLead]
    /\ UNCHANGED <<st, ver, mark, damaged, count, msgs, damages, stops, chosen, stopped_by>>

\* A step finds a damaged entry of the log. The node stops voting and
\* repairs while its count allows it, else it waits for the operator.
Damage(n, k) ==
    /\ damages < MaxDamage
    /\ Votes(n)
    /\ k \in 1..Len(st[n].log)
    /\ damages' = damages + 1
    /\ damaged' = [damaged EXCEPT ![n] = k]
    /\ status' = [status EXCEPT ![n] = IF count[n] < AutoRepairs THEN "repair" ELSE "waitop"]
    /\ lead' = [lead EXCEPT ![n] = NoLead]
    /\ UNCHANGED <<st, ver, mark, count, msgs, crashes, stops, chosen, stopped_by>>

\* The repair in place: a peer that is not retired holds the entry, the
\* copy fits the chain, the node keeps its promise and votes again. Plan
\* 2.9: the count is written at the end.
RepairDone(n, p) ==
    /\ status[n] = "repair"
    /\ p # n /\ status[p] \in {"running", "repair", "waitop"}
    /\ Len(st[p].log) >= damaged[n]
    /\ SubSeq(st[p].log, 1, damaged[n]) = SubSeq(st[n].log, 1, damaged[n])
    /\ damaged' = [damaged EXCEPT ![n] = 0]
    /\ count' = [count EXCEPT ![n] = count[n] + 1]
    /\ status' = [status EXCEPT ![n] = "running"]
    /\ UNCHANGED <<st, ver, mark, lead, msgs, crashes, damages, stops, chosen, stopped_by>>

OperatorAllowsRepair(n) ==
    /\ status[n] = "waitop"
    /\ count' = [count EXCEPT ![n] = 0]
    /\ status' = [status EXCEPT ![n] = "repair"]
    /\ UNCHANGED <<st, ver, mark, damaged, lead, msgs, crashes, damages, stops, chosen, stopped_by>>

Next ==
    \/ \E l \in Nodes, b \in Ballots : Prepare(l, b)
    \/ \E n \in Nodes, m \in msgs : HandlePrepare(n, m) \/ HandlePromise(n, m) \/ HandleAccept(n, m)
                                     \/ HandleAck(n, m) \/ HandleDecide(n, m) \/ TakeClusterStop(n, m)
    \/ \E l \in Nodes, v \in {"w", "stop"} : Propose(l, v)
    \/ \E n \in Nodes : Verify(n) \/ Ack(n) \/ SendClusterStop(n) \/ CleanStop(n) \/ CleanStart(n)
                        \/ Crash(n) \/ OperatorAllowsRepair(n)
    \/ \E n \in Nodes, k \in 1..MaxLog : Damage(n, k)
    \/ \E n, p \in Nodes : RepairDone(n, p)

Spec == Init /\ [][Next]_vars

TypeOK ==
    /\ status \in [Nodes -> {"running", "repair", "waitop", "stopped", "retired"}]
    /\ \A n \in Nodes : ver[n] <= Len(st[n].log) \/ status[n] = "stopped"

\* A chosen log is a prefix of every log a leader works with, and of every
\* decided prefix.
ChosenKept ==
    /\ \A l \in Nodes : lead[l].phase = "accept" /\ status[l] = "running" => Prefix(chosen, lead[l].log)
    /\ \A n \in Nodes : LET d == SubSeq(st[n].log, 1, st[n].dec)
                        IN Prefix(d, chosen) \/ Prefix(chosen, d)

\* A node stops on a cluster_stop only under the ballot that appended the
\* stop entry, and that entry is chosen.
StopOnlyUnderTheDecidingBallot ==
    \A b \in stopped_by : \E k \in 1..Len(chosen) : chosen[k] = Stop(b)

RepairsBounded == \A n \in Nodes : count[n] <= AutoRepairs

\* Reachability: a node that was stopped before the cluster stop starts,
\* catches up over the stop entry and runs. TLC finds a violation when
\* it is reachable.
NoCatchUpAfterTheStop ==
    ~\E n \in Nodes : /\ status[n] = "running" /\ HasStop(st[n].log)
                      /\ st[n].dec >= 1 /\ stopped_by # {}
                      /\ \A b \in stopped_by : st[n].prom > b

=============================================================================
