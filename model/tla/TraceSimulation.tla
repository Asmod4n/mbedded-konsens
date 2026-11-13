---- MODULE TraceSimulation ----
\* Trace validation of the simulation (doc/plan-trace-validation.md, F7 and
\* F8). TraceData.tla is one trace that test/simulation.hpp writes. The line
\* counter l moves only when the model state equals the record: the state of
\* each node and, for each pair of nodes, the types of the messages in the
\* channel. Up to MaxSub extra actions of the touched node run inside one
\* record. TLC reports the invariant NotDone as violated when it matched the
\* whole trace; any other end is a refusal.
EXTENDS SequencePaxos, TraceData

VARIABLES l, sub

TV(v) == [type |-> "Value", session |-> 0, request_number |-> v]
TraceValues == {TV(v) : v \in 1..60}
LogOf(r, n) == [k \in DOMAIN r.s[n].log |-> TV(r.s[n].log[k])]
MaxSub == 3

TypeOf == [n \in {"prepare_req", "prepare", "promise", "accept_sync", "accept_decide",
                  "accepted", "decide", "not_accepted", "proposal_forward"} |->
           CASE n = "prepare_req" -> "PrepareReq" [] n = "prepare" -> "Prepare"
             [] n = "promise" -> "Promise" [] n = "accept_sync" -> "AcceptSync"
             [] n = "accept_decide" -> "AcceptDecide" [] n = "accepted" -> "Accepted"
             [] n = "decide" -> "Decide" [] n = "not_accepted" -> "NotAccepted"
             [] n = "proposal_forward" -> "ProposalForward"]

Rec == Trace[l]

NetOf(r, p) ==
  LET sel == SelectSeq(r.net, LAMBDA m : m[1] = p[1] /\ m[2] = p[2] /\ m[3] \in DOMAIN TypeOf)
  IN [k \in 1..Len(sel) |-> TypeOf[sel[k][3]]]

TypesOf(c) == [k \in 1..Len(c) |-> c[k].type]
E == Rec.ev[1]

MatchNow(r) == \A n \in Nodes :
  /\ promise[n] = r.s[n].promise /\ n_accepted[n] = r.s[n].n_accepted
  /\ log[n] = LogOf(r, n) /\ decided_idx[n] = r.s[n].decided
  /\ role[n] = r.s[n].role /\ phase[n] = r.s[n].phase
  /\ \A p \in Pairs : TypesOf(chan[p]) = NetOf(r, p)

MatchP(r) == \A n \in Nodes :
  /\ promise'[n] = r.s[n].promise /\ n_accepted'[n] = r.s[n].n_accepted
  /\ log'[n] = LogOf(r, n) /\ decided_idx'[n] = r.s[n].decided
  /\ role'[n] = r.s[n].role /\ phase'[n] = r.s[n].phase
  /\ \A p \in Pairs : TypesOf(chan'[p]) = NetOf(r, p)

Touched(e) == IF e.k \in {"deliver", "drop", "duplicate"} THEN e.to ELSE e.node

Extra(e) ==
  LET i == Touched(e) IN
  \/ \E k \in 1..MaxBallot :
        /\ Rec.s[i].role = "Leader" /\ Ballot(k, i) = Rec.s[i].promise
        /\ HandleLeader(i, k)
  \/ LeaderCountsOwnPromise(i)
  \/ LeaderCountsOwnAccepted(i)
  \/ e.k = "tick" /\ ResendMessageTimeout(i)

Main(e) ==
  CASE e.k = "deliver" /\ e.paxos ->
         /\ chan[<<e.from, e.to>>] # <<>>
         /\ Head(chan[<<e.from, e.to>>]).type = TypeOf[e.name]
         /\ Receive(e.from, e.to)
    [] e.k = "append" /\ e.ok -> Propose(e.node, TV(e.value))
    [] e.k = "drop" /\ e.paxos ->
         \E k \in 1..Len(chan[<<e.from, e.to>>]) :
            chan[<<e.from, e.to>>][k].type = TypeOf[e.name] /\ Lose(<<e.from, e.to>>, k)
    [] e.k = "duplicate" /\ e.paxos ->
         \E k \in 1..Len(chan[<<e.from, e.to>>]) :
            chan[<<e.from, e.to>>][k].type = TypeOf[e.name] /\ Duplicate(<<e.from, e.to>>, k)
    [] e.k = "crash" /\ e.clean -> Crash(e.node)
    [] OTHER -> Extra(e)

Zero(e) ==
  \/ e.k = "tick"
  \/ e.k = "deliver" /\ ~e.paxos
  \/ e.k = "append" /\ ~e.ok
  \/ e.k \in {"drop", "duplicate"} /\ ~e.paxos

TraceInit ==
  /\ Init
  /\ MatchNow(Trace[1])
  /\ l = 2
  /\ sub = 0
  /\ TLCSet(1, 2)

TraceNext ==
  /\ l <= Len(Trace)
  /\ Len(Rec.ev) = 1
  /\ \/ /\ sub = 0 /\ Zero(E) /\ MatchNow(Rec)
        /\ UNCHANGED vars /\ l' = l + 1 /\ sub' = 0
     \/ /\ IF sub = 0 THEN Main(E) ELSE Extra(E)
        /\ StateFollows /\ EVFollows
        /\ IF MatchP(Rec) THEN l' = l + 1 /\ sub' = 0
           ELSE sub < MaxSub /\ l' = l /\ sub' = sub + 1

Track == IF l > TLCGet(1) THEN TLCSet(1, l) /\ PrintT(<<"reached", l, Len(Trace)>>) ELSE TRUE

NotDone == l <= Len(Trace)

TraceSpec == TraceInit /\ [][TraceNext]_<<vars, l, sub>>
====
