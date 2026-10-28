------------------------- MODULE BallotLeaderElection -------------------------
(* Ballot leader election of include/konsens/ballot_leader_election.hpp,    *)
(* after omnipaxos/src/ballot_leader_election.rs at 532ff79, and the part   *)
(* of Sequence Paxos that it drives: handle_leader, handle_prepare and the  *)
(* promise. Plan: doc/plan-ble-liveness.md. Files cited: ble.hpp =          *)
(* include/konsens/ballot_leader_election.hpp, node.hpp, sp.hpp =           *)
(* include/konsens/sequence_paxos.hpp.                                      *)
EXTENDS Integers, FiniteSets, Sequences

CONSTANTS
  Nodes,
  Links,
  Priority,
  MaxCrash,
  MaxRestart,
  MaxCut,
  MaxN,
  MaxEarlyTimeout,
  AbovePromise,
  Mutation

VARIABLES
  alive,
  recovering,
  links,
  cur,
  leader,
  happy,
  replies,
  promise,
  n_leader,
  role,
  reqs,
  reps,
  prep,
  stable,
  overflow,
  crashes,
  restarts,
  cuts,
  early,
  was_stable

vars == <<alive, recovering, links, cur, leader, happy, replies, promise, n_leader, role,
          reqs, reps, prep, stable, overflow, crashes, restarts, cuts, early, was_stable>>

----------------------------------------------------------------------------
(* A ballot is (n, priority, pid), ordered in that order (ble.hpp uses the *)
(* order of struct ballot; config_id is one constant here).                *)
Ballot(n, p) == [n |-> n, prio |-> Priority[p], pid |-> p]
DefaultBallot == [n |-> 0, prio |-> 0, pid |-> 0]
Less(a, b) ==
  \/ a.n < b.n
  \/ a.n = b.n /\ a.prio < b.prio
  \/ a.n = b.n /\ a.prio = b.prio /\ a.pid < b.pid
Leq(a, b) == a = b \/ Less(a, b)
MaxB(a, b) == IF Less(a, b) THEN b ELSE a

\* ble.hpp:30 majority_ = (peers + 1) / 2 + 1
Majority == Cardinality(Nodes) \div 2 + 1
Peers(i) == Nodes \ {i}
Pairs == {p \in Nodes \X Nodes : p[1] # p[2]}
Linked(i, j) == <<i, j>> \in links \/ <<j, i>> \in links

NoReply == [none |-> TRUE]
Reply(b, l, h) == [none |-> FALSE, ballot |-> b, leader |-> l, happy |-> h]
\* ble.hpp:25-26
InitialRound == 1
RecoveryRound == 0

Got(i) == {j \in Peers(i) : ~replies[i][j].none}
Rep(i, j) == replies[i][j]

\* A node takes as leader only a node it reaches itself; the property
\* asks for a node that reaches a majority of live nodes.
Reach(i) == {i} \cup {j \in alive : Linked(i, j)}
QuorumConnected(i) == i \in alive /\ Cardinality(Reach(i)) >= Majority

----------------------------------------------------------------------------
Init ==
  /\ alive = Nodes
  /\ recovering = [i \in Nodes |-> FALSE]
  /\ links = Links
  /\ cur = [i \in Nodes |-> Ballot(InitialRound, i)]
  /\ leader = [i \in Nodes |-> Ballot(InitialRound, i)]
  /\ happy = [i \in Nodes |-> TRUE]
  /\ replies = [i \in Nodes |-> [j \in Nodes |-> NoReply]]
  /\ promise = [i \in Nodes |-> DefaultBallot]
  /\ n_leader = [i \in Nodes |-> DefaultBallot]
  /\ role = [i \in Nodes |-> "Follower"]
  \* ble.hpp:44 with() calls new_hb_round
  /\ reqs = {p \in Pairs : Linked(p[1], p[2])}
  /\ reps = [p \in Pairs |-> NoReply]
  /\ prep = [p \in Pairs |-> <<>>]
  /\ stable = FALSE
  /\ overflow = FALSE
  /\ crashes = 0
  /\ restarts = 0
  /\ cuts = 0
  /\ early = 0
  /\ was_stable = {}

----------------------------------------------------------------------------
(* The five steps of hb_timeout (ble.hpp:58-75) in the order of the code,  *)
(* then the call of handle_leader in the same tick (node.hpp:126-131).      *)

\* ble.hpp:98-105 update_leader: the highest ballot of the repliers.
UpdateLeader(i) ==
  LET bs == {Rep(i, j).ballot : j \in Got(i)}
  IN IF bs = {} THEN leader[i]
     ELSE LET m == CHOOSE b \in bs : \A c \in bs : Leq(c, b) IN MaxB(leader[i], m)

\* ble.hpp:107-122 update_happiness
Happiness(i, l) ==
  IF l # cur[i]
  THEN \E j \in Got(i) : Rep(i, j).ballot = l /\ Rep(i, j).happy
  ELSE LET pf == Cardinality({j \in Got(i) : Leq(Rep(i, j).leader, cur[i])}) IN
       IF pf + 1 >= Majority THEN TRUE
       ELSE \E j \in Got(i) : Less(cur[i], Rep(i, j).leader) /\ Rep(i, j).happy

\* ble.hpp:124-136 check_takeover
TakesOver(i, h) ==
  /\ ~h
  /\ \A j \in Got(i) : ~Rep(i, j).happy
  /\ Mutation = "NoQuorumCheck" \/ Cardinality(Got(i)) + 1 >= Majority

\* node.hpp:372-379 ballot_above_promise
AbovePromiseOf(i, b) ==
  IF ~AbovePromise \/ b.pid # i \/ Less(promise[i], b) \/ role[i] = "Leader" THEN b
  ELSE [b EXCEPT !.n = promise[i].n + 1]

\* After Stabilize, a round ends only when each reply of it on a live link
\* has arrived: no request of i and no reply to i is on a live link.
RoundDone(i) ==
  /\ \A j \in Peers(i) : (<<i, j>> \in reqs /\ Linked(i, j) /\ j \in alive) => FALSE
  /\ \A j \in Peers(i) : (~reps[<<j, i>>].none /\ Linked(i, j)) => FALSE

HbTimeout(i) ==
  /\ i \in alive
  /\ ~overflow
  /\ stable => Mutation # "NoTimeoutAfterStabilize" /\ RoundDone(i)
  \* Before Stabilize, at most MaxEarlyTimeout rounds end in all.
  /\ ~stable => early < MaxEarlyTimeout
  /\ early' = IF stable THEN early ELSE early + 1
  /\ LET l1 == UpdateLeader(i)
         h1 == Happiness(i, l1)
         take == TakesOver(i, h1)
         c2 == IF take THEN [cur[i] EXCEPT !.n = l1.n + 1] ELSE cur[i]
         l2 == IF take THEN c2 ELSE l1
         h2 == IF take THEN TRUE ELSE h1
         \* ble.hpp:63-70 the raise to the promise of Sequence Paxos
         raise == Less(l2, promise[i])
         l3 == IF raise THEN promise[i] ELSE l2
         c3 == IF raise /\ promise[i].pid = i THEN promise[i] ELSE c2
         h3 == IF raise THEN TRUE ELSE h2
         elected == l3 = c3
         \* node.hpp:127-129, sp.hpp:372-393 handle_leader
         nb == AbovePromiseOf(i, c3)
         lead == /\ elected
                 /\ ~recovering[i]
                 /\ Less(n_leader[i], nb)
                 /\ Less(promise[i], nb)
     IN /\ cur' = [cur EXCEPT ![i] = c3]
        /\ leader' = [leader EXCEPT ![i] = l3]
        /\ happy' = [happy EXCEPT ![i] = h3]
        /\ replies' = [replies EXCEPT ![i] = [j \in Nodes |-> NoReply]]
        \* ble.hpp:88-96 new_hb_round; the requests of the old round and
        \* the replies to them leave, as handle_reply drops them.
        /\ reqs' = {p \in reqs : p[1] # i} \cup {<<i, j>> : j \in {k \in Peers(i) : Linked(i, k)}}
        /\ reps' = [p \in Pairs |-> IF p[2] = i THEN NoReply ELSE reps[p]]
        /\ IF lead
           THEN /\ promise' = [promise EXCEPT ![i] = nb]
                /\ n_leader' = [n_leader EXCEPT ![i] = nb]
                /\ role' = [role EXCEPT ![i] = "Leader"]
                /\ prep' = [p \in Pairs |-> IF p[1] = i /\ Linked(i, p[2])
                                            THEN Append(prep[p], nb) ELSE prep[p]]
           ELSE UNCHANGED <<promise, n_leader, role, prep>>
        /\ overflow' = (c3.n > MaxN \/ nb.n > MaxN)
  /\ UNCHANGED <<alive, recovering, links, stable, crashes, restarts, cuts>>

\* ble.hpp:138-141 handle_request; node.hpp:358-360 a recovering node
\* drops every BLE message; a message to a crashed node or on a cut link
\* is lost.
DeliverRequest(p) ==
  /\ ~overflow
  /\ p \in reqs
  /\ reqs' = reqs \ {p}
  /\ LET i == p[1]
         j == p[2]
     IN reps' = IF j \in alive /\ ~recovering[j] /\ Linked(i, j)
                THEN [reps EXCEPT ![<<j, i>>] = Reply(cur[j], leader[j], happy[j])]
                ELSE reps
  /\ UNCHANGED <<alive, recovering, links, cur, leader, happy, replies, promise, n_leader,
                 role, prep, stable, overflow, crashes, restarts, cuts, early>>

\* ble.hpp:143-150 handle_reply; one reply for each peer in a round.
DeliverReply(p) ==
  /\ ~overflow
  /\ ~reps[p].none
  /\ LET j == p[1]
         i == p[2]
     IN /\ reps' = [reps EXCEPT ![p] = NoReply]
        /\ replies' = IF i \in alive /\ ~recovering[i] /\ Linked(i, j) /\ replies[i][j].none
                      THEN [replies EXCEPT ![i][j] = reps[p]]
                      ELSE replies
  /\ UNCHANGED <<alive, recovering, links, cur, leader, happy, reqs, promise, n_leader,
                 role, prep, stable, overflow, crashes, restarts, cuts, early>>

\* sp.hpp:704-731 handle_prepare: a higher ballot becomes the promise and
\* the node becomes follower. The answer is not modelled.
DeliverPrepare(p) ==
  /\ ~overflow
  /\ prep[p] # <<>>
  /\ prep' = [prep EXCEPT ![p] = Tail(@)]
  /\ LET b == Head(prep[p])
         j == p[2]
     IN IF j \in alive /\ ~recovering[j] /\ Linked(p[1], j) /\ Less(promise[j], b)
        THEN /\ promise' = [promise EXCEPT ![j] = b]
             /\ role' = [role EXCEPT ![j] = "Follower"]
        ELSE UNCHANGED <<promise, role>>
  /\ UNCHANGED <<alive, recovering, links, cur, leader, happy, replies, reqs, reps, n_leader,
                 stable, overflow, crashes, restarts, cuts, early>>

Crash(i) ==
  /\ ~stable /\ ~overflow
  /\ i \in alive
  /\ crashes < MaxCrash
  /\ crashes' = crashes + 1
  /\ alive' = alive \ {i}
  /\ UNCHANGED <<recovering, links, cur, leader, happy, replies, promise, n_leader, role,
                 reqs, reps, prep, stable, overflow, restarts, cuts, early>>

\* node.hpp:114-115 and ble.hpp:28-45: BLE starts with the stored promise
\* as recovered leader and recovery_round; Sequence Paxos recovers.
Restart(i) ==
  /\ ~stable /\ ~overflow
  /\ i \in alive
  /\ restarts < MaxRestart
  /\ restarts' = restarts + 1
  /\ recovering' = [recovering EXCEPT ![i] = TRUE]
  /\ cur' = [cur EXCEPT ![i] = IF promise[i] # DefaultBallot THEN Ballot(RecoveryRound, i)
                               ELSE Ballot(InitialRound, i)]
  /\ leader' = [leader EXCEPT ![i] = IF promise[i] # DefaultBallot THEN promise[i]
                                     ELSE Ballot(InitialRound, i)]
  /\ happy' = [happy EXCEPT ![i] = TRUE]
  /\ replies' = [replies EXCEPT ![i] = [j \in Nodes |-> NoReply]]
  /\ n_leader' = [n_leader EXCEPT ![i] = promise[i]]
  /\ role' = [role EXCEPT ![i] = "Follower"]
  /\ reqs' = {p \in reqs : p[1] # i} \cup {<<i, j>> : j \in {k \in Peers(i) : Linked(i, k)}}
  /\ reps' = [p \in Pairs |-> IF p[2] = i THEN NoReply ELSE reps[p]]
  /\ UNCHANGED <<alive, links, promise, prep, stable, overflow, crashes, cuts, early>>

\* The recovery of VR Revisited ends after f + 1 answers; here it ends
\* when a majority is reachable.
EndRecovery(i) ==
  /\ ~overflow
  /\ i \in alive /\ recovering[i]
  /\ Cardinality({j \in Reach(i) : ~recovering[j] \/ j = i}) >= Majority
  /\ recovering' = [recovering EXCEPT ![i] = FALSE]
  /\ UNCHANGED <<alive, links, cur, leader, happy, replies, promise, n_leader, role,
                 reqs, reps, prep, stable, overflow, crashes, restarts, cuts, early>>

Disconnect(e) ==
  /\ ~stable /\ ~overflow
  /\ e \in links
  /\ cuts < MaxCut
  /\ cuts' = cuts + 1
  /\ links' = links \ {e}
  /\ UNCHANGED <<alive, recovering, cur, leader, happy, replies, promise, n_leader, role,
                 reqs, reps, prep, stable, overflow, crashes, restarts, early>>

Stabilize ==
  /\ ~stable /\ ~overflow
  /\ stable' = TRUE
  /\ UNCHANGED <<alive, recovering, links, cur, leader, happy, replies, promise, n_leader,
                 role, reqs, reps, prep, overflow, crashes, restarts, cuts, early>>

----------------------------------------------------------------------------
StableLeaderAt(l) ==
  /\ l \in alive
  /\ ~recovering[l]
  /\ QuorumConnected(l)
  /\ happy[l]
  /\ leader[l] = cur[l]
  /\ role[l] = "Leader"
  /\ promise[l] = cur[l]
  /\ Cardinality({j \in alive : promise[j] = cur[l]}) >= Majority

Step ==
  \/ \E i \in Nodes : HbTimeout(i) \/ Crash(i) \/ Restart(i) \/ EndRecovery(i)
  \/ \E p \in Pairs : DeliverRequest(p) \/ DeliverReply(p) \/ DeliverPrepare(p)
  \/ \E e \in Links : Disconnect(e)
  \/ Stabilize

Next == Step /\ was_stable' = was_stable \cup {l \in Nodes : StableLeaderAt(l)}

Fairness ==
  /\ \A i \in Nodes : WF_vars(HbTimeout(i) /\ was_stable' = was_stable \cup {l \in Nodes : StableLeaderAt(l)})
  /\ \A i \in Nodes : WF_vars(EndRecovery(i) /\ was_stable' = was_stable \cup {l \in Nodes : StableLeaderAt(l)})
  /\ \A p \in Pairs : WF_vars(DeliverRequest(p) /\ was_stable' = was_stable \cup {l \in Nodes : StableLeaderAt(l)})
  /\ \A p \in Pairs : WF_vars(DeliverReply(p) /\ was_stable' = was_stable \cup {l \in Nodes : StableLeaderAt(l)})
  /\ \A p \in Pairs : WF_vars(DeliverPrepare(p) /\ was_stable' = was_stable \cup {l \in Nodes : StableLeaderAt(l)})
  /\ WF_vars(Stabilize /\ was_stable' = was_stable \cup {l \in Nodes : StableLeaderAt(l)})

Spec == Init /\ [][Next]_vars /\ Fairness

----------------------------------------------------------------------------
NoOverflow == ~overflow

PromiseMonotone == [][\A i \in Nodes : Leq(promise[i], promise'[i])]_vars

\* If some live node reaches a majority after Stabilize, then eventually
\* a stable leader holds and stays.
EventuallyStableLeader ==
  (stable /\ \E i \in Nodes : QuorumConnected(i)) ~> [](\E l \in Nodes : StableLeaderAt(l))

\* Cover, must fail: a stable leader leaves, and another one appears.
StableLeaderReplaced ==
  ~(\E a, b \in Nodes : a # b /\ a \in was_stable /\ a \notin alive /\ StableLeaderAt(b))

\* Values for the configurations.
ZeroPriority == [i \in Nodes |-> 0]
AllLinks == {p \in Nodes \X Nodes : p[1] < p[2]}
\* Chained (paper section 6): 1 - 2 - 3, no link 1 - 3.
ChainLinks == {<<1, 2>>, <<2, 3>>}

TypeOK ==
  /\ alive \subseteq Nodes
  /\ links \subseteq Links
  /\ reqs \subseteq Pairs
=============================================================================
