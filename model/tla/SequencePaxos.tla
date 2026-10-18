---------------------------- MODULE SequencePaxos ----------------------------
(* Model S of Omni-Paxos Sequence Paxos, origin github.com/haraldng/omnipaxos *)
(* commit 532ff79. Up to two configurations, StopSign, no compaction, no BLE. *)
(* Files cited: mod.rs, leader.rs, follower.rs = omnipaxos/src/sequence_paxos/ *)
(* internal_storage.rs = omnipaxos/src/storage/, util.rs = omnipaxos/src/,    *)
(* memory_storage.rs = omnipaxos_storage/src/.                                *)
EXTENDS Integers, Sequences, FiniteSets, TLC

CONSTANTS
  \* @typeAlias: entry = {type: Str, session: Int, request_number: Int};
  \* @typeAlias: seqNum = {session: Int, counter: Int};
  \* @typeAlias: logSync = {suffix: Seq($entry), sync_idx: Int};
  \* @typeAlias: msg = {type: Str, n: Int, decided_idx: Int, n_accepted: Int,
  \*   accepted_idx: Int, log_sync: Seq($logSync), seq_num: $seqNum,
  \*   entries: Seq($entry), nonce: Int, promise: Int, primary: Bool,
  \*   log: Seq($entry), read: Int, id: Int, reads: Set(Int), read_index: Int};
  \* @typeAlias: promiseMeta = {state: Str, n_accepted: Int, accepted_idx: Int, decided_idx: Int};
  \* @typeAlias: maxMeta = {n_accepted: Int, accepted_idx: Int, decided_idx: Int, pid: Int};
  \* @typeAlias: leaderState = {n_leader: Int, promises_meta: Int -> $promiseMeta,
  \*   follower_seq_nums: Int -> $seqNum, accepted_indexes: Int -> Int,
  \*   max_promise_meta: $maxMeta, max_promise_sync: Seq($logSync)};
  \* @typeAlias: durable = {promise: Int, n_accepted: Int, log: Seq($entry), decided_idx: Int};
  \* @typeAlias: response = {from: Int, promise: Int, primary: Bool, n_accepted: Int,
  \*   log: Seq($entry), decided_idx: Int};
  \* @typeAlias: read = {state: Str, node: Int, decided_at_start: Seq($entry),
  \*   read_index: Int, result: Seq($entry), stale: Bool, contract_stale_ok: Bool,
  \*   crashes: Int, rolled_back: Set(Int), ballot: Int, decided_new_at_start: Seq($entry)};
  \* @typeAlias: round = {n: Int, id: Int, reads: Set(Int), read_index: Int, acks: Set(Int)};
  \* @typeAlias: session = {open: Bool, request_number: Int, reply: Int};
  \* @typeAlias: forceSingle = {node: Int, log: Seq($entry), decided: Seq($entry),
  \*   history: Seq($entry)};
  \* @typeAlias: stage1Ghost = {down: Set(Int), leave_decided: Int};
  \* @type: Set(Int);
  Nodes,
  \* @type: Set($entry);
  Values,
  \* @type: Int;
  MaxBallot,
  \* @type: Int;
  MaxLog,
  \* @type: Int;
  MaxCounter,
  \* @type: Int;
  MaxSession,
  \* @type: Int;
  Cap,
  \* @type: Int;
  MaxCrash,
  \* @type: Int;
  MaxWipe,
  \* @type: Int;
  MaxLoss,
  \* @type: Int;
  MaxDuplicate,
  \* @type: Int;
  MaxTimeout,
  \* @type: Int;
  MaxElection,
  \* @type: Bool;
  RuleR,
  \* @type: Bool;
  DecidedIdxMonotone,
  \* @type: Int;
  MaxRollback,
  \* @type: Bool;
  Recovery,
  \* @type: Str;
  Mutation,
  \* @type: Bool;
  ClientSessions,
  \* @type: Int;
  MaxClientSend,
  \* @type: Int;
  MaxRequest,
  \* @type: Bool;
  ReadIndex,
  \* @type: Int;
  MaxRead,
  \* @type: Set(Int);
  NextConfigNodes,
  \* @type: Int;
  MaxReconfigure,
  \* @type: Bool;
  SingleNode,
  \* @type: Int;
  MaxForceSingle,
  \* @type: Int;
  MaxReadOnly,
  \* @type: Int;
  MaxHalt,
  \* @type: Bool;
  StaleReadNode

VARIABLES
  \* @type: Int -> Int;
  promise,
  \* @type: Int -> Int;
  n_accepted,
  \* @type: Int -> Seq($entry);
  log,
  \* @type: Int -> Int;
  decided_idx,
  \* @type: Int -> Str;
  role,
  \* @type: Int -> Str;
  phase,
  \* @type: Int -> Int;
  accepted_idx,
  \* @type: Int -> $seqNum;
  current_seq_num,
  \* @type: Int -> Seq($msg);
  cached_promise_message,
  \* @type: Int -> Seq($entry);
  buffered_proposals,
  \* @type: Int -> $leaderState;
  leader_state,
  \* @type: <<Int, Int>> -> Seq($msg);
  chan,
  \* @type: Int -> Bool;
  wiped,
  \* @type: Int;
  crashes,
  \* @type: Int;
  wipes,
  \* @type: Int;
  losses,
  \* @type: Int;
  duplicates,
  \* @type: Int;
  timeouts,
  \* @type: Int;
  elections,
  \* @type: Set($entry);
  proposed,
  \* @type: Seq($entry);
  decided_history,
  \* @type: Int -> Int;
  decided_ballot,
  \* @type: Int -> Bool;
  recovering,
  \* @type: Int -> Int;
  nonce,
  \* @type: Int -> Set($response);
  recovery_responses,
  \* @type: Set(Int);
  rolled_back,
  \* @type: Int -> Set($durable);
  past_durable,
  \* @type: {session: Int, request_number: Int, sends: Int};
  client,
  \* @type: Int -> $read;
  reads,
  \* @type: Int -> Set(Int);
  read_pending,
  \* @type: Int -> $round;
  read_round,
  \* @type: Int -> Seq($entry);
  buffered_stopsign,
  \* @type: Set(Int);
  running,
  \* @type: Int -> Seq($entry);
  first_log,
  \* @type: Int;
  reconfigurations,
  \* @type: $forceSingle;
  force_single,
  \* @type: Int -> Bool;
  read_only,
  \* @type: Int -> Bool;
  deactivated,
  \* @type: Int;
  halts,
  \* @type: Int;
  read_only_entries,
  \* @type: Int -> $stage1Ghost;
  stage1_ghost,
  \* @type: Int -> Int;
  build_source

durable  == <<promise, n_accepted, log, decided_idx>>
volatile == <<role, phase, accepted_idx, current_seq_num,
              cached_promise_message, buffered_proposals, leader_state>>
env      == <<wiped, crashes, wipes, losses, duplicates, timeouts, elections>>
ghost    == <<proposed, decided_history, decided_ballot>>
recovery == <<recovering, nonce, recovery_responses>>
rollback == <<rolled_back, past_durable>>
readvars == <<reads, read_pending, read_round>>
reconfig == <<running, first_log, reconfigurations, force_single, read_only, deactivated,
              halts, read_only_entries, stage1_ghost, build_source>>
vars     == <<durable, volatile, chan, env, ghost, recovery, rollback, client, readvars,
              buffered_stopsign, reconfig>>

----------------------------------------------------------------------------
(* Ballot (n, pid) is the integer n * PidBase + pid. Ballot::default is 0.   *)
(* The order of the integers is the order of (n, pid); config_id and         *)
(* priority are constant in one configuration without BLE.                   *)
PidBase == 10
Ballot(n, p) == n * PidBase + p
BallotPid(b) == b % PidBase
DefaultBallot == 0

Min(a, b) == IF a < b THEN a ELSE b
Max(a, b) == IF a > b THEN a ELSE b
\* @type: Set(Int);
Servers == Nodes \cup NextConfigNodes
\* @type: Int => Int;
ConfigurationId(i) == IF i \in NextConfigNodes THEN 2 ELSE 1
\* @type: Int => Set(Int);
ClusterNodes(i) == IF i \in NextConfigNodes THEN NextConfigNodes ELSE Nodes
\* @type: Int => Set(Int);
Peers(i) == ClusterNodes(i) \ {i}
\* @type: Set(<<Int, Int>>);
Pairs == {p \in Servers \X Servers : p[1] # p[2] /\ ConfigurationId(p[1]) = ConfigurationId(p[2])}
         \cup (IF MaxForceSingle > 0 THEN NextConfigNodes \X Nodes ELSE {})

\* util.rs:445 Quorum::Majority(num_nodes / 2 + 1)
\* @type: Int => Int;
Majority(i) == Cardinality(ClusterNodes(i)) \div 2 + 1

\* util.rs:371-378 SequenceNumber, derived lexicographic order
DefaultSeq == [session |-> 0, counter |-> 0]
\* @type: ($seqNum, $seqNum) => Bool;
SeqLeq(a, b) == a.session < b.session \/ (a.session = b.session /\ a.counter <= b.counter)

\* util.rs:382-390 check_msg_status
\* @type: ($seqNum, $seqNum) => Str;
CheckMsgStatus(cur, msg) ==
  IF Mutation = "SeqNumUnchecked" THEN "Expected"
  ELSE IF msg.session = cur.session /\ msg.counter = cur.counter + 1 THEN "Expected"
  ELSE IF SeqLeq(msg, cur) THEN "Outdated"
  ELSE "DroppedPreceding"

\* Indices are 0-based as in the source. memory_storage.rs:102-107 get_suffix
\* @type: (Seq($entry), Int) => Seq($entry);
Suffix(l, k) == IF k >= Len(l) THEN <<>> ELSE SubSeq(l, k + 1, Len(l))
\* memory_storage.rs:64-67 append_on_prefix, Vec::truncate keeps a shorter log
\* @type: (Seq($entry), Int, Seq($entry)) => Seq($entry);
AppendOnPrefix(l, k, s) == (IF k >= Len(l) THEN l ELSE SubSeq(l, 1, k)) \o s

\* @type: (Seq($entry), Seq($entry)) => Bool;
IsPrefix(a, b) == Len(a) <= Len(b) /\ SubSeq(b, 1, Len(a)) = a
\* @type: (Seq($entry), Seq($entry)) => Bool;
Compatible(a, b) == IsPrefix(a, b) \/ IsPrefix(b, a)

\* util.rs:62-71 PromiseState, util.rs:27-34 PromiseMetaData
\* @type: $promiseMeta;
NotPromised == [state |-> "NotPromised", n_accepted |-> 0, accepted_idx |-> 0, decided_idx |-> 0]
\* @type: $promiseMeta;
PromisedHigher == [state |-> "PromisedHigher", n_accepted |-> 0, accepted_idx |-> 0, decided_idx |-> 0]
\* @type: $maxMeta;
DefaultMeta == [n_accepted |-> 0, accepted_idx |-> 0, decided_idx |-> 0, pid |-> 0]

\* Each message has every field, so that all messages have one type. A field
\* that a message type does not carry keeps its value of EmptyMessage. The
\* log_sync of an AcceptSync is a sequence of one LogSync; the log_sync of a
\* Promise is a sequence of at most one (Option<LogSync>).
\* @type: $msg;
EmptyMessage ==
  [type |-> "", n |-> DefaultBallot, decided_idx |-> 0, n_accepted |-> DefaultBallot,
   accepted_idx |-> 0, log_sync |-> <<>>, seq_num |-> DefaultSeq, entries |-> <<>>,
   nonce |-> 0, promise |-> DefaultBallot, primary |-> FALSE, log |-> <<>>,
   read |-> 0, id |-> 0, reads |-> {}, read_index |-> 0]

\* util.rs:96-108 LeaderState::with
\* @type: Int => $leaderState;
LeaderStateWith(n) ==
  [n_leader          |-> n,
   promises_meta     |-> [k \in Servers |-> NotPromised],
   follower_seq_nums |-> [k \in Servers |-> DefaultSeq],
   accepted_indexes  |-> [k \in Servers |-> 0],
   max_promise_meta  |-> DefaultMeta,
   max_promise_sync  |-> <<>>]

\* Rule R: a node whose storage was lost sends no Promise and no Accepted
\* and counts in no quorum.
Votes(i) == ~(RuleR /\ wiped[i])

\* @type: $entry;
StopSignEntry == [type |-> "StopSign", session |-> 0, request_number |-> 0]
\* @type: Seq($entry) => Bool;
HasStopSign(l) == \E k \in 1..Len(l) : l[k] = StopSignEntry
\* @type: Int => Bool;
AcceptedReconfiguration(i) == i \in Nodes /\ HasStopSign(log[i])
\* @type: Int => Bool;
IsReconfigured(i) == AcceptedReconfiguration(i) /\ decided_idx[i] = accepted_idx[i]

\* A send to a full channel is a loss.
\* @type: (Seq($msg), Seq($msg)) => Seq($msg);
Enqueue(q, ms) == q \o SubSeq(ms, 1, Min(Len(ms), Max(0, Cap - Len(q))))

\* out is a function from destinations to the sequence of messages for each.
\* @type: (<<Int, Int>> -> Seq($msg), Int, Int -> Seq($msg)) => <<Int, Int>> -> Seq($msg);
Send(c, i, out) ==
  [p \in Pairs |-> IF p[1] = i /\ p[2] \in DOMAIN out
                   THEN Enqueue(c[p], out[p[2]]) ELSE c[p]]

\* @type: Int -> Seq($msg);
NoOut == [k \in {} |-> <<>>]
\* @type: (Int, $msg) => Int -> Seq($msg);
One(j, m) == [k \in {j} |-> <<m>>]
\* @type: (Int, Int, $msg) => Int -> Seq($msg);
IfVotes(i, j, m) == IF Votes(i) THEN One(j, m) ELSE NoOut

\* @type: $leaderState => Set(Int);
Promised(ls) == {k \in Servers : ls.promises_meta[k].state = "Promised"}
\* util.rs:197-208 get_promised_followers
\* @type: $leaderState => Set(Int);
PromisedFollowers(ls) == {k \in Promised(ls) : k # BallotPid(ls.n_leader)}

\* util.rs:131-149 set_promise. A tie in (n_accepted, accepted_idx) with
\* another pid is resolved by `replace`; the source keeps the first.
\* @type: ($leaderState, $maxMeta, Seq($logSync), Int, Bool, Bool) => $leaderState;
SetPromise(ls, meta, sync, from, check, replace) ==
  LET mm  == ls.max_promise_meta
      gt  == meta.n_accepted > mm.n_accepted
             \/ (meta.n_accepted = mm.n_accepted /\ meta.accepted_idx > mm.accepted_idx)
      tie == meta.n_accepted = mm.n_accepted /\ meta.accepted_idx = mm.accepted_idx
             /\ meta.pid # mm.pid
      upd == check /\ (gt \/ (tie /\ replace))
  IN [ls EXCEPT !.max_promise_meta = IF upd THEN meta ELSE @,
                !.max_promise_sync = IF upd THEN sync ELSE @,
                !.promises_meta[from] = [state |-> "Promised",
                                         n_accepted |-> meta.n_accepted,
                                         accepted_idx |-> meta.accepted_idx,
                                         decided_idx |-> meta.decided_idx]]

\* util.rs:252-259 is_chosen
\* @type: ($leaderState, Int) => Bool;
IsChosen(ls, idx) ==
  Cardinality({k \in Servers : ls.accepted_indexes[k] >= idx}) >= Majority(BallotPid(ls.n_leader))

\* util.rs:168-177 get_max_decided_idx
\* @type: $leaderState => Int;
MaxDecided(ls) ==
  LET ds == {ls.promises_meta[k].decided_idx : k \in Promised(ls)}
  IN IF ds = {} THEN 0 ELSE CHOOSE d \in ds : \A e \in ds : d >= e

\* util.rs:121-125 next_seq_num, applied to a set of followers
\* @type: ($leaderState, Set(Int)) => $leaderState;
NextSeqNums(ls, fs) ==
  [ls EXCEPT !.follower_seq_nums =
     [k \in Servers |-> IF k \in fs
                      THEN [session |-> @[k].session, counter |-> @[k].counter + 1]
                      ELSE @[k]]]

\* leader.rs:97-109 send_prepare
\* @type: Int => $msg;
PrepareMsg(i) ==
  [EmptyMessage EXCEPT !.type = "Prepare", !.n = leader_state[i].n_leader,
   !.decided_idx = decided_idx[i], !.n_accepted = n_accepted[i],
   !.accepted_idx = accepted_idx[i]]

\* @type: Int => $msg;
PrepareReqMsg(b) == [EmptyMessage EXCEPT !.type = "PrepareReq", !.n = b]

\* leader.rs:150-191 send_accsync with mod.rs:405-437 create_log_sync
\* (no snapshots: suffix from common_prefix_idx, sync_idx = common_prefix_idx).
\* The session is incremented first (util.rs:115-119), then next_seq_num.
\* @type: ($leaderState, Int) => $seqNum;
AccSyncSeq(ls, to) == [session |-> ls.follower_seq_nums[to].session + 1, counter |-> 1]
\* @type: ($leaderState, Int, Seq($entry), Int) => $msg;
AccSyncMsg(ls, to, lg, dec) ==
  LET mm    == ls.max_promise_meta
      fm    == ls.promises_meta[to]
      valid == IF fm.n_accepted = ls.n_leader THEN fm.accepted_idx
               ELSE IF fm.n_accepted = mm.n_accepted THEN Min(mm.accepted_idx, fm.accepted_idx)
               ELSE fm.decided_idx
  IN [EmptyMessage EXCEPT !.type = "AcceptSync", !.n = ls.n_leader,
      !.seq_num = AccSyncSeq(ls, to), !.decided_idx = dec,
      !.log_sync = <<[suffix |-> Suffix(lg, valid), sync_idx |-> valid]>>]
\* @type: ($leaderState, Set(Int)) => $leaderState;
WithAccSyncSeqs(ls, fs) ==
  [ls EXCEPT !.follower_seq_nums =
     [k \in Servers |-> IF k \in fs THEN AccSyncSeq(ls, k) ELSE @[k]]]

\* F6: the durable state of node k as one record.
\* @type: Int => $durable;
DurableOf(k) ==
  [promise |-> promise[k], n_accepted |-> n_accepted[k], log |-> log[k],
   decided_idx |-> decided_idx[k]]

(* Recovery of Viewstamped Replication Revisited (Liskov, Cowling,         *)
(* MIT-CSAIL-TR-2012-021, section 4.3), applied to Sequence Paxos (plan     *)
(* item 26). Names from that paper: Recovery, RecoveryResponse, nonce. A    *)
(* node in (Leader, Accept) is in status normal and is the primary of its   *)
(* ballot. Only the primary sends its log, as in the paper. The log is sent *)
(* whole: a suffix from index 0.                                            *)
\* @type: Int => $msg;
RecoveryMsg(x) == [EmptyMessage EXCEPT !.type = "Recovery", !.nonce = x]

\* @type: (Int, Int) => $msg;
RecoveryResponseMsg(k, x) ==
  LET primary == role[k] = "Leader" /\ phase[k] = "Accept" IN
  [EmptyMessage EXCEPT !.type = "RecoveryResponse", !.nonce = x, !.promise = promise[k],
   !.primary = primary,
   !.n_accepted = IF primary THEN n_accepted[k] ELSE DefaultBallot,
   !.log = IF primary THEN log[k] ELSE <<>>,
   !.decided_idx = IF primary THEN decided_idx[k] ELSE 0]

\* The answers are complete with f + 1 = Majority answers from other nodes,
\* one of them from the primary of the highest ballot among the answers.
\* @type: Set($response) => Set($response);
RecoveryPrimaries(rs) ==
  LET mp == CHOOSE p \in {r.promise : r \in rs} : \A r \in rs : p >= r.promise
  IN {r \in rs : r.primary /\ r.promise = mp /\ BallotPid(mp) = r.from}
\* @type: (Set($response), Int) => Bool;
RecoveryComplete(rs, i) == Cardinality(rs) >= Majority(i) /\ RecoveryPrimaries(rs) # {}

(* Read index, plan item 30, run (e). Omni-Paxos 532ff79 has no read. Names *)
(* from Raft (Ongaro, dissertation 2014, section 6.4): readIndex, heartbeat. *)
(* Owner decisions: in the cluster a read goes only through a read index:    *)
(* one round to a majority, batched, with no clock, after FoundationDB      *)
(* GetReadVersion. A follower asks the leader for the read index and         *)
(* answers from its own log once it has decided up to that index. The read *)
(* index is the accepted_idx of the leader when the round starts: the log   *)
(* of a leader in Accept holds each entry decided at a ballot not higher     *)
(* than its own (S2b), and the majority round shows that no higher ballot    *)
(* had a majority before the round. A read with no round has the mark      *)
(* stale, or the leader in Accept of a configuration with one node answers  *)
(* it from its own log.                                                      *)
(* reads[r].decided_at_start and reads[r].result are ghost values.          *)
\* @type: $read;
NoRead == [state |-> "none", node |-> 0, decided_at_start |-> <<>>,
           read_index |-> 0, result |-> <<>>, stale |-> FALSE, contract_stale_ok |-> FALSE,
           crashes |-> 0, rolled_back |-> {}, ballot |-> DefaultBallot,
           decided_new_at_start |-> <<>>]
\* @type: $round;
NoRound == [n |-> DefaultBallot, id |-> 0, reads |-> {}, read_index |-> 0, acks |-> {}]
ReadMessages == {"ReadIndex", "Heartbeat", "HeartbeatAck", "ReadIndexReply"}

\* @type: $forceSingle;
NoForceSingle == [node |-> 0, log |-> <<>>, decided |-> <<>>, history |-> <<>>]
\* @type: $stage1Ghost;
NoStage1Ghost == [down |-> {}, leave_decided |-> -1]
\* @type: Int => Bool;
InStage2(k) == force_single.node # 0 /\ k \in NextConfigNodes
\* @type: Int => Bool;
SingleNodeMode(k) == read_only[k] \/ InStage2(k)
\* @type: Int => Bool;
Quiet(k) == SingleNodeMode(k) \/ deactivated[k]
\* @type: Int => Bool;
SingleNodeLeader(i) ==
  /\ SingleNode
  /\ ClusterNodes(i) = {i}
  /\ role[i] = "Leader" /\ phase[i] = "Accept"

\* Raft dissertation 6.4: a leader serves a read only after it has committed
\* an entry of its own term. Here: after it has decided the log it started with.
\* @type: Int => Bool;
SingleNodeServes(i) == SingleNodeLeader(i) /\ decided_idx[i] >= Len(first_log[i])

----------------------------------------------------------------------------
Init ==
  /\ promise = [i \in Servers |-> DefaultBallot]
  /\ n_accepted = [i \in Servers |-> DefaultBallot]
  /\ log = [i \in Servers |-> <<>>]
  /\ decided_idx = [i \in Servers |-> 0]
  /\ role = [i \in Servers |-> "Follower"]
  /\ phase = [i \in Servers |-> "None"]
  /\ accepted_idx = [i \in Servers |-> 0]
  /\ current_seq_num = [i \in Servers |-> DefaultSeq]
  /\ cached_promise_message = [i \in Servers |-> <<>>]
  /\ buffered_proposals = [i \in Servers |-> <<>>]
  /\ leader_state = [i \in Servers |-> LeaderStateWith(DefaultBallot)]
  /\ chan = [p \in Pairs |-> <<>>]
  /\ wiped = [i \in Servers |-> FALSE]
  /\ crashes = 0
  /\ wipes = 0
  /\ losses = 0
  /\ duplicates = 0
  /\ timeouts = 0
  /\ elections = 0
  /\ proposed = {}
  /\ decided_history = <<>>
  /\ decided_ballot = [k \in 1..MaxLog |-> DefaultBallot]
  /\ recovering = [i \in Servers |-> FALSE]
  /\ nonce = [i \in Servers |-> 0]
  /\ recovery_responses = [i \in Servers |-> {}]
  /\ rolled_back = {}
  /\ past_durable = [i \in Servers |-> {}]
  /\ client = [session |-> 0, request_number |-> 1, sends |-> 0]
  /\ reads = [r \in 1..MaxRead |-> NoRead]
  /\ read_pending = [i \in Servers |-> {}]
  /\ read_round = [i \in Servers |-> NoRound]
  /\ buffered_stopsign = [i \in Servers |-> <<>>]
  /\ running = Nodes
  /\ first_log = [i \in Servers |-> <<>>]
  /\ reconfigurations = 0
  /\ force_single = NoForceSingle
  /\ read_only = [i \in Servers |-> FALSE]
  /\ deactivated = [i \in Servers |-> FALSE]
  /\ halts = 0
  /\ read_only_entries = 0
  /\ stage1_ghost = [i \in Nodes |-> NoStage1Ghost]
  /\ build_source = [s \in NextConfigNodes |-> 0]

----------------------------------------------------------------------------
(* Shared steps. Each sets the primed node variables it changes and chan'. *)

\* mod.rs:371-384 forward_proposals
\* @type: (Int, Seq($entry), <<Int, Int>> -> Seq($msg)) => Bool;
ForwardProposals(i, es, c) ==
  LET ld == BallotPid(promise[i]) IN
  IF ld > 0 /\ ld # i
  THEN /\ chan' = Send(c, i, One(ld, [EmptyMessage EXCEPT !.type = "ProposalForward",
                                                          !.entries = es]))
       /\ UNCHANGED buffered_proposals
  ELSE /\ buffered_proposals' = [buffered_proposals EXCEPT ![i] = @ \o es]
       /\ chan' = c

\* leader.rs:123-133 accept_entries_leader, leader.rs:193-221 send_acceptdecide
\* @type: (Int, Seq($entry), <<Int, Int>> -> Seq($msg)) => Bool;
AcceptEntriesLeader(i, es, c) ==
  LET acc == accepted_idx[i] + Len(es)
      ls0 == [leader_state[i] EXCEPT !.accepted_indexes[i] = IF Votes(i) THEN acc ELSE @]
      fs  == PromisedFollowers(ls0)
      ls1 == NextSeqNums(ls0, fs)
  IN /\ log' = [log EXCEPT ![i] = @ \o es]
     /\ accepted_idx' = [accepted_idx EXCEPT ![i] = acc]
     /\ leader_state' = [leader_state EXCEPT ![i] = ls1]
     /\ chan' = Send(c, i, [k \in fs |->
                  <<[EmptyMessage EXCEPT !.type = "AcceptDecide", !.n = ls1.n_leader,
                     !.seq_num = ls1.follower_seq_nums[k],
                     !.decided_idx = decided_idx[i], !.entries = es]>>])

\* @type: (Int, $seqNum) => $msg;
AcceptStopSignMsg(n, s) == [EmptyMessage EXCEPT !.type = "AcceptStopSign", !.n = n, !.seq_num = s]

\* @type: (Int, <<Int, Int>> -> Seq($msg)) => Bool;
AcceptStopSignLeader(i, c) ==
  LET acc == accepted_idx[i] + 1
      ls0 == [leader_state[i] EXCEPT !.accepted_indexes[i] = IF Votes(i) THEN acc ELSE @]
      fs  == PromisedFollowers(ls0)
      ls1 == NextSeqNums(ls0, fs)
  IN /\ log' = [log EXCEPT ![i] = Append(@, StopSignEntry)]
     /\ accepted_idx' = [accepted_idx EXCEPT ![i] = acc]
     /\ leader_state' = [leader_state EXCEPT ![i] = ls1]
     /\ chan' = Send(c, i, [k \in fs |->
                  <<AcceptStopSignMsg(ls1.n_leader, ls1.follower_seq_nums[k])>>])

\* @type: (Int, <<Int, Int>> -> Seq($msg)) => Bool;
ForwardStopSign(i, c) ==
  LET ld == BallotPid(promise[i]) IN
  IF ld > 0 /\ ld # i
  THEN /\ chan' = Send(c, i, One(ld, [EmptyMessage EXCEPT !.type = "ForwardStopSign"]))
       /\ UNCHANGED buffered_stopsign
  ELSE /\ buffered_stopsign' = [buffered_stopsign EXCEPT ![i] = <<StopSignEntry>>]
       /\ chan' = c

\* mod.rs:343-357 reconnected, returns [recover, out]
\* @type: (Int, Int) => {valid: Bool, recover: Bool, out: Int -> Seq($msg)};
Reconnect(i, pid) ==
  [valid   |-> FALSE,
   recover |-> pid = BallotPid(promise[i]),
   out     |-> One(pid, PrepareReqMsg(promise[i]))]

\* follower.rs:196-227 check_valid_ballot, returns [valid, recover, out]
\* @type: (Int, Int) => {valid: Bool, recover: Bool, out: Int -> Seq($msg)};
CheckValidBallot(i, b) ==
  IF promise[i] = b THEN [valid |-> TRUE, recover |-> FALSE, out |-> NoOut]
  ELSE IF promise[i] > b
       THEN [valid |-> FALSE, recover |-> FALSE,
             out |-> One(BallotPid(b), [EmptyMessage EXCEPT !.type = "NotAccepted",
                                                            !.n = promise[i]])]
       ELSE LET r == Reconnect(i, BallotPid(b))
            IN [valid |-> FALSE, recover |-> r.recover, out |-> r.out]

\* Effect of an invalid ballot or of DroppedPreceding: Recover and PrepareReq.
\* @type: (Int, {valid: Bool, recover: Bool, out: Int -> Seq($msg)}, <<Int, Int>> -> Seq($msg)) => Bool;
ApplyRecover(i, r, c) ==
  /\ role' = IF r.recover THEN [role EXCEPT ![i] = "Follower"] ELSE role
  /\ phase' = IF r.recover THEN [phase EXCEPT ![i] = "Recover"] ELSE phase
  /\ chan' = Send(c, i, r.out)
  /\ UNCHANGED <<durable, accepted_idx, current_seq_num, cached_promise_message,
                 buffered_proposals, leader_state>>

----------------------------------------------------------------------------
(* Handlers of incoming messages. c is the channel state with the message  *)
(* removed. j is the sender.                                                *)

\* leader.rs:66-74 handle_preparereq
\* @type: (Int, Int, $msg, <<Int, Int>> -> Seq($msg)) => Bool;
HandlePrepareReq(i, j, m, c) ==
  IF role[i] = "Leader" /\ m.n <= leader_state[i].n_leader
  THEN /\ leader_state' = [leader_state EXCEPT ![i].promises_meta[j] = NotPromised]
       /\ chan' = Send(c, i, One(j, PrepareMsg(i)))
       /\ UNCHANGED <<durable, role, phase, accepted_idx, current_seq_num,
                      cached_promise_message, buffered_proposals>>
  ELSE /\ chan' = c
       /\ UNCHANGED <<durable, volatile>>

\* follower.rs:13-51 handle_prepare
\* @type: (Int, Int, $msg, <<Int, Int>> -> Seq($msg)) => Bool;
HandlePrepare(i, j, m, c) ==
  IF promise[i] < m.n \/ (promise[i] = m.n /\ phase[i] = "Recover")
  THEN LET na  == n_accepted[i]
           acc == accepted_idx[i]
           ls  == IF na > m.n_accepted
                  THEN <<[suffix |-> Suffix(log[i], m.decided_idx), sync_idx |-> m.decided_idx]>>
                  ELSE IF na = m.n_accepted /\ acc > m.accepted_idx
                  THEN <<[suffix |-> Suffix(log[i], m.accepted_idx), sync_idx |-> m.accepted_idx]>>
                  ELSE <<>>
           blind == Mutation = "PromiseWithoutAccepted"
           pr  == [EmptyMessage EXCEPT !.type = "Promise", !.n = m.n,
                   !.n_accepted = IF blind THEN DefaultBallot ELSE na,
                   !.decided_idx = decided_idx[i],
                   !.accepted_idx = IF blind THEN 0 ELSE acc,
                   !.log_sync = IF blind THEN <<>> ELSE ls]
       IN /\ promise' = [promise EXCEPT ![i] = m.n]
          /\ role' = [role EXCEPT ![i] = "Follower"]
          /\ phase' = [phase EXCEPT ![i] = "Prepare"]
          /\ current_seq_num' = [current_seq_num EXCEPT ![i] = DefaultSeq]
          /\ cached_promise_message' = [cached_promise_message EXCEPT ![i] = <<pr>>]
          /\ chan' = Send(c, i, IfVotes(i, j, pr))
          /\ UNCHANGED <<n_accepted, log, decided_idx, accepted_idx,
                         buffered_proposals, leader_state>>
  ELSE /\ chan' = c
       /\ UNCHANGED <<durable, volatile>>

\* leader.rs:257-285 handle_majority_promises with
\* internal_storage.rs:313-360 sync_log as one atomic write.
\* @type: (Int, $leaderState, <<Int, Int>> -> Seq($msg)) => Bool;
HandleMajorityPromises(i, ls1, c) ==
  LET sync == ls1.max_promise_sync
      dec  == MaxDecided(ls1)
      skip == sync = <<>> \/ Mutation = "SyncIgnored"
      lg1  == IF skip THEN log[i]
              ELSE AppendOnPrefix(log[i], sync[1].sync_idx, sync[1].suffix)
      acc1 == IF skip THEN accepted_idx[i]
              ELSE sync[1].sync_idx + Len(sync[1].suffix)
      stop == i \in Nodes /\ HasStopSign(lg1)
      bp   == IF stop THEN <<>> ELSE buffered_proposals[i]
      bss  == IF stop THEN <<>> ELSE buffered_stopsign[i]
      lg2  == lg1 \o bp \o bss
      acc2 == acc1 + Len(bp) + Len(bss)
      ls2  == [ls1 EXCEPT !.max_promise_sync = <<>>,
                          !.accepted_indexes[i] = IF Votes(i) THEN acc2 ELSE @]
      fs   == PromisedFollowers(ls2)
  IN /\ n_accepted' = [n_accepted EXCEPT ![i] = ls1.n_leader]
     /\ decided_idx' = [decided_idx EXCEPT ![i] = dec]
     /\ log' = [log EXCEPT ![i] = lg2]
     /\ accepted_idx' = [accepted_idx EXCEPT ![i] = acc2]
     /\ buffered_proposals' = IF stop THEN buffered_proposals
                              ELSE [buffered_proposals EXCEPT ![i] = <<>>]
     /\ buffered_stopsign' = IF stop THEN buffered_stopsign
                             ELSE [buffered_stopsign EXCEPT ![i] = <<>>]
     /\ phase' = [phase EXCEPT ![i] = "Accept"]
     /\ leader_state' = [leader_state EXCEPT ![i] = WithAccSyncSeqs(ls2, fs)]
     /\ chan' = Send(c, i, [k \in fs |-> <<AccSyncMsg(ls2, k, lg2, dec)>>])
     /\ UNCHANGED <<promise, role, current_seq_num, cached_promise_message>>

\* mod.rs:267-271 dispatch, leader.rs:287-299 handle_promise_prepare,
\* leader.rs:301-314 handle_promise_accept
\* @type: (Int, Int, $msg, <<Int, Int>> -> Seq($msg)) => Bool;
HandlePromise(i, j, m, c) ==
  LET ls   == leader_state[i]
      meta == [n_accepted |-> m.n_accepted, accepted_idx |-> m.accepted_idx,
               decided_idx |-> m.decided_idx, pid |-> j]
  IN
  IF role[i] = "Leader" /\ phase[i] = "Prepare" /\ m.n = ls.n_leader
  THEN \E replace \in BOOLEAN :
         LET ls1 == SetPromise(ls, meta, m.log_sync, j, TRUE, replace) IN
         IF Cardinality(Promised(ls1)) >= Majority(i)
         THEN HandleMajorityPromises(i, ls1, c)
         ELSE /\ leader_state' = [leader_state EXCEPT ![i] = ls1]
              /\ chan' = c
              /\ UNCHANGED <<durable, role, phase, accepted_idx, current_seq_num,
                             cached_promise_message, buffered_proposals, buffered_stopsign>>
  ELSE IF role[i] = "Leader" /\ phase[i] = "Accept" /\ m.n = ls.n_leader
  THEN LET ls1 == SetPromise(ls, meta, m.log_sync, j, FALSE, FALSE) IN
       /\ leader_state' = [leader_state EXCEPT ![i] = WithAccSyncSeqs(ls1, {j})]
       /\ chan' = Send(c, i, One(j, AccSyncMsg(ls1, j, log[i], decided_idx[i])))
       /\ UNCHANGED <<durable, role, phase, accepted_idx, current_seq_num,
                      cached_promise_message, buffered_proposals, buffered_stopsign>>
  ELSE /\ chan' = c
       /\ UNCHANGED <<durable, volatile, buffered_stopsign>>

\* follower.rs:53-79 handle_acceptsync, follower.rs:81-86
\* forward_buffered_proposals. DecidedIdxMonotone = FALSE is the source:
\* sync_log writes SetDecidedIndex as given (internal_storage.rs:319-323).
\* TRUE is our variant: the follower keeps max(old, new).
\* @type: (Int, Int, $msg, <<Int, Int>> -> Seq($msg)) => Bool;
HandleAcceptSync(i, j, m, c) ==
  LET cv == CheckValidBallot(i, m.n) IN
  IF ~cv.valid THEN ApplyRecover(i, cv, c)
  ELSE IF role[i] = "Follower" /\ phase[i] = "Prepare"
  THEN LET s   == m.log_sync[1]
           bp  == buffered_proposals[i]
           ld  == BallotPid(promise[i])
           lg  == AppendOnPrefix(log[i], s.sync_idx, s.suffix)
           fwd == bp # <<>> /\ ld > 0 /\ ld # i /\ ~(i \in Nodes /\ HasStopSign(lg))
           acc == [EmptyMessage EXCEPT !.type = "Accepted", !.n = m.n,
                                       !.accepted_idx = s.sync_idx + Len(s.suffix)]
           out == [k \in (IF fwd THEN {ld} ELSE {}) \cup (IF Votes(i) THEN {j} ELSE {}) |->
                     (IF fwd /\ k = ld
                      THEN <<[EmptyMessage EXCEPT !.type = "ProposalForward", !.entries = bp]>>
                      ELSE <<>>)
                     \o (IF Votes(i) /\ k = j THEN <<acc>> ELSE <<>>)]
       IN /\ n_accepted' = [n_accepted EXCEPT ![i] = m.n]
          /\ decided_idx' = [decided_idx EXCEPT ![i] =
                               IF DecidedIdxMonotone THEN Max(@, m.decided_idx) ELSE m.decided_idx]
          /\ log' = [log EXCEPT ![i] = lg]
          /\ accepted_idx' = [accepted_idx EXCEPT ![i] = s.sync_idx + Len(s.suffix)]
          /\ buffered_proposals' = IF fwd THEN [buffered_proposals EXCEPT ![i] = <<>>]
                                   ELSE buffered_proposals
          /\ phase' = [phase EXCEPT ![i] = "Accept"]
          /\ current_seq_num' = [current_seq_num EXCEPT ![i] = m.seq_num]
          /\ cached_promise_message' = [cached_promise_message EXCEPT ![i] = <<>>]
          /\ chan' = Send(c, i, out)
          /\ UNCHANGED <<promise, role, leader_state>>
  ELSE /\ chan' = c
       /\ UNCHANGED <<durable, volatile>>

\* follower.rs:142-158 update_decided_idx_and_get_accepted_idx without
\* batching. Returns [dec, flushed]; flushed is TRUE where it returns Some.
\* @type: (Int, Int, Int) => {dec: Int, flushed: Bool};
UpdateDecided(dec, acc, new) ==
  IF new <= dec THEN [dec |-> dec, flushed |-> FALSE]
  ELSE IF new > acc THEN [dec |-> Min(new, acc), flushed |-> TRUE]
  ELSE [dec |-> new, flushed |-> FALSE]

\* follower.rs:88-110 handle_acceptdecide, follower.rs:230-238
\* handle_sequence_num, follower.rs:160-175 reply_accepted
\* @type: (Int, Int, $msg, <<Int, Int>> -> Seq($msg)) => Bool;
HandleAcceptDecide(i, j, m, c) ==
  LET cv == CheckValidBallot(i, m.n) IN
  IF ~cv.valid THEN ApplyRecover(i, cv, c)
  ELSE IF ~(role[i] = "Follower" /\ phase[i] = "Accept")
  THEN /\ chan' = c
       /\ UNCHANGED <<durable, volatile>>
  ELSE LET st == CheckMsgStatus(current_seq_num[i], m.seq_num) IN
       IF st = "Expected"
       THEN LET acc == accepted_idx[i] + Len(m.entries)
                ud  == UpdateDecided(decided_idx[i], acc, m.decided_idx)
                rep == m.entries # <<>> \/ ud.flushed
            IN /\ log' = [log EXCEPT ![i] = @ \o m.entries]
               /\ accepted_idx' = [accepted_idx EXCEPT ![i] = acc]
               /\ decided_idx' = [decided_idx EXCEPT ![i] = ud.dec]
               /\ current_seq_num' = [current_seq_num EXCEPT ![i] = m.seq_num]
               /\ chan' = Send(c, i, IF rep THEN IfVotes(i, BallotPid(m.n),
                              [EmptyMessage EXCEPT !.type = "Accepted", !.n = m.n,
                                                   !.accepted_idx = acc])
                                     ELSE NoOut)
               /\ UNCHANGED <<promise, n_accepted, role, phase,
                              cached_promise_message, buffered_proposals, leader_state>>
       ELSE IF st = "DroppedPreceding"
       THEN ApplyRecover(i, Reconnect(i, BallotPid(m.n)), c)
       ELSE /\ chan' = c
            /\ UNCHANGED <<durable, volatile>>

\* follower.rs:128-138 handle_decide
\* @type: (Int, Int, $msg, <<Int, Int>> -> Seq($msg)) => Bool;
HandleDecide(i, j, m, c) ==
  LET cv == CheckValidBallot(i, m.n) IN
  IF ~cv.valid THEN ApplyRecover(i, cv, c)
  ELSE IF phase[i] # "Accept"
  THEN /\ chan' = c
       /\ UNCHANGED <<durable, volatile>>
  ELSE LET st == CheckMsgStatus(current_seq_num[i], m.seq_num) IN
       IF st = "Expected"
       THEN LET ud == UpdateDecided(decided_idx[i], accepted_idx[i], m.decided_idx)
            IN /\ decided_idx' = [decided_idx EXCEPT ![i] = ud.dec]
               /\ current_seq_num' = [current_seq_num EXCEPT ![i] = m.seq_num]
               /\ chan' = Send(c, i, IF ud.flushed THEN IfVotes(i, BallotPid(m.n),
                              [EmptyMessage EXCEPT !.type = "Accepted", !.n = m.n,
                                                   !.accepted_idx = accepted_idx[i]])
                                     ELSE NoOut)
               /\ UNCHANGED <<promise, n_accepted, log, role, phase, accepted_idx,
                              cached_promise_message, buffered_proposals, leader_state>>
       ELSE IF st = "DroppedPreceding"
       THEN ApplyRecover(i, Reconnect(i, BallotPid(m.n)), c)
       ELSE /\ chan' = c
            /\ UNCHANGED <<durable, volatile>>

\* @type: (Int, Int, $msg, <<Int, Int>> -> Seq($msg)) => Bool;
HandleAcceptStopSign(i, j, m, c) ==
  LET cv == CheckValidBallot(i, m.n) IN
  IF ~cv.valid THEN ApplyRecover(i, cv, c)
  ELSE IF ~(role[i] = "Follower" /\ phase[i] = "Accept")
  THEN /\ chan' = c
       /\ UNCHANGED <<durable, volatile>>
  ELSE LET st == CheckMsgStatus(current_seq_num[i], m.seq_num) IN
       IF st = "Expected"
       THEN LET lg == IF HasStopSign(log[i]) THEN log[i] ELSE Append(log[i], StopSignEntry)
            IN /\ log' = [log EXCEPT ![i] = lg]
               /\ accepted_idx' = [accepted_idx EXCEPT ![i] = Len(lg)]
               /\ current_seq_num' = [current_seq_num EXCEPT ![i] = m.seq_num]
               /\ chan' = Send(c, i, IfVotes(i, BallotPid(m.n),
                              [EmptyMessage EXCEPT !.type = "Accepted", !.n = m.n,
                                                   !.accepted_idx = Len(lg)]))
               /\ UNCHANGED <<promise, n_accepted, decided_idx, role, phase,
                              cached_promise_message, buffered_proposals, leader_state>>
       ELSE IF st = "DroppedPreceding"
       THEN ApplyRecover(i, Reconnect(i, BallotPid(m.n)), c)
       ELSE /\ chan' = c
            /\ UNCHANGED <<durable, volatile>>

\* @type: (Int, Int, $msg, <<Int, Int>> -> Seq($msg)) => Bool;
HandleForwardStopSign(i, j, m, c) ==
  IF AcceptedReconfiguration(i)
  THEN /\ chan' = c
       /\ UNCHANGED <<durable, volatile, buffered_stopsign>>
  ELSE IF role[i] = "Leader" /\ phase[i] = "Prepare"
  THEN /\ buffered_stopsign' = [buffered_stopsign EXCEPT ![i] = <<StopSignEntry>>]
       /\ chan' = c
       /\ UNCHANGED <<durable, volatile>>
  ELSE IF role[i] = "Leader" /\ phase[i] = "Accept"
  THEN /\ AcceptStopSignLeader(i, c)
       /\ UNCHANGED <<promise, n_accepted, decided_idx, role, phase, current_seq_num,
                      cached_promise_message, buffered_proposals, buffered_stopsign>>
  ELSE /\ ForwardStopSign(i, c)
       /\ UNCHANGED <<durable, volatile>>

\* leader.rs:316-345 handle_accepted, leader.rs:240-255 send_decide
\* @type: (Int, Int, $msg, <<Int, Int>> -> Seq($msg)) => Bool;
HandleAccepted(i, j, m, c) ==
  IF m.n = leader_state[i].n_leader /\ role[i] = "Leader" /\ phase[i] = "Accept"
  THEN LET ls1 == [leader_state[i] EXCEPT !.accepted_indexes[j] = m.accepted_idx] IN
       IF m.accepted_idx > decided_idx[i] /\ IsChosen(ls1, m.accepted_idx)
       THEN LET fs  == PromisedFollowers(ls1)
                ls2 == NextSeqNums(ls1, fs)
            IN /\ decided_idx' = [decided_idx EXCEPT ![i] = m.accepted_idx]
               /\ leader_state' = [leader_state EXCEPT ![i] = ls2]
               /\ chan' = Send(c, i, [k \in fs |->
                            <<[EmptyMessage EXCEPT !.type = "Decide", !.n = ls2.n_leader,
                               !.seq_num = ls2.follower_seq_nums[k],
                               !.decided_idx = m.accepted_idx]>>])
               /\ UNCHANGED <<promise, n_accepted, log, role, phase, accepted_idx,
                              current_seq_num, cached_promise_message, buffered_proposals>>
       ELSE /\ leader_state' = [leader_state EXCEPT ![i] = ls1]
            /\ chan' = c
            /\ UNCHANGED <<durable, role, phase, accepted_idx, current_seq_num,
                           cached_promise_message, buffered_proposals>>
  ELSE /\ chan' = c
       /\ UNCHANGED <<durable, volatile>>

\* leader.rs:365-369 handle_notaccepted, util.rs:156-158 lost_promise
\* @type: (Int, Int, $msg, <<Int, Int>> -> Seq($msg)) => Bool;
HandleNotAccepted(i, j, m, c) ==
  IF role[i] = "Leader" /\ leader_state[i].n_leader < m.n
  THEN /\ leader_state' = [leader_state EXCEPT ![i].promises_meta[j] = PromisedHigher]
       /\ chan' = c
       /\ UNCHANGED <<durable, role, phase, accepted_idx, current_seq_num,
                      cached_promise_message, buffered_proposals>>
  ELSE /\ chan' = c
       /\ UNCHANGED <<durable, volatile>>

\* leader.rs:76-84 handle_forwarded_proposal
\* @type: (Int, Int, $msg, <<Int, Int>> -> Seq($msg)) => Bool;
HandleProposalForward(i, j, m, c) ==
  IF AcceptedReconfiguration(i)
  THEN /\ chan' = c
       /\ UNCHANGED <<durable, volatile>>
  ELSE IF role[i] = "Leader" /\ phase[i] = "Prepare"
  THEN /\ buffered_proposals' = [buffered_proposals EXCEPT ![i] = @ \o m.entries]
       /\ chan' = c
       /\ UNCHANGED <<durable, role, phase, accepted_idx, current_seq_num,
                      cached_promise_message, leader_state>>
  ELSE IF role[i] = "Leader" /\ phase[i] = "Accept"
  THEN /\ AcceptEntriesLeader(i, m.entries, c)
       /\ UNCHANGED <<promise, n_accepted, decided_idx, role, phase, current_seq_num,
                      cached_promise_message, buffered_proposals>>
  ELSE /\ ForwardProposals(i, m.entries, c)
       /\ UNCHANGED <<durable, role, phase, accepted_idx, current_seq_num,
                      cached_promise_message, leader_state>>

\* VR Revisited 4.3: a node in status normal answers Recovery. A node that
\* does not vote does not answer.
\* @type: (Int, Int, $msg, <<Int, Int>> -> Seq($msg)) => Bool;
HandleRecovery(i, j, m, c) ==
  /\ chan' = Send(c, i, IfVotes(i, j, RecoveryResponseMsg(i, m.nonce)))
  /\ UNCHANGED <<durable, volatile, recovery>>

\* VR Revisited 4.3: the recovering node keeps one answer per sender with
\* its own nonce. When the answers are complete, it takes the state of the
\* primary, keeps the higher promise, and continues as a node that
\* restarts with that durable state (Phase::Recover, PrepareReq).
\* @type: (Int, Int, $msg, <<Int, Int>> -> Seq($msg)) => Bool;
HandleRecoveryResponse(i, j, m, c) ==
  IF ~recovering[i] \/ m.nonce # nonce[i]
  THEN /\ chan' = c
       /\ UNCHANGED <<durable, volatile, recovery>>
  ELSE LET r  == [from |-> j, promise |-> m.promise, primary |-> m.primary,
                  n_accepted |-> m.n_accepted, log |-> m.log, decided_idx |-> m.decided_idx]
           rs == {x \in recovery_responses[i] : x.from # j} \cup {r}
       IN IF RecoveryComplete(rs, i)
          THEN LET l  == CHOOSE x \in RecoveryPrimaries(rs) : TRUE
                   pr == Max(promise[i], l.promise)
               IN /\ promise' = [promise EXCEPT ![i] = pr]
                  /\ n_accepted' = [n_accepted EXCEPT ![i] = l.n_accepted]
                  /\ log' = [log EXCEPT ![i] = l.log]
                  /\ decided_idx' = [decided_idx EXCEPT ![i] = l.decided_idx]
                  /\ role' = [role EXCEPT ![i] = "Follower"]
                  /\ phase' = [phase EXCEPT ![i] = "Recover"]
                  /\ accepted_idx' = [accepted_idx EXCEPT ![i] = Len(l.log)]
                  /\ current_seq_num' = [current_seq_num EXCEPT ![i] = DefaultSeq]
                  /\ cached_promise_message' = [cached_promise_message EXCEPT ![i] = <<>>]
                  /\ buffered_proposals' = [buffered_proposals EXCEPT ![i] = <<>>]
                  /\ leader_state' = [leader_state EXCEPT ![i] = LeaderStateWith(pr)]
                  /\ recovering' = [recovering EXCEPT ![i] = FALSE]
                  /\ recovery_responses' = [recovery_responses EXCEPT ![i] = {}]
                  /\ chan' = Send(c, i, [k \in Peers(i) |-> <<PrepareReqMsg(pr)>>])
                  /\ UNCHANGED nonce
          ELSE /\ recovery_responses' = [recovery_responses EXCEPT ![i] = rs]
               /\ chan' = c
               /\ UNCHANGED <<durable, volatile, recovering, nonce>>

\* Raft 6.4.1: a follower asks the leader for the read index of read r.
\* The leader takes r into the next round. A node that is no leader drops it.
\* @type: (Int, Int, $msg, <<Int, Int>> -> Seq($msg)) => Bool;
HandleReadIndex(i, j, m, c) ==
  /\ read_pending' = IF role[i] = "Leader"
                     THEN [read_pending EXCEPT ![i] = @ \cup {m.read}]
                     ELSE read_pending
  /\ chan' = c
  /\ UNCHANGED <<durable, volatile, reads, read_round>>

\* Raft 6.4: a node acknowledges the heartbeat of the leader of the ballot
\* it promised. A node that does not vote does not answer.
\* @type: (Int, Int, $msg, <<Int, Int>> -> Seq($msg)) => Bool;
HandleHeartbeat(i, j, m, c) ==
  /\ chan' = Send(c, i, IF promise[i] = m.n
                        THEN IfVotes(i, j, [EmptyMessage EXCEPT !.type = "HeartbeatAck",
                                                                !.n = m.n, !.id = m.id])
                        ELSE NoOut)
  /\ UNCHANGED <<durable, volatile, readvars>>

\* Raft 6.4: with acknowledgements from a majority, the read index of the
\* round holds for each read of the round. The leader sends one reply per
\* node with the reads of that node.
\* @type: (Int, Int, $msg, <<Int, Int>> -> Seq($msg)) => Bool;
HandleHeartbeatAck(i, j, m, c) ==
  LET rd   == read_round[i]
      ok   == role[i] = "Leader" /\ phase[i] = "Accept" /\ rd.reads # {}
              /\ rd.n = m.n /\ rd.id = m.id /\ leader_state[i].n_leader = m.n
      acks == rd.acks \cup {j}
      own  == {r \in rd.reads : reads[r].node = i}
      dst  == {reads[r].node : r \in rd.reads} \ {i}
  IN IF ~ok
     THEN /\ chan' = c
          /\ UNCHANGED <<durable, volatile, readvars>>
     ELSE IF Cardinality(acks) < Majority(i)
     THEN /\ read_round' = [read_round EXCEPT ![i].acks = acks]
          /\ chan' = c
          /\ UNCHANGED <<durable, volatile, reads, read_pending>>
     ELSE /\ reads' = [r \in DOMAIN reads |->
                         IF r \in own
                         THEN [reads[r] EXCEPT !.state = "indexed", !.read_index = rd.read_index,
                                               !.crashes = crashes, !.rolled_back = rolled_back,
                                               !.ballot = promise[i]]
                         ELSE reads[r]]
          /\ read_round' = [read_round EXCEPT ![i].reads = {}, ![i].acks = {}]
          /\ chan' = Send(c, i, [k \in dst |->
                       <<[EmptyMessage EXCEPT !.type = "ReadIndexReply",
                          !.reads = {r \in rd.reads : reads[r].node = k},
                          !.read_index = rd.read_index]>>])
          /\ UNCHANGED <<durable, volatile, read_pending>>

\* Raft 6.4.1: the follower keeps the read index for each of its reads.
\* @type: (Int, Int, $msg, <<Int, Int>> -> Seq($msg)) => Bool;
HandleReadIndexReply(i, j, m, c) ==
  /\ reads' = [r \in DOMAIN reads |->
                 IF r \in m.reads /\ reads[r].state = "pending" /\ reads[r].node = i
                 THEN [reads[r] EXCEPT !.state = "indexed", !.read_index = m.read_index,
                                       !.crashes = crashes, !.rolled_back = rolled_back,
                                       !.ballot = promise[i]]
                 ELSE reads[r]]
  /\ chan' = c
  /\ UNCHANGED <<durable, volatile, read_pending, read_round>>

----------------------------------------------------------------------------
(* Ghost: decided_history grows with every decided prefix of a node with a  *)
(* vote; decided_ballot[k] is n_accepted of the node that first decided k,  *)
(* and Ballot::default while k is not decided.                              *)
\* @type: Int => Seq($entry);
DecidedPrefixP(i) == SubSeq(log'[i], 1, Min(decided_idx'[i], Len(log'[i])))
VotesP(i) == ~(RuleR /\ wiped'[i])

HistoryUpdate ==
  LET longer == {i \in Servers : VotesP(i)
                   /\ Len(DecidedPrefixP(i)) > Len(decided_history)
                   /\ IsPrefix(decided_history, DecidedPrefixP(i))}
  IN IF longer = {}
     THEN UNCHANGED <<decided_history, decided_ballot>>
     ELSE LET i == CHOOSE x \in longer : TRUE
              p == DecidedPrefixP(i)
          IN /\ decided_history' = p
             /\ decided_ballot' = [k \in 1..MaxLog |->
                                    IF k <= Len(decided_history) \/ k > Len(p) THEN decided_ballot[k]
                                    ELSE n_accepted'[i]]

\* F6: past_durable[k] holds each earlier durable state of k until the
\* rollback has happened. Each step that changes the durable state of k
\* adds the state before the step.
\* @type: Int => $durable;
DurableOfP(k) ==
  [promise |-> promise'[k], n_accepted |-> n_accepted'[k], log |-> log'[k],
   decided_idx |-> decided_idx'[k]]

PastUpdate ==
  past_durable' =
    [k \in Servers |->
       IF MaxRollback > 0 /\ rolled_back' = {}
       THEN IF DurableOfP(k) # DurableOf(k) THEN past_durable[k] \cup {DurableOf(k)}
            ELSE past_durable[k]
       ELSE {}]

GhostUpdate == HistoryUpdate /\ PastUpdate

----------------------------------------------------------------------------
(* Actions *)

\* @type: (Int, <<Int, Int>> -> Seq($msg)) => Bool;
HandleDeactivated(i, c) ==
  /\ deactivated' = [deactivated EXCEPT ![i] = TRUE]
  /\ role' = [role EXCEPT ![i] = "Follower"]
  /\ phase' = [phase EXCEPT ![i] = "None"]
  /\ chan' = c
  /\ UNCHANGED <<durable, accepted_idx, current_seq_num, cached_promise_message,
                 buffered_proposals, leader_state, recovery, readvars, buffered_stopsign>>

\* mod.rs:263-282 handle
\* @type: (Int, Int, $msg, <<Int, Int>> -> Seq($msg)) => Bool;
Dispatch(i, j, m, c) ==
  IF m.type = "RecoveryResponse"
  THEN HandleRecoveryResponse(i, j, m, c) /\ UNCHANGED <<readvars, buffered_stopsign>>
  ELSE IF recovering[i]
  THEN /\ chan' = c
       /\ UNCHANGED <<durable, volatile, recovery, readvars, buffered_stopsign>>
  ELSE IF m.type = "Recovery"
  THEN HandleRecovery(i, j, m, c) /\ UNCHANGED <<readvars, buffered_stopsign>>
  ELSE IF m.type \in ReadMessages
  THEN /\ CASE m.type = "ReadIndex"      -> HandleReadIndex(i, j, m, c)
            [] m.type = "Heartbeat"      -> HandleHeartbeat(i, j, m, c)
            [] m.type = "HeartbeatAck"   -> HandleHeartbeatAck(i, j, m, c)
            [] m.type = "ReadIndexReply" -> HandleReadIndexReply(i, j, m, c)
       /\ UNCHANGED <<recovery, buffered_stopsign>>
  ELSE IF m.type = "Promise"
  THEN HandlePromise(i, j, m, c) /\ UNCHANGED <<readvars, recovery>>
  ELSE IF m.type = "ForwardStopSign"
  THEN HandleForwardStopSign(i, j, m, c) /\ UNCHANGED <<readvars, recovery>>
  ELSE /\ UNCHANGED <<readvars, buffered_stopsign>>
       /\ CASE m.type = "PrepareReq"      -> HandlePrepareReq(i, j, m, c)
            [] m.type = "Prepare"         -> HandlePrepare(i, j, m, c)
            [] m.type = "AcceptSync"      -> HandleAcceptSync(i, j, m, c)
            [] m.type = "AcceptDecide"    -> HandleAcceptDecide(i, j, m, c)
            [] m.type = "Decide"          -> HandleDecide(i, j, m, c)
            [] m.type = "Accepted"        -> HandleAccepted(i, j, m, c)
            [] m.type = "NotAccepted"     -> HandleNotAccepted(i, j, m, c)
            [] m.type = "ProposalForward" -> HandleProposalForward(i, j, m, c)
            [] m.type = "AcceptStopSign"  -> HandleAcceptStopSign(i, j, m, c)
       /\ UNCHANGED recovery

\* mod.rs:263-282 handle: deliver the head of the channel from j to i.
Receive(j, i) ==
  /\ i \in running
  /\ chan[<<j, i>>] # <<>>
  /\ LET m == Head(chan[<<j, i>>])
         c == [chan EXCEPT ![<<j, i>>] = Tail(@)]
     IN /\ SingleNodeMode(i) => m.type = "Deactivated"
        /\ IF m.type = "Deactivated"
           THEN HandleDeactivated(i, c)
           ELSE IF deactivated[i] /\ Mutation # "DeactivatedStillVotes"
           THEN /\ chan' = c
                /\ UNCHANGED <<durable, volatile, recovery, readvars, buffered_stopsign,
                               deactivated>>
           ELSE /\ UNCHANGED deactivated
                /\ Dispatch(i, j, m, c)
  /\ UNCHANGED <<env, proposed, rolled_back, client, running, first_log, reconfigurations,
                 force_single, read_only, halts, read_only_entries, stage1_ghost, build_source>>
  /\ GhostUpdate

\* leader.rs:16-60 handle_leader, own pid only (no BLE).
HandleLeader(i, k) ==
  LET n   == Ballot(k, i)
      na  == n_accepted[i]
      dec == decided_idx[i]
      acc == accepted_idx[i]
      ls0 == LeaderStateWith(n)
      me  == [n_accepted |-> na, accepted_idx |-> acc, decided_idx |-> dec, pid |-> i]
  IN /\ ~recovering[i]
     /\ ~read_only[i]
     /\ ~deactivated[i]
     /\ elections < MaxElection
     /\ elections' = elections + 1
     /\ n > leader_state[i].n_leader
     /\ n > promise[i]
     /\ \E replace \in BOOLEAN :
          leader_state' = [leader_state EXCEPT ![i] =
                             IF Votes(i) THEN SetPromise(ls0, me, <<>>, i, TRUE, replace) ELSE ls0]
     /\ promise' = [promise EXCEPT ![i] = n]
     /\ role' = [role EXCEPT ![i] = "Leader"]
     /\ phase' = [phase EXCEPT ![i] = "Prepare"]
     /\ chan' = Send(chan, i, [p \in Peers(i) |->
                  <<[EmptyMessage EXCEPT !.type = "Prepare", !.n = n, !.decided_idx = dec,
                     !.n_accepted = na, !.accepted_idx = acc]>>])
     /\ UNCHANGED <<n_accepted, log, decided_idx, accepted_idx, current_seq_num,
                    cached_promise_message, buffered_proposals,
                    wiped, crashes, wipes, losses, duplicates, timeouts, proposed,
                    recovery, rolled_back, client, readvars, buffered_stopsign, reconfig>>
  /\ GhostUpdate

\* mod.rs:298-305 append, mod.rs:359-365 propose_entry,
\* leader.rs:111-121 accept_entry_leader
Propose(i, v) ==
  /\ ~ClientSessions
  /\ ~recovering[i]
  /\ ~AcceptedReconfiguration(i) \/ Mutation = "AppendAfterStopSign"
  /\ ~read_only[i] \/ Mutation = "ReadOnlyProposes"
  /\ ~deactivated[i]
  /\ v \notin proposed
  /\ proposed' = proposed \cup {v}
  /\ IF read_only[i]
     THEN /\ log' = [log EXCEPT ![i] = Append(@, v)]
          /\ accepted_idx' = [accepted_idx EXCEPT ![i] = @ + 1]
          /\ UNCHANGED <<promise, n_accepted, decided_idx, role, phase, current_seq_num,
                         cached_promise_message, buffered_proposals, leader_state, chan>>
     ELSE IF role[i] = "Leader" /\ phase[i] = "Prepare"
     THEN /\ buffered_proposals' = [buffered_proposals EXCEPT ![i] = Append(@, v)]
          /\ UNCHANGED <<durable, role, phase, accepted_idx, current_seq_num,
                         cached_promise_message, leader_state, chan>>
     ELSE IF role[i] = "Leader" /\ phase[i] = "Accept"
     THEN /\ \E w \in IF Mutation = "LeaderReplacesValue" THEN Values ELSE {v} :
               AcceptEntriesLeader(i, <<w>>, chan)
          /\ UNCHANGED <<promise, n_accepted, decided_idx, role, phase, current_seq_num,
                         cached_promise_message, buffered_proposals>>
     ELSE /\ ForwardProposals(i, <<v>>, chan)
          /\ UNCHANGED <<durable, role, phase, accepted_idx, current_seq_num,
                         cached_promise_message, leader_state>>
  /\ UNCHANGED <<env, recovery, rolled_back, client, readvars, buffered_stopsign, reconfig>>
  /\ GhostUpdate

\* Restart of each node i in R with the durable state d[i], through
\* mod.rs:53-128 `with`.
\* The promise is always set (bootstrap and `with` write it), so the node
\* restarts in Phase::Recover and sends PrepareReq to every peer.
\* internal_storage.rs:55-74 load_cache: accepted_idx = log length.
\* With Recovery, the node first sends Recovery with a fresh nonce and
\* takes part in nothing else until its answers are complete
\* (owner decision: after each start a node votes only after f + 1 answers).
\* @type: (Set(Int), Int -> $durable, Set(Int), Bool) => Bool;
Restart(R, d, Q, rec) ==
  LET \* @type: (Int -> a, Int -> a) => Int -> a;
      At(f, g) == [k \in Servers |-> IF k \in R THEN g[k] ELSE f[k]]
      msg(k)   == IF rec THEN RecoveryMsg(nonce[k] + 1) ELSE PrepareReqMsg(d[k].promise)
  IN
  /\ promise' = At(promise, [k \in R |-> d[k].promise])
  /\ n_accepted' = At(n_accepted, [k \in R |-> d[k].n_accepted])
  /\ log' = At(log, [k \in R |-> d[k].log])
  /\ decided_idx' = At(decided_idx, [k \in R |-> d[k].decided_idx])
  /\ role' = At(role, [k \in R |-> "Follower"])
  /\ phase' = At(phase, [k \in R |-> "Recover"])
  /\ accepted_idx' = At(accepted_idx, [k \in R |-> Len(d[k].log)])
  /\ current_seq_num' = At(current_seq_num, [k \in R |-> DefaultSeq])
  /\ cached_promise_message' = At(cached_promise_message, [k \in R |-> <<>>])
  /\ buffered_proposals' = At(buffered_proposals, [k \in R |-> <<>>])
  /\ buffered_stopsign' = At(buffered_stopsign, [k \in R |-> <<>>])
  /\ leader_state' = At(leader_state, [k \in R |-> LeaderStateWith(d[k].promise)])
  /\ chan' = [p \in Pairs |-> IF p[1] \in R \ Q /\ p[2] \in Peers(p[1])
                             THEN Enqueue(chan[p], <<msg(p[1])>>) ELSE chan[p]]
  /\ recovering' = At(recovering, [k \in R |-> rec /\ k \notin Q])
  /\ nonce' = At(nonce, [k \in R |-> IF rec /\ k \notin Q THEN nonce[k] + 1 ELSE nonce[k]])
  /\ recovery_responses' = At(recovery_responses, [k \in R |-> {}])
  /\ read_pending' = At(read_pending, [k \in R |-> {}])
  /\ read_round' = At(read_round, [k \in R |-> [read_round[k] EXCEPT !.reads = {}, !.acks = {}]])
  /\ UNCHANGED reads

\* F3: crash keeping durable state.
Crash(i) ==
  /\ crashes < MaxCrash
  /\ crashes' = crashes + 1
  /\ Restart({i}, [k \in {i} |-> DurableOf(k)], {k \in {i} : Quiet(k)}, Recovery)
  /\ UNCHANGED <<wiped, wipes, losses, duplicates, timeouts, elections, proposed, rolled_back,
                 client, reconfig>>
  /\ GhostUpdate

\* F6: rollback. Each node in R restarts with an earlier durable state of its
\* own (lost fsync, backup), all of them in one step (plan item 26: at the
\* same time). The states have valid checksums, so F5 does not see them.
\* Exactly MaxRollback nodes roll back, once; a full cluster from a backup is
\* no fault (plan item 27).
Rollback(R) ==
  /\ rolled_back = {}
  /\ Cardinality(R) = MaxRollback
  /\ R # {}
  /\ rolled_back' = R
  /\ \E d \in [R -> UNION {past_durable[k] : k \in R}] :
       /\ \A k \in R : d[k] \in past_durable[k] /\ d[k] # DurableOf(k)
       /\ Restart(R, d, {k \in R : Quiet(k)}, Recovery)
  /\ UNCHANGED <<wiped, crashes, wipes, losses, duplicates, timeouts, elections, proposed,
                 client, reconfig>>
  /\ GhostUpdate

\* F4: disk loss. The restart finds no promise: (Follower, Phase::None),
\* Ballot::default, mod.rs:78, and writes the default promise, mod.rs:112-115.
Wipe(i) ==
  /\ wipes < MaxWipe
  /\ wipes' = wipes + 1
  /\ wiped' = [wiped EXCEPT ![i] = TRUE]
  /\ promise' = [promise EXCEPT ![i] = DefaultBallot]
  /\ n_accepted' = [n_accepted EXCEPT ![i] = DefaultBallot]
  /\ log' = [log EXCEPT ![i] = <<>>]
  /\ decided_idx' = [decided_idx EXCEPT ![i] = 0]
  /\ role' = [role EXCEPT ![i] = "Follower"]
  /\ phase' = [phase EXCEPT ![i] = "None"]
  /\ accepted_idx' = [accepted_idx EXCEPT ![i] = 0]
  /\ current_seq_num' = [current_seq_num EXCEPT ![i] = DefaultSeq]
  /\ cached_promise_message' = [cached_promise_message EXCEPT ![i] = <<>>]
  /\ buffered_proposals' = [buffered_proposals EXCEPT ![i] = <<>>]
  /\ buffered_stopsign' = [buffered_stopsign EXCEPT ![i] = <<>>]
  /\ leader_state' = [leader_state EXCEPT ![i] = LeaderStateWith(DefaultBallot)]
  /\ recovering' = [recovering EXCEPT ![i] = FALSE]
  /\ recovery_responses' = [recovery_responses EXCEPT ![i] = {}]
  /\ read_pending' = [read_pending EXCEPT ![i] = {}]
  /\ read_round' = [read_round EXCEPT ![i].reads = {}, ![i].acks = {}]
  /\ deactivated' = IF Mutation = "StopSignLost" THEN [deactivated EXCEPT ![i] = FALSE]
                    ELSE deactivated
  /\ UNCHANGED reads
  /\ UNCHANGED <<chan, crashes, losses, duplicates, timeouts, elections, proposed,
                 nonce, rolled_back, client, running, first_log, reconfigurations,
                 force_single, read_only, halts, read_only_entries, stage1_ghost, build_source>>
  /\ GhostUpdate

\* mod.rs:232-237 resend_message_timeout, leader.rs:371-403
\* resend_messages_leader, follower.rs:240-282 resend_messages_follower.
\* util.rs:211-222 get_preparable_peers.
ResendMessageTimeout(i) ==
  /\ ~read_only[i] /\ ~deactivated[i]
  /\ timeouts < MaxTimeout
  /\ timeouts' = timeouts + 1
  /\ IF recovering[i]
     THEN LET open == {k \in Peers(i) : \A r \in recovery_responses[i] : r.from # k}
          IN /\ chan' = Send(chan, i, [k \in open |-> <<RecoveryMsg(nonce[i])>>])
             /\ UNCHANGED <<durable, volatile>>
     ELSE IF role[i] = "Leader"
     THEN IF phase[i] \in {"Prepare", "Accept"}
          THEN LET ls == leader_state[i]
                   pp == {k \in Peers(i) : ls.promises_meta[k].state = "NotPromised"}
                   ss == IF phase[i] = "Accept" /\ AcceptedReconfiguration(i)
                         THEN {k \in PromisedFollowers(ls) :
                                 IsReconfigured(i) \/ ls.accepted_indexes[k] # accepted_idx[i]}
                         ELSE {}
                   \* @type: Int => $msg;
                   resend(k) ==
                     IF IsReconfigured(i)
                     THEN [EmptyMessage EXCEPT !.type = "Decide", !.n = ls.n_leader,
                           !.seq_num = ls.follower_seq_nums[k], !.decided_idx = decided_idx[i]]
                     ELSE AcceptStopSignMsg(ls.n_leader, ls.follower_seq_nums[k])
               IN /\ chan' = Send(chan, i, [k \in pp \cup ss |->
                                    IF k \in pp THEN <<PrepareMsg(i)>> ELSE <<resend(k)>>])
                  /\ UNCHANGED <<durable, volatile>>
          ELSE UNCHANGED <<durable, volatile, chan>>
     ELSE IF phase[i] = "Prepare"
     THEN IF cached_promise_message[i] # <<>>
          THEN LET pr == cached_promise_message[i][1]
               IN /\ chan' = Send(chan, i, IfVotes(i, BallotPid(pr.n), pr))
                  /\ UNCHANGED <<durable, volatile>>
          ELSE /\ phase' = [phase EXCEPT ![i] = "Recover"]
               /\ chan' = Send(chan, i, [k \in Peers(i) |-> <<PrepareReqMsg(promise[i])>>])
               /\ UNCHANGED <<durable, role, accepted_idx, current_seq_num,
                              cached_promise_message, buffered_proposals, leader_state>>
     ELSE IF phase[i] = "Recover"
     THEN /\ chan' = Send(chan, i, [k \in Peers(i) |-> <<PrepareReqMsg(promise[i])>>])
          /\ UNCHANGED <<durable, volatile>>
     ELSE UNCHANGED <<durable, volatile, chan>>
  /\ UNCHANGED <<wiped, crashes, wipes, losses, duplicates, elections, proposed,
                 recovery, rolled_back, client, readvars, buffered_stopsign, reconfig>>
  /\ chan' # chan \/ phase' # phase
  /\ GhostUpdate

\* F1: loss of any message on a channel.
\* @type: (<<Int, Int>>, Int) => Bool;
Lose(p, k) ==
  /\ losses < MaxLoss
  /\ losses' = losses + 1
  /\ chan' = [chan EXCEPT ![p] = SubSeq(@, 1, k - 1) \o SubSeq(@, k + 1, Len(@))]
  /\ UNCHANGED <<durable, volatile, wiped, crashes, wipes, duplicates, timeouts, elections, proposed,
                 recovery, rolled_back, client, readvars, buffered_stopsign, reconfig>>
  /\ GhostUpdate

\* F1: duplication; the copy is appended at the end of the channel.
\* @type: (<<Int, Int>>, Int) => Bool;
Duplicate(p, k) ==
  /\ duplicates < MaxDuplicate
  /\ duplicates' = duplicates + 1
  /\ Len(chan[p]) < Cap
  /\ chan' = [chan EXCEPT ![p] = Append(@, @[k])]
  /\ UNCHANGED <<durable, volatile, wiped, crashes, wipes, losses, timeouts, elections, proposed,
                 recovery, rolled_back, client, readvars, buffered_stopsign, reconfig>>
  /\ GhostUpdate


----------------------------------------------------------------------------
(* Client sessions, plan item 31, run (f). Omni-Paxos 532ff79 has no client *)
(* session. Names from Raft (Ongaro, dissertation 2014, section 6.3):       *)
(* RegisterClient, session, expire; and from VR Revisited (Liskov, Cowling, *)
(* section 4.1): client-table, request-number. Owner decisions: the session *)
(* ID is the log index of the RegisterClient entry; the client numbers each *)
(* request; the replicated state keeps the last request-number and its      *)
(* reply for each session; an entry in the log expires a session. A client *)
(* sends to any node, and a follower forwards the entry to the leader. One  *)
(* client. A reply is no message: the reply step reads the decided state of *)
(* one node. A lost reply is a reply step that does not occur.              *)
\* @type: Int => Seq($entry);
DecidedPrefix(i) == SubSeq(log[i], 1, Min(decided_idx[i], Len(log[i])))

\* @type: $entry;
RegisterClientEntry == [type |-> "RegisterClient", session |-> 0, request_number |-> 0]
\* @type: (Int, Int) => $entry;
RequestEntry(s, n) == [type |-> "Request", session |-> s, request_number |-> n]
\* @type: Int => $entry;
ExpireSessionEntry(s) == [type |-> "ExpireSession", session |-> s, request_number |-> 0]

\* @type: $session;
NoSession == [open |-> FALSE, request_number |-> 0, reply |-> 0]

\* The state machine applies the decided entries in the order of the log. Its
\* state is the client-table. The reply of a request is its log index. A
\* request of a session that is not open is not executed (Raft 6.3:
\* SESSION_EXPIRED). The RegisterClient entry at index s opens session s; an
\* ExpireSession entry closes it. Executed(p, k) is TRUE where the request at
\* index k of p is executed.
\* @type: (Seq($entry), Int) => Bool;
Opened(p, s) == s \in 1..MaxLog /\ s \in DOMAIN p /\ p[s] = RegisterClientEntry

\* @type: (Seq($entry), Int, Int) => Bool;
ExpiredBefore(p, s, k) == \E j \in DOMAIN p : s < j /\ j < k /\ p[j] = ExpireSessionEntry(s)

\* @type: (Seq($entry), Int) => Bool;
Executed(p, k) ==
  LET e == p[k] IN
  /\ e.type = "Request"
  /\ e.session < k
  /\ Opened(p, e.session)
  /\ ~ExpiredBefore(p, e.session, k)
  /\ \/ Mutation = "RequestNumberUnchecked"
     \/ /\ e.request_number > 0
        /\ \A j \in DOMAIN p :
             (e.session < j /\ j < k /\ p[j].type = "Request" /\ p[j].session = e.session)
               => p[j].request_number < e.request_number

\* @type: Seq($entry) => Int -> $session;
ClientTable(p) ==
  [s \in 1..MaxLog |->
     LET ex   == {k \in DOMAIN p : Executed(p, k) /\ p[k].session = s}
         last == CHOOSE k \in ex : \A j \in ex : j <= k
     IN IF ~Opened(p, s) THEN NoSession
        ELSE [open |-> ~ExpiredBefore(p, s, Len(p) + 1),
              request_number |-> IF ex = {} THEN 0 ELSE p[last].request_number,
              reply |-> IF ex = {} THEN 0 ELSE last]]

\* The client sends entry e to node i: mod.rs:298-305 append, mod.rs:359-365
\* propose_entry, as Propose. Each send counts, a resend too.
\* @type: (Int, $entry) => Bool;
ClientSend(i, e) ==
  /\ ClientSessions
  /\ ~recovering[i]
  /\ ~read_only[i]
  /\ ~deactivated[i]
  /\ ~AcceptedReconfiguration(i)
  /\ client.sends < MaxClientSend
  /\ client' = [client EXCEPT !.sends = @ + 1]
  /\ proposed' = proposed \cup {e}
  /\ IF role[i] = "Leader" /\ phase[i] = "Prepare"
     THEN /\ buffered_proposals' = [buffered_proposals EXCEPT ![i] = Append(@, e)]
          /\ UNCHANGED <<durable, role, phase, accepted_idx, current_seq_num,
                         cached_promise_message, leader_state, chan>>
     ELSE IF role[i] = "Leader" /\ phase[i] = "Accept"
     THEN /\ AcceptEntriesLeader(i, <<e>>, chan)
          /\ UNCHANGED <<promise, n_accepted, decided_idx, role, phase, current_seq_num,
                         cached_promise_message, buffered_proposals>>
     ELSE /\ ForwardProposals(i, <<e>>, chan)
          /\ UNCHANGED <<durable, role, phase, accepted_idx, current_seq_num,
                         cached_promise_message, leader_state>>
  /\ UNCHANGED <<env, recovery, rolled_back, readvars, buffered_stopsign, reconfig>>
  /\ GhostUpdate

\* Raft 6.3: the client registers while it has no session.
ClientRegister(i) ==
  /\ client.session = 0
  /\ ClientSend(i, RegisterClientEntry)

\* VR Revisited 4.1: the client sends its current request-number again
\* until it has the reply.
ClientRequest(i) ==
  /\ client.session > 0
  /\ client.request_number <= MaxRequest
  /\ ClientSend(i, RequestEntry(client.session, client.request_number))

\* Raft 6.3: the reply to RegisterClient carries the session ID s.
ClientRegistered(j, s) ==
  /\ ClientSessions
  /\ Votes(j)
  /\ client.session = 0
  /\ s <= Len(DecidedPrefix(j))
  /\ DecidedPrefix(j)[s] = RegisterClientEntry
  /\ ClientTable(DecidedPrefix(j))[s].open
  /\ client' = [client EXCEPT !.session = s, !.request_number = 1]
  /\ UNCHANGED <<durable, volatile, chan, env, ghost, recovery, rollback, readvars,
                 buffered_stopsign, reconfig>>

\* VR Revisited 4.1: node j replies with the result of the request; the
\* client takes the next request-number.
ClientReplied(j) ==
  /\ ClientSessions
  /\ Votes(j)
  /\ client.session > 0
  /\ client.request_number <= MaxRequest
  /\ LET t == ClientTable(DecidedPrefix(j))[client.session] IN
       t.open /\ t.request_number >= client.request_number
  /\ client' = [client EXCEPT !.request_number = @ + 1]
  /\ UNCHANGED <<durable, volatile, chan, env, ghost, recovery, rollback, readvars,
                 buffered_stopsign, reconfig>>

\* Raft 6.3: node j replies SESSION_EXPIRED; the client registers again.
ClientSessionExpired(j) ==
  /\ ClientSessions
  /\ Votes(j)
  /\ client.session > 0
  /\ client.session <= Len(DecidedPrefix(j))
  /\ ~ClientTable(DecidedPrefix(j))[client.session].open
  /\ client' = [client EXCEPT !.session = 0, !.request_number = 1]
  /\ UNCHANGED <<durable, volatile, chan, env, ghost, recovery, rollback, readvars,
                 buffered_stopsign, reconfig>>

\* Raft 6.3: the leader appends an entry that expires session s. Owner
\* decision: this follows a heartbeat timeout of the client. The timeout is
\* a free choice here.
ExpireSession(i, s) ==
  /\ ClientSessions
  /\ ~recovering[i]
  /\ ~read_only[i]
  /\ role[i] = "Leader" /\ phase[i] = "Accept"
  /\ ~AcceptedReconfiguration(i)
  /\ s <= Len(DecidedPrefix(i))
  /\ ClientTable(DecidedPrefix(i))[s].open
  /\ \A k \in 1..Len(log[i]) : log[i][k] # ExpireSessionEntry(s)
  /\ proposed' = proposed \cup {ExpireSessionEntry(s)}
  /\ AcceptEntriesLeader(i, <<ExpireSessionEntry(s)>>, chan)
  /\ UNCHANGED <<promise, n_accepted, decided_idx, role, phase, current_seq_num,
                 cached_promise_message, buffered_proposals>>
  /\ UNCHANGED <<env, recovery, rolled_back, client, readvars, buffered_stopsign, reconfig>>
  /\ GhostUpdate

----------------------------------------------------------------------------
(* Read index actions, run (e). *)

\* @type: Seq($entry);
DecidedNew ==
  LET ns == {DecidedPrefix(s) : s \in NextConfigNodes \cap running}
  IN IF force_single.node = 0 \/ ns = {} THEN <<>>
     ELSE CHOOSE l \in ns : \A x \in ns : Len(x) <= Len(l)

\* Raft 6.4.1
ReadStart(r, i) ==
  LET \* @type: Bool => $read;
      started(ok) == [NoRead EXCEPT !.state = "pending", !.node = i,
                                    !.decided_at_start = decided_history,
                                    !.decided_new_at_start = DecidedNew,
                                    !.contract_stale_ok = ok]
      ld          == BallotPid(promise[i])
  IN
  /\ ReadIndex
  /\ ~recovering[i]
  /\ read_only[i] \/ ~deactivated[i]
  /\ reads[r].state = "none"
  /\ SingleNode /\ ClusterNodes(i) = {i} => SingleNodeServes(i) \/ Mutation = "ReadStartBeforeServing"
  /\ \E ok \in IF StaleReadNode THEN BOOLEAN ELSE {FALSE} :
     IF read_only[i] \/ (ok /\ ~InStage2(i))
     THEN /\ reads' = [reads EXCEPT ![r] = [started(ok) EXCEPT !.state = "done",
                                                               !.result = DecidedPrefix(i),
                                                               !.stale = TRUE]]
          /\ UNCHANGED <<chan, read_pending>>
     ELSE IF Mutation = "LocalRead" \/ SingleNodeServes(i)
     THEN /\ reads' = [reads EXCEPT ![r] = [started(ok) EXCEPT !.state = "done",
                                                               !.result = DecidedPrefix(i)]]
          /\ UNCHANGED <<chan, read_pending>>
     ELSE IF role[i] = "Leader"
     THEN /\ reads' = [reads EXCEPT ![r] = started(ok)]
          /\ read_pending' = [read_pending EXCEPT ![i] = @ \cup {r}]
          /\ UNCHANGED chan
     ELSE /\ ld > 0 /\ ld # i
          /\ reads' = [reads EXCEPT ![r] = started(ok)]
          /\ chan' = Send(chan, i, One(ld, [EmptyMessage EXCEPT !.type = "ReadIndex",
                                                               !.read = r]))
          /\ UNCHANGED read_pending
  /\ UNCHANGED <<durable, volatile, env, ghost, recovery, rollback, client, read_round,
                 buffered_stopsign, reconfig>>

\* Raft 6.4 and FoundationDB GetReadVersion: the leader takes all waiting
\* reads into one round (a batch). The read index is its accepted_idx at the
\* start of the round. The round sends a heartbeat to each peer.
StartReadRound(i) ==
  LET n  == leader_state[i].n_leader
      id == read_round[i].id + 1
  IN
  /\ ReadIndex
  /\ ~recovering[i]
  /\ role[i] = "Leader" /\ phase[i] = "Accept"
  /\ read_pending[i] # {}
  /\ read_round[i].reads = {} \/ read_round[i].n # n
  /\ read_round' = [read_round EXCEPT ![i] =
                      [n |-> n, id |-> id, reads |-> read_pending[i],
                       read_index |-> accepted_idx[i],
                       acks |-> IF Votes(i) THEN {i} ELSE {}]]
  /\ read_pending' = [read_pending EXCEPT ![i] = {}]
  /\ chan' = Send(chan, i, [k \in Peers(i) |->
                <<[EmptyMessage EXCEPT !.type = "Heartbeat", !.n = n, !.id = id]>>])
  /\ UNCHANGED <<durable, volatile, env, ghost, recovery, rollback, client, reads,
                 buffered_stopsign, reconfig>>

\* Raft 6.4: the node answers read r from its own log once its decided index
\* has reached the read index.
ReadReply(r) ==
  LET i == reads[r].node IN
  /\ ReadIndex
  /\ reads[r].state = "indexed"
  /\ decided_idx[i] >= reads[r].read_index
  /\ \/ Mutation = "ReadBeforeServing"
     \/ /\ reads[r].crashes = crashes
        /\ reads[r].rolled_back = rolled_back
        /\ reads[r].ballot = promise[i]
  /\ reads' = [reads EXCEPT ![r].state = "done", ![r].result = DecidedPrefix(i)]
  /\ UNCHANGED <<durable, volatile, chan, env, ghost, recovery, rollback, client,
                 read_pending, read_round, buffered_stopsign, reconfig>>

----------------------------------------------------------------------------
\* @type: Int => Bool;
Reconfigure(i) ==
  /\ i \in Nodes
  /\ reconfigurations < MaxReconfigure
  /\ reconfigurations' = reconfigurations + 1
  /\ ~recovering[i]
  /\ ~read_only[i]
  /\ ~deactivated[i]
  /\ ~AcceptedReconfiguration(i)
  /\ proposed' = proposed \cup {StopSignEntry}
  /\ IF role[i] = "Leader" /\ phase[i] = "Prepare"
     THEN /\ buffered_stopsign' = [buffered_stopsign EXCEPT ![i] = <<StopSignEntry>>]
          /\ UNCHANGED <<durable, volatile, chan>>
     ELSE IF role[i] = "Leader" /\ phase[i] = "Accept"
     THEN /\ AcceptStopSignLeader(i, chan)
          /\ UNCHANGED <<promise, n_accepted, decided_idx, role, phase, current_seq_num,
                         cached_promise_message, buffered_proposals, buffered_stopsign>>
     ELSE /\ ForwardStopSign(i, chan)
          /\ UNCHANGED <<durable, volatile>>
  /\ UNCHANGED <<env, recovery, rolled_back, client, readvars, running, first_log, force_single,
                 read_only, deactivated, halts, read_only_entries, stage1_ghost,
                 build_source>>
  /\ GhostUpdate

\* @type: (Int, Int) => Bool;
BuildForServer(i, j) ==
  LET l == DecidedPrefix(j) IN
  /\ i \in NextConfigNodes \ running
  /\ j \in Nodes
  /\ (j \in running /\ ~recovering[j]) \/ Mutation = "BuildFromRecovering"
  /\ Votes(j)
  /\ ~read_only[j] /\ ~deactivated[j]
  /\ IsReconfigured(j) \/ Mutation = "BuildBeforeStopSignDecided"
  /\ running' = running \cup {i}
  /\ log' = [log EXCEPT ![i] = l]
  /\ decided_idx' = [decided_idx EXCEPT ![i] = Len(l)]
  /\ accepted_idx' = [accepted_idx EXCEPT ![i] = Len(l)]
  /\ first_log' = [first_log EXCEPT ![i] = l]
  /\ build_source' = [build_source EXCEPT ![i] = j]
  /\ UNCHANGED <<promise, n_accepted, role, phase, current_seq_num, cached_promise_message,
                 buffered_proposals, leader_state, chan, env, proposed, recovery, rolled_back,
                 client, readvars, buffered_stopsign, reconfigurations, force_single,
                 read_only, deactivated, halts, read_only_entries, stage1_ghost>>
  /\ GhostUpdate

----------------------------------------------------------------------------
(* Single-node mode actions, run (d). *)

\* leader.rs:257-285 handle_majority_promises
\* @type: Int => Bool;
LeaderCountsOwnPromise(i) ==
  /\ SingleNode
  /\ ClusterNodes(i) = {i}
  /\ ~deactivated[i] \/ Mutation = "DeactivatedStillVotes"
  /\ ~read_only[i]
  /\ ~recovering[i]
  /\ role[i] = "Leader" /\ phase[i] = "Prepare"
  /\ Cardinality(Promised(leader_state[i])) >= Majority(i)
  /\ HandleMajorityPromises(i, leader_state[i], chan)
  /\ UNCHANGED <<env, proposed, recovery, rolled_back, client, readvars, reconfig>>
  /\ GhostUpdate

\* leader.rs:316-345 handle_accepted
\* @type: Int => Bool;
LeaderCountsOwnAccepted(i) ==
  LET own == leader_state[i].accepted_indexes[i] IN
  /\ SingleNode
  /\ ClusterNodes(i) = {i}
  /\ ~deactivated[i] \/ Mutation = "DeactivatedStillVotes"
  /\ ~read_only[i]
  /\ ~recovering[i]
  /\ Votes(i)
  /\ role[i] = "Leader" /\ phase[i] = "Accept"
  /\ own > decided_idx[i]
  /\ IsChosen(leader_state[i], own)
  /\ HandleAccepted(i, i, [EmptyMessage EXCEPT !.type = "Accepted",
                           !.n = leader_state[i].n_leader, !.accepted_idx = own], chan)
  /\ UNCHANGED <<env, proposed, recovery, rolled_back, client, readvars,
                 buffered_stopsign, reconfig>>
  /\ GhostUpdate

\* @type: (Int, Int) => Bool;
ForceSingle(i, s) ==
  LET l == CASE Mutation = "ForceWithoutLog"  -> <<>>
             [] Mutation = "ForceDecidedOnly" -> DecidedPrefix(i)
             [] OTHER                         -> log[i]
  IN
  /\ SingleNode
  /\ MaxForceSingle > 0
  /\ force_single.node = 0
  /\ i \in Nodes /\ s \in NextConfigNodes \ running
  /\ Votes(i) /\ ~recovering[i]
  /\ read_only[i] \/ Mutation = "ForceWithoutStage1"
  /\ Cardinality(Nodes \ running) >= Majority(i) \/ Mutation = "OldNodeVotesAfterForce"
  /\ force_single' = [node |-> i, log |-> log[i], decided |-> DecidedPrefix(i),
                      history |-> decided_history]
  /\ running' = (running \ {i}) \cup {s}
  /\ log' = [log EXCEPT ![s] = l]
  /\ decided_idx' = [decided_idx EXCEPT ![s] = Min(decided_idx[i], Len(l))]
  /\ accepted_idx' = [accepted_idx EXCEPT ![s] = Len(l)]
  /\ first_log' = [first_log EXCEPT ![s] = l]
  /\ UNCHANGED <<promise, n_accepted, role, phase, current_seq_num, cached_promise_message,
                 buffered_proposals, leader_state, chan, env, proposed, recovery, rolled_back,
                 client, readvars, buffered_stopsign, reconfigurations, read_only, deactivated,
                 halts, read_only_entries, stage1_ghost, build_source>>
  /\ GhostUpdate

\* @type: (Int, Int) => Bool;
SendDeactivated(s, k) ==
  /\ InStage2(s)
  /\ k \in Nodes \ {force_single.node}
  /\ chan[<<s, k>>] = <<>>
  /\ chan' = Send(chan, s, One(k, [EmptyMessage EXCEPT !.type = "Deactivated"]))
  /\ UNCHANGED <<durable, volatile, env, ghost, recovery, rollback, client, readvars,
                 buffered_stopsign, reconfig>>

\* @type: Int => Bool;
EnterReadOnly(i) ==
  LET l == DecidedPrefix(i)
      d == IF Mutation = "ReadOnlyTruncates"
           THEN [promise |-> promise[i], n_accepted |-> n_accepted[i], log |-> l,
                 decided_idx |-> Len(l)]
           ELSE DurableOf(i)
  IN
  /\ SingleNode
  /\ i \in Nodes
  /\ Votes(i)
  /\ ~read_only[i]
  /\ read_only_entries < MaxReadOnly
  /\ read_only_entries' = read_only_entries + 1
  /\ read_only' = [read_only EXCEPT ![i] = TRUE]
  /\ Restart({i}, [k \in {i} |-> d], {i}, Recovery)
  /\ UNCHANGED <<wiped, crashes, wipes, losses, duplicates, timeouts, elections, proposed,
                 rolled_back, client, running, first_log, reconfigurations, force_single,
                 deactivated, halts, stage1_ghost, build_source>>
  /\ GhostUpdate

\* @type: Int => Bool;
LeaveReadOnly(i) ==
  /\ read_only[i]
  /\ force_single.node = 0 \/ Mutation = "ForceAtTwoNodes"
  /\ read_only' = [read_only EXCEPT ![i] = FALSE]
  /\ stage1_ghost' = [stage1_ghost EXCEPT ![i].leave_decided = Len(decided_history)]
  /\ Restart({i}, [k \in {i} |-> DurableOf(k)], {k \in {i} : deactivated[k]}, Recovery)
  /\ UNCHANGED <<wiped, crashes, wipes, losses, duplicates, timeouts, elections, proposed,
                 rolled_back, client, running, first_log, reconfigurations, force_single,
                 deactivated, halts, read_only_entries, build_source>>
  /\ GhostUpdate

\* @type: Int => Bool;
Halt(i) ==
  /\ i \in Nodes
  /\ halts < MaxHalt
  /\ halts' = halts + 1
  /\ running' = running \ {i}
  /\ stage1_ghost' = [k \in Nodes |-> IF k # i /\ read_only[k]
                                       THEN [stage1_ghost[k] EXCEPT !.down = @ \cup {i}]
                                       ELSE stage1_ghost[k]]
  /\ UNCHANGED <<durable, volatile, chan, env, ghost, recovery, rollback, client, readvars,
                 buffered_stopsign, first_log, reconfigurations, force_single, read_only,
                 deactivated, read_only_entries, build_source>>

\* @type: Int => Bool;
Resume(i) ==
  /\ i \in Nodes \ running
  /\ i # force_single.node
  /\ running' = running \cup {i}
  /\ Restart({i}, [k \in {i} |-> DurableOf(k)], {k \in {i} : Quiet(k)},
             Recovery /\ Mutation # "ResumeWithoutRecovery")
  /\ UNCHANGED <<wiped, crashes, wipes, losses, duplicates, timeouts, elections, proposed,
                 rolled_back, client, first_log, reconfigurations, force_single, read_only,
                 deactivated, halts, read_only_entries, stage1_ghost, build_source>>
  /\ GhostUpdate

----------------------------------------------------------------------------
Next ==
  \/ \E p \in Pairs : Receive(p[1], p[2])
  \/ \E i \in running, k \in 1..MaxBallot : HandleLeader(i, k)
  \/ \E i \in running, v \in Values : Propose(i, v)
  \/ \E i \in running : Crash(i)
  \/ \E i \in running : Wipe(i)
  \/ \E R \in SUBSET running : Rollback(R)
  \/ \E i \in running : ResendMessageTimeout(i)
  \/ \E p \in Pairs, k \in 1..Cap : k <= Len(chan[p]) /\ Lose(p, k)
  \/ \E p \in Pairs, k \in 1..Cap : k <= Len(chan[p]) /\ Duplicate(p, k)
  \/ \E i \in running : ClientRegister(i) \/ ClientRequest(i)
  \/ \E j \in running, s \in 1..MaxLog : ClientRegistered(j, s)
  \/ \E j \in running : ClientReplied(j) \/ ClientSessionExpired(j)
  \/ \E i \in running, s \in 1..MaxLog : ExpireSession(i, s)
  \/ \E r \in 1..MaxRead, i \in running : ReadStart(r, i)
  \/ \E i \in running : StartReadRound(i)
  \/ \E r \in 1..MaxRead : ReadReply(r)
  \/ \E i \in running : Reconfigure(i)
  \/ \E i \in NextConfigNodes \ running, j \in Nodes : BuildForServer(i, j)
  \/ \E i \in running : LeaderCountsOwnPromise(i) \/ LeaderCountsOwnAccepted(i)
  \/ \E i \in running, s \in NextConfigNodes \ running : ForceSingle(i, s)
  \/ \E i \in running : EnterReadOnly(i) \/ LeaveReadOnly(i)
  \/ \E s \in running, k \in Nodes : SendDeactivated(s, k)
  \/ \E i \in running : Halt(i)
  \/ \E i \in Nodes \ running : Resume(i)

Spec == Init /\ [][Next]_vars

ASSUME MaxRollback < Cardinality(Nodes)

\* Mutations for the checks of the checks, plan section 11. "none" is the
\* model. Each other value puts one fault into the model:
\* PromiseWithoutAccepted: a Promise reports no accepted state (target S1).
\* SyncIgnored: a new leader does not apply the log_sync of the Promises (S2b).
\* LeaderReplacesValue: a leader in Accept can append a value other than the
\* proposed one (S3).
\* SeqNumUnchecked: a follower takes each AcceptDecide and Decide as Expected (S5).
\* RequestNumberUnchecked: the state machine executes a request of an open
\* session without a check of its request-number (E1).
\* LocalRead: a node in the cluster answers a read at once from its own log,
\* with no round (R1).
\* ForceWithoutLog: ForceSingle starts the new node with an empty log
\* (ForceKeepsDecided).
\* ForceDecidedOnly: ForceSingle copies only the decided prefix of the old
\* node (S2Force).
\* OldNodeVotesAfterForce: ForceSingle runs while a majority of the old
\* configuration is up (S1Running).
\* ReadOnlyProposes: a node in stage 1 appends a proposal to its own log
\* (ReadOnlyWritesNothing).
\* ReadOnlyTruncates: EnterReadOnly cuts the log of the node to its decided
\* prefix (ReadOnlyLosesNothing).
\* DeactivatedStillVotes: a node with deactivated handles each message as
\* before (DeactivatedNodeIsSilent).
\* ResumeWithoutRecovery: an old node that resumes skips Recovery (NoTwoWriters).
\* BuildFromRecovering: BuildForServer takes a source that is halted or
\* recovering (S6).
\* ForceWithoutStage1: ForceSingle runs at a node that is not in stage 1
\* (NoTwoWriters).
\* ForceAtTwoNodes: a node leaves stage 1 after stage 2 at another node
\* (NoTwoWriters).
\* ReadBeforeServing: ReadReply answers after a crash, a rollback or a new
\* ballot since the index step (R1).
\* StopSignLost: a wipe clears deactivated (DeactivatedStays).
\* ReadStartBeforeServing: a leader of a configuration with one node starts
\* a read before it serves (ReadStartOnlyWhenServing).
ASSUME Mutation \in {"none", "PromiseWithoutAccepted", "SyncIgnored",
                     "LeaderReplacesValue", "SeqNumUnchecked",
                     "RequestNumberUnchecked", "LocalRead",
                     "AppendAfterStopSign", "BuildBeforeStopSignDecided",
                     "ForceWithoutLog", "ForceDecidedOnly", "OldNodeVotesAfterForce",
                     "ReadOnlyProposes", "ReadOnlyTruncates",
                     "DeactivatedStillVotes", "ResumeWithoutRecovery",
                     "BuildFromRecovering", "ForceWithoutStage1", "ForceAtTwoNodes",
                     "ReadBeforeServing", "StopSignLost", "ReadStartBeforeServing"}
\* omni_paxos.rs:106 validate
ASSUME SingleNode \in BOOLEAN
ASSUME ~SingleNode => Cardinality(NextConfigNodes) # 1
ASSUME MaxForceSingle \in {0, 1}
ASSUME MaxForceSingle > 0 => SingleNode /\ Cardinality(NextConfigNodes) = 1
ASSUME MaxReadOnly \in Nat
ASSUME MaxHalt \in Nat
ASSUME StaleReadNode \in BOOLEAN
ASSUME ClientSessions \in BOOLEAN
ASSUME ReadIndex \in BOOLEAN
ASSUME NextConfigNodes \cap Nodes = {}
ASSUME \A i \in Servers : 0 < i /\ i < PidBase
ASSUME MaxReconfigure \in Nat

----------------------------------------------------------------------------
(* Bounds of the model, used as a state constraint. *)
Bounds ==
  /\ \A i \in Servers : Len(log[i]) <= MaxLog /\ Len(buffered_proposals[i]) <= MaxLog
  /\ \A i, k \in Servers : leader_state[i].follower_seq_nums[k].counter <= MaxCounter
                         /\ leader_state[i].follower_seq_nums[k].session <= MaxSession

----------------------------------------------------------------------------
(* Properties *)

\* internal_storage.rs:302-311: the cached accepted_idx is the log length,
\* follower.rs:140: decided index <= accepted index.
AcceptedIdxIsLogLength ==
  \A i \in Servers : accepted_idx[i] = Len(log[i]) /\ decided_idx[i] <= accepted_idx[i]

S1 == \A i, j \in Servers : Votes(i) /\ Votes(j) => Compatible(DecidedPrefix(i), DecidedPrefix(j))

S2 == \A i \in Servers : Votes(i) => IsPrefix(DecidedPrefix(i), decided_history)

S2Grows == [][IsPrefix(decided_history, decided_history')]_vars

\* A leader in (Leader, Accept) with ballot b holds every entry of
\* decided_history that was decided at a ballot <= b.
S2b == \A i \in Servers :
         role[i] = "Leader" /\ phase[i] = "Accept" =>
           \A k \in 1..Len(decided_history) :
             decided_ballot[k] <= leader_state[i].n_leader
             /\ ConfigurationId(BallotPid(decided_ballot[k])) = ConfigurationId(i) =>
               IsPrefix(SubSeq(decided_history, 1, k), log[i])

S3 == /\ \A i \in Servers : \A k \in 1..Len(DecidedPrefix(i)) : DecidedPrefix(i)[k] \in proposed
      /\ \A k \in 1..Len(decided_history) : decided_history[k] \in proposed

\* @type: Bool;
S4 == \A i \in Nodes :
        Votes(i) =>
          \A k \in 1..Len(DecidedPrefix(i)) :
            DecidedPrefix(i)[k] = StopSignEntry => k = Len(DecidedPrefix(i))

S5 == \A i, j \in Servers :
        n_accepted[i] = n_accepted[j] /\ n_accepted[i] # DefaultBallot
          => Compatible(log[i], log[j])

\* @type: Bool;
S6 == \A i \in NextConfigNodes \cap running :
        /\ \A j \in Nodes : Votes(j) => IsPrefix(DecidedPrefix(j), first_log[i])
        /\ \A k \in 1..Len(decided_history) :
             ConfigurationId(BallotPid(decided_ballot[k])) = 1 =>
               k <= Len(first_log[i]) /\ first_log[i][k] = decided_history[k]

\* E1, run (f): each (session, request_number) is executed at most once.
\* @type: Seq($entry) => Bool;
ExecutedOnce(p) == \A a, b \in DOMAIN p : a # b /\ Executed(p, a) /\ Executed(p, b) => p[a] # p[b]
E1 == /\ ExecutedOnce(decided_history)
      /\ \A i \in Servers : Votes(i) => ExecutedOnce(DecidedPrefix(i))

\* R1, run (e): a read sees each entry that was decided before the read started.
\* A read with the mark stale is outside R1.
R1 == \A r \in DOMAIN reads :
        reads[r].state = "done" /\ ~reads[r].stale
          => IsPrefix(reads[r].decided_at_start, reads[r].result)

\* Run (d), case 2. No entry is lost where the log of the old node held all
\* of decided_history at the time of ForceSingle.
\* @type: Bool;
NoForceLoss == force_single.node = 0 \/ IsPrefix(force_single.history, force_single.log)

\* @type: Bool;
S1Running == \A i, j \in running :
               Votes(i) /\ Votes(j) => Compatible(DecidedPrefix(i), DecidedPrefix(j))

\* Run (d), case 2 with a loss. Among the new configuration, S1, S2 and R1
\* hold after stage 2: decided prefixes agree, each starts with the log of
\* the forced node, and a read sees each entry decided in the new
\* configuration before it started.
\* @type: Bool;
S1New == \A i, j \in NextConfigNodes \cap running :
           force_single.node # 0 => Compatible(DecidedPrefix(i), DecidedPrefix(j))

\* @type: Bool;
S2New == \A s \in NextConfigNodes \cap running :
           force_single.node # 0 => Compatible(first_log[s], DecidedPrefix(s))

\* @type: Bool;
R1New == \A r \in DOMAIN reads :
           reads[r].state = "done" /\ ~reads[r].stale /\ reads[r].node \in NextConfigNodes
             => IsPrefix(reads[r].decided_new_at_start, reads[r].result)

\* @type: Bool;
ForceRetiresOldNodes == force_single.node \notin running

\* @type: Bool;
ForceKeepsDecided == force_single.node # 0 =>
                       \A s \in NextConfigNodes \cap running :
                         IsPrefix(force_single.decided, DecidedPrefix(s))

\* @type: Bool;
S1Force == NoForceLoss => S1Running

\* @type: Bool;
S2Force == NoForceLoss => S2

\* @type: Bool;
R1Force == NoForceLoss => R1

\* Run (d), case 1. A check of the checks: run d1_reach must violate it, so
\* that the configuration with one node decides a new entry.
\* @type: Bool;
SingleNodeDecidesNothing == \A s \in NextConfigNodes : decided_idx[s] <= Len(first_log[s])

\* The warning of plan item 29: where accepted_idx of the old node is at
\* least the highest decided index, nothing is lost.
\* @type: Bool;
ForceWarning == force_single.node # 0 /\ Len(force_single.history) <= Len(force_single.log)
                  => NoForceLoss

\* @type: Int => Bool;
SendsNothing(i) ==
  duplicates' = duplicates => \A p \in Pairs : p[1] = i => Len(chan'[p]) <= Len(chan[p])

ReadOnlyWritesNothing ==
  [][\A i \in Servers :
       read_only[i] /\ read_only'[i] /\ wipes' = wipes /\ rolled_back' = rolled_back
         => DurableOfP(i) = DurableOf(i) /\ SendsNothing(i)]_vars

DeactivatedNodeIsSilent ==
  [][\A i \in Nodes :
       /\ deactivated'[i] => role'[i] = "Follower"
       /\ deactivated[i] => SendsNothing(i)]_vars

ForceOnlyFromStage1 ==
  [][force_single.node = 0 /\ force_single'.node # 0 => read_only[force_single'.node]]_vars

ReadOnlyEndsBeforeForce ==
  [][\A i \in Nodes : read_only[i] /\ ~read_only'[i] => force_single.node = 0]_vars

DeactivatedKeepsDurable ==
  [][\A i \in Nodes :
       deactivated[i] /\ wipes' = wipes /\ rolled_back' = rolled_back
         => DurableOfP(i) = DurableOf(i)]_vars

BuildOnlyFromRunning ==
  [][\A s \in NextConfigNodes :
       build_source'[s] # build_source[s] =>
         LET j == build_source'[s] IN j \in running /\ ~recovering[j]]_vars

ReplyOnlyIfUnchanged ==
  [][\A r \in DOMAIN reads :
       reads[r].state = "indexed" /\ reads'[r].state = "done" =>
         LET i == reads[r].node IN
         /\ reads[r].crashes = crashes
         /\ reads[r].rolled_back = rolled_back
         /\ reads[r].ballot = promise[i]]_vars

ReadStartOnlyWhenServing ==
  [][\A r \in DOMAIN reads :
       reads[r].state = "none" /\ reads'[r].state # "none" =>
         LET i == reads'[r].node IN
         SingleNode /\ ClusterNodes(i) = {i} => SingleNodeServes(i)]_vars

DeactivatedStays == [][\A i \in Servers : deactivated[i] => deactivated'[i]]_vars

\* @type: Bool;
NoTwoWriters ==
  force_single.node # 0 =>
    ~ /\ \E s \in NextConfigNodes : decided_idx[s] > Len(first_log[s])
      /\ \E k \in Nodes : Votes(k) /\ decided_idx[k] > Len(force_single.history)

\* @type: Bool;
ClusterDecidesNothingAfterStage1 ==
  \A k \in Nodes :
    ~ /\ stage1_ghost[k].leave_decided >= 0
      /\ Cardinality(stage1_ghost[k].down) >= Cardinality(Nodes) \div 2 + 1
      /\ Nodes \subseteq running
      /\ decided_idx[k] > stage1_ghost[k].leave_decided

\* @type: Bool;
EachReadSeesDecided == \A r \in DOMAIN reads :
                         reads[r].state = "done" /\ ~reads[r].stale
                           => IsPrefix(reads[r].decided_at_start, reads[r].result)

\* @type: Bool;
StaleReadIsMarked == \A r \in DOMAIN reads :
                       reads[r].state = "done"
                       /\ ~IsPrefix(reads[r].decided_at_start, reads[r].result)
                         => reads[r].stale

\* Stage 1 is reversible: S2 holds while nodes crash and come back with
\* their durable state and one node is in stage 1.
\* @type: Bool;
ReadOnlyLosesNothing == S1 /\ S2

S7 == [][\A i \in Servers : ~wiped'[i] => decided_idx'[i] >= decided_idx[i]]_vars

ValueSymmetry == Permutations(Values)

(* State view for TLC. It drops values that no action reads in the current  *)
(* role and phase: leader_state except n_leader of a Follower, the cached   *)
(* Promise outside (Follower, Prepare), current_seq_num outside            *)
(* (Follower, Accept).                                                      *)
View ==
  <<durable, role, phase, accepted_idx,
    [i \in Servers |-> IF role[i] = "Follower" /\ phase[i] = "Accept"
                     THEN current_seq_num[i] ELSE DefaultSeq],
    [i \in Servers |-> IF role[i] = "Follower" /\ phase[i] = "Prepare"
                     THEN cached_promise_message[i] ELSE <<>>],
    buffered_proposals,
    [i \in Servers |-> IF role[i] = "Leader" THEN leader_state[i]
                     ELSE LeaderStateWith(leader_state[i].n_leader)],
    chan, env, ghost,
    recovering, nonce,
    [i \in Servers |-> IF recovering[i] THEN recovery_responses[i] ELSE {}],
    rollback, client, readvars, buffered_stopsign, reconfig>>
=============================================================================
