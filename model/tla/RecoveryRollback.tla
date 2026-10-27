--------------------------- MODULE RecoveryRollback ---------------------------
(***************************************************************************)
(* A small model of the recovery after a start whose store holds the       *)
(* values of the sync before its last one (OneEnvironment, RollbackCrash). *)
(* Each write is synced. The leader sends its whole log in each accept     *)
(* message. An entry is chosen when a majority acked it in one ballot.     *)
(***************************************************************************)
EXTENDS Naturals, Sequences, FiniteSets, TLC

CONSTANTS Nodes, Values, MaxBallot, MaxLog, MaxCrash,
          Recovery            \* "promise": the highest promise of f + 1 answers;
                              \* "answerlog": also the log of the answer with the highest acc;
                              \* "newleader": the node votes again at the first accept of a
                              \* ballot above every promise of f + 1 answers, and takes its log

VARIABLES st, prev, status, lead, msgs, answers, crashes

vars == <<st, prev, status, lead, msgs, answers, crashes>>

N == Cardinality(Nodes)
IsMajority(Q) == 2 * Cardinality(Q) > N
F == (N - 1) \div 2
Ballots == 1..MaxBallot
NodeSeq == CHOOSE s \in [1..N -> Nodes] : \A a, b \in 1..N : a # b => s[a] # s[b]
Owner(b) == NodeSeq[((b - 1) % N) + 1]
SetMax(S) == CHOOSE x \in S : \A y \in S : x >= y

Empty == [prom |-> 0, acc |-> 0, log |-> <<>>]
NoLead == [b |-> 0, phase |-> "none", promises |-> {}, log |-> <<>>]

\* The record with the highest accepted ballot, then the longest log.
Best(S) == CHOOSE a \in S : \A o \in S :
               \/ a.acc > o.acc
               \/ a.acc = o.acc /\ Len(a.log) >= Len(o.log)

\* A synced write. The store keeps the values before it for a rollback.
Write(n, r) ==
    /\ st' = [st EXCEPT ![n] = r]
    /\ prev' = [prev EXCEPT ![n] = IF st[n] # r THEN st[n] ELSE prev[n]]

Init ==
    /\ st = [n \in Nodes |-> Empty]
    /\ prev = [n \in Nodes |-> Empty]
    /\ status = [n \in Nodes |-> "running"]
    /\ lead = [n \in Nodes |-> NoLead]
    /\ msgs = {}
    /\ answers = [n \in Nodes |-> {}]
    /\ crashes = 0

Prepare(l, b) ==
    /\ status[l] = "running"
    /\ Owner(b) = l
    /\ b > st[l].prom
    /\ Write(l, [st[l] EXCEPT !.prom = b])
    /\ lead' = [lead EXCEPT ![l] = [b |-> b, phase |-> "prepare",
                                    promises |-> {[from |-> l, acc |-> st[l].acc, log |-> st[l].log]},
                                    log |-> <<>>]]
    /\ msgs' = msgs \cup {[type |-> "prepare", b |-> b]}
    /\ UNCHANGED <<status, answers, crashes>>

HandlePrepare(n, m) ==
    /\ m.type = "prepare"
    /\ status[n] = "running"
    /\ m.b > st[n].prom
    /\ Write(n, [st[n] EXCEPT !.prom = m.b])
    /\ msgs' = msgs \cup {[type |-> "promise", b |-> m.b, from |-> n, to |-> Owner(m.b),
                           acc |-> st[n].acc, log |-> st[n].log]}
    /\ UNCHANGED <<status, lead, answers, crashes>>

HandlePromise(l, m) ==
    /\ m.type = "promise"
    /\ m.to = l
    /\ status[l] = "running"
    /\ lead[l].b = m.b
    /\ lead[l].phase = "prepare"
    /\ st[l].prom = m.b
    /\ LET P == lead[l].promises \cup {[from |-> m.from, acc |-> m.acc, log |-> m.log]}
       IN IF IsMajority({p.from : p \in P})
          THEN LET lg == Best(P).log
               IN /\ lead' = [lead EXCEPT ![l] = [@ EXCEPT !.phase = "accept", !.promises = P, !.log = lg]]
                  /\ Write(l, [st[l] EXCEPT !.acc = m.b, !.log = lg])
                  /\ msgs' = msgs \cup {[type |-> "accept", b |-> m.b, log |-> lg],
                                        [type |-> "accepted", b |-> m.b, from |-> l, log |-> lg]}
          ELSE /\ lead' = [lead EXCEPT ![l].promises = P]
               /\ UNCHANGED <<st, prev, msgs>>
    /\ UNCHANGED <<status, answers, crashes>>

Propose(l, v) ==
    /\ status[l] = "running"
    /\ lead[l].phase = "accept"
    /\ st[l].prom = lead[l].b
    /\ Len(lead[l].log) < MaxLog
    /\ LET lg == Append(lead[l].log, v)
       IN /\ lead' = [lead EXCEPT ![l].log = lg]
          /\ Write(l, [st[l] EXCEPT !.acc = lead[l].b, !.log = lg])
          /\ msgs' = msgs \cup {[type |-> "accept", b |-> lead[l].b, log |-> lg],
                                [type |-> "accepted", b |-> lead[l].b, from |-> l, log |-> lg]}
    /\ UNCHANGED <<status, answers, crashes>>

HandleAccept(n, m) ==
    /\ m.type = "accept"
    /\ status[n] = "running"
    /\ m.b >= st[n].prom
    /\ ~ (m.b = st[n].acc /\ Len(m.log) <= Len(st[n].log))
    /\ Write(n, [prom |-> m.b, acc |-> m.b, log |-> m.log])
    /\ msgs' = msgs \cup {[type |-> "accepted", b |-> m.b, from |-> n, log |-> m.log]}
    /\ UNCHANGED <<status, lead, answers, crashes>>

\* A start after which the store holds the values before its last write.
RollbackCrash(n) ==
    /\ status[n] = "running"
    /\ crashes < MaxCrash
    /\ st[n] # prev[n]
    /\ crashes' = crashes + 1
    /\ st' = [st EXCEPT ![n] = prev[n]]
    /\ status' = [status EXCEPT ![n] = "recovering"]
    /\ lead' = [lead EXCEPT ![n] = NoLead]
    /\ answers' = [answers EXCEPT ![n] = {}]
    /\ UNCHANGED <<prev, msgs>>

RecoveryAnswer(n, j) ==
    /\ status[n] = "recovering"
    /\ status[j] = "running"
    /\ j # n
    /\ answers' = [answers EXCEPT ![n] = @ \cup {[from |-> j] @@ st[j]}]
    /\ UNCHANGED <<st, prev, status, lead, msgs, crashes>>

RecoveryComplete(n) ==
    /\ status[n] = "recovering"
    /\ Cardinality({a.from : a \in answers[n]}) >= F + 1
    /\ LET own == [from |-> n] @@ st[n]
           p   == SetMax({a.prom : a \in answers[n]} \cup {st[n].prom})
           b   == Best(answers[n] \cup {own})
       IN CASE Recovery = "promise"   -> Write(n, [st[n] EXCEPT !.prom = p])
            [] Recovery = "answerlog" -> Write(n, [prom |-> p, acc |-> b.acc, log |-> b.log])
            [] Recovery = "newleader" ->
                 \E m \in msgs : /\ m.type = "accept"
                                /\ m.b > p
                                /\ Write(n, [prom |-> m.b, acc |-> m.b, log |-> m.log])
    /\ status' = [status EXCEPT ![n] = "running"]
    /\ answers' = [answers EXCEPT ![n] = {}]
    /\ UNCHANGED <<lead, msgs, crashes>>

Next ==
    \/ \E l \in Nodes, b \in Ballots : Prepare(l, b)
    \/ \E l \in Nodes, v \in Values : Propose(l, v)
    \/ \E m \in msgs, n \in Nodes : HandlePrepare(n, m) \/ HandlePromise(n, m) \/ HandleAccept(n, m)
    \/ \E n \in Nodes : RollbackCrash(n) \/ RecoveryComplete(n)
    \/ \E n, j \in Nodes : RecoveryAnswer(n, j)

Spec == Init /\ [][Next]_vars

\* An entry is chosen when a majority acked it in one ballot.
Chosen == {<<i, v>> \in (1..MaxLog) \X Values :
             \E b \in Ballots :
                IsMajority({a.from : a \in {x \in msgs : x.type = "accepted" /\ x.b = b
                                                     /\ Len(x.log) >= i /\ x.log[i] = v}})}

ChosenOnce == \A c, d \in Chosen : c[1] = d[1] => c[2] = d[2]

\* Cover: a node that fell back votes again. TLC must find this violated.
CoverRejoin == ~(crashes = 1 /\ \A n \in Nodes : status[n] = "running")

=============================================================================
