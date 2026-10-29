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
  \*   log: Seq($entry), read: Int, id: Int, reads: Set(Int), read_index: Int,
  \*   recovering: Bool, last: Seq($entry)};
  \* @typeAlias: promiseMeta = {state: Str, n_accepted: Int, accepted_idx: Int, decided_idx: Int,
  \*   last: Seq($entry)};
  \* @typeAlias: maxMeta = {n_accepted: Int, accepted_idx: Int, decided_idx: Int, pid: Int,
  \*   last: Seq($entry)};
  \* @typeAlias: leaderState = {n_leader: Int, promises_meta: Int -> $promiseMeta,
  \*   follower_seq_nums: Int -> $seqNum, accepted_indexes: Int -> Int,
  \*   max_promise_meta: $maxMeta, max_promise_sync: Seq($logSync)};
  \* @typeAlias: durable = {promise: Int, n_accepted: Int, log: Seq($entry), decided_idx: Int};
  \* @typeAlias: response = {from: Int, promise: Int, primary: Bool, n_accepted: Int,
  \*   log: Seq($entry), decided_idx: Int, recovering: Bool};
  \* @typeAlias: read = {state: Str, node: Int, decided_at_start: Seq($entry),
  \*   read_index: Int, result: Seq($entry), stale: Bool, contract_stale_ok: Bool,
  \*   crashes: Int, rolled_back: Set(Int), ballot: Int, decided_new_at_start: Seq($entry),
  \*   forced_decided: Bool};
  \* @typeAlias: round = {n: Int, id: Int, reads: Set(Int), read_index: Int, acks: Set(Int)};
  \* @typeAlias: mark = {promise: Int, n_accepted: Int, acc: Int, decided: Int};
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
  \* @type: Bool;
  PowerLoss,
  \* @type: Str;
  Mutation,
  \* @type: Int;
  MaxStorageFault,
  \* @type: Bool;
  StartChecks,
  \* @type: Bool;
  RunningFaults,
  \* @type: Bool;
  StepChecks,
  \* @type: Bool;
  VerifyCrc,
  ChainCheck,
  MetaRecord,
  PeerCheck,
  \* @type: Bool;
  HighWater,
  \* @type: Bool;
  RepairFromPeer,
  \* @type: Bool;
  VerifyPromise,
  \* @type: Str;
  MismatchAction,
  \* @type: Bool;
  StateInLmdb,
  \* @type: Int;
  MaxInFlight,
  \* @type: Str;
  StateRepair,
  \* @type: Bool;
  StartDamageCounts,
  \* @type: Set(Str);
  StorageFaultKind,
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
  StaleReadNode,
  \* @type: Bool;
  Witness,
  \* @type: Int;
  LeaseLength,
  \* @type: Bool;
  ExecuteVerify,
  \* @type: Int;
  MaxWrong,
  \* @type: Bool;
  Spread,
  \* @type: Bool;
  Flip,
  \* @type: Int;
  MaxFlip,
  \* @type: Int;
  MaxResultThrow,
  \* @type: Str;
  SpreadAction

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
  \* @type: Int;
  storage_faults,
  \* @type: Set(Int);
  refused,
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
  power_loss_completed,
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
  build_source,
  \* @type: Int;
  witness_epoch,
  \* @type: Int -> Int;
  node_epoch,
  \* @type: Int -> Int;
  lease_request,
  \* @type: Int -> Int;
  lease_reply,
  \* @type: Int -> Int;
  lease_remaining,
  \* @type: Int;
  wait_remaining,
  \* @type: Int;
  forced_decided_at_force,
  \* @type: Bool;
  forced_has_decided,
  \* @type: Int;
  old_decided_max,
  \* @type: Bool;
  old_decided_after_forced,
  \* @type: Int -> (Int -> {n: Int, idx: Int});
  peer_seen,
  \* @type: Int -> $mark;
  high_water,
  \* @type: Int -> Int;
  repairs,
  \* @type: Int -> Seq($entry);
  applied,
  \* @type: Int -> Seq($entry);
  in_txn,
  selected,
  counts,
  own,
  \* @type: Int -> Int;
  verified_idx,
  \* @type: Int -> Set({kind: Str, idx: Int, n: Int});
  proof,
  \* @type: Int -> Int;
  open_mark,
  \* @type: Int -> Bool;
  mark_covered,
  result_throws,
  \* @type: Int -> {n: Int, ptr: Int};
  answered,
  \* @type: Int -> {n: Int, idx: Int};
  refusal,
  \* @type: Set(<<Int, $entry>>);
  wrong,
  runs,
  \* @type: Set(<<Int, Int, Int, Int>>);
  flips,
  reported

durable  == <<promise, n_accepted, log, decided_idx>>
volatile == <<role, phase, accepted_idx, current_seq_num,
              cached_promise_message, buffered_proposals, leader_state>>
env      == <<wiped, crashes, wipes, losses, duplicates, timeouts, elections,
              storage_faults, refused>>
ghost    == <<proposed, decided_history, decided_ballot>>
recovery == <<recovering, nonce, recovery_responses, power_loss_completed>>
rollback == <<rolled_back, past_durable, peer_seen, high_water, repairs>>
readvars == <<reads, read_pending, read_round>>
reconfig == <<running, first_log, reconfigurations, force_single, read_only, deactivated,
              halts, read_only_entries, stage1_ghost, build_source>>
leasevars  == <<witness_epoch, node_epoch, lease_request, lease_reply, lease_remaining,
                wait_remaining>>
leaseghost == <<forced_decided_at_force, forced_has_decided, old_decided_max,
                old_decided_after_forced>>
statevars == <<applied, in_txn>>
evvars    == <<selected, counts, own, verified_idx, proof, open_mark, mark_covered,
               result_throws, answered, refusal, wrong, runs, flips, reported>>
vars_but_state == <<durable, volatile, chan, env, ghost, recovery, rollback, client, readvars,
                    buffered_stopsign, reconfig, leasevars, leaseghost>>
vars     == <<durable, volatile, chan, env, ghost, recovery, rollback, client, readvars,
              buffered_stopsign, reconfig, leasevars, leaseghost, statevars, evvars>>

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
NotPromised == [state |-> "NotPromised", n_accepted |-> 0, accepted_idx |-> 0, decided_idx |-> 0,
                last |-> <<>>]
\* @type: $promiseMeta;
PromisedHigher == [state |-> "PromisedHigher", n_accepted |-> 0, accepted_idx |-> 0, decided_idx |-> 0,
                   last |-> <<>>]
\* @type: $maxMeta;
DefaultMeta == [n_accepted |-> 0, accepted_idx |-> 0, decided_idx |-> 0, pid |-> 0, last |-> <<>>]

\* Each message has every field, so that all messages have one type. A field
\* that a message type does not carry keeps its value of EmptyMessage. The
\* log_sync of an AcceptSync is a sequence of one LogSync; the log_sync of a
\* Promise is a sequence of at most one (Option<LogSync>).
\* @type: $msg;
EmptyMessage ==
  [type |-> "", n |-> DefaultBallot, decided_idx |-> 0, n_accepted |-> DefaultBallot,
   accepted_idx |-> 0, log_sync |-> <<>>, seq_num |-> DefaultSeq, entries |-> <<>>,
   nonce |-> 0, promise |-> DefaultBallot, primary |-> FALSE, log |-> <<>>,
   read |-> 0, id |-> 0, reads |-> {}, read_index |-> 0, recovering |-> FALSE, last |-> <<>>,
   ptr |-> 0, base |-> <<>>, results |-> <<>>, idx |-> 0]

\* The log up to position k. It stands for the digest of the state up to k,
\* as each node reads it back after the commit (owner, 2026-10-24 15:30 to
\* 15:35, doc/plan-lmdb-only-source.md). A log shorter than k gives itself,
\* which equals no prefix of length k.
\* @type: (Seq($entry), Int) => Seq($entry);
PrefixAt(l, k) == IF k <= Len(l) THEN SubSeq(l, 1, k) ELSE l

\* With VerifyCrc, a node raises its decided index to d only when its own log
\* up to d equals the digest in the message. Else it keeps the old index.
\* @type: (Seq($entry), Int, Int, $msg) => Int;
CheckedDecided(lg, old, d, m) ==
  IF VerifyCrc /\ d > old /\ ~(d <= Len(m.last) /\ PrefixAt(lg, d) = SubSeq(m.last, 1, d))
  THEN old ELSE d

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
                                         decided_idx |-> meta.decided_idx,
                                         last |-> meta.last]]

\* util.rs:252-259 is_chosen
\* @type: ($leaderState, Int) => Bool;
IsChosen(ls, idx) ==
  Cardinality({k \in Servers : ls.accepted_indexes[k] >= idx}) >= Majority(BallotPid(ls.n_leader))

\* util.rs:168-177 get_max_decided_idx
\* @type: $leaderState => Int;
MaxDecided(ls) ==
  LET ds == {ls.promises_meta[k].decided_idx : k \in Promised(ls)}
  IN IF ds = {} THEN 0 ELSE CHOOSE d \in ds : \A e \in ds : d >= e

\* VerifyPromise (doc/plan-lmdb-only-source.md 2.5 item 4). The leader takes
\* the decided index of a promise only when the promise carries the same
\* prefix up to that index as the synced log of the leader. With no such
\* promise, it keeps its own decided index.
\* @type: ($leaderState, Seq($entry), Int) => Int;
MaxDecidedVerified(ls, lg, kept) ==
  LET ds == {ls.promises_meta[k].decided_idx :
               k \in {x \in Promised(ls) :
                      LET d == ls.promises_meta[x].decided_idx
                      IN d <= Len(lg) /\ PrefixAt(lg, d) = ls.promises_meta[x].last}}
  IN IF ds = {} THEN kept ELSE CHOOSE d \in ds : \A e \in ds : d >= e

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
      !.seq_num = AccSyncSeq(ls, to), !.decided_idx = dec, !.last = PrefixAt(lg, dec),
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

(* ExecuteVerify (doc/plan-execute-verify.md section 4). Names from the    *)
(* plan, all provisional: Results, Rejected, answer, try_again, refused,    *)
(* verified_idx, proof, open mark. A result is a record [rv, j]: rv is the  *)
(* value of the result, j its jitter. Two results agree when their values   *)
(* are equal and their jitters differ by at most 1 (plan 2.2.2, 4.2). The   *)
(* value of a right run of entry e on base b is RightRv(e, b); it holds the *)
(* values of the base, so a run on another base gives another value. The    *)
(* value of a wrong run of node i is BadRv(i, e). With ExecuteVerify FALSE  *)
(* each new action is disabled, each new variable keeps its initial value,  *)
(* and each changed expression is the old one.                              *)
\* @type: Seq($entry);
EmptyFn == [k \in {} |-> 0]
VoidEntry == [type |-> "Void", session |-> 0, request_number |-> 0]
ResultsEntry(f, t, lst) == [type |-> "Results", session |-> 0, request_number |-> 0,
                            from |-> f, to |-> t, list |-> lst]
\* The plan gives rejected(k) no other field. A follower cannot check 2.7.3
\* without the counted answers, so the model carries them (provisional).
RejectedEntry(k, cnt) == [type |-> "Rejected", session |-> 0, request_number |-> 0,
                          from |-> k, to |-> k, list |-> cnt]
EVMessages == {"Answer", "TryAgain", "Refused"}

IsVerdict(e) == e \notin Values /\ e.type \in {"Results", "Rejected"}
IsRequest(e) == e \in Values \/ e.type \in {"RegisterClient", "Request", "ExpireSession"}

RightRv(e, b) == [v |-> e, on |-> [n \in 1..Len(b) |-> b[n][2].rv], bad |-> 0]
BadRv(i, e) == [v |-> e, on |-> <<>>, bad |-> i]
NoResult == [rv |-> [v |-> VoidEntry, on |-> <<>>, bad |-> 0], j |-> 0]
Agree(a, b) == a.rv = b.rv /\ a.j - b.j \in -1..1
Jitters == IF Spread THEN 0..2 ELSE 0..1

\* The results that a run of node i on entry e and base b can give, with the
\* ghost w of the wrong pairs after the run.
RunResults(i, e, b, w) ==
  {[rv |-> RightRv(e, b), j |-> x] : x \in Jitters}
  \cup (IF <<i, e>> \in w THEN {[rv |-> BadRv(i, e), j |-> 0]} ELSE {})

\* A run may add the pair <<i, e>> while fewer than a majority of the nodes
\* hold a pair for e and fewer than MaxWrong pairs exist (plan 4.2).
WrongAfter(i, e, w) ==
  {w} \cup (IF /\ <<i, e>> \notin w
              /\ Cardinality(w) < MaxWrong
              /\ Cardinality({n \in Servers : <<n, e>> \in w}) + 1 < Majority(i)
           THEN {w \cup {<<i, e>>}} ELSE {})

\* @type: Int => Seq($entry);
DecidedPrefix(i) == SubSeq(log[i], 1, Min(decided_idx[i], Len(log[i])))

\* The verdict function V of plan 2.3.1 over a sequence of entries.
NoChoice == [p |-> 0, chosen |-> EmptyFn, void |-> {}]
HasRequest(l, f, t) == \E q \in f..t : q \in 1..Len(l) /\ IsRequest(l[q])
\* 2.6.5: the first request entry of l above p and below m, else 0.
FirstRequestAbove(l, p, m) ==
  LET rs == {q \in (p + 1)..(m - 1) : q \in 1..Len(l) /\ IsRequest(l[q])}
  IN IF rs = {} THEN 0 ELSE CHOOSE q \in rs : \A x \in rs : q <= x
VStep(l, m, a) ==
  LET e == l[m] IN
  IF ~IsVerdict(e) THEN a
  ELSE IF /\ e.type = "Results"
          /\ \/ e.from = a.p + 1
             \/ Mutation = "StaleVerdictTaken" /\ e.from <= a.p + 1
          /\ e.to < m /\ HasRequest(l, e.from, e.to)
  THEN [a EXCEPT !.chosen = e.list @@ @, !.p = Max(@, e.to)]
  ELSE IF e.type = "Rejected" /\ e.from = FirstRequestAbove(l, a.p, m)
  THEN [a EXCEPT !.void = @ \cup {e.from}, !.p = e.from]
  ELSE a
RECURSIVE VFrom(_, _, _)
VFrom(l, m, a) == IF m > Len(l) THEN a ELSE VFrom(l, m + 1, VStep(l, m, a))
V(l) == VFrom(l, 1, NoChoice)

\* Plan 2.3.3: the highest index of l such that every request entry at or
\* below it is at or below p. Mutation AppliedIsPointer takes p.
AppliedIdxOf(l) ==
  LET a  == V(l)
      rs == {q \in (a.p + 1)..Len(l) : IsRequest(l[q])}
  IN IF Mutation = "AppliedIsPointer" THEN a.p
     ELSE IF rs = {} THEN Len(l)
     ELSE (CHOOSE q \in rs : \A x \in rs : q <= x) - 1

\* Plan 4.9: the effective prefix, with each void entry kept as a no-op.
EffPrefix(l) ==
  LET a == V(l) IN
  [k \in 1..AppliedIdxOf(l) |-> IF k \in a.void THEN VoidEntry ELSE l[k]]

\* The prefix that the state machine and the client read.
\* @type: Int => Seq($entry);
ClientPrefix(i) == IF ExecuteVerify THEN EffPrefix(DecidedPrefix(i)) ELSE DecidedPrefix(i)

\* The indexes below q that hold a request entry that is not void, in order.
RECURSIVE ReqIdx(_, _, _)
ReqIdx(l, vd, q) ==
  IF q <= 1 THEN <<>>
  ELSE ReqIdx(l, vd, q - 1) \o (IF IsRequest(l[q - 1]) /\ (q - 1) \notin vd THEN <<q - 1>> ELSE <<>>)

\* The base of index q of l (plan 2.4.1): each request entry below q that is
\* not void, with its result: chosen at or below p, res(k) above p.
BaseWith(l, a, q, res(_)) ==
  LET ks == ReqIdx(l, a.void, q) IN
  [n \in 1..Len(ks) |-> <<l[ks[n]], IF ks[n] <= a.p THEN a.chosen[ks[n]].res ELSE res(ks[n])>>]

\* Plan 2.7.4: verified_idx passes each entry that is not a verdict entry.
VerifiedAfter(lg, v) ==
  LET ws == {w \in v..Len(lg) : \A k \in (v + 1)..w : ~IsVerdict(lg[k])}
  IN CHOOSE w \in ws : \A x \in ws : w >= x
CommonPrefixLen(a, b) ==
  CHOOSE k \in 0..Min(Len(a), Len(b)) :
    /\ SubSeq(a, 1, k) = SubSeq(b, 1, k)
    /\ k = Min(Len(a), Len(b)) \/ a[k + 1] # b[k + 1]
\* The verified index of node i after its log becomes lg.
NewVerified(i, lg) == VerifiedAfter(lg, Min(verified_idx[i], CommonPrefixLen(log[i], lg)))
\* An accepted index that node i sends or counts for itself (plan 2.7.4).
AccBound(i, lg, acc) == IF ExecuteVerify THEN Min(acc, NewVerified(i, lg)) ELSE acc
\* Plan 2.7.5: a promise carries the log only up to verified_idx.
PromiseLog(i) == IF ExecuteVerify THEN SubSeq(log[i], 1, Min(verified_idx[i], Len(log[i]))) ELSE log[i]
PromiseAcc(i) == IF ExecuteVerify THEN Len(PromiseLog(i)) ELSE accepted_idx[i]
\* Plan 2.4.4: a leader proposes no more request entries while it holds
\* MaxInFlight results above its applied index.
InFlightOk(i) ==
  IF ~ExecuteVerify \/ ~(role[i] = "Leader" /\ phase[i] = "Accept") THEN TRUE
  ELSE Cardinality({k \in DOMAIN own[i] : k > AppliedIdxOf(DecidedPrefix(i))}) < MaxInFlight
\* Plan 2.3.6: a StopSign only when p covers every request entry.
StopSignAllowedOn(i, lg) ==
  IF ~ExecuteVerify THEN TRUE
  ELSE \A k \in 1..Len(lg) : IsRequest(lg[k]) => k <= V(DecidedPrefix(i)).p
StopSignAllowed(i) == StopSignAllowedOn(i, log[i])
\* plan-reconfiguration.md 2.3: the leader checks every StopSign it appends.
LeaderChecksStopSign(i, lg) == StopSignAllowedOn(i, lg) \/ Mutation = "StopSignUnchecked"

\* The state of a decided prefix: one item for each index up to the applied
\* index, with the chosen result of each request entry that is not void.
StateItems(l) ==
  LET a == V(l) IN
  [k \in 1..AppliedIdxOf(l) |->
     [e |-> IF k \in a.void THEN VoidEntry ELSE l[k],
      r |-> IF IsRequest(l[k]) /\ k \notin a.void THEN a.chosen[k].res ELSE NoResult]]

NoAnswer == [ptr |-> -1, map |-> EmptyFn]
NoCounts == [ans |-> [j \in Servers |-> NoAnswer], tried |-> {}, again |-> {},
             refusals |-> {}, appended |-> {}]
NoAnswered == [n |-> -1, ptr |-> -1]
NoRefusal == [n |-> -1, idx |-> 0]

(* Recovery of Viewstamped Replication Revisited (Liskov, Cowling,         *)
(* MIT-CSAIL-TR-2012-021, section 4.3), applied to Sequence Paxos (plan     *)
(* item 26). Names from that paper: Recovery, RecoveryResponse, nonce. A    *)
(* node in (Leader, Accept) is in status normal and is the primary of its   *)
(* ballot. Only the primary sends its log, as in the paper. The log is sent *)
(* whole: a suffix from index 0.                                            *)
\* The kinds of RunningStorageFault that StorageFault at a start does not take.
RunningOnlyKinds == {"stale_meta", "rollback"}

\* The number of automatic repairs of a node before the operator acts
\* (owner, 2026-10-24 20:55).
AutoRepairs == 1

NothingSeen == [n |-> DefaultBallot, idx |-> 0]

(* HighWater (owner, 2026-10-24 19:51: the node keeps the highest index    *)
(* that it saw). The state machine of a node keeps, in memory, the highest *)
(* promise, the highest pair of accepted round and log length, and the     *)
(* highest decided index that its own transactions read or wrote. It is a  *)
(* check, not a source: a step reads LMDB, and a value below the mark is a  *)
(* damage. A restart forgets the mark. Provisional names: HighWater, mark.  *)
\* @type: $mark;
NoMark == [promise |-> DefaultBallot, n_accepted |-> DefaultBallot, acc |-> 0, decided |-> 0]
\* @type: $durable => $mark;
Mark(d) == [promise |-> d.promise, n_accepted |-> d.n_accepted, acc |-> Len(d.log),
            decided |-> d.decided_idx]
\* @type: ($mark, $mark) => Bool;
BelowMark(m, h) ==
  \/ m.promise < h.promise
  \/ m.decided < h.decided
  \/ m.n_accepted < h.n_accepted
  \/ m.n_accepted = h.n_accepted /\ m.acc < h.acc
\* @type: ($mark, $mark) => $mark;
MaxMark(h, m) ==
  [promise |-> Max(h.promise, m.promise), decided |-> Max(h.decided, m.decided),
   n_accepted |-> IF BelowMark([h EXCEPT !.promise = 0, !.decided = 0],
                               [m EXCEPT !.promise = 0, !.decided = 0])
                  THEN m.n_accepted ELSE h.n_accepted,
   acc |-> IF BelowMark([h EXCEPT !.promise = 0, !.decided = 0],
                        [m EXCEPT !.promise = 0, !.decided = 0])
           THEN m.acc ELSE h.acc]

\* The messages whose field n is a ballot that the sender holds or held.
BallotTypes == {"Prepare", "Promise", "AcceptSync", "AcceptDecide", "Accepted", "Decide"}

Raised(seen, m) ==
  IF m.type \notin BallotTypes THEN seen
  ELSE IF m.n > seen.n
       THEN [n |-> m.n, idx |-> IF m.type = "Accepted" THEN m.accepted_idx ELSE 0]
  ELSE IF m.n = seen.n /\ m.type = "Accepted" /\ m.accepted_idx > seen.idx
       THEN [seen EXCEPT !.idx = m.accepted_idx]
  ELSE seen

\* A peer sends a lower ballot, or a lower accepted index in the same
\* ballot, than it sent before: its disk went back.
SteppedBack(i, j, m) ==
  /\ PeerCheck
  /\ m.type \in BallotTypes
  /\ \/ m.n < peer_seen[i][j].n
     \/ m.n = peer_seen[i][j].n /\ m.type = "Accepted" /\ m.accepted_idx < peer_seen[i][j].idx

\* @type: Int => $msg;
RecoveryMsg(x) == [EmptyMessage EXCEPT !.type = "Recovery", !.nonce = x]

\* @type: (Int, Int) => $msg;
RecoveryResponseMsg(k, x) ==
  LET primary == role[k] = "Leader" /\ phase[k] = "Accept"
      full    == primary \/ PowerLoss IN
  [EmptyMessage EXCEPT !.type = "RecoveryResponse", !.nonce = x, !.promise = promise[k],
   !.primary = primary,
   !.n_accepted = IF full THEN n_accepted[k] ELSE DefaultBallot,
   !.log = IF full THEN log[k] ELSE <<>>,
   !.decided_idx = IF full THEN decided_idx[k] ELSE 0,
   !.recovering = PowerLoss /\ recovering[k]]

\* The answers are complete with f + 1 = Majority answers from other nodes
\* that do not recover, one of them from the primary of the highest ballot
\* among these answers.
\* @type: Set($response) => Set($response);
RecoveryPrimaries(rs) ==
  LET mp == CHOOSE p \in {r.promise : r \in rs} : \A r \in rs : p >= r.promise
  IN {r \in rs : r.primary /\ r.promise = mp /\ BallotPid(mp) = r.from}
\* @type: (Set($response), Int) => Bool;
RecoveryComplete(rs, i) ==
  LET normal == {r \in rs : ~r.recovering}
  IN Cardinality(normal) >= Majority(i) /\ RecoveryPrimaries(normal) # {}

(* Recovery after a restart of every node (owner, 2026-10-19 and           *)
(* 2026-10-21; plan model/plan-power-loss.md; provisional name PowerLoss).  *)
(* With an answer from every peer and no primary among them, the node with  *)
(* the highest (n_accepted, accepted_idx) among the answers and the node    *)
(* itself is the source, the rule by which Prepare/Promise of Omni-Paxos    *)
(* picks a log. Mutation PowerLossWithF completes with f answers.           *)
\* @type: (Set($response), Int) => Bool;
PowerLossComplete(rs, i) ==
  /\ PowerLoss
  /\ \A r \in rs : ~r.primary
  /\ IF Mutation = "PowerLossWithF"
     THEN Cardinality(rs) >= Cardinality(ClusterNodes(i)) - Majority(i)
     ELSE {r.from : r \in rs} = Peers(i)
\* @type: (Set($response), Int) => $response;
PowerLossSource(rs, i) ==
  LET self == [from |-> i, promise |-> promise[i], primary |-> FALSE, n_accepted |-> n_accepted[i],
               log |-> log[i], decided_idx |-> decided_idx[i], recovering |-> TRUE]
      all  == rs \cup {self}
  IN CHOOSE x \in all : \A y \in all :
       \/ x.n_accepted > y.n_accepted
       \/ x.n_accepted = y.n_accepted /\ Len(x.log) > Len(y.log)
       \/ x.n_accepted = y.n_accepted /\ Len(x.log) = Len(y.log)
          /\ (x.from = i \/ (y.from # i /\ x.from <= y.from))

(* The code keeps the entries before the start index and takes the rest  *)
(* from the source (doc/plan-power-loss-code.md): from its accepted_idx   *)
(* with the same n_accepted, else from its decided_idx, the rule of        *)
(* follower.rs:13-51. The model takes the whole log of the source; this   *)
(* states that both give the same log. It holds on both paths: the answer *)
(* of a primary and power loss (plan model/plan-recovery-delta.md). The   *)
(* mutations PrimaryStartAtAccepted and PowerLossStartAtAccepted always   *)
(* start at accepted_idx on their path.                                   *)
\* @type: (Int, $response, Bool) => Bool;
RecoveryKeptPrefix(i, l, atAccepted) ==
  LET start == IF atAccepted \/ n_accepted[i] = l.n_accepted
               THEN accepted_idx[i] ELSE decided_idx[i]
  IN start <= Len(l.log) /\ SubSeq(log[i], 1, start) = SubSeq(l.log, 1, start)

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
           decided_new_at_start |-> <<>>, forced_decided |-> FALSE]
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

(* The lease at the witness, model/plan-witness-lease.md. Names: lease       *)
(* (Gray and Cheriton, SOSP 1989), epoch (Chubby, ZooKeeper), witness       *)
(* (Paris, 1986). The witness holds one durable value, witness_epoch, and   *)
(* changes it only in ForceSingle. The epoch of a node comes from its       *)
(* configuration. Time is kept as remaining durations: Tick lowers each     *)
(* positive duration by one. lease_request[i] is the time that remains of   *)
(* a lease measured from the request, -1 with no request. lease_reply[i] is *)
(* -1 with no reply, 0 for a refusal, 1 for a grant. lease_remaining[i] is  *)
(* the volatile lease of a node of the old cluster. wait_remaining is -1    *)
(* before ForceSingle, then the time that the forced node still waits. With *)
(* Witness FALSE each new action is disabled, each new variable keeps its   *)
(* initial value, and each changed guard is TRUE.                           *)
\* @type: Int => Bool;
LeaseValid(i) == i \notin Nodes \/ ~Witness \/ lease_remaining[i] > 0

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
  /\ peer_seen = [i \in Servers |-> [j \in Servers |-> NothingSeen]]
  /\ high_water = [i \in Servers |-> NoMark]
  /\ repairs = [i \in Servers |-> 0]
  /\ applied = [i \in Servers |-> <<>>]
  /\ in_txn = [i \in Servers |-> <<>>]
  /\ wiped = [i \in Servers |-> FALSE]
  /\ crashes = 0
  /\ wipes = 0
  /\ losses = 0
  /\ duplicates = 0
  /\ timeouts = 0
  /\ elections = 0
  /\ storage_faults = 0
  /\ refused = {}
  /\ proposed = {}
  /\ decided_history = <<>>
  /\ decided_ballot = [k \in 1..MaxLog |-> DefaultBallot]
  /\ recovering = [i \in Servers |-> FALSE]
  /\ nonce = [i \in Servers |-> 0]
  /\ recovery_responses = [i \in Servers |-> {}]
  /\ power_loss_completed = {}
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
  /\ witness_epoch = 0
  /\ node_epoch = [i \in Servers |-> 0]
  /\ lease_request = [i \in Nodes |-> -1]
  /\ lease_reply = [i \in Nodes |-> -1]
  /\ lease_remaining = [i \in Nodes |-> 0]
  /\ wait_remaining = -1
  /\ forced_decided_at_force = 0
  /\ forced_has_decided = FALSE
  /\ old_decided_max = 0
  /\ old_decided_after_forced = FALSE
  /\ selected = [i \in Servers |-> EmptyFn]
  /\ counts = [i \in Servers |-> NoCounts]
  /\ own = [i \in Servers |-> EmptyFn]
  /\ verified_idx = [i \in Servers |-> 0]
  /\ proof = [i \in Servers |-> {}]
  /\ open_mark = [i \in Servers |-> 0]
  /\ mark_covered = [i \in Servers |-> FALSE]
  /\ result_throws = <<>>
  /\ answered = [i \in Servers |-> NoAnswered]
  /\ refusal = [i \in Servers |-> NoRefusal]
  /\ wrong = {}
  /\ runs = {}
  /\ flips = {}
  /\ reported = {}

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
      own_acc == IF Mutation = "OwnAcceptUnchecked" THEN acc ELSE AccBound(i, log[i] \o es, acc)
      ls0 == [leader_state[i] EXCEPT !.accepted_indexes[i] = IF Votes(i) THEN own_acc ELSE @]
      fs  == PromisedFollowers(ls0)
      ls1 == NextSeqNums(ls0, fs)
  IN /\ log' = [log EXCEPT ![i] = @ \o es]
     /\ accepted_idx' = [accepted_idx EXCEPT ![i] = acc]
     /\ leader_state' = [leader_state EXCEPT ![i] = ls1]
     /\ chan' = Send(c, i, [k \in fs |->
                  <<[EmptyMessage EXCEPT !.type = "AcceptDecide", !.n = ls1.n_leader,
                     !.seq_num = ls1.follower_seq_nums[k],
                     !.decided_idx = decided_idx[i], !.entries = es,
                     !.last = PrefixAt(log[i], decided_idx[i])]>>])

\* @type: (Int, $seqNum) => $msg;
AcceptStopSignMsg(n, s) == [EmptyMessage EXCEPT !.type = "AcceptStopSign", !.n = n, !.seq_num = s]

\* @type: (Int, <<Int, Int>> -> Seq($msg)) => Bool;
AcceptStopSignLeader(i, c) ==
  LET acc == accepted_idx[i] + 1
      ls0 == [leader_state[i] EXCEPT !.accepted_indexes[i] =
                IF Votes(i) THEN AccBound(i, Append(log[i], StopSignEntry), acc) ELSE @]
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
           acc == PromiseAcc(i)
           ls  == IF na > m.n_accepted
                  THEN <<[suffix |-> Suffix(PromiseLog(i), m.decided_idx), sync_idx |-> m.decided_idx]>>
                  ELSE IF na = m.n_accepted /\ acc > m.accepted_idx
                  THEN <<[suffix |-> Suffix(PromiseLog(i), m.accepted_idx), sync_idx |-> m.accepted_idx]>>
                  ELSE <<>>
           blind == Mutation = "PromiseWithoutAccepted"
           pr  == [EmptyMessage EXCEPT !.type = "Promise", !.n = m.n,
                   !.n_accepted = IF blind THEN DefaultBallot ELSE na,
                   !.decided_idx = decided_idx[i],
                   !.last = IF VerifyPromise THEN PrefixAt(log[i], decided_idx[i]) ELSE <<>>,
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
      skip == sync = <<>> \/ Mutation = "SyncIgnored"
      lg1  == IF skip THEN PromiseLog(i)
              ELSE AppendOnPrefix(PromiseLog(i), sync[1].sync_idx, sync[1].suffix)
      acc1 == IF skip THEN PromiseAcc(i)
              ELSE sync[1].sync_idx + Len(sync[1].suffix)
      stop == i \in Nodes /\ HasStopSign(lg1)
      bp   == IF stop THEN <<>> ELSE buffered_proposals[i]
      bss  == IF stop \/ ~LeaderChecksStopSign(i, lg1 \o bp) THEN <<>> ELSE buffered_stopsign[i]
      lg2  == lg1 \o bp \o bss
      acc2 == acc1 + Len(bp) + Len(bss)
      dec  == IF VerifyPromise THEN MaxDecidedVerified(ls1, lg1, decided_idx[i])
              ELSE MaxDecided(ls1)
      ls2  == [ls1 EXCEPT !.max_promise_sync = <<>>,
                          !.accepted_indexes[i] = IF Votes(i) THEN AccBound(i, lg2, acc2) ELSE @]
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
               decided_idx |-> m.decided_idx, pid |-> j, last |-> m.last]
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
           ab  == AccBound(i, lg, s.sync_idx + Len(s.suffix))
           acc == [EmptyMessage EXCEPT !.type = "Accepted", !.n = m.n,
                                       !.accepted_idx = ab,
                                       !.last = PrefixAt(lg, ab)]
           out == [k \in (IF fwd THEN {ld} ELSE {}) \cup (IF Votes(i) THEN {j} ELSE {}) |->
                     (IF fwd /\ k = ld
                      THEN <<[EmptyMessage EXCEPT !.type = "ProposalForward", !.entries = bp]>>
                      ELSE <<>>)
                     \o (IF Votes(i) /\ k = j THEN <<acc>> ELSE <<>>)]
       IN /\ n_accepted' = [n_accepted EXCEPT ![i] = m.n]
          /\ decided_idx' = [decided_idx EXCEPT ![i] =
                               CheckedDecided(lg, @, IF DecidedIdxMonotone THEN Max(@, m.decided_idx)
                                                       ELSE m.decided_idx, m)]
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
               /\ decided_idx' = [decided_idx EXCEPT ![i] =
                                    CheckedDecided(log[i] \o m.entries, @, ud.dec, m)]
               /\ current_seq_num' = [current_seq_num EXCEPT ![i] = m.seq_num]
               /\ chan' = Send(c, i, IF rep THEN IfVotes(i, BallotPid(m.n),
                              [EmptyMessage EXCEPT !.type = "Accepted", !.n = m.n,
                                                   !.accepted_idx = AccBound(i, log[i] \o m.entries, acc),
                                                   !.last = PrefixAt(log[i] \o m.entries,
                                                                     AccBound(i, log[i] \o m.entries, acc))])
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
            IN /\ decided_idx' = [decided_idx EXCEPT ![i] = CheckedDecided(log[i], @, ud.dec, m)]
               /\ current_seq_num' = [current_seq_num EXCEPT ![i] = m.seq_num]
               /\ chan' = Send(c, i, IF ud.flushed THEN IfVotes(i, BallotPid(m.n),
                              [EmptyMessage EXCEPT !.type = "Accepted", !.n = m.n,
                                                   !.accepted_idx = AccBound(i, log[i], accepted_idx[i]),
                                                   !.last = PrefixAt(log[i], AccBound(i, log[i], accepted_idx[i]))])
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
                                                   !.accepted_idx = AccBound(i, lg, Len(lg)),
                                                   !.last = PrefixAt(lg, AccBound(i, lg, Len(lg)))]))
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
  ELSE IF role[i] = "Leader" /\ phase[i] = "Accept" /\ ~LeaderChecksStopSign(i, log[i])
  THEN /\ chan' = c
       /\ UNCHANGED <<durable, volatile, buffered_stopsign>>
  ELSE IF role[i] = "Leader" /\ phase[i] = "Accept"
  THEN /\ AcceptStopSignLeader(i, c)
       /\ UNCHANGED <<promise, n_accepted, decided_idx, role, phase, current_seq_num,
                      cached_promise_message, buffered_proposals, buffered_stopsign>>
  ELSE /\ ForwardStopSign(i, c)
       /\ UNCHANGED <<durable, volatile>>

\* leader.rs:316-345 handle_accepted, leader.rs:240-255 send_decide
\* @type: (Int, Int, $msg, <<Int, Int>> -> Seq($msg)) => Bool;
HandleAccepted(i, j, m, c) ==
  IF /\ m.n = leader_state[i].n_leader /\ role[i] = "Leader" /\ phase[i] = "Accept"
     /\ VerifyCrc => PrefixAt(log[i], m.accepted_idx) = m.last
  THEN LET ls1 == [leader_state[i] EXCEPT !.accepted_indexes[j] = m.accepted_idx] IN
       IF /\ m.accepted_idx > decided_idx[i]
          /\ IsChosen(ls1, m.accepted_idx)
          /\ LeaseValid(i) \/ Mutation = "DecideWithoutLease"
       THEN LET fs  == PromisedFollowers(ls1)
                ls2 == NextSeqNums(ls1, fs)
            IN /\ decided_idx' = [decided_idx EXCEPT ![i] = m.accepted_idx]
               /\ leader_state' = [leader_state EXCEPT ![i] = ls2]
               /\ chan' = Send(c, i, [k \in fs |->
                            <<[EmptyMessage EXCEPT !.type = "Decide", !.n = ls2.n_leader,
                               !.seq_num = ls2.follower_seq_nums[k],
                               !.decided_idx = m.accepted_idx,
                               !.last = PrefixAt(log[i], m.accepted_idx)]>>])
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
                  n_accepted |-> m.n_accepted, log |-> m.log, decided_idx |-> m.decided_idx,
                  recovering |-> m.recovering]
           rs == {x \in recovery_responses[i] : x.from # j} \cup {r}
           normal == RecoveryComplete(rs, i)
       IN IF normal \/ PowerLossComplete(rs, i)
          THEN LET l  == IF normal
                         THEN CHOOSE x \in RecoveryPrimaries({y \in rs : ~y.recovering}) : TRUE
                         ELSE PowerLossSource(rs, i)
                   pr == IF normal THEN Max(promise[i], l.promise)
                         ELSE CHOOSE p \in {promise[i]} \cup {x.promise : x \in rs} :
                                \A x \in rs : p >= promise[i] /\ p >= x.promise
                   di == IF DecidedIdxMonotone THEN Max(decided_idx[i], l.decided_idx)
                         ELSE l.decided_idx
               IN /\ IF normal
                     THEN Assert(RecoveryKeptPrefix(i, l, Mutation = "PrimaryStartAtAccepted"),
                                 "RecoveryKeptPrefix after the answer of a primary")
                     ELSE Assert(RecoveryKeptPrefix(i, l, Mutation = "PowerLossStartAtAccepted"),
                                 "RecoveryKeptPrefix after a power loss")
                  /\ promise' = [promise EXCEPT ![i] = pr]
                  /\ n_accepted' = [n_accepted EXCEPT ![i] = l.n_accepted]
                  /\ log' = [log EXCEPT ![i] = l.log]
                  /\ decided_idx' = [decided_idx EXCEPT ![i] = di]
                  /\ role' = [role EXCEPT ![i] = "Follower"]
                  /\ phase' = [phase EXCEPT ![i] = "Recover"]
                  /\ accepted_idx' = [accepted_idx EXCEPT ![i] = Len(l.log)]
                  /\ current_seq_num' = [current_seq_num EXCEPT ![i] = DefaultSeq]
                  /\ cached_promise_message' = [cached_promise_message EXCEPT ![i] = <<>>]
                  /\ buffered_proposals' = [buffered_proposals EXCEPT ![i] = <<>>]
                  /\ leader_state' = [leader_state EXCEPT ![i] = LeaderStateWith(pr)]
                  /\ recovering' = [recovering EXCEPT ![i] = FALSE]
                  /\ recovery_responses' = [recovery_responses EXCEPT ![i] = {}]
                  /\ power_loss_completed' =
                       IF ~normal /\ \A k \in ClusterNodes(i) : recovering[k]
                       THEN power_loss_completed \cup {i} ELSE power_loss_completed
                  /\ chan' = Send(c, i, [k \in Peers(i) |-> <<PrepareReqMsg(pr)>>])
                  /\ UNCHANGED nonce
          ELSE /\ recovery_responses' = [recovery_responses EXCEPT ![i] = rs]
               /\ chan' = c
               /\ UNCHANGED <<durable, volatile, recovering, nonce, power_loss_completed>>

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
      mine == {r \in rd.reads : reads[r].node = i}
      dst  == {reads[r].node : r \in rd.reads} \ {i}
  IN IF ~ok
     THEN /\ chan' = c
          /\ UNCHANGED <<durable, volatile, readvars>>
     ELSE IF Cardinality(acks) < Majority(i)
     THEN /\ read_round' = [read_round EXCEPT ![i].acks = acks]
          /\ chan' = c
          /\ UNCHANGED <<durable, volatile, reads, read_pending>>
     ELSE /\ reads' = [r \in DOMAIN reads |->
                         IF r \in mine
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
       IF (MaxRollback > 0 \/ RunningOnlyKinds \cap StorageFaultKind # {}) /\ rolled_back' = {}
       THEN IF DurableOfP(k) # DurableOf(k) THEN past_durable[k] \cup {DurableOf(k)}
            ELSE past_durable[k]
       ELSE {}]

(* The ghosts of the lease. old_decided_max is the highest decided_idx that *)
(* a node of the old cluster ever held. forced_decided_at_force is the      *)
(* decided_idx of the forced node after the step of ForceSingle.            *)
(* forced_has_decided is set when the forced node decides above that value  *)
(* after ForceSingle. old_decided_after_forced is set when old_decided_max  *)
(* rises in a state where forced_has_decided is TRUE. Each condition reads  *)
(* force_single and forced_decided_at_force in the state before the step,   *)
(* so the step of ForceSingle itself does not count.                        *)
LeaseGhostUpdate ==
  IF ~Witness
  THEN UNCHANGED leaseghost
  ELSE LET \* @type: Set(Int);
           ds  == {old_decided_max} \cup {decided_idx'[k] : k \in Nodes}
           odm == CHOOSE d \in ds : \A e \in ds : d >= e
       IN /\ old_decided_max' = odm
          /\ forced_has_decided' =
               \/ forced_has_decided
               \/ /\ force_single.node # 0
                  /\ \E s \in NextConfigNodes : decided_idx'[s] > forced_decided_at_force
          /\ old_decided_after_forced' =
               old_decided_after_forced \/ (forced_has_decided /\ odm > old_decided_max)
          /\ forced_decided_at_force' =
               IF force_single.node = 0 /\ force_single'.node # 0
               THEN decided_idx'[CHOOSE s \in NextConfigNodes : s \in running']
               ELSE forced_decided_at_force

(* PeerCheck (owner, 2026-10-24 19:19; doc/plan-lmdb-only-source.md 2.3   *)
(* item 6). Each node keeps, in memory, the highest ballot that a peer      *)
(* sent it, and the highest accepted index of an Accepted of that ballot.  *)
(* A received message is the head of a channel that shrinks by one; with   *)
(* no loss and no duplicate, nothing else removes one head alone. A node   *)
(* that leaves running forgets what it saw.                                *)
PeerSeenUpdate ==
  peer_seen' =
    [i \in Servers |-> [j \in Servers |->
       IF ~PeerCheck THEN peer_seen[i][j]
       ELSE IF ~(i \in running /\ i \in running') THEN NothingSeen
       ELSE IF <<j, i>> \in Pairs /\ chan[<<j, i>>] # <<>>
               /\ chan'[<<j, i>>] = Tail(chan[<<j, i>>])
       THEN Raised(peer_seen[i][j], Head(chan[<<j, i>>]))
       ELSE peer_seen[i][j]]]

\* The nodes of R restart in the step and forget their mark.
\* @type: Set(Int) => Bool;
HighWaterUpdate(R) ==
  high_water' =
    [i \in Servers |->
       IF ~HighWater THEN high_water[i]
       ELSE IF i \in R \/ ~(i \in running /\ i \in running') THEN Mark(DurableOfP(i))
       ELSE MaxMark(high_water[i], Mark(DurableOfP(i)))]

\* @type: Set(Int) => Bool;
\* The nodes of P count one more automatic repair. The count is durable.
\* @type: (Set(Int), Set(Int)) => Bool;
GhostUpdateRP(R, P) ==
  /\ HistoryUpdate /\ PastUpdate /\ LeaseGhostUpdate /\ PeerSeenUpdate /\ HighWaterUpdate(R)
  /\ repairs' = [k \in Servers |-> IF k \in P THEN repairs[k] + 1 ELSE repairs[k]]
\* @type: Set(Int) => Bool;
GhostUpdateR(R) == GhostUpdateRP(R, {})
GhostUpdate == GhostUpdateR({})

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
  ELSE IF recovering[i] /\ ~(PowerLoss /\ m.type = "Recovery")
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

\* MismatchAction (doc/plan-lmdb-only-source.md 2.5 item 3). The head of
\* the channel from j to i would raise the decided index of i to d, and the
\* prefix of i up to d differs from the one in the message. "keep": the
\* handler keeps the old decided index (CheckedDecided). "damage": the node
\* takes it as a damage (DamageFound). "stop": the node leaves running.
\* @type: (Int, Int) => Bool;
Mismatch(i, j) ==
  /\ VerifyCrc
  /\ i \in running /\ ~recovering[i] /\ ~deactivated[i] /\ ~SingleNodeMode(i)
  /\ chan[<<j, i>>] # <<>>
  /\ LET m  == Head(chan[<<j, i>>])
         ok == ~SteppedBack(i, j, m) /\ CheckValidBallot(i, m.n).valid
         old == decided_idx[i]
         Rej(lg, d) == d > old /\ CheckedDecided(lg, old, d, m) # d
     IN /\ ok
        /\ \/ /\ m.type = "AcceptSync" /\ role[i] = "Follower" /\ phase[i] = "Prepare"
              /\ Rej(AppendOnPrefix(log[i], m.log_sync[1].sync_idx, m.log_sync[1].suffix),
                     IF DecidedIdxMonotone THEN Max(old, m.decided_idx) ELSE m.decided_idx)
           \/ /\ m.type = "AcceptDecide" /\ role[i] = "Follower" /\ phase[i] = "Accept"
              /\ CheckMsgStatus(current_seq_num[i], m.seq_num) = "Expected"
              /\ Rej(log[i] \o m.entries,
                     UpdateDecided(old, accepted_idx[i] + Len(m.entries), m.decided_idx).dec)
           \/ /\ m.type = "Decide" /\ phase[i] = "Accept"
              /\ CheckMsgStatus(current_seq_num[i], m.seq_num) = "Expected"
              /\ Rej(log[i], UpdateDecided(old, accepted_idx[i], m.decided_idx).dec)

\* mod.rs:263-282 handle: deliver the head of the channel from j to i.
Receive(j, i) ==
  /\ i \in running
  /\ chan[<<j, i>>] # <<>>
  /\ MismatchAction # "keep" => ~Mismatch(i, j)
  /\ LET m == Head(chan[<<j, i>>])
         c == [p \in Pairs |-> IF p = <<j, i>> THEN Tail(chan[p]) ELSE chan[p]]
     IN /\ m.type \notin EVMessages
        /\ SingleNodeMode(i) => m.type = "Deactivated"
        /\ IF m.type = "Deactivated"
           THEN HandleDeactivated(i, c)
           ELSE IF deactivated[i] /\ Mutation # "DeactivatedStillVotes"
           THEN /\ chan' = c
                /\ UNCHANGED <<durable, volatile, recovery, readvars, buffered_stopsign,
                               deactivated>>
           ELSE IF SteppedBack(i, j, m)
           THEN /\ chan' = c
                /\ UNCHANGED <<durable, volatile, recovery, readvars, buffered_stopsign,
                               deactivated>>
           ELSE /\ UNCHANGED deactivated
                /\ Dispatch(i, j, m, c)
  /\ UNCHANGED <<env, proposed, rolled_back, client, running, first_log, reconfigurations,
                 force_single, read_only, halts, read_only_entries, stage1_ghost, build_source,
                 leasevars>>
  /\ GhostUpdate

\* leader.rs:16-60 handle_leader, own pid only (no BLE).
HandleLeader(i, k) ==
  LET n   == Ballot(k, i)
      na  == n_accepted[i]
      dec == decided_idx[i]
      acc == PromiseAcc(i)
      ls0 == LeaderStateWith(n)
      me  == [n_accepted |-> na, accepted_idx |-> acc, decided_idx |-> dec, pid |-> i,
              last |-> IF VerifyPromise THEN PrefixAt(log[i], dec) ELSE <<>>]
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
     /\ UNCHANGED <<storage_faults, refused, n_accepted, log, decided_idx, accepted_idx, current_seq_num,
                    cached_promise_message, buffered_proposals,
                    wiped, crashes, wipes, losses, duplicates, timeouts, proposed,
                    recovery, rolled_back, client, readvars, buffered_stopsign, reconfig,
                    leasevars>>
  /\ GhostUpdate

\* mod.rs:298-305 append, mod.rs:359-365 propose_entry,
\* leader.rs:111-121 accept_entry_leader
Propose(i, v) ==
  /\ ~ClientSessions
  /\ ~recovering[i]
  /\ ~AcceptedReconfiguration(i) \/ Mutation = "AppendAfterStopSign"
  /\ ~read_only[i] \/ Mutation = "ReadOnlyProposes"
  /\ ~deactivated[i]
  /\ InFlightOk(i)
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
  /\ UNCHANGED <<env, recovery, rolled_back, client, readvars, buffered_stopsign, reconfig,
                 leasevars>>
  /\ GhostUpdate

\* Restart of each node i in R with the durable state d[i], through
\* mod.rs:53-128 `with`.
\* The promise is always set (bootstrap and `with` write it), so the node
\* restarts in Phase::Recover and sends PrepareReq to every peer.
\* internal_storage.rs:55-74 load_cache: accepted_idx = log length.
\* With Recovery, the node first sends Recovery with a fresh nonce and
\* takes part in nothing else until its answers are complete
\* (owner decision: after each start a node votes only after f + 1 answers).
\* @type: (Set(Int), Int -> $durable, Set(Int), Bool, Str) => Bool;
RestartInPhase(R, d, Q, rec, ph) ==
  LET \* @type: (Int -> a, Int -> a) => Int -> a;
      At(f, g) == [k \in Servers |-> IF k \in R THEN g[k] ELSE f[k]]
      msg(k)   == IF rec THEN RecoveryMsg(nonce[k] + 1) ELSE PrepareReqMsg(d[k].promise)
  IN
  /\ promise' = At(promise, [k \in R |-> d[k].promise])
  /\ n_accepted' = At(n_accepted, [k \in R |-> d[k].n_accepted])
  /\ log' = At(log, [k \in R |-> d[k].log])
  /\ decided_idx' = At(decided_idx, [k \in R |-> d[k].decided_idx])
  /\ role' = At(role, [k \in R |-> "Follower"])
  /\ phase' = At(phase, [k \in R |-> ph])
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
  /\ UNCHANGED power_loss_completed
  /\ read_pending' = At(read_pending, [k \in R |-> {}])
  /\ read_round' = At(read_round, [k \in R |-> [read_round[k] EXCEPT !.reads = {}, !.acks = {}]])
  /\ UNCHANGED reads
  /\ lease_request' = [k \in Nodes |-> IF k \in R THEN -1 ELSE lease_request[k]]
  /\ lease_reply' = [k \in Nodes |-> IF k \in R THEN -1 ELSE lease_reply[k]]
  /\ lease_remaining' = [k \in Nodes |-> IF k \in R THEN 0 ELSE lease_remaining[k]]
  /\ wait_remaining' = IF R \cap NextConfigNodes # {} /\ wait_remaining > 0
                       THEN LeaseLength ELSE wait_remaining
  /\ UNCHANGED <<witness_epoch, node_epoch>>

\* @type: (Set(Int), Int -> $durable, Set(Int), Bool) => Bool;
Restart(R, d, Q, rec) == RestartInPhase(R, d, Q, rec, "Recover")

\* F3: crash keeping durable state.
Crash(i) ==
  /\ crashes < MaxCrash
  /\ crashes' = crashes + 1
  /\ Restart({i}, [k \in {i} |-> DurableOf(k)], {k \in {i} : Quiet(k)}, Recovery)
  /\ UNCHANGED <<storage_faults, refused, wiped, wipes, losses, duplicates, timeouts, elections, proposed, rolled_back,
                 client, reconfig>>
  /\ GhostUpdateR({i})

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
  /\ UNCHANGED <<storage_faults, refused, wiped, crashes, wipes, losses, duplicates, timeouts, elections, proposed,
                 client, reconfig>>
  /\ GhostUpdateR(R)

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
  /\ UNCHANGED power_loss_completed
  /\ read_pending' = [read_pending EXCEPT ![i] = {}]
  /\ read_round' = [read_round EXCEPT ![i].reads = {}, ![i].acks = {}]
  /\ deactivated' = IF Mutation = "StopSignLost" THEN [deactivated EXCEPT ![i] = FALSE]
                    ELSE deactivated
  /\ UNCHANGED reads
  \* Provisional: a wipe drops the volatile lease and starts the wait again,
  \* as Restart does. The epoch of the node comes from its configuration
  \* and stays.
  /\ lease_request' = [k \in Nodes |-> IF k = i THEN -1 ELSE lease_request[k]]
  /\ lease_reply' = [k \in Nodes |-> IF k = i THEN -1 ELSE lease_reply[k]]
  /\ lease_remaining' = [k \in Nodes |-> IF k = i THEN 0 ELSE lease_remaining[k]]
  /\ wait_remaining' = IF i \in NextConfigNodes /\ wait_remaining > 0
                       THEN LeaseLength ELSE wait_remaining
  /\ UNCHANGED <<witness_epoch, node_epoch>>
  /\ UNCHANGED <<storage_faults, refused, chan, crashes, losses, duplicates, timeouts, elections, proposed,
                 nonce, rolled_back, client, running, first_log, reconfigurations,
                 force_single, read_only, halts, read_only_entries, stage1_ghost, build_source>>
  /\ GhostUpdateR({i})

\* doc/plan-start-checks.md section 3. A node restarts after a crash and
\* reads a damaged durable state of one kind. Provisional names:
\* StorageFault, StartChecks, the kinds.
\* missing_entry: LMDB lost the key k. LogWithout is an approximation of
\* what the node reads without the checks: read_entries works by key, so a
\* range read up to the count returns the keys below the count, and the
\* last key is not read. Only the mutation StartCheckMissing uses it; with
\* the checks, each k below the last position is found.
\* @type: (Seq($entry), Int) => Seq($entry);
LogWithout(l, k) == SubSeq(l, 1, k - 1) \o SubSeq(l, k + 1, Len(l))

\* @type: (Int, Str) => Set($durable);
\* stale_meta: the disk gives back an earlier meta record with a valid CRC,
\* and the log stays as it is (review 1 of doc/plan-lmdb-only-source.md,
\* finding 4). rollback: the disk gives back an earlier version of the log
\* and of the meta record together, while the node runs.
\* @type: (Int, Str, $durable) => $durable;
StaleOf(i, kind, q) ==
  IF kind = "stale_meta"
  THEN [DurableOf(i) EXCEPT !.promise = q.promise, !.n_accepted = q.n_accepted,
                            !.decided_idx = q.decided_idx]
  ELSE q

DamagedStates(i, kind) ==
  LET d == DurableOf(i) IN
  CASE kind = "missing_entry" ->
         {[d EXCEPT !.log = LogWithout(log[i], k)] : k \in 1..Len(log[i])}
    [] kind = "missing_accepted" ->
         IF Len(log[i]) > 0 /\ n_accepted[i] # DefaultBallot
         THEN {[d EXCEPT !.n_accepted = DefaultBallot]} ELSE {}
    [] kind = "missing_promise" ->
         IF promise[i] # DefaultBallot
         THEN {[d EXCEPT !.promise = DefaultBallot]} ELSE {}
    [] kind = "missing_decided" ->
         IF decided_idx[i] > 0 THEN {[d EXCEPT !.decided_idx = 0]} ELSE {}
    [] kind = "stale_meta" ->
         {StaleOf(i, kind, q) : q \in past_durable[i]} \ {d}
    [] kind = "rollback" ->
         past_durable[i] \ {d}

\* The checks of the plan find a damaged state when the continuity of the
\* log breaks (a missing last key keeps it), or when a non-empty log has no
\* accepted round, or when the accepted round is above the promise, or when
\* the decided index is above the log length.
\* @type: ($durable) => Bool;
ChecksFind(d) ==
  \/ Len(d.log) > 0 /\ d.n_accepted = DefaultBallot
  \/ d.n_accepted > d.promise
  \/ d.decided_idx > Len(d.log)

\* A missing key below the last key breaks the continuity. The model of the
\* damaged state keeps no keys, so the action passes the position.
\* @type: (Int, Str, $durable, Int) => Bool;
Detected(i, kind, d, k) ==
  /\ StartChecks /\ Mutation # "StartCheckMissing"
  /\ \/ kind = "missing_entry" /\ k < Len(log[i])
     \/ ChecksFind(d)

\* The checks of a step (RunningStorageFault). With ExecuteVerify the start
\* applies them to the whole log, with no mark (plan 4.8). q is the earlier
\* durable state that a stale kind gives back.
\* @type: (Int, Str, Int, $durable, $durable, $mark) => Bool;
StepFinds(i, kind, k, q, d, h) ==
  \/ kind = "missing_entry" /\ (k < Len(log[i]) \/ ChainCheck)
  \/ MetaRecord /\ kind \in {"missing_accepted", "missing_promise", "missing_decided"}
  \/ MetaRecord /\ kind = "stale_meta" /\ q.log # log[i]
  \/ HighWater /\ BelowMark(Mark(d), h)
  \/ ChecksFind(d)

\* @type: (Int, Str, $durable, Int) => Bool;
DetectedAtStart(i, kind, d, k) ==
  IF ExecuteVerify
  THEN StartChecks /\ Mutation # "StartCheckMissing" /\ StepFinds(i, kind, k, d, d, NoMark)
  ELSE Detected(i, kind, d, k)

\* Plan 3.3: a proof at or below the open mark is covered and counts
\* nothing, once for each open mark; mark_covered says that the open mark
\* covered a proof (provisional name). The proof of a damage carries the
\* length of the log.
\* @type: (Int, Int) => Bool;
Covered(i, q) ==
  /\ ExecuteVerify
  /\ open_mark[i] # 0 /\ q <= open_mark[i]
  /\ ~mark_covered[i]
  /\ Mutation # "CountProofTwice"

\* A node that reads no promise gets a full copy of the durable state of a
\* node that votes and does not recover, and runs the recovery, as every
\* other start does (owner, 2026-10-24 12:04). The copy is the whole
\* durable state; BuildForServer copies only the decided prefix and has
\* more conditions. Only a seed node at the first start of a cluster starts with no recovery, and that is Init.
\* Mutation StartAsNewNode is the code at 4c57d44: (Follower, None), no
\* recovery, no message (sequence_paxos.hpp:268-271). A refused
\* node leaves running with its state, as Halt does; it takes no further
\* step. Only a node of Nodes gets a storage fault. With StartDamageCounts
\* (owner, 2026-10-25 03:54), a damage that the start checks find is a
\* damage of 2.4 item 8: the node repairs itself from a peer while its
\* count allows it, and the repair counts. Else it is refused.
\* @type: (Int, Str) => Bool;
StorageFault(i, kind) ==
  /\ i \in Nodes
  /\ kind \in StorageFaultKind
  /\ kind \notin RunningOnlyKinds
  /\ storage_faults < MaxStorageFault
  /\ storage_faults' = storage_faults + 1
  /\ \E k \in 0..Len(log[i]), d \in DamagedStates(i, kind) :
       /\ kind = "missing_entry" => k > 0 /\ d.log = LogWithout(log[i], k)
       /\ kind # "missing_entry" => k = 0
       /\ IF DetectedAtStart(i, kind, d, k) /\ StartDamageCounts /\ RepairFromPeer
             /\ IF repairs[i] < AutoRepairs THEN TRUE ELSE Covered(i, Len(log[i]))
             /\ \E src \in running \ {i} : Votes(src) /\ ~recovering[src]
          THEN \E src \in running \ {i} :
                 /\ Votes(src) /\ ~recovering[src]
                 /\ Restart({i}, [j \in {i} |-> DurableOf(src)], {j \in {i} : Quiet(j)}, Recovery)
                 /\ UNCHANGED <<refused, wiped, crashes, wipes, losses, duplicates, timeouts,
                                elections, proposed, rolled_back, client, reconfig>>
                 /\ GhostUpdateRP({i}, IF Covered(i, Len(log[i])) THEN {} ELSE {i})
          ELSE IF DetectedAtStart(i, kind, d, k)
          THEN /\ running' = running \ {i}
               /\ refused' = refused \cup {i}
               /\ stage1_ghost' = [j \in Nodes |-> IF j # i /\ read_only[j]
                                                    THEN [stage1_ghost[j] EXCEPT !.down = @ \cup {i}]
                                                    ELSE stage1_ghost[j]]
               /\ UNCHANGED <<durable, volatile, chan, wiped, crashes, wipes, losses, duplicates,
                              timeouts, elections, ghost, recovery, rollback, client, readvars,
                              buffered_stopsign, first_log, reconfigurations, force_single,
                              read_only, deactivated, halts, read_only_entries, build_source,
                              leasevars, leaseghost>>
          ELSE /\ IF d.promise = DefaultBallot /\ Mutation = "StartAsNewNode"
                  THEN RestartInPhase({i}, [j \in {i} |-> d], {i}, FALSE, "None")
                  ELSE IF d.promise = DefaultBallot
                  THEN \E src \in running \ {i} :
                         /\ Votes(src) /\ ~recovering[src]
                         /\ Restart({i}, [j \in {i} |-> DurableOf(src)], {j \in {i} : Quiet(j)},
                                    Recovery)
                  ELSE Restart({i}, [j \in {i} |-> d], {j \in {i} : Quiet(j)}, Recovery)
               /\ UNCHANGED <<refused, wiped, crashes, wipes, losses, duplicates, timeouts,
                              elections, proposed, rolled_back, client, reconfig>>
               /\ GhostUpdateR({i})

\* A running node that finds a damage stops voting and leaves running, as
\* stop_voting does. With RepairFromPeer (owner, 2026-10-24 19:21), it
\* takes the durable state of a node that votes and does not recover, and
\* restarts with the recovery, as StorageFault does for a node with no
\* promise. With no such node, it leaves running. The step does not change
\* storage_faults. A node repairs itself automatically at most AutoRepairs
\* times; after that it leaves running, and the operator acts (owner,
\* 2026-10-24 20:55). This counts only a damage that the checks find, not
\* a restart.

\* The node stops voting and leaves running, as stop_voting does.
\* @type: Int => Bool;
LeaveRunning(i) ==
  /\ running' = running \ {i}
  /\ refused' = refused \cup {i}
  /\ stage1_ghost' = [j \in Nodes |-> IF j # i /\ read_only[j]
                                       THEN [stage1_ghost[j] EXCEPT !.down = @ \cup {i}]
                                       ELSE stage1_ghost[j]]
  /\ UNCHANGED <<durable, volatile, chan, wiped, crashes, wipes, losses, duplicates,
                 timeouts, elections, ghost, recovery, rollback, client, readvars,
                 buffered_stopsign, first_log, reconfigurations, force_single,
                 read_only, deactivated, halts, read_only_entries, build_source,
                 leasevars, leaseghost>>


\* After an automatic repair, the operator gets a notice. The operator can
\* allow one more automatic repair at the next damage (owner, 2026-10-24
\* 20:57). Without that, the node repairs no more by itself.
\* @type: Int => Bool;
OperatorAllowsRepair(i) ==
  /\ RepairFromPeer
  /\ repairs[i] = AutoRepairs
  /\ repairs' = [repairs EXCEPT ![i] = 0]
  /\ UNCHANGED <<durable, volatile, chan, env, ghost, recovery, rolled_back, past_durable,
                 peer_seen, high_water, client, readvars, buffered_stopsign, reconfig,
                 leasevars, leaseghost>>

\* @type: Int => Bool;
DamageFound(i) ==
  IF RepairFromPeer /\ (repairs[i] < AutoRepairs \/ Covered(i, Len(log[i])))
     /\ \E src \in running \ {i} : Votes(src) /\ ~recovering[src]
  THEN \E src \in running \ {i} :
         /\ Votes(src) /\ ~recovering[src]
         /\ Restart({i}, [j \in {i} |-> DurableOf(src)], {j \in {i} : Quiet(j)}, Recovery)
         /\ UNCHANGED <<refused, wiped, crashes, wipes, losses, duplicates, timeouts,
                        elections, proposed, rolled_back, client, reconfig>>
         /\ GhostUpdateRP({i}, IF Covered(i, Len(log[i])) THEN {} ELSE {i})
  ELSE LeaveRunning(i)

\* @type: Int => Bool;
MismatchFound(i) ==
  /\ MismatchAction # "keep"
  /\ \E j \in Servers \ {i} : <<j, i>> \in Pairs /\ Mismatch(i, j)
  /\ UNCHANGED storage_faults
  /\ IF MismatchAction = "damage" THEN DamageFound(i) ELSE LeaveRunning(i)

\* RunningStorageFault, RunningFaults, StepChecks (doc/plan-lmdb-only-source.md).
\* The durable state of a running node changes. The node holds no copy of
\* it, so its next step reads the damaged state (owner, 2026-10-24 14:44:
\* LMDB is the only source). A missing key at the last position is a lost
\* last entry: LMDB drops the key and the count, so the stored log stays
\* consistent and one entry shorter. With StepChecks, the read of the next
\* step finds what the checks find, and the node leaves running, as
\* stop_voting does. With ChainCheck, each entry carries the CRC over the
\* CRC of its predecessor and its own bytes, and the meta record holds the
\* last index and its chain CRC; so the step check also finds a lost last
\* entry. With MetaRecord, the promise, the accepted round, the decided
\* index, the last index and the chain CRC are one record with one CRC.
\* The node writes it when it is created and it never goes away, so a lost
\* or changed value is found. Every other damage is the new state of the
\* node.
\* @type: (Int, Str) => Bool;
RunningStorageFault(i, kind) ==
  /\ RunningFaults
  /\ i \in Nodes
  /\ kind \in StorageFaultKind
  /\ storage_faults < MaxStorageFault
  /\ storage_faults' = storage_faults + 1
  /\ \E k \in 0..Len(log[i]),
       q \in (IF kind \in RunningOnlyKinds THEN past_durable[i] ELSE {DurableOf(i)}),
       d \in DamagedStates(i, kind) :
       /\ kind = "missing_entry" => k > 0 /\ d.log = LogWithout(log[i], k)
       /\ kind # "missing_entry" => k = 0
       /\ kind \in RunningOnlyKinds => d = StaleOf(i, kind, q)
       /\ IF StepChecks /\ StepFinds(i, kind, k, q, d, high_water[i])
          THEN DamageFound(i)
          ELSE /\ promise' = [promise EXCEPT ![i] = d.promise]
               /\ n_accepted' = [n_accepted EXCEPT ![i] = d.n_accepted]
               /\ log' = [log EXCEPT ![i] = d.log]
               /\ decided_idx' = [decided_idx EXCEPT ![i] = d.decided_idx]
               /\ accepted_idx' = [accepted_idx EXCEPT ![i] = Len(d.log)]
               /\ leader_state' = [leader_state EXCEPT ![i].accepted_indexes[i] =
                                      IF @ > Len(d.log) THEN Len(d.log) ELSE @]
               /\ UNCHANGED <<refused, role, phase, current_seq_num, cached_promise_message,
                              buffered_proposals, chan, wiped, crashes, wipes,
                              losses, duplicates, timeouts, elections, proposed, rolled_back,
                              recovery, client, readvars, buffered_stopsign, reconfig, leasevars>>
               /\ GhostUpdate

\* The read at the next step of a running node finds a durable state that
\* the checks find, also one that its own step wrote after an undetected
\* damage. The node may take one more step before it: the model allows
\* more than the code, which checks before each step.
StepCheck(i) ==
  /\ RunningFaults /\ StepChecks
  /\ i \in Nodes
  /\ ChecksFind(DurableOf(i))
  /\ UNCHANGED storage_faults
  /\ DamageFound(i)

\* mod.rs:232-237 resend_message_timeout, leader.rs:371-403
\* resend_messages_leader, follower.rs:240-282 resend_messages_follower.
\* util.rs:211-222 get_preparable_peers.
ResendMessageTimeout(i) ==
  /\ ~read_only[i] /\ ~deactivated[i]
  /\ timeouts < MaxTimeout
  /\ timeouts' = timeouts + 1
  /\ IF recovering[i]
     THEN /\ chan' = Send(chan, i, [k \in Peers(i) |-> <<RecoveryMsg(nonce[i])>>])
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
                           !.seq_num = ls.follower_seq_nums[k], !.decided_idx = decided_idx[i],
                           !.last = PrefixAt(log[i], decided_idx[i])]
                     ELSE AcceptStopSignMsg(ls.n_leader, ls.follower_seq_nums[k])
                   df == IF phase[i] = "Accept" /\ Mutation # "NoDecideOnResend"
                         THEN PromisedFollowers(ls) \ ss ELSE {}
                   ls1 == IF Mutation = "DecideOnResendOldSeq" THEN ls ELSE NextSeqNums(ls, df)
                   \* @type: Int => $msg;
                   decide(k) == [EmptyMessage EXCEPT !.type = "Decide", !.n = ls.n_leader,
                                 !.seq_num = ls1.follower_seq_nums[k], !.decided_idx = decided_idx[i],
                                 !.last = PrefixAt(log[i], decided_idx[i])]
               IN /\ chan' = Send(chan, i, [k \in pp \cup ss \cup df |->
                                    IF k \in pp THEN <<PrepareMsg(i)>>
                                    ELSE IF k \in ss THEN <<resend(k)>>
                                    ELSE <<decide(k)>>])
                  /\ leader_state' = [leader_state EXCEPT ![i] = ls1]
                  /\ UNCHANGED <<durable, role, phase, accepted_idx, current_seq_num,
                                 cached_promise_message, buffered_proposals>>
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
  /\ UNCHANGED <<storage_faults, refused, wiped, crashes, wipes, losses, duplicates, elections, proposed,
                 recovery, rolled_back, client, readvars, buffered_stopsign, reconfig,
                 leasevars>>
  /\ chan' # chan \/ phase' # phase
  /\ GhostUpdate

\* F1: loss of any message on a channel.
\* @type: (<<Int, Int>>, Int) => Bool;
Lose(p, k) ==
  /\ losses < MaxLoss
  /\ losses' = losses + 1
  /\ chan' = [x \in Pairs |-> IF x = p THEN SubSeq(chan[p], 1, k - 1) \o SubSeq(chan[p], k + 1, Len(chan[p]))
                                ELSE chan[x]]
  /\ UNCHANGED <<storage_faults, refused, durable, volatile, wiped, crashes, wipes, duplicates, timeouts, elections, proposed,
                 recovery, rolled_back, client, readvars, buffered_stopsign, reconfig,
                 leasevars>>
  /\ GhostUpdate

\* F1: duplication; the copy is appended at the end of the channel.
\* @type: (<<Int, Int>>, Int) => Bool;
Duplicate(p, k) ==
  /\ duplicates < MaxDuplicate
  /\ duplicates' = duplicates + 1
  /\ Len(chan[p]) < Cap
  /\ chan' = [x \in Pairs |-> IF x = p THEN Append(chan[p], chan[p][k]) ELSE chan[x]]
  /\ UNCHANGED <<storage_faults, refused, durable, volatile, wiped, crashes, wipes, losses, timeouts, elections, proposed,
                 recovery, rolled_back, client, readvars, buffered_stopsign, reconfig,
                 leasevars>>
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
  /\ InFlightOk(i)
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
  /\ UNCHANGED <<env, recovery, rolled_back, readvars, buffered_stopsign, reconfig,
                 leasevars>>
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
  /\ s <= Len(ClientPrefix(j))
  /\ ClientPrefix(j)[s] = RegisterClientEntry
  /\ ClientTable(ClientPrefix(j))[s].open
  /\ client' = [client EXCEPT !.session = s, !.request_number = 1]
  /\ UNCHANGED <<durable, volatile, chan, env, ghost, recovery, rollback, readvars,
                 buffered_stopsign, reconfig, leasevars, leaseghost>>

\* VR Revisited 4.1: node j replies with the result of the request; the
\* client takes the next request-number.
ClientReplied(j) ==
  /\ ClientSessions
  /\ Votes(j)
  /\ client.session > 0
  /\ client.request_number <= MaxRequest
  /\ LET t == ClientTable(ClientPrefix(j))[client.session] IN
       t.open /\ t.request_number >= client.request_number
  /\ client' = [client EXCEPT !.request_number = @ + 1]
  /\ UNCHANGED <<durable, volatile, chan, env, ghost, recovery, rollback, readvars,
                 buffered_stopsign, reconfig, leasevars, leaseghost>>

\* Raft 6.3: node j replies SESSION_EXPIRED; the client registers again.
ClientSessionExpired(j) ==
  /\ ClientSessions
  /\ Votes(j)
  /\ client.session > 0
  /\ client.session <= Len(ClientPrefix(j))
  /\ ~ClientTable(ClientPrefix(j))[client.session].open
  /\ client' = [client EXCEPT !.session = 0, !.request_number = 1]
  /\ UNCHANGED <<durable, volatile, chan, env, ghost, recovery, rollback, readvars,
                 buffered_stopsign, reconfig, leasevars, leaseghost>>

\* Raft 6.3: the leader appends an entry that expires session s. Owner
\* decision: this follows a heartbeat timeout of the client. The timeout is
\* a free choice here.
ExpireSession(i, s) ==
  /\ ClientSessions
  /\ ~recovering[i]
  /\ ~read_only[i]
  /\ role[i] = "Leader" /\ phase[i] = "Accept"
  /\ ~AcceptedReconfiguration(i)
  /\ s <= Len(ClientPrefix(i))
  /\ ClientTable(ClientPrefix(i))[s].open
  /\ \A k \in 1..Len(log[i]) : log[i][k] # ExpireSessionEntry(s)
  /\ proposed' = proposed \cup {ExpireSessionEntry(s)}
  /\ AcceptEntriesLeader(i, <<ExpireSessionEntry(s)>>, chan)
  /\ UNCHANGED <<promise, n_accepted, decided_idx, role, phase, current_seq_num,
                 cached_promise_message, buffered_proposals>>
  /\ UNCHANGED <<env, recovery, rolled_back, client, readvars, buffered_stopsign, reconfig,
                 leasevars>>
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
                                    !.decided_at_start = IF ExecuteVerify
                                                         THEN EffPrefix(decided_history)
                                                         ELSE decided_history,
                                    !.decided_new_at_start = DecidedNew,
                                    !.contract_stale_ok = ok,
                                    !.forced_decided = forced_has_decided]
      ld          == BallotPid(promise[i])
  IN
  /\ ReadIndex
  /\ ~recovering[i]
  /\ read_only[i] \/ ~deactivated[i]
  /\ reads[r].state = "none"
  /\ SingleNode /\ ClusterNodes(i) = {i} => SingleNodeServes(i) \/ Mutation = "ReadStartBeforeServing"
  /\ i \in NextConfigNodes => wait_remaining <= 0
  /\ \E ok \in IF StaleReadNode THEN BOOLEAN ELSE {FALSE} :
     IF read_only[i] \/ (ok /\ ~InStage2(i))
     THEN /\ reads' = [reads EXCEPT ![r] = [started(ok) EXCEPT !.state = "done",
                                                               !.result = ClientPrefix(i),
                                                               !.stale = TRUE]]
          /\ UNCHANGED <<chan, read_pending>>
     ELSE IF Mutation = "LocalRead" \/ SingleNodeServes(i)
     THEN /\ reads' = [reads EXCEPT ![r] = [started(ok) EXCEPT !.state = "done",
                                                               !.result = ClientPrefix(i)]]
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
                 buffered_stopsign, reconfig, leasevars, leaseghost>>

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
  /\ LeaseValid(i) \/ Mutation = "ReadWithoutLease"
  /\ read_round' = [read_round EXCEPT ![i] =
                      [n |-> n, id |-> id, reads |-> read_pending[i],
                       read_index |-> accepted_idx[i],
                       acks |-> IF Votes(i) THEN {i} ELSE {}]]
  /\ read_pending' = [read_pending EXCEPT ![i] = {}]
  /\ chan' = Send(chan, i, [k \in Peers(i) |->
                <<[EmptyMessage EXCEPT !.type = "Heartbeat", !.n = n, !.id = id]>>])
  /\ UNCHANGED <<durable, volatile, env, ghost, recovery, rollback, client, reads,
                 buffered_stopsign, reconfig, leasevars, leaseghost>>

\* Raft 6.4: the node answers read r from its own log once its decided index
\* has reached the read index.
ReadReply(r) ==
  LET i == reads[r].node IN
  /\ ReadIndex
  /\ reads[r].state = "indexed"
  /\ IF ExecuteVerify THEN AppliedIdxOf(DecidedPrefix(i)) >= reads[r].read_index
     ELSE decided_idx[i] >= reads[r].read_index
  /\ \/ Mutation = "ReadBeforeServing"
     \/ /\ reads[r].crashes = crashes
        /\ reads[r].rolled_back = rolled_back
        /\ reads[r].ballot = promise[i]
  /\ reads' = [reads EXCEPT ![r].state = "done", ![r].result = ClientPrefix(i)]
  /\ UNCHANGED <<durable, volatile, chan, env, ghost, recovery, rollback, client,
                 read_pending, read_round, buffered_stopsign, reconfig, leasevars, leaseghost>>

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
  /\ StopSignAllowed(i)
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
                 build_source, leasevars>>
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
                 read_only, deactivated, halts, read_only_entries, stage1_ghost, leasevars>>
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
  /\ wait_remaining <= 0 \/ Mutation = "ForcedWithoutWait"
  /\ HandleMajorityPromises(i, leader_state[i], chan)
  /\ UNCHANGED <<env, proposed, recovery, rolled_back, client, readvars, reconfig, leasevars>>
  /\ GhostUpdate

\* leader.rs:316-345 handle_accepted
\* @type: Int => Bool;
LeaderCountsOwnAccepted(i) ==
  LET own_idx == leader_state[i].accepted_indexes[i] IN
  /\ SingleNode
  /\ ClusterNodes(i) = {i}
  /\ ~deactivated[i] \/ Mutation = "DeactivatedStillVotes"
  /\ ~read_only[i]
  /\ ~recovering[i]
  /\ Votes(i)
  /\ role[i] = "Leader" /\ phase[i] = "Accept"
  /\ own_idx > decided_idx[i]
  /\ IsChosen(leader_state[i], own_idx)
  /\ wait_remaining <= 0 \/ Mutation = "ForcedWithoutWait"
  /\ HandleAccepted(i, i, [EmptyMessage EXCEPT !.type = "Accepted",
                           !.n = leader_state[i].n_leader, !.accepted_idx = own_idx,
                           !.last = PrefixAt(log[i], own_idx)], chan)
  /\ UNCHANGED <<env, proposed, recovery, rolled_back, client, readvars,
                 buffered_stopsign, reconfig, leasevars>>
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
  /\ Witness \/ Cardinality(Nodes \ running) >= Majority(i) \/ Mutation = "OldNodeVotesAfterForce"
  /\ IF Witness
     THEN /\ witness_epoch' = IF Mutation = "ForceWithoutRaise" THEN witness_epoch
                              ELSE witness_epoch + 1
          /\ node_epoch' = [node_epoch EXCEPT ![s] = witness_epoch + 1]
          /\ wait_remaining' = LeaseLength
     ELSE UNCHANGED <<witness_epoch, node_epoch, wait_remaining>>
  /\ UNCHANGED <<lease_request, lease_reply, lease_remaining>>
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
                 buffered_stopsign, reconfig, leasevars, leaseghost>>

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
  /\ UNCHANGED <<storage_faults, refused, wiped, crashes, wipes, losses, duplicates, timeouts, elections, proposed,
                 rolled_back, client, running, first_log, reconfigurations, force_single,
                 deactivated, halts, stage1_ghost, build_source>>
  /\ GhostUpdateR({i})

\* @type: Int => Bool;
LeaveReadOnly(i) ==
  /\ read_only[i]
  /\ force_single.node = 0 \/ Mutation = "ForceAtTwoNodes"
  /\ read_only' = [read_only EXCEPT ![i] = FALSE]
  /\ stage1_ghost' = [stage1_ghost EXCEPT ![i].leave_decided = Len(decided_history)]
  /\ Restart({i}, [k \in {i} |-> DurableOf(k)], {k \in {i} : deactivated[k]}, Recovery)
  /\ UNCHANGED <<storage_faults, refused, wiped, crashes, wipes, losses, duplicates, timeouts, elections, proposed,
                 rolled_back, client, running, first_log, reconfigurations, force_single,
                 deactivated, halts, read_only_entries, build_source>>
  /\ GhostUpdateR({i})

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
                 deactivated, read_only_entries, build_source, leasevars, leaseghost>>

\* @type: Int => Bool;
Resume(i) ==
  /\ i \in Nodes \ running
  /\ i \notin refused
  /\ i # force_single.node
  /\ running' = running \cup {i}
  /\ Restart({i}, [k \in {i} |-> DurableOf(k)], {k \in {i} : Quiet(k)},
             Recovery /\ Mutation # "ResumeWithoutRecovery")
  /\ UNCHANGED <<storage_faults, refused, wiped, crashes, wipes, losses, duplicates, timeouts, elections, proposed,
                 rolled_back, client, first_log, reconfigurations, force_single, read_only,
                 deactivated, halts, read_only_entries, stage1_ghost, build_source>>
  /\ GhostUpdateR({i})

----------------------------------------------------------------------------
(* Actions of the lease at the witness. Names for the owner, no source:     *)
(* Tick, LeaseRequest, LeaseGrant, LeaseReply, LeaderDecidesChosen,         *)
(* WitnessRestart.                                                          *)

\* Each positive duration falls by one.
Tick ==
  /\ Witness
  /\ \/ \E i \in Nodes : lease_request[i] > 0 \/ lease_remaining[i] > 0
     \/ wait_remaining > 0
  /\ lease_request' = [k \in Nodes |-> IF lease_request[k] > 0 THEN lease_request[k] - 1
                                       ELSE lease_request[k]]
  /\ lease_remaining' = [k \in Nodes |-> IF lease_remaining[k] > 0 THEN lease_remaining[k] - 1
                                         ELSE lease_remaining[k]]
  /\ wait_remaining' = IF wait_remaining > 0 THEN wait_remaining - 1 ELSE wait_remaining
  /\ UNCHANGED <<durable, volatile, chan, env, ghost, recovery, rollback, client, readvars,
                 buffered_stopsign, reconfig, witness_epoch, node_epoch, lease_reply,
                 leaseghost>>

\* A leader of the old cluster reads its clock and sends a request with its
\* epoch. The model keeps one request for each node.
\* @type: Int => Bool;
LeaseRequest(i) ==
  /\ Witness
  /\ i \in Nodes
  /\ role[i] = "Leader" /\ phase[i] = "Accept"
  /\ lease_request[i] = -1
  /\ lease_request' = [lease_request EXCEPT ![i] = LeaseLength]
  /\ UNCHANGED <<durable, volatile, chan, env, ghost, recovery, rollback, client, readvars,
                 buffered_stopsign, reconfig, witness_epoch, node_epoch, lease_reply,
                 lease_remaining, wait_remaining, leaseghost>>

\* The witness grants the request only if the epoch of the request is its
\* own epoch.
\* @type: Int => Bool;
LeaseGrant(i) ==
  /\ Witness
  /\ lease_request[i] >= 0
  /\ lease_reply[i] = -1
  /\ lease_reply' = [lease_reply EXCEPT ![i] =
                       IF node_epoch[i] = witness_epoch \/ Mutation = "WitnessGrantsAnyEpoch"
                       THEN 1 ELSE 0]
  /\ UNCHANGED <<durable, volatile, chan, env, ghost, recovery, rollback, client, readvars,
                 buffered_stopsign, reconfig, witness_epoch, node_epoch, lease_request,
                 lease_remaining, wait_remaining, leaseghost>>

\* The node takes the reply. A grant sets the lease from the time of the
\* request, which Tick has lowered since. A refusal drops the lease at once.
\* Provisional: the mutation AdoptEpochFromRefusal acts here, at the node
\* that takes the refusal, and not in LeaseGrant.
\* @type: Int => Bool;
LeaseReply(i) ==
  /\ Witness
  /\ i \in Nodes
  /\ lease_reply[i] >= 0
  /\ lease_remaining' = [lease_remaining EXCEPT ![i] =
                           IF lease_reply[i] = 1
                           THEN IF Mutation = "LeaseFromReply" THEN LeaseLength
                                ELSE lease_request[i]
                           ELSE 0]
  /\ node_epoch' = IF lease_reply[i] = 0 /\ Mutation = "AdoptEpochFromRefusal"
                   THEN [node_epoch EXCEPT ![i] = witness_epoch]
                   ELSE node_epoch
  /\ lease_request' = [lease_request EXCEPT ![i] = -1]
  /\ lease_reply' = [lease_reply EXCEPT ![i] = -1]
  /\ UNCHANGED <<durable, volatile, chan, env, ghost, recovery, rollback, client, readvars,
                 buffered_stopsign, reconfig, witness_epoch, wait_remaining, leaseghost>>

\* leader.rs:316-345 handle_accepted, leader.rs:240-255 send_decide: a leader
\* of the old cluster with a valid lease decides the highest chosen index
\* above its decided_idx, as the decide branch of HandleAccepted does. It
\* does not write accepted_indexes.
\* @type: Int => Bool;
LeaderDecidesChosen(i) ==
  LET ls == leader_state[i]
      \* @type: Set(Int);
      cs == {k \in (decided_idx[i] + 1)..accepted_idx[i] : IsChosen(ls, k)}
      d  == CHOOSE k \in cs : \A x \in cs : x <= k
      fs == PromisedFollowers(ls)
      ls2 == NextSeqNums(ls, fs)
  IN
  /\ Witness
  /\ i \in Nodes
  /\ ~recovering[i]
  /\ role[i] = "Leader" /\ phase[i] = "Accept"
  /\ LeaseValid(i) \/ Mutation = "DecideWithoutLease"
  /\ cs # {}
  /\ decided_idx' = [decided_idx EXCEPT ![i] = d]
  /\ leader_state' = [leader_state EXCEPT ![i] = ls2]
  /\ chan' = Send(chan, i, [k \in fs |->
               <<[EmptyMessage EXCEPT !.type = "Decide", !.n = ls2.n_leader,
                  !.seq_num = ls2.follower_seq_nums[k], !.decided_idx = d,
                  !.last = PrefixAt(log[i], d)]>>])
  /\ UNCHANGED <<promise, n_accepted, log, role, phase, accepted_idx, current_seq_num,
                 cached_promise_message, buffered_proposals, env, proposed, recovery,
                 rolled_back, client, readvars, buffered_stopsign, reconfig, leasevars>>
  /\ GhostUpdate

\* Only with the mutation WitnessForgetsEpoch: the witness restarts with
\* epoch 0. Without the mutation the witness keeps only durable state.
WitnessRestart ==
  /\ Witness
  /\ Mutation = "WitnessForgetsEpoch"
  /\ witness_epoch # 0
  /\ witness_epoch' = 0
  /\ UNCHANGED <<durable, volatile, chan, env, ghost, recovery, rollback, client, readvars,
                 buffered_stopsign, reconfig, node_epoch, lease_request, lease_reply,
                 lease_remaining, wait_remaining, leaseghost>>

----------------------------------------------------------------------------
ProtocolNext ==
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
  \/ \E i \in running, kind \in StorageFaultKind : StorageFault(i, kind)
  \/ \E i \in running, kind \in StorageFaultKind : RunningStorageFault(i, kind)
  \/ \E i \in running : StepCheck(i)
  \/ \E i \in Nodes : OperatorAllowsRepair(i)
  \/ \E i \in Nodes : MismatchFound(i)
  \/ \E i \in Nodes \ running : Resume(i)
  \/ Tick
  \/ \E i \in running : LeaseRequest(i) \/ LeaseReply(i) \/ LeaderDecidesChosen(i)
  \/ \E i \in Nodes : LeaseGrant(i)
  \/ WitnessRestart

\* StateInLmdb (doc/plan-lmdb-only-source.md 2.6). `applied[i]` is the
\* committed state of node i in its own LMDB environment, as the entries
\* that it applied. `in_txn[i]` is the open write transaction of the
\* state: the entries that ran on top of `applied[i]` before their decide. A
\* step of the protocol leaves both, except: a node that stops running or
\* starts a recovery aborts the open transaction; a wipe loses the state;
\* a repair from a peer (2.4) keeps the state when one of the state and
\* the decided prefix of the repaired log is a prefix of the other, else
\* takes the state of the peer, up to its decided index
\* (StateRepair = "copy"). A fault of the log environment leaves the
\* state environment as it is.
\* @type: Bool;
StateFollows ==
  IF ~StateInLmdb THEN UNCHANGED statevars
  ELSE
  /\ applied' = [i \in Servers |->
        LET dp == SubSeq(log'[i], 1, Min(decided_idx'[i], Len(log'[i])))
            st == IF ExecuteVerify THEN StateItems(dp) ELSE dp
        IN IF wiped'[i] /\ ~wiped[i] THEN <<>>
           ELSE IF repairs'[i] > repairs[i] /\ StateRepair = "copy" /\ ~Compatible(applied[i], st)
           THEN st
           ELSE applied[i]]
  /\ in_txn' = [i \in Servers |->
        IF (wiped'[i] /\ ~wiped[i]) \/ i \notin running' \/ (recovering'[i] /\ ~recovering[i])
           \/ repairs'[i] > repairs[i]
        THEN <<>> ELSE in_txn[i]]

\* 2.6 items 4, 5 and 7. Before each run and each commit, the node compares
\* the entries of its open transaction with its log (the chain CRC at the
\* last entry that ran). When they differ, a write cut or replaced the log
\* below the end of the open transaction, and the node aborts it.
\* Mutation "NoAbortOnCut" leaves out this comparison.
\* @type: Int => Bool;
StateCurrent(i) ==
  Mutation = "NoAbortOnCut" \/ IsPrefix(applied[i] \o in_txn[i], log[i])

\* @type: Int => Bool;
StateAbort(i) ==
  /\ StateInLmdb
  /\ i \in running
  /\ in_txn[i] # <<>>
  /\ ~StateCurrent(i)
  /\ in_txn' = [in_txn EXCEPT ![i] = <<>>]
  /\ UNCHANGED <<vars_but_state, applied>>

\* Execute-Verify: the node runs the next entry of its log in the open
\* transaction, before the decide, up to MaxInFlight entries.
\* @type: Int => Bool;
StateRun(i) ==
  /\ StateInLmdb /\ ~ExecuteVerify
  /\ i \in running /\ ~recovering[i]
  /\ StateCurrent(i)
  /\ Len(in_txn[i]) < MaxInFlight
  /\ Len(applied[i]) + Len(in_txn[i]) < Len(log[i])
  /\ in_txn' = [in_txn EXCEPT ![i] = Append(@, log[i][Len(applied[i]) + Len(in_txn[i]) + 1])]
  /\ UNCHANGED <<vars_but_state, applied>>

\* The decide reached the node: it commits the entries of the open
\* transaction up to the decided index. The entries after it run again on
\* the committed state, which gives the same entries in in_txn.
\* @type: Int => Bool;
StateApply(i) ==
  /\ StateInLmdb /\ ~ExecuteVerify
  /\ i \in running /\ ~recovering[i]
  /\ StateCurrent(i)
  /\ Len(applied[i]) < decided_idx[i]
  /\ Len(applied[i]) + Len(in_txn[i]) >= decided_idx[i]
  /\ LET ran == applied[i] \o in_txn[i] IN
       /\ applied' = [applied EXCEPT ![i] = SubSeq(ran, 1, decided_idx[i])]
       /\ in_txn' = [in_txn EXCEPT ![i] = SubSeq(ran, decided_idx[i] + 1, Len(ran))]
  /\ UNCHANGED vars_but_state


----------------------------------------------------------------------------
(* Execute-Verify actions (doc/plan-execute-verify.md 4.5 to 4.8). Every     *)
(* node runs every decided request entry above its pointer p, in log order,  *)
(* and answers the leader of its promise. own[i] maps an index to the run of *)
(* node i: the entry, the result and the base. The leader keeps the latest   *)
(* answer of each node in counts[i].ans, selects a result for each index in  *)
(* selected[i], and appends one verdict entry at a time. Each node checks a  *)
(* verdict entry before verified_idx passes it. runs, wrong and flips are    *)
(* ghosts. result_throws lists each result throw. A list item of a Results  *)
(* entry carries, as a ghost, the base on which its result ran.             *)

\* @type: Int => Bool;
EVNode(i) ==
  /\ ExecuteVerify
  /\ i \in running /\ ~recovering[i] /\ Votes(i)
  /\ ~deactivated[i] /\ ~SingleNodeMode(i)

Chosen(i) == V(DecidedPrefix(i))
Ptr(i) == Chosen(i).p
AppliedIdx(i) == AppliedIdxOf(DecidedPrefix(i))

\* The base of index q at node i with the own results o (plan 2.4.1).
NodeBaseOk(i, o, q) ==
  LET a  == Chosen(i)
      ks == ReqIdx(log[i], a.void, q)
  IN \A n \in 1..Len(ks) : ks[n] <= a.p \/ ks[n] \in DOMAIN o
NodeBase(i, o, q) == BaseWith(log[i], Chosen(i), q, LAMBDA k : o[k].res)

\* 2.4.3: the own results above p ran on the current log and base.
OwnCurrent(i) ==
  \A k \in DOMAIN own[i] :
    k > Ptr(i) =>
      /\ k <= Len(log[i]) /\ own[i][k].v = log[i][k]
      /\ NodeBaseOk(i, own[i], k) /\ own[i][k].base = NodeBase(i, own[i], k)

\* The own results from lo up, as an answer carries them.
MapOf(o, lo, hi) ==
  [k \in {x \in DOMAIN o : lo <= x /\ x <= hi} |-> [res |-> o[k].res, base |-> o[k].base]]

\* 2.5.2: answer(ballot, p, from, base, results). The base is sent whole in
\* place of its CRC. idx names the index of a second run (2.6.4), else 0.
AnswerMsg(i, o, f, x) ==
  [EmptyMessage EXCEPT !.type = "Answer", !.n = promise[i], !.ptr = Ptr(i),
   !.base = MapOf(o, Ptr(i) + 1, f - 1), !.results = MapOf(o, f, MaxLog), !.idx = x]

\* The counts c of a leader with the answer m of node j (2.6.4: idx > 0
\* marks a second run).
CountAnswer(c, j, m) ==
  [c EXCEPT !.ans = [@ EXCEPT ![j] = [ptr |-> m.ptr, map |-> m.base @@ m.results]],
            !.again = IF m.idx > 0 THEN @ \cup {<<j, m.idx>>} ELSE @]

\* The leader of the promise of i takes the answer: a message to a peer, or
\* its own entry in counts.
AnswerOut(i, m) ==
  LET ld == BallotPid(promise[i]) IN
  IF ld = i
  THEN /\ counts' = IF role[i] = "Leader" /\ leader_state[i].n_leader = promise[i]
                    THEN [counts EXCEPT ![i] = CountAnswer(@, i, m)]
                    ELSE counts
       /\ UNCHANGED chan
  ELSE /\ chan' = IF ld > 0 THEN Send(chan, i, One(ld, m)) ELSE chan
       /\ UNCHANGED counts

EVOnly == <<durable, volatile, env, ghost, recovery, rollback, client, readvars,
            buffered_stopsign, reconfig, leasevars, leaseghost, statevars>>

RunOf(i, e, b, r) == [node |-> i, v |-> e, base |-> b, res |-> r]

\* 2.5.1, 2.5.2: node i runs the lowest decided request entry above p that
\* it has not run, and answers at once.
RunEntry(i) ==
  LET a  == Chosen(i)
      d  == Min(decided_idx[i], Len(log[i]))
      qs == {k \in (a.p + 1)..d : IsRequest(log[i][k]) /\ k \notin DOMAIN own[i]}
      q  == CHOOSE k \in qs : \A x \in qs : k <= x
  IN
  /\ EVNode(i)
  /\ OwnCurrent(i)
  /\ qs # {}
  /\ NodeBaseOk(i, own[i], q)
  /\ LET b == NodeBase(i, own[i], q) IN
     \E w \in WrongAfter(i, log[i][q], wrong) :
     \E r \in RunResults(i, log[i][q], b, w) :
       LET o1 == (q :> [v |-> log[i][q], res |-> r, base |-> b]) @@ own[i] IN
       /\ wrong' = w
       /\ own' = [own EXCEPT ![i] = o1]
       /\ runs' = runs \cup {RunOf(i, log[i][q], b, r)}
       /\ answered' = [answered EXCEPT ![i] = [n |-> promise[i], ptr |-> a.p]]
       /\ AnswerOut(i, AnswerMsg(i, o1, q, 0))
  /\ UNCHANGED <<selected, verified_idx, proof, open_mark, mark_covered, result_throws, refusal,
                 flips, reported>>
  /\ UNCHANGED EVOnly

\* 2.4.3: a verdict, a cut of the log or a second run changed the base of
\* an own result: the node aborts the open transaction.
OwnAbort(i) ==
  /\ ExecuteVerify
  /\ i \in running
  /\ DOMAIN own[i] # {}
  /\ ~OwnCurrent(i)
  /\ own' = [own EXCEPT ![i] = EmptyFn]
  /\ UNCHANGED <<selected, counts, verified_idx, proof, open_mark, mark_covered, result_throws,
                 answered, refusal, wrong, runs, flips, reported>>
  /\ UNCHANGED <<EVOnly, chan>>

\* 2.5.3: when p rises or the promise changes, the node answers again from p.
Reanswer(i) ==
  /\ EVNode(i)
  /\ OwnCurrent(i)
  /\ promise[i] # DefaultBallot
  /\ answered[i] # [n |-> promise[i], ptr |-> Ptr(i)]
  /\ MapOf(own[i], Ptr(i) + 1, MaxLog) # EmptyFn
  /\ answered' = [answered EXCEPT ![i] = [n |-> promise[i], ptr |-> Ptr(i)]]
  /\ AnswerOut(i, AnswerMsg(i, own[i], Ptr(i) + 1, 0))
  /\ UNCHANGED <<selected, own, verified_idx, proof, open_mark, mark_covered, result_throws,
                 refusal, wrong, runs, flips, reported>>
  /\ UNCHANGED EVOnly

\* 2.6.4: the second run of index q on the same base. When its bytes differ,
\* the own results above q ran on another base and go.
Rerun(o, q, r) ==
  IF r = o[q].res THEN o
  ELSE [k \in {x \in DOMAIN o : x <= q} |-> IF k = q THEN [o[q] EXCEPT !.res = r] ELSE o[k]]

\* @type: (Int, Int, $msg, <<Int, Int>> -> Seq($msg)) => Bool;
HandleAnswer(i, j, m, c) ==
  /\ counts' = IF role[i] = "Leader" /\ m.n = leader_state[i].n_leader
               THEN [counts EXCEPT ![i] = CountAnswer(@, j, m)]
               ELSE counts
  /\ chan' = c
  /\ UNCHANGED <<selected, own, verified_idx, proof, open_mark, mark_covered, result_throws,
                 answered, refusal, wrong, runs, flips, reported>>

\* @type: (Int, Int, $msg, <<Int, Int>> -> Seq($msg)) => Bool;
HandleTryAgain(i, j, m, c) ==
  LET q == m.idx IN
  IF EVNode(i) /\ m.n = promise[i] /\ q \in DOMAIN own[i] /\ q > Ptr(i) /\ OwnCurrent(i)
  THEN \E w \in WrongAfter(i, own[i][q].v, wrong) :
       \E r \in RunResults(i, own[i][q].v, own[i][q].base, w) :
         LET o2 == Rerun(own[i], q, r) IN
         /\ wrong' = w
         /\ own' = [own EXCEPT ![i] = o2]
         /\ runs' = runs \cup {RunOf(i, own[i][q].v, own[i][q].base, r)}
         /\ chan' = Send(c, i, One(j, AnswerMsg(i, o2, q, q)))
         /\ UNCHANGED <<selected, counts, verified_idx, proof, open_mark, mark_covered,
                        result_throws, answered, refusal, flips, reported>>
  ELSE /\ chan' = c
       /\ UNCHANGED evvars

\* @type: (Int, Int, $msg, <<Int, Int>> -> Seq($msg)) => Bool;
HandleRefused(i, j, m, c) ==
  /\ counts' = IF role[i] = "Leader" /\ m.n = leader_state[i].n_leader
               THEN [counts EXCEPT ![i] = [@ EXCEPT !.refusals = @ \cup {<<j, m.idx>>}]]
               ELSE counts
  /\ chan' = c
  /\ UNCHANGED <<selected, own, verified_idx, proof, open_mark, mark_covered, result_throws,
                 answered, refusal, wrong, runs, flips, reported>>

\* The head of the channel from j to i is a message of Execute-Verify.
ReceiveEV(j, i) ==
  /\ ExecuteVerify
  /\ i \in running
  /\ chan[<<j, i>>] # <<>>
  /\ LET m == Head(chan[<<j, i>>])
         c == [p \in Pairs |-> IF p = <<j, i>> THEN Tail(chan[p]) ELSE chan[p]]
     IN /\ m.type \in EVMessages
        /\ IF recovering[i] \/ deactivated[i]
           THEN /\ chan' = c
                /\ UNCHANGED evvars
           ELSE CASE m.type = "Answer"   -> HandleAnswer(i, j, m, c)
                  [] m.type = "TryAgain" -> HandleTryAgain(i, j, m, c)
                  [] m.type = "Refused"  -> HandleRefused(i, j, m, c)
  /\ UNCHANGED EVOnly

\* The leader runs again an index of its own that it asked itself to run again.
LeaderRunAgain(i) ==
  /\ EVNode(i)
  /\ role[i] = "Leader"
  /\ OwnCurrent(i)
  /\ \E q \in DOMAIN own[i] :
       /\ q > Ptr(i)
       /\ <<i, q>> \in counts[i].tried /\ <<i, q>> \notin counts[i].again
       /\ \E w \in WrongAfter(i, own[i][q].v, wrong) :
          \E r \in RunResults(i, own[i][q].v, own[i][q].base, w) :
            LET o2 == Rerun(own[i], q, r) IN
            /\ wrong' = w
            /\ own' = [own EXCEPT ![i] = o2]
            /\ runs' = runs \cup {RunOf(i, own[i][q].v, own[i][q].base, r)}
            /\ AnswerOut(i, AnswerMsg(i, o2, q, q))
  /\ UNCHANGED <<selected, verified_idx, proof, open_mark, mark_covered, result_throws, answered,
                 refusal, flips, reported>>
  /\ UNCHANGED EVOnly

\* 2.6.2: the leader counts the result of j for q only when j ran it on the
\* selected base and the pointer of the answer equals the pointer of the
\* leader. Mutation CountOtherBase leaves out the base.
SelRes(i, a, k) == IF k <= a.p THEN a.chosen[k].res ELSE selected[i][k].res
Counted(i, j, q) ==
  LET an == counts[i].ans[j]
      a  == Chosen(i)
  IN /\ an.ptr = a.p
     /\ q \in DOMAIN an.map /\ q <= Min(decided_idx[i], Len(log[i]))
     /\ \/ Mutation = "CountOtherBase"
        \/ \A k \in (an.ptr + 1)..(q - 1) :
             IsRequest(log[i][k]) =>
               /\ k \notin a.void
               /\ k <= a.p \/ k \in DOMAIN selected[i]
               /\ k \in DOMAIN an.map
               /\ an.map[k].res = SelRes(i, a, k)
Ans(i, j, q) == counts[i].ans[j].map[q].res

\* 2.6.3: a selection for q holds while its base is the base that chosen
\* and the selections below q define. A selection that does not hold is
\* counted again.
SelHolds(i, q) ==
  LET a == Chosen(i) IN
  /\ q \in DOMAIN selected[i]
  /\ \A k \in (a.p + 1)..(q - 1) : IsRequest(log[i][k]) => k \in DOMAIN selected[i]
  /\ selected[i][q].base = BaseWith(log[i], a, q, LAMBDA k : selected[i][k].res)

\* The request indexes above p that are decided at the leader and have no
\* selection that holds.
OpenIdx(i) ==
  {k \in (Ptr(i) + 1)..Min(decided_idx[i], Len(log[i])) :
     IsRequest(log[i][k]) /\ ~SelHolds(i, k)}

\* 2.6.3, 2.6.4: the leader selects the counted result that agrees with
\* the most counted results, ties to the lowest node id, once a majority
\* agrees with it. Before, it asks each counted node that does not agree to
\* run again, once. Mutation SelectWithoutMajority takes any counted result.
Select(i) ==
  LET qs   == OpenIdx(i)
      q    == CHOOSE k \in qs : \A x \in qs : k <= x
      CR   == {j \in ClusterNodes(i) : Counted(i, j, q)}
      Ag(c) == {j \in CR : Agree(Ans(i, j, q), Ans(i, c, q))}
      best == {c \in CR : \A x \in CR : Cardinality(Ag(c)) >= Cardinality(Ag(x))}
      cand == CHOOSE c \in best : \A x \in best : c <= x
  IN
  /\ EVNode(i)
  /\ role[i] = "Leader" /\ phase[i] = "Accept"
  /\ qs # {}
  /\ CR # {}
  /\ \E c \in (IF Mutation = "SelectWithoutMajority" THEN CR ELSE {cand}) :
       /\ Mutation = "SelectWithoutMajority" \/ Cardinality(Ag(c)) >= Majority(i)
       /\ LET dis == {j \in CR : ~Agree(Ans(i, j, q), Ans(i, c, q)) /\ <<j, q>> \notin counts[i].tried}
          IN /\ counts' = [counts EXCEPT ![i] = [@ EXCEPT !.tried = @ \cup {<<j, q>> : j \in dis}]]
             /\ chan' = Send(chan, i, [j \in dis \ {i} |->
                          <<[EmptyMessage EXCEPT !.type = "TryAgain", !.n = leader_state[i].n_leader,
                                                 !.idx = q]>>])
       /\ selected' = [selected EXCEPT ![i] =
                         (q :> [res |-> Ans(i, c, q), node |-> c,
                                base |-> counts[i].ans[c].map[q].base]) @@ @]
  /\ UNCHANGED <<own, verified_idx, proof, open_mark, mark_covered, result_throws, answered,
                 refusal, wrong, runs, flips, reported>>
  /\ UNCHANGED EVOnly

\* 2.6.4: the counted nodes that did not agree, also in their second answer.
Out(i, q) ==
  {x \in ClusterNodes(i) :
     /\ <<x, q>> \in counts[i].tried /\ <<x, q>> \in counts[i].again
     /\ Counted(i, x, q)
     /\ ~Agree(Ans(i, x, q), selected[i][q].res)}

\* Question 7.5, option: the leader appends rejected(k) in place of a throw
\* of a node whose result lies 2 apart with the same value.
SpreadOut(i, q) ==
  SpreadAction = "reject" /\ \E x \in Out(i, q) : Ans(i, x, q).rv = selected[i][q].res.rv

\* 2.6.5: with results selected for p + 1 .. t, and no verdict entry above
\* p that is not decided, the leader appends results(p + 1, t, list); when
\* the counted answers for k, the first request entry above p, leave no
\* result that a majority could reach, rejected(k). V takes rejected(k)
\* for that k (provisional: plan 2.3.1 still names p + 1). With Flip, one
\* step may replace a selected result by a wrong one, move a node into or
\* out of the list, or turn the selection for k into rejected(k) (4.3,
\* 4.13); with two flips left, it may replace a result and move a node in
\* one entry. Each change counts one flip. Mutation VerdictWithoutRequest
\* appends a results entry over a range with no request entry. Mutations
\* VerdictWhileOpen and StaleVerdictTaken leave out the guard on verdict
\* entries that are not decided.
AppendVerdict(i) ==
  LET a    == Chosen(i)
      lo   == a.p + 1
      reqs == {k \in lo..Len(log[i]) : IsRequest(log[i][k])}
      gap  == {k \in reqs : ~SelHolds(i, k) \/ SpreadOut(i, k)}
      hi   == IF gap = {} THEN Len(log[i]) ELSE (CHOOSE k \in gap : \A x \in gap : k <= x) - 1
      sel  == {k \in reqs : k <= hi}
      t    == CHOOSE k \in sel : \A x \in sel : x <= k
      q0   == CHOOSE k \in reqs : \A x \in reqs : k <= x
      lst  == [q \in sel |-> [res |-> selected[i][q].res, node |-> selected[i][q].node,
                              out |-> Out(i, q), base |-> selected[i][q].base, by |-> i]]
      left == IF Flip THEN MaxFlip - Cardinality(flips) ELSE 0
      Bad(q) == [rv |-> BadRv(i, log[i][q]), j |-> 0]
      Moved(o, x) == (o \ {x}) \cup ({x} \ o)
      flipped ==
        (IF left >= 1
         THEN {<<[lst EXCEPT ![q].res = Bad(q)], 1>> : q \in sel}
              \cup {<<[lst EXCEPT ![q].out = Moved(@, x)], 1>> : q \in sel, x \in ClusterNodes(i)}
         ELSE {})
        \cup (IF left >= 2
               THEN {<<[lst EXCEPT ![q].res = Bad(q), ![q].out = Moved(@, x)], 2>> :
                       q \in sel, x \in ClusterNodes(i)}
               ELSE {})
      CR0  == {j \in ClusterNodes(i) :
                 Counted(i, j, q0) /\ (<<j, q0>> \in counts[i].tried => <<j, q0>> \in counts[i].again)}
      U    == ClusterNodes(i) \ CR0
      Ag0(c) == {j \in CR0 : Agree(Ans(i, j, q0), Ans(i, c, q0))}
      noMaj == /\ Cardinality(U) < Majority(i)
               /\ \A c \in CR0 : Cardinality(Ag0(c)) + Cardinality(U) < Majority(i)
      rej  == RejectedEntry(q0, [x \in CR0 |-> Ans(i, x, q0)])
      m    == Len(log[i]) + 1
      cands ==
        (IF sel # {} THEN {<<ResultsEntry(lo, t, lst), 0>>}
                          \cup {<<ResultsEntry(lo, t, f[1]), f[2]>> : f \in flipped}
         ELSE {})
        \cup (IF reqs # {} /\ ~SelHolds(i, q0) /\ noMaj THEN {<<rej, 0>>} ELSE {})
        \cup (IF reqs # {} /\ SelHolds(i, q0) /\ SpreadOut(i, q0)
              THEN {<<RejectedEntry(q0, [x \in {y \in ClusterNodes(i) : Counted(i, y, q0)} |->
                                           Ans(i, x, q0)]), 0>>}
              ELSE {})
        \cup (IF reqs # {} /\ q0 \in sel /\ left >= 1
              THEN {<<RejectedEntry(q0, [x \in {y \in ClusterNodes(i) : Counted(i, y, q0)} |->
                                           Ans(i, x, q0)]), 1>>}
              ELSE {})
        \cup (IF Mutation = "VerdictWithoutRequest" /\ reqs = {} /\ lo <= Len(log[i])
              THEN {<<ResultsEntry(lo, Len(log[i]), EmptyFn), 0>>} ELSE {})
  IN
  /\ EVNode(i)
  /\ role[i] = "Leader" /\ phase[i] = "Accept"
  /\ ~AcceptedReconfiguration(i)
  /\ IF Mutation \in {"VerdictWhileOpen", "StaleVerdictTaken"} THEN TRUE
     ELSE \A k \in lo..Len(log[i]) : IsVerdict(log[i][k]) => k <= decided_idx[i]
  /\ \E x \in cands :
       /\ AcceptEntriesLeader(i, <<x[1]>>, chan)
       /\ proposed' = proposed \cup {x[1]}
       /\ flips' = flips \cup {<<i, m, leader_state[i].n_leader, c>> : c \in 1..x[2]}
       /\ counts' = [counts EXCEPT ![i] = [@ EXCEPT !.appended = @ \cup {m}]]
       /\ verified_idx' = [verified_idx EXCEPT ![i] = NewVerified(i, log[i] \o <<x[1]>>)]
  /\ UNCHANGED <<promise, n_accepted, decided_idx, role, phase, current_seq_num,
                 cached_promise_message, buffered_proposals>>
  /\ UNCHANGED <<env, recovery, rolled_back, client, readvars, buffered_stopsign, reconfig,
                 leasevars, statevars>>
  /\ UNCHANGED <<selected, own, proof, open_mark, mark_covered, result_throws, answered, refusal,
                 wrong, runs, reported>>
  /\ GhostUpdate

\* 2.7: node i checks the verdict entry at verified_idx + 1 on the check
\* base, V over its log before it. It uses its own result where it ran on
\* that base, else runs; where the result differs, it runs again. A node
\* that is not listed accepts when its result agrees; a listed node only
\* when its result still differs. A rejected(k) is accepted only when no
\* result of the counted answers and the own one has a majority (2.7.3). A
\* stale verdict entry changes nothing and passes. On accept, verified_idx
\* passes the entry and the node sends accepted; else it sends refused.
\* 2.7.2: the node checks only in the Accept phase of its promise, so the
\* entry is one that the leader of that ballot sent.
\* Mutations: AcceptWithoutCheck (2.7 off), ListedSkipsCheck (a listed
\* node accepts with no compare), RejectedUnchecked (2.7.3 off).
CheckVerdict(i) ==
  LET m    == verified_idx[i] + 1
      pre  == SubSeq(log[i], 1, m - 1)
      e    == log[i][m]
      cv   == V(pre)
      valid == VStep(SubSeq(log[i], 1, m), m, cv) # cv
      Qs   == IF ~valid THEN {}
              ELSE IF e.type = "Results" THEN DOMAIN e.list ELSE {e.from}
      full == e.type = "Results" =>
                \A k \in e.from..e.to : k < m /\ IsRequest(pre[k]) => k \in DOMAIN e.list
      BaseQ(q) == BaseWith(pre, cv, q, LAMBDA k : e.list[k].res)
      OwnOk(q) == /\ q \in DOMAIN own[i] /\ own[i][q].v = pre[q]
                  /\ own[i][q].base = BaseQ(q)
      ld   == BallotPid(promise[i])
      lead == role[i] = "Leader" /\ phase[i] = "Accept" /\ leader_state[i].n_leader = promise[i]
  IN
  /\ EVNode(i)
  /\ phase[i] = "Accept"
  /\ m <= Len(log[i])
  /\ IsVerdict(e)
  /\ refusal[i] # [n |-> promise[i], idx |-> m]
  /\ \E w \in {wrong} \cup UNION {WrongAfter(i, pre[q], wrong) : q \in (IF full THEN Qs ELSE {})} :
     LET RC(q) == RunResults(i, pre[q], BaseQ(q), w)
         All   == UNION {RC(q) \X RC(q) : q \in (IF full THEN Qs ELSE {})}
     IN
     \E f \in [(IF full THEN Qs ELSE {}) -> All] :
       /\ \A q \in DOMAIN f : f[q][1] \in RC(q) /\ f[q][2] \in RC(q)
       /\ LET R1(q) == IF OwnOk(q) THEN own[i][q].res ELSE f[q][1]
              Again(q) == ~Agree(R1(q), e.list[q].res)
              Listed(q) == i \in e.list[q].out
              OkResults(q) ==
                CASE Mutation = "AcceptWithoutCheck" -> TRUE
                  [] Listed(q) /\ Mutation = "ListedSkipsCheck" -> TRUE
                  [] Listed(q) -> Again(q) /\ ~Agree(f[q][2], e.list[q].res)
                  [] OTHER -> ~Again(q) \/ Agree(f[q][2], e.list[q].res)
              C == [x \in DOMAIN e.list \ {i} |-> e.list[x]] @@ (i :> R1(e.from))
              OkRejected ==
                \/ Mutation \in {"AcceptWithoutCheck", "RejectedUnchecked"}
                \/ \A c \in DOMAIN C :
                     Cardinality({x \in DOMAIN C : Agree(C[x], C[c])}) < Majority(i)
                \/ SpreadAction = "reject"
                   /\ \E x, y \in DOMAIN C : C[x].rv = C[y].rv /\ C[x].j - C[y].j \in {-2, 2}
              ok == \/ ~valid
                    \/ Mutation = "AcceptWithoutCheck"
                    \/ full /\ IF e.type = "Results" THEN \A q \in Qs : OkResults(q) ELSE OkRejected
              ran == IF Mutation = "AcceptWithoutCheck" \/ ~full THEN {}
                     ELSE {RunOf(i, pre[q], BaseQ(q), f[q][1]) : q \in {x \in Qs : ~OwnOk(x)}}
                          \cup (IF e.type = "Results"
                                THEN {RunOf(i, pre[q], BaseQ(q), f[q][2]) : q \in {x \in Qs : Again(x)}}
                                ELSE {})
              nv == VerifiedAfter(log[i], m)
              acc == [EmptyMessage EXCEPT !.type = "Accepted", !.n = promise[i],
                                          !.accepted_idx = nv, !.last = PrefixAt(log[i], nv)]
          IN /\ wrong' = w
             /\ runs' = runs \cup ran
             /\ IF ok
                THEN /\ verified_idx' = [verified_idx EXCEPT ![i] = nv]
                     /\ UNCHANGED <<refusal, counts>>
                     /\ IF lead
                        THEN HandleAccepted(i, i,
                               [acc EXCEPT !.accepted_idx = Max(nv, leader_state[i].accepted_indexes[i]),
                                           !.last = PrefixAt(log[i], Max(nv, leader_state[i].accepted_indexes[i]))],
                               chan)
                        ELSE /\ chan' = IF role[i] = "Follower" /\ phase[i] = "Accept" /\ ld # 0 /\ ld # i
                                        THEN Send(chan, i, One(ld, acc)) ELSE chan
                             /\ UNCHANGED <<durable, volatile>>
                ELSE /\ refusal' = [refusal EXCEPT ![i] = [n |-> promise[i], idx |-> m]]
                     /\ UNCHANGED <<verified_idx, durable, volatile>>
                     /\ IF lead
                        THEN /\ counts' = [counts EXCEPT ![i] = [@ EXCEPT !.refusals = @ \cup {<<i, m>>}]]
                             /\ UNCHANGED chan
                        ELSE /\ chan' = IF ld # 0 /\ ld # i
                                        THEN Send(chan, i, One(ld, [EmptyMessage EXCEPT !.type = "Refused",
                                                                    !.n = promise[i], !.idx = m]))
                                        ELSE chan
                             /\ UNCHANGED counts
  /\ UNCHANGED <<selected, own, proof, open_mark, mark_covered, result_throws, answered, flips,
                 reported>>
  /\ UNCHANGED <<buffered_stopsign, env, proposed, recovery, rolled_back, client, readvars,
                 reconfig, leasevars, statevars>>
  /\ GhostUpdate

\* The base of index q in a decided prefix: chosen results only.
ChosenBase(l, a, q) == BaseWith(l, a, q, LAMBDA k : NoResult)

\* 2.8.1: node i is listed at an index whose result is chosen and that it
\* has not applied, and it has not reported the leader of that entry.
Reported(i, a, q) == \E x \in reported : x.node = i /\ x.idx = q /\ x.chosen = a.chosen[q]
WouldThrow(i, a, q) == i \in a.chosen[q].out /\ ~Reported(i, a, q)
ListedAt(i) ==
  LET a == Chosen(i) IN
  {q \in DOMAIN a.chosen : q > Len(applied[i]) /\ WouldThrow(i, a, q)}
\* 2.7.6: refused by a majority, for a verdict entry of its own ballot.
RefusedAt(i) ==
  IF role[i] = "Leader"
  THEN {x \in counts[i].appended :
          Cardinality({j \in Servers : <<j, x>> \in counts[i].refusals}) >= Majority(i)}
  ELSE {}

\* Section 3: the throw writes the proof, and the start reads it in the same
\* step. A covered proof counts nothing. Else the count rises, the open mark
\* takes the index, and the node repairs while the count allows it (3.4:
\* the abort of the open transaction and the recovery, with its own log),
\* else it waits for the operator. A second covered proof while the same
\* mark is open counts. proof[i] keeps each proof that counted, with the
\* count n that it set (provisional: the plan clears it; the ghost serves
\* RepairsBounded). result_throws records whether the throw was covered,
\* and the open mark and mark_covered before it.
\* Mutation CountProofTwice counts a covered proof.
ThrowStart(i, t) ==
  LET cov == Covered(i, t.idx)
      cnt == repairs[i] + 1
  IN
  /\ IF cov \/ cnt <= AutoRepairs
     THEN /\ Restart({i}, [k \in {i} |-> DurableOf(k)], {k \in {i} : Quiet(k)}, Recovery)
          /\ UNCHANGED <<storage_faults, refused, wiped, crashes, wipes, losses, duplicates,
                         timeouts, elections, proposed, rolled_back, client, reconfig>>
          /\ GhostUpdateRP({i}, IF cov THEN {} ELSE {i})
     ELSE /\ running' = running \ {i}
          /\ refused' = refused \cup {i}
          /\ repairs' = [repairs EXCEPT ![i] = cnt]
          /\ stage1_ghost' = [j \in Nodes |-> IF j # i /\ read_only[j]
                                               THEN [stage1_ghost[j] EXCEPT !.down = @ \cup {i}]
                                               ELSE stage1_ghost[j]]
          /\ UNCHANGED <<durable, volatile, chan, wiped, crashes, wipes, losses, duplicates,
                         timeouts, elections, storage_faults, ghost, recovery, rolled_back,
                         past_durable, peer_seen, high_water, client, readvars,
                         buffered_stopsign, first_log, reconfigurations, force_single,
                         read_only, deactivated, halts, read_only_entries, build_source,
                         leasevars, leaseghost>>
  /\ open_mark' = IF cov THEN open_mark ELSE [open_mark EXCEPT ![i] = t.idx]
  /\ mark_covered' = [mark_covered EXCEPT ![i] = cov]
  /\ proof' = IF cov THEN proof
              ELSE [proof EXCEPT ![i] = @ \cup {[kind |-> t.kind, idx |-> t.idx, n |-> cnt]}]
  /\ result_throws' =
       Append(result_throws, t @@ [cov |-> cov, mark |-> open_mark[i], used |-> mark_covered[i]])
  /\ own' = [own EXCEPT ![i] = EmptyFn]
  /\ verified_idx' = [verified_idx EXCEPT ![i] = VerifiedAfter(log[i], Min(decided_idx[i], Len(log[i])))]
  /\ answered' = [answered EXCEPT ![i] = NoAnswered]
  /\ refusal' = [refusal EXCEPT ![i] = NoRefusal]
  /\ counts' = [counts EXCEPT ![i] = NoCounts]
  /\ selected' = [selected EXCEPT ![i] = EmptyFn]
  /\ UNCHANGED <<flips, reported, statevars>>

\* 2.8.1: a listed node runs the index again on the check base. It throws
\* only when that result does not agree with the chosen result; else it
\* reports the leader of the entry with a proof and applies (the ghost
\* reported). Mutation ThrowWithoutRerun throws without the second run.
\* 2.7.6: a leader throws for an entry that a majority refused. Each throw
\* is bounded by MaxResultThrow.
ResultThrow(i) ==
  LET a == Chosen(i)
      l == DecidedPrefix(i)
  IN
  /\ EVNode(i)
  /\ \/ \E q \in ListedAt(i) :
          LET b == ChosenBase(l, a, q)
              t == [node |-> i, idx |-> q, kind |-> "listed", v |-> l[q], base |-> b,
                    chosen |-> a.chosen[q].res]
          IN
          IF Mutation = "ThrowWithoutRerun"
          THEN /\ Len(result_throws) < MaxResultThrow
               /\ ThrowStart(i, t)
               /\ UNCHANGED <<wrong, runs>>
          ELSE \E w \in WrongAfter(i, l[q], wrong) :
               \E r \in RunResults(i, l[q], b, w) :
                 /\ wrong' = w
                 /\ runs' = runs \cup {RunOf(i, l[q], b, r)}
                 /\ IF Agree(r, a.chosen[q].res)
                    THEN /\ reported' = reported \cup {[node |-> i, idx |-> q, chosen |-> a.chosen[q],
                                                        leader |-> a.chosen[q].by]}
                         /\ UNCHANGED <<selected, counts, own, verified_idx, proof, open_mark,
                                        mark_covered, result_throws, answered, refusal, flips,
                                        statevars>>
                         /\ UNCHANGED vars_but_state
                    ELSE /\ Len(result_throws) < MaxResultThrow
                         /\ ThrowStart(i, t)
     \/ \E x \in RefusedAt(i) :
          LET t == [node |-> i, idx |-> x, kind |-> "refused", v |-> VoidEntry, base |-> <<>>,
                    chosen |-> NoResult]
          IN
          /\ Len(result_throws) < MaxResultThrow
          /\ ThrowStart(i, t)
          /\ UNCHANGED <<wrong, runs>>

\* 2.3.4: the node applies the chosen results up to its applied index. It
\* does not apply an index where it is listed, unless the open mark covers
\* it. The open mark closes when the applied index passes it (3.5).
\* Mutation ApplyOwnResult applies the own result.
ApplyChosen(i) ==
  LET a  == Chosen(i)
      l  == DecidedPrefix(i)
      d  == AppliedIdx(i)
      lo == Len(applied[i]) + 1
      Item(k) == [e |-> IF k \in a.void THEN VoidEntry ELSE l[k],
                  r |-> IF IsRequest(l[k]) /\ k \notin a.void
                        THEN IF Mutation = "ApplyOwnResult" /\ k \in DOMAIN own[i]
                             THEN own[i][k].res ELSE a.chosen[k].res
                        ELSE NoResult]
  IN
  /\ ExecuteVerify
  /\ i \in running /\ ~recovering[i]
  /\ d >= lo
  /\ \A k \in lo..d : k \in DOMAIN a.chosen /\ WouldThrow(i, a, k) =>
                        open_mark[i] # 0 /\ k <= open_mark[i]
  /\ applied' = [applied EXCEPT ![i] = @ \o [n \in 1..(d - lo + 1) |-> Item(lo + n - 1)]]
  /\ own' = [own EXCEPT ![i] = [k \in {x \in DOMAIN own[i] : x > d} |-> own[i][k]]]
  /\ open_mark' = IF open_mark[i] # 0 /\ d >= open_mark[i]
                  THEN [open_mark EXCEPT ![i] = 0] ELSE open_mark
  /\ UNCHANGED <<selected, counts, verified_idx, proof, mark_covered, result_throws, answered,
                 refusal, wrong, runs, flips, reported, in_txn>>
  /\ UNCHANGED vars_but_state

\* 2.5.3, 2.7.6: on the resend timeout a node sends its answer and its
\* refusal again.
ResendVerify(i) ==
  LET ld == BallotPid(promise[i])
      ms == (IF OwnCurrent(i) /\ MapOf(own[i], Ptr(i) + 1, MaxLog) # EmptyFn
             THEN <<AnswerMsg(i, own[i], Ptr(i) + 1, 0)>> ELSE <<>>)
            \o (IF refusal[i].n = promise[i]
                THEN <<[EmptyMessage EXCEPT !.type = "Refused", !.n = promise[i],
                                            !.idx = refusal[i].idx]>>
                ELSE <<>>)
  IN
  /\ EVNode(i)
  /\ timeouts < MaxTimeout
  /\ ld # 0 /\ ld # i
  /\ ms # <<>>
  /\ timeouts' = timeouts + 1
  /\ chan' = Send(chan, i, [k \in {ld} |-> ms])
  /\ UNCHANGED <<durable, volatile, wiped, crashes, wipes, losses, duplicates, elections,
                 storage_faults, refused, ghost, recovery, rollback, client, readvars,
                 buffered_stopsign, reconfig, leasevars, leaseghost, statevars, evvars>>

EVNext ==
  \/ \E i \in Servers : \/ RunEntry(i) \/ OwnAbort(i) \/ Reanswer(i) \/ LeaderRunAgain(i)
                        \/ Select(i) \/ AppendVerdict(i) \/ CheckVerdict(i)
                        \/ ResultThrow(i) \/ ApplyChosen(i) \/ ResendVerify(i)
  \/ \E p \in Pairs : ReceiveEV(p[1], p[2])

\* The variables of Execute-Verify after a step of the protocol: a restart,
\* a recovery, a repair or a stop aborts the open transaction (own) and
\* forgets the verified index, which then holds the decided prefix
\* (provisional); a new log keeps the verified part that it shares with
\* the old one. A node that is no leader, or a new ballot, has no counts
\* and no selection. A repair that counts sets the open mark at the length
\* of the log (plan 4.8); the operator clears the proofs.
EVFollows ==
  IF ~ExecuteVerify THEN UNCHANGED evvars
  ELSE
  LET Reset(i) == \/ wiped'[i] /\ ~wiped[i]
                  \/ i \notin running'
                  \/ recovering'[i] /\ ~recovering[i]
                  \/ repairs'[i] > repairs[i]
      Leads(i) == /\ role'[i] = "Leader" /\ ~Reset(i)
                  /\ leader_state'[i].n_leader = leader_state[i].n_leader
  IN
  /\ own' = [i \in Servers |-> IF Reset(i) THEN EmptyFn ELSE own[i]]
  /\ verified_idx' = [i \in Servers |->
                        IF Reset(i) THEN VerifiedAfter(log'[i], Min(decided_idx'[i], Len(log'[i])))
                        ELSE NewVerified(i, log'[i])]
  /\ answered' = [i \in Servers |-> IF Reset(i) \/ promise'[i] # promise[i] THEN NoAnswered
                                   ELSE answered[i]]
  /\ refusal' = [i \in Servers |-> IF Reset(i) \/ promise'[i] # promise[i] THEN NoRefusal
                                  ELSE refusal[i]]
  /\ counts' = [i \in Servers |-> IF Leads(i) THEN counts[i] ELSE NoCounts]
  /\ selected' = [i \in Servers |-> IF Leads(i) THEN selected[i] ELSE EmptyFn]
  /\ open_mark' = [i \in Servers |-> IF repairs'[i] > repairs[i] THEN Len(log[i]) ELSE open_mark[i]]
  /\ mark_covered' = [i \in Servers |-> IF repairs'[i] > repairs[i] THEN FALSE ELSE mark_covered[i]]
  /\ proof' = [i \in Servers |->
                 IF repairs'[i] > repairs[i]
                 THEN proof[i] \cup {[kind |-> "damage", idx |-> Len(log[i]), n |-> repairs'[i]]}
                 ELSE IF repairs'[i] < repairs[i] THEN {}
                 ELSE proof[i]]
  /\ UNCHANGED <<result_throws, wrong, runs, flips, reported>>

Next ==
  \/ ProtocolNext /\ StateFollows /\ EVFollows
  \/ (\E i \in Servers : StateRun(i) \/ StateApply(i) \/ StateAbort(i)) /\ UNCHANGED evvars
  \/ EVNext

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
\* The mutations of the lease, model/plan-witness-lease.md section 5, with the
\* direct property and then the far property:
\* DecideWithoutLease: a leader decides with no valid lease, in HandleAccepted
\* and in LeaderDecidesChosen (LeaseHonoured, NoConcurrentWriters).
\* ForcedWithoutWait: the forced node decides during the wait
\* (WaitHonoured, NoConcurrentWriters).
\* WitnessGrantsAnyEpoch: the witness grants a request of each epoch
\* (GrantOnlyCurrentEpoch, NoConcurrentWriters).
\* ForceWithoutRaise: ForceSingle does not raise the epoch at the witness
\* (ForceRaisesEpoch, NoConcurrentWriters).
\* WitnessForgetsEpoch: the witness restarts with epoch 0
\* (WitnessEpochGrows, NoConcurrentWriters).
\* LeaseFromReply: a grant sets the lease to LeaseLength from the reply
\* (LeaseWithinGrant, LeasesDisjoint).
\* AdoptEpochFromRefusal: a refusal writes witness_epoch into node_epoch
\* (EpochFromConfiguration, NoConcurrentWriters).
\* ReadWithoutLease: a leader starts a read round with no valid lease
\* (ReadRoundHonoursLease, StaleReadAfterForce).
\* NoDecideOnResend: the leader sends no Decide at a resend timeout, the form
\* before doc/plan-decide-on-resend.md (no property; the old arm).
\* DecideOnResendOldSeq: the Decide at a resend timeout carries the sequence
\* number of the last message and does not raise it, as leader.rs:371-403
\* with resend = true (S5).
\* The mutations of Execute-Verify, doc/plan-execute-verify.md 4.13, with the
\* invariant that each must break: ApplyOwnResult (StatesAgree),
\* CountOtherBase (ChosenOnOwnBase), SelectWithoutMajority (ChosenByMajority),
\* AcceptWithoutCheck (ChosenByMajority), ThrowWithoutRerun (ThrowOnlyWrong),
\* CountProofTwice (RepairsBounded), VerdictWithoutRequest
\* (VerdictCoversRequest), ListedSkipsCheck (ChosenByMajority, MaxFlip 2),
\* OwnAcceptUnchecked (PromiseIsVerified), StaleVerdictTaken with the guard
\* of VerdictWhileOpen off (ChosenOnOwnBase), VerdictWhileOpen alone (none,
\* the control of StaleVerdictTaken), RejectedUnchecked (RejectedHasNoMajority),
\* AppliedIsPointer (the cover NoIdleRead holds).
ASSUME Mutation \in {"none", "StopSignUnchecked", "PromiseWithoutAccepted", "SyncIgnored",
                     "LeaderReplacesValue", "SeqNumUnchecked",
                     "RequestNumberUnchecked", "LocalRead",
                     "AppendAfterStopSign", "BuildBeforeStopSignDecided",
                     "ForceWithoutLog", "ForceDecidedOnly", "OldNodeVotesAfterForce",
                     "ReadOnlyProposes", "ReadOnlyTruncates",
                     "DeactivatedStillVotes", "ResumeWithoutRecovery",
                     "BuildFromRecovering", "ForceWithoutStage1", "ForceAtTwoNodes",
                     "ReadBeforeServing", "StopSignLost", "ReadStartBeforeServing",
                     "DecideWithoutLease", "ForcedWithoutWait", "WitnessGrantsAnyEpoch",
                     "ForceWithoutRaise", "WitnessForgetsEpoch", "LeaseFromReply",
                     "AdoptEpochFromRefusal", "ReadWithoutLease",
                     "NoDecideOnResend", "DecideOnResendOldSeq",
                     "PowerLossWithF", "PowerLossStartAtAccepted", "PrimaryStartAtAccepted",
                     "StartCheckMissing", "StartAsNewNode", "NoAbortOnCut",
                     "ApplyOwnResult", "CountOtherBase", "SelectWithoutMajority",
                     "AcceptWithoutCheck", "ThrowWithoutRerun", "CountProofTwice",
                     "VerdictWithoutRequest", "ListedSkipsCheck", "OwnAcceptUnchecked",
                     "StaleVerdictTaken", "VerdictWhileOpen", "RejectedUnchecked",
                     "AppliedIsPointer"}
\* omni_paxos.rs:106 validate
ASSUME SingleNode \in BOOLEAN
ASSUME ~SingleNode => Cardinality(NextConfigNodes) # 1
ASSUME MaxForceSingle \in {0, 1}
ASSUME MaxForceSingle > 0 => SingleNode /\ Cardinality(NextConfigNodes) = 1
ASSUME MaxReadOnly \in Nat
ASSUME MaxHalt \in Nat
ASSUME MaxStorageFault \in Nat
ASSUME StartChecks \in BOOLEAN
ASSUME RunningFaults \in BOOLEAN
ASSUME StepChecks \in BOOLEAN
ASSUME VerifyCrc \in BOOLEAN
ASSUME ChainCheck \in BOOLEAN
ASSUME MetaRecord \in BOOLEAN
ASSUME PeerCheck \in BOOLEAN
ASSUME PeerCheck => MaxLoss = 0 /\ MaxDuplicate = 0
ASSUME HighWater \in BOOLEAN
ASSUME VerifyPromise \in BOOLEAN
ASSUME MismatchAction \in {"keep", "damage", "stop"}
ASSUME StateInLmdb \in BOOLEAN
ASSUME MaxInFlight \in Nat
ASSUME StateRepair \in {"keep", "copy"}
ASSUME StartDamageCounts \in BOOLEAN
ASSUME RepairFromPeer \in BOOLEAN
ASSUME StorageFaultKind \subseteq {"missing_entry", "missing_accepted", "missing_promise",
                                   "missing_decided", "stale_meta", "rollback"}
ASSUME StaleReadNode \in BOOLEAN
ASSUME ClientSessions \in BOOLEAN
ASSUME ReadIndex \in BOOLEAN
ASSUME NextConfigNodes \cap Nodes = {}
ASSUME \A i \in Servers : 0 < i /\ i < PidBase
ASSUME MaxReconfigure \in Nat
ASSUME Witness \in BOOLEAN
ASSUME LeaseLength \in Nat
ASSUME ExecuteVerify \in BOOLEAN
ASSUME MaxWrong \in Nat
ASSUME Spread \in BOOLEAN
ASSUME Flip \in BOOLEAN
ASSUME MaxFlip \in Nat
ASSUME MaxResultThrow \in Nat
ASSUME SpreadAction \in {"throw", "reject"}

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

\* HighWater: no step of a node that is not a damage takes its durable state
\* below its mark. A break is a false report of a damage.
HighWaterHolds ==
  HighWater => \A i \in running : ~BelowMark(Mark(DurableOf(i)), high_water[i])

\* The 19:38 rule of the owner: outside a transaction a node knows only
\* its state machine. The copy accepted_idx of the code at 4c57d44 is the
\* log length that LMDB holds. Where this holds, the model can read the
\* log length in place of the copy.
AcceptedIdxIsLogLen == \A i \in running : accepted_idx[i] = Len(log[i])

\* MismatchAction: where a follower finds a mismatch, the prefix in the
\* message agrees with the decided history. So the follower is the node
\* that differs, and "damage" blames the right node.
MismatchBlamesFollower ==
  \A i \in Nodes, j \in Servers :
    (<<j, i>> \in Pairs /\ Mismatch(i, j)) =>
      LET l == Head(chan[<<j, i>>]).last
      IN IsPrefix(l, decided_history) \/ IsPrefix(decided_history, l)

\* Cover: a mismatch happens. A run with an undetected damage must violate it.
NoMismatch == \A i \in Nodes, j \in Servers : <<j, i>> \in Pairs => ~Mismatch(i, j)

\* Cover of the repair limit (owner, 2026-10-24 20:55): a node that used its
\* automatic repair and then found a second damage leaves running. A run
\* with two faults must violate it.
NoRepairLimitReached == ~\E i \in Servers : repairs[i] = AutoRepairs /\ i \in refused

S1 == \A i, j \in Servers : Votes(i) /\ Votes(j) => Compatible(DecidedPrefix(i), DecidedPrefix(j))

S2 == \A i \in Servers : Votes(i) => IsPrefix(DecidedPrefix(i), decided_history)

\* doc/plan-lmdb-only-source.md 2.6 item 5: an entry runs on the state of
\* the entries before it in the log. Each node that holds a decided entry
\* at its index holds it after the same entries, so its run gives the
\* result that the decided entry carries. A duplicated forward can put the
\* same value at a second index (round 12); there the session and the
\* request number make the second run a no-op (2.6 item 7). A node that
\* accepted in an older ballot can hold the value after other entries; its
\* run counts only in that ballot, and the sync of the new ballot cuts it
\* (model round 12, plan 2.6 item 7). So j accepted in the ballot of i.
EntryRunsOnOnePrefix ==
  \A i, j \in Servers :
    Votes(i) /\ Votes(j) /\ n_accepted[j] = n_accepted[i] =>
      \A k \in 1..Min(decided_idx[i], Len(log[j])) :
        log[j][k] = log[i][k] => SubSeq(log[j], 1, k - 1) = SubSeq(log[i], 1, k - 1)

\* StateInLmdb: the committed state of each node holds only decided
\* entries, in the decided order. A read from the state then gives a
\* decided result.
StateIsDecided ==
  \A i \in Servers :
    IF ExecuteVerify
    THEN IsPrefix([k \in DOMAIN applied[i] |-> applied[i][k].e], EffPrefix(decided_history))
    ELSE IsPrefix(applied[i], decided_history)

\* The open transaction of a node that compared it with its log holds the
\* entries of its log in order (2.6 item 5).
OpenRunsOnLog == \A i \in running : StateCurrent(i) => IsPrefix(applied[i] \o in_txn[i], log[i])

\* Covers of StateInLmdb: two entries run before their decide; a decide
\* covers only part of the entries that ran; a write cut the log below the
\* end of an open transaction.
NoTwoInFlight == \A i \in Servers : Len(in_txn[i]) < 2
NoPartialDecide == \A i \in Servers :
  ~(Len(applied[i]) < decided_idx[i] /\ decided_idx[i] < Len(applied[i]) + Len(in_txn[i]))
NoCutOfOpen == \A i \in running : in_txn[i] # <<>> => IsPrefix(applied[i] \o in_txn[i], log[i])

\* Cover: a leader holds two entries that are not decided.
NoPipeline == \A i \in Servers : role[i] = "Leader" => Len(log[i]) < decided_idx[i] + 2

S2Grows == [][IsPrefix(decided_history, decided_history')]_vars

\* doc/plan-start-checks.md: an undamaged durable state passes the checks.
NoFalseRefusal == \A i \in Servers \ refused : ~ChecksFind(DurableOf(i))

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
E1 == /\ ExecutedOnce(IF ExecuteVerify THEN EffPrefix(decided_history) ELSE decided_history)
      /\ \A i \in Servers : Votes(i) => ExecutedOnce(ClientPrefix(i))

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

----------------------------------------------------------------------------
(* Properties of the lease at the witness, model/plan-witness-lease.md      *)
(* section 4. They hold with Witness TRUE. Names for the owner, no source.  *)

\* The new variables stay in their ranges.
\* @type: Bool;
LeaseTypeInvariant ==
  /\ witness_epoch \in Nat
  /\ node_epoch \in [Servers -> Nat]
  /\ lease_request \in [Nodes -> -1..LeaseLength]
  /\ lease_reply \in [Nodes -> {-1, 0, 1}]
  /\ lease_remaining \in [Nodes -> 0..LeaseLength]
  /\ wait_remaining \in -1..LeaseLength
  /\ forced_decided_at_force \in Nat
  /\ forced_has_decided \in BOOLEAN
  /\ old_decided_max \in Nat
  /\ old_decided_after_forced \in BOOLEAN
  /\ \A r \in DOMAIN reads : reads[r].forced_decided \in BOOLEAN

\* A leader of the old cluster that stays in (Leader, Accept) and raises its
\* decided_idx starts the step with a valid lease. Only the decide branch of
\* HandleAccepted and LeaderDecidesChosen raise decided_idx at a node that is
\* in (Leader, Accept) before and after the step.
LeaseHonoured ==
  [][\A i \in Nodes :
       (/\ role[i] = "Leader" /\ phase[i] = "Accept"
        /\ role'[i] = "Leader" /\ phase'[i] = "Accept"
        /\ decided_idx'[i] > decided_idx[i])
         => lease_remaining[i] > 0]_vars

\* After ForceSingle, a step that raises decided_idx of the forced node above
\* its value at ForceSingle starts after the wait.
WaitHonoured ==
  [][\A s \in NextConfigNodes :
       (/\ force_single.node # 0
        /\ decided_idx'[s] > decided_idx[s]
        /\ decided_idx'[s] > forced_decided_at_force)
         => wait_remaining = 0]_vars

GrantOnlyCurrentEpoch ==
  [][\A i \in Nodes : lease_reply[i] # 1 /\ lease_reply'[i] = 1 => node_epoch[i] = witness_epoch]_vars

\* @type: Bool;
EpochFromConfiguration == \A i \in Nodes : node_epoch[i] = 0

\* The action part of EpochFromConfiguration.
EpochFromConfigurationAction ==
  [][\A s \in NextConfigNodes :
       node_epoch'[s] # node_epoch[s] => force_single.node = 0 /\ force_single'.node # 0]_vars

ForceRaisesEpoch ==
  [][force_single.node = 0 /\ force_single'.node # 0 => witness_epoch' > witness_epoch]_vars

WitnessEpochGrows == [][witness_epoch' >= witness_epoch]_vars

\* The step that sets the lease from a grant gives at most the time that
\* remains of the request.
LeaseWithinGrant ==
  [][\A i \in Nodes :
       lease_reply[i] = 1 /\ lease_reply'[i] = -1 => lease_remaining'[i] <= lease_request[i]]_vars

\* Only StartReadRound changes read_round[i].id.
ReadRoundHonoursLease ==
  [][\A i \in Nodes : read_round'[i].id # read_round[i].id => lease_remaining[i] > 0]_vars

\* @type: Bool;
LeasesDisjoint ==
  force_single.node # 0 /\ wait_remaining = 0 => \A i \in Nodes : lease_remaining[i] = 0

\* It checks the claim that a follower needs no lease.
OldDecidedFrozen == [][wait_remaining = 0 => old_decided_max' = old_decided_max]_vars

\* @type: Bool;
NoConcurrentWriters == ~old_decided_after_forced

\* @type: Bool;
StaleReadAfterForce ==
  \A r \in DOMAIN reads :
    ~ /\ reads[r].state = "done"
      /\ reads[r].node \in Nodes
      /\ reads[r].forced_decided
      /\ ~reads[r].stale

(* Run MC_w_reach. A check of the checks: each invariant must break, so    *)
(* that the case is in the state space. Provisional: the plan asks for      *)
(* ghost counters; these invariants read the existing ghosts instead.       *)
(* Names for the owner, no source.                                          *)

\* An old node decides during the wait.
\* @type: Bool;
OldDecidesNothingDuringWait ==
  ~ (wait_remaining > 0 /\ old_decided_max > Len(force_single.history))

\* The forced node decides after the wait an entry that differs from an
\* entry that an old node decided.
\* @type: Bool;
ForcedAgreesWithOld ==
  forced_has_decided =>
    \A s \in NextConfigNodes, k \in Nodes : Compatible(DecidedPrefix(s), DecidedPrefix(k))

\* The witness refuses a request after ForceSingle.
\* @type: Bool;
NoRefusalAfterForce == ~ (force_single.node # 0 /\ \E i \in Nodes : lease_reply[i] = 0)

(* Invariants and covers of Execute-Verify, doc/plan-execute-verify.md     *)
(* 4.11 and 4.12. Each cover must be violated by a trace.                    *)

\* Each chosen result agrees with the runs on its base of a majority.
ChosenByMajority ==
  \A i \in Servers :
    LET l == DecidedPrefix(i)
        a == V(l)
    IN \A q \in DOMAIN a.chosen :
         Cardinality({n \in Servers :
                        \E r \in runs : /\ r.node = n /\ r.v = l[q]
                                       /\ r.base = ChosenBase(l, a, q)
                                       /\ Agree(r.res, a.chosen[q].res)}) >= Majority(i)

\* Without Spread, each chosen result agrees with the right result on its base.
ChosenIsRight ==
  Spread \/
  \A i \in Servers :
    LET l == DecidedPrefix(i)
        a == V(l)
    IN \A q \in DOMAIN a.chosen : a.chosen[q].res.rv = RightRv(l[q], ChosenBase(l, a, q))

\* Each chosen result ran on the base that the chosen results before it define.
ChosenOnOwnBase ==
  \A i \in Servers :
    LET l == DecidedPrefix(i)
        a == V(l)
    IN \A q \in DOMAIN a.chosen : a.chosen[q].base = ChosenBase(l, a, q)

StatesAgree == \A i, j \in Servers : Compatible(applied[i], applied[j])

\* Only a node with a wrong run of the entry on the chosen base takes a
\* listed throw; with Spread a run 2 away from the chosen result counts as
\* wrong (provisional, question 7.5). A leader throws for a refused entry
\* only where it flipped that entry.
ThrowOnlyWrong ==
  \A n \in 1..Len(result_throws) :
    LET t == result_throws[n] IN
    IF t.kind = "listed"
    THEN \E r \in runs :
           /\ r.node = t.node /\ r.v = t.v /\ r.base = t.base
           /\ \/ r.res.rv.bad # 0
              \/ Spread /\ r.res.rv = t.chosen.rv /\ r.res.j - t.chosen.j \in {-2, 2}
    ELSE \E f \in flips : f[1] = t.node /\ f[2] = t.idx

\* The count of a node never passes the proofs that counted, and a node that
\* runs has a count of at most AutoRepairs.
RepairsBounded ==
  \A i \in Nodes :
    /\ repairs[i] <= Cardinality(proof[i])
    /\ i \in running => repairs[i] <= AutoRepairs

\* 3.3: the first proof at or below an open mark counts nothing.
CoveredProofCountsNothing ==
  \A n \in 1..Len(result_throws) :
    LET t == result_throws[n] IN
    t.mark # 0 /\ t.idx <= t.mark /\ ~t.used => t.cov

\* Each results entry covers a request entry below it.
VerdictCoversRequest ==
  \A i \in Servers : \A k \in 1..Len(log[i]) :
    IsVerdict(log[i][k]) /\ log[i][k].type = "Results" =>
      HasRequest(log[i], log[i][k].from, Min(log[i][k].to, k - 1))

\* With SpreadAction "throw", a decided rejected(k) has no result that a
\* majority of the counted runs on its base agree on. The counted runs are
\* the answers that the entry carries (provisional: a run outside the count,
\* such as the fresh run of a check, can agree with a counted one).
RejectedHasNoMajority ==
  SpreadAction # "throw" \/
  \A i \in Servers :
    LET l == DecidedPrefix(i)
        a == V(l)
    IN \A m \in 1..Len(l) :
         IsVerdict(l[m]) /\ l[m].type = "Rejected" /\ l[m].from \in a.void =>
           \A c \in DOMAIN l[m].list :
             Cardinality({x \in DOMAIN l[m].list : Agree(l[m].list[x], l[m].list[c])})
               < Majority(i)

\* A promise carries no entry above the verified_idx of its sender, and the
\* accept that a leader counts for itself is no promise above it either.
PromiseIsVerified ==
  \A i \in Servers :
    /\ role[i] = "Leader" => leader_state[i].accepted_indexes[i] <= verified_idx[i]
    /\ cached_promise_message[i] # <<>> /\ phase[i] = "Prepare"
       /\ cached_promise_message[i][1].n = promise[i] =>
         cached_promise_message[i][1].accepted_idx <= verified_idx[i]

NoResults == \A i \in Servers : V(DecidedPrefix(i)).chosen = EmptyFn
NoReject == \A i \in Servers : V(DecidedPrefix(i)).void = {}
NoFollowerThrow == \A n \in 1..Len(result_throws) : result_throws[n].kind # "listed"
NoLeaderThrow == \A n \in 1..Len(result_throws) : result_throws[n].kind # "refused"
\* A results entry is decided in a later ballot than the first entry.
NoResultsAfterLeaderChange ==
  \A k \in 1..Len(decided_history) :
    IsVerdict(decided_history[k]) /\ decided_history[k].type = "Results" =>
      decided_ballot[k] = decided_ballot[1]
\* A node runs one entry on two bases (2.4.3).
NoRunAgain == \A r1, r2 \in runs : r1.node = r2.node /\ r1.v = r2.v => r1.base = r2.base
NoTryAgain == \A i \in Servers : counts[i].tried = {}
\* 3.3: a node whose throws all name one index, one of them covered, waits
\* for the operator.
NoOperatorWait ==
  ~ \E i \in Nodes :
      LET ts == {n \in 1..Len(result_throws) : result_throws[n].node = i} IN
      /\ i \in refused /\ repairs[i] > AutoRepairs
      /\ \E n \in ts : result_throws[n].cov
      /\ \A n, x \in ts : result_throws[n].idx = result_throws[x].idx
\* A read answered on a prefix that ends with a verdict entry.
NoIdleRead ==
  \A r \in DOMAIN reads :
    reads[r].state = "done" /\ ~reads[r].stale /\ reads[r].result # <<>> =>
      ~IsVerdict(reads[r].result[Len(reads[r].result)])

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
    power_loss_completed, rollback, client, readvars, buffered_stopsign, reconfig, leasevars, leaseghost>>
\* Cover of doc/plan-decide-on-resend.md: a follower in Recover at sequence
\* number {1, 2} with decided index 0 while a leader in Accept has decided
\* index 1. A run must violate it.
NoSeed4 == ~\E k, l \in Nodes : role[k] = "Follower" /\ phase[k] = "Recover"
             /\ current_seq_num[k] = [session |-> 1, counter |-> 2] /\ decided_idx[k] = 0
             /\ role[l] = "Leader" /\ phase[l] = "Accept" /\ decided_idx[l] = 1

\* Reach check of the power-loss mode (plan-power-loss.md R7): a run must
\* violate it, so that a node completes in that mode while every node of
\* its configuration recovers.
NoPowerLossCompletion == power_loss_completed = {}

=============================================================================
