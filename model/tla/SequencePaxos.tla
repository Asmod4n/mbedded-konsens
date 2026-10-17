---------------------------- MODULE SequencePaxos ----------------------------
(* Model S of Omni-Paxos Sequence Paxos, origin github.com/haraldng/omnipaxos *)
(* commit 532ff79. One configuration, no StopSign, no compaction, no BLE.     *)
(* Files cited: mod.rs, leader.rs, follower.rs = omnipaxos/src/sequence_paxos/ *)
(* internal_storage.rs = omnipaxos/src/storage/, util.rs = omnipaxos/src/,    *)
(* memory_storage.rs = omnipaxos_storage/src/.                                *)
EXTENDS Naturals, Sequences, FiniteSets, TLC

CONSTANTS Nodes, Values, MaxBallot, MaxLog, MaxCounter, MaxSession,
          Cap, MaxCrash, MaxWipe, MaxLoss, MaxDuplicate, MaxTimeout, MaxElection, RuleR,
          DecidedIdxMonotone, MaxRollback, Recovery

VARIABLES
  promise, n_accepted, log, decided_idx,
  role, phase, accepted_idx, current_seq_num, cached_promise_message,
  buffered_proposals, leader_state,
  chan,
  wiped, crashes, wipes, losses, duplicates, timeouts, elections,
  proposed, decided_history, decided_ballot,
  recovering, nonce, recovery_responses,
  rolled_back, past_durable

durable  == <<promise, n_accepted, log, decided_idx>>
volatile == <<role, phase, accepted_idx, current_seq_num,
              cached_promise_message, buffered_proposals, leader_state>>
env      == <<wiped, crashes, wipes, losses, duplicates, timeouts, elections>>
ghost    == <<proposed, decided_history, decided_ballot>>
recovery == <<recovering, nonce, recovery_responses>>
rollback == <<rolled_back, past_durable>>
vars     == <<durable, volatile, chan, env, ghost, recovery, rollback>>

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
Peers(i) == Nodes \ {i}
Pairs == {p \in Nodes \X Nodes : p[1] # p[2]}

\* util.rs:445 Quorum::Majority(num_nodes / 2 + 1)
Majority == Cardinality(Nodes) \div 2 + 1

\* util.rs:371-378 SequenceNumber, derived lexicographic order
DefaultSeq == [session |-> 0, counter |-> 0]
SeqLeq(a, b) == a.session < b.session \/ (a.session = b.session /\ a.counter <= b.counter)

\* util.rs:382-390 check_msg_status
CheckMsgStatus(cur, msg) ==
  IF msg.session = cur.session /\ msg.counter = cur.counter + 1 THEN "Expected"
  ELSE IF SeqLeq(msg, cur) THEN "Outdated"
  ELSE "DroppedPreceding"

\* Indices are 0-based as in the source. memory_storage.rs:102-107 get_suffix
Suffix(l, k) == IF k >= Len(l) THEN <<>> ELSE SubSeq(l, k + 1, Len(l))
\* memory_storage.rs:64-67 append_on_prefix, Vec::truncate keeps a shorter log
AppendOnPrefix(l, k, s) == (IF k >= Len(l) THEN l ELSE SubSeq(l, 1, k)) \o s

IsPrefix(a, b) == Len(a) <= Len(b) /\ SubSeq(b, 1, Len(a)) = a
Compatible(a, b) == IsPrefix(a, b) \/ IsPrefix(b, a)

\* util.rs:62-71 PromiseState, util.rs:27-34 PromiseMetaData
NotPromised == [state |-> "NotPromised", n_accepted |-> 0, accepted_idx |-> 0, decided_idx |-> 0]
PromisedHigher == [state |-> "PromisedHigher", n_accepted |-> 0, accepted_idx |-> 0, decided_idx |-> 0]
DefaultMeta == [n_accepted |-> 0, accepted_idx |-> 0, decided_idx |-> 0, pid |-> 0]

\* util.rs:96-108 LeaderState::with
LeaderStateWith(n) ==
  [n_leader          |-> n,
   promises_meta     |-> [k \in Nodes |-> NotPromised],
   follower_seq_nums |-> [k \in Nodes |-> DefaultSeq],
   accepted_indexes  |-> [k \in Nodes |-> 0],
   max_promise_meta  |-> DefaultMeta,
   max_promise_sync  |-> <<>>]

\* Rule R: a node whose storage was lost sends no Promise and no Accepted
\* and counts in no quorum.
Votes(i) == ~(RuleR /\ wiped[i])

\* A send to a full channel is a loss.
RECURSIVE Enqueue(_, _)
Enqueue(q, ms) ==
  IF ms = <<>> THEN q
  ELSE Enqueue(IF Len(q) < Cap THEN Append(q, Head(ms)) ELSE q, Tail(ms))

\* out is a function from destinations to the sequence of messages for each.
Send(c, i, out) ==
  [p \in Pairs |-> IF p[1] = i /\ p[2] \in DOMAIN out
                   THEN Enqueue(c[p], out[p[2]]) ELSE c[p]]

NoOut == [k \in {} |-> <<>>]
One(j, m) == [k \in {j} |-> <<m>>]
IfVotes(i, j, m) == IF Votes(i) THEN One(j, m) ELSE NoOut

Promised(ls) == {k \in Nodes : ls.promises_meta[k].state = "Promised"}
\* util.rs:197-208 get_promised_followers
PromisedFollowers(ls) == {k \in Promised(ls) : k # BallotPid(ls.n_leader)}

\* util.rs:131-149 set_promise. A tie in (n_accepted, accepted_idx) with
\* another pid is resolved by `replace`; the source keeps the first.
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
IsChosen(ls, idx) == Cardinality({k \in Nodes : ls.accepted_indexes[k] >= idx}) >= Majority

\* util.rs:168-177 get_max_decided_idx
MaxDecided(ls) ==
  LET ds == {ls.promises_meta[k].decided_idx : k \in Promised(ls)}
  IN IF ds = {} THEN 0 ELSE CHOOSE d \in ds : \A e \in ds : d >= e

\* util.rs:121-125 next_seq_num, applied to a set of followers
NextSeqNums(ls, fs) ==
  [ls EXCEPT !.follower_seq_nums =
     [k \in Nodes |-> IF k \in fs
                      THEN [session |-> @[k].session, counter |-> @[k].counter + 1]
                      ELSE @[k]]]

\* leader.rs:97-109 send_prepare
PrepareMsg(i) ==
  [type |-> "Prepare", n |-> leader_state[i].n_leader, decided_idx |-> decided_idx[i],
   n_accepted |-> n_accepted[i], accepted_idx |-> accepted_idx[i]]

PrepareReqMsg(b) == [type |-> "PrepareReq", n |-> b]

\* leader.rs:150-191 send_accsync with mod.rs:405-437 create_log_sync
\* (no snapshots: suffix from common_prefix_idx, sync_idx = common_prefix_idx).
\* The session is incremented first (util.rs:115-119), then next_seq_num.
AccSyncSeq(ls, to) == [session |-> ls.follower_seq_nums[to].session + 1, counter |-> 1]
AccSyncMsg(ls, to, lg, dec) ==
  LET mm    == ls.max_promise_meta
      fm    == ls.promises_meta[to]
      valid == IF fm.n_accepted = ls.n_leader THEN fm.accepted_idx
               ELSE IF fm.n_accepted = mm.n_accepted THEN Min(mm.accepted_idx, fm.accepted_idx)
               ELSE fm.decided_idx
  IN [type |-> "AcceptSync", n |-> ls.n_leader, seq_num |-> AccSyncSeq(ls, to),
      decided_idx |-> dec, log_sync |-> [suffix |-> Suffix(lg, valid), sync_idx |-> valid]]
WithAccSyncSeqs(ls, fs) ==
  [ls EXCEPT !.follower_seq_nums =
     [k \in Nodes |-> IF k \in fs THEN AccSyncSeq(ls, k) ELSE @[k]]]

\* F6: the durable state of node k as one record.
DurableOf(k) ==
  [promise |-> promise[k], n_accepted |-> n_accepted[k], log |-> log[k],
   decided_idx |-> decided_idx[k]]

(* Recovery of Viewstamped Replication Revisited (Liskov, Cowling,         *)
(* MIT-CSAIL-TR-2012-021, section 4.3), applied to Sequence Paxos (plan     *)
(* item 26). Names from that paper: Recovery, RecoveryResponse, nonce. A    *)
(* node in (Leader, Accept) is in status normal and is the primary of its   *)
(* ballot. Only the primary sends its log, as in the paper. The log is sent *)
(* whole: a suffix from index 0.                                            *)
RecoveryMsg(x) == [type |-> "Recovery", nonce |-> x]

RecoveryResponseMsg(k, x) ==
  LET primary == role[k] = "Leader" /\ phase[k] = "Accept" IN
  [type |-> "RecoveryResponse", nonce |-> x, promise |-> promise[k],
   primary |-> primary,
   n_accepted |-> IF primary THEN n_accepted[k] ELSE DefaultBallot,
   log |-> IF primary THEN log[k] ELSE <<>>,
   decided_idx |-> IF primary THEN decided_idx[k] ELSE 0]

\* The answers are complete with f + 1 = Majority answers from other nodes,
\* one of them from the primary of the highest ballot among the answers.
RecoveryPrimaries(rs) ==
  LET mp == CHOOSE p \in {r.promise : r \in rs} : \A r \in rs : p >= r.promise
  IN {r \in rs : r.primary /\ r.promise = mp /\ BallotPid(mp) = r.from}
RecoveryComplete(rs) == Cardinality(rs) >= Majority /\ RecoveryPrimaries(rs) # {}

----------------------------------------------------------------------------
Init ==
  /\ promise = [i \in Nodes |-> DefaultBallot]
  /\ n_accepted = [i \in Nodes |-> DefaultBallot]
  /\ log = [i \in Nodes |-> <<>>]
  /\ decided_idx = [i \in Nodes |-> 0]
  /\ role = [i \in Nodes |-> "Follower"]
  /\ phase = [i \in Nodes |-> "None"]
  /\ accepted_idx = [i \in Nodes |-> 0]
  /\ current_seq_num = [i \in Nodes |-> DefaultSeq]
  /\ cached_promise_message = [i \in Nodes |-> <<>>]
  /\ buffered_proposals = [i \in Nodes |-> <<>>]
  /\ leader_state = [i \in Nodes |-> LeaderStateWith(DefaultBallot)]
  /\ chan = [p \in Pairs |-> <<>>]
  /\ wiped = [i \in Nodes |-> FALSE]
  /\ crashes = 0
  /\ wipes = 0
  /\ losses = 0
  /\ duplicates = 0
  /\ timeouts = 0
  /\ elections = 0
  /\ proposed = {}
  /\ decided_history = <<>>
  /\ decided_ballot = <<>>
  /\ recovering = [i \in Nodes |-> FALSE]
  /\ nonce = [i \in Nodes |-> 0]
  /\ recovery_responses = [i \in Nodes |-> {}]
  /\ rolled_back = {}
  /\ past_durable = [i \in Nodes |-> {}]

----------------------------------------------------------------------------
(* Shared steps. Each sets the primed node variables it changes and chan'. *)

\* mod.rs:371-384 forward_proposals
ForwardProposals(i, es, c) ==
  LET ld == BallotPid(promise[i]) IN
  IF ld > 0 /\ ld # i
  THEN /\ chan' = Send(c, i, One(ld, [type |-> "ProposalForward", entries |-> es]))
       /\ UNCHANGED buffered_proposals
  ELSE /\ buffered_proposals' = [buffered_proposals EXCEPT ![i] = @ \o es]
       /\ chan' = c

\* leader.rs:123-133 accept_entries_leader, leader.rs:193-221 send_acceptdecide
AcceptEntriesLeader(i, es, c) ==
  LET acc == accepted_idx[i] + Len(es)
      ls0 == [leader_state[i] EXCEPT !.accepted_indexes[i] = IF Votes(i) THEN acc ELSE @]
      fs  == PromisedFollowers(ls0)
      ls1 == NextSeqNums(ls0, fs)
  IN /\ log' = [log EXCEPT ![i] = @ \o es]
     /\ accepted_idx' = [accepted_idx EXCEPT ![i] = acc]
     /\ leader_state' = [leader_state EXCEPT ![i] = ls1]
     /\ chan' = Send(c, i, [k \in fs |->
                  <<[type |-> "AcceptDecide", n |-> ls1.n_leader,
                     seq_num |-> ls1.follower_seq_nums[k],
                     decided_idx |-> decided_idx[i], entries |-> es]>>])

\* mod.rs:343-357 reconnected, returns [recover, out]
Reconnect(i, pid) ==
  [recover |-> pid = BallotPid(promise[i]),
   out     |-> One(pid, PrepareReqMsg(promise[i]))]

\* follower.rs:196-227 check_valid_ballot, returns [valid, recover, out]
CheckValidBallot(i, b) ==
  IF promise[i] = b THEN [valid |-> TRUE, recover |-> FALSE, out |-> NoOut]
  ELSE IF promise[i] > b
       THEN [valid |-> FALSE, recover |-> FALSE,
             out |-> One(BallotPid(b), [type |-> "NotAccepted", n |-> promise[i]])]
       ELSE LET r == Reconnect(i, BallotPid(b))
            IN [valid |-> FALSE, recover |-> r.recover, out |-> r.out]

\* Effect of an invalid ballot or of DroppedPreceding: Recover and PrepareReq.
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
HandlePrepareReq(i, j, m, c) ==
  IF role[i] = "Leader" /\ m.n <= leader_state[i].n_leader
  THEN /\ leader_state' = [leader_state EXCEPT ![i].promises_meta[j] = NotPromised]
       /\ chan' = Send(c, i, One(j, PrepareMsg(i)))
       /\ UNCHANGED <<durable, role, phase, accepted_idx, current_seq_num,
                      cached_promise_message, buffered_proposals>>
  ELSE /\ chan' = c
       /\ UNCHANGED <<durable, volatile>>

\* follower.rs:13-51 handle_prepare
HandlePrepare(i, j, m, c) ==
  IF promise[i] < m.n \/ (promise[i] = m.n /\ phase[i] = "Recover")
  THEN LET na  == n_accepted[i]
           acc == accepted_idx[i]
           ls  == IF na > m.n_accepted
                  THEN <<[suffix |-> Suffix(log[i], m.decided_idx), sync_idx |-> m.decided_idx]>>
                  ELSE IF na = m.n_accepted /\ acc > m.accepted_idx
                  THEN <<[suffix |-> Suffix(log[i], m.accepted_idx), sync_idx |-> m.accepted_idx]>>
                  ELSE <<>>
           pr  == [type |-> "Promise", n |-> m.n, n_accepted |-> na,
                   decided_idx |-> decided_idx[i], accepted_idx |-> acc, log_sync |-> ls]
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
HandleMajorityPromises(i, ls1, c) ==
  LET sync == ls1.max_promise_sync
      dec  == MaxDecided(ls1)
      lg1  == IF sync = <<>> THEN log[i]
              ELSE AppendOnPrefix(log[i], sync[1].sync_idx, sync[1].suffix)
      acc1 == IF sync = <<>> THEN accepted_idx[i]
              ELSE sync[1].sync_idx + Len(sync[1].suffix)
      bp   == buffered_proposals[i]
      lg2  == lg1 \o bp
      acc2 == acc1 + Len(bp)
      ls2  == [ls1 EXCEPT !.max_promise_sync = <<>>,
                          !.accepted_indexes[i] = IF Votes(i) THEN acc2 ELSE @]
      fs   == PromisedFollowers(ls2)
  IN /\ n_accepted' = [n_accepted EXCEPT ![i] = ls1.n_leader]
     /\ decided_idx' = [decided_idx EXCEPT ![i] = dec]
     /\ log' = [log EXCEPT ![i] = lg2]
     /\ accepted_idx' = [accepted_idx EXCEPT ![i] = acc2]
     /\ buffered_proposals' = [buffered_proposals EXCEPT ![i] = <<>>]
     /\ phase' = [phase EXCEPT ![i] = "Accept"]
     /\ leader_state' = [leader_state EXCEPT ![i] = WithAccSyncSeqs(ls2, fs)]
     /\ chan' = Send(c, i, [k \in fs |-> <<AccSyncMsg(ls2, k, lg2, dec)>>])
     /\ UNCHANGED <<promise, role, current_seq_num, cached_promise_message>>

\* mod.rs:267-271 dispatch, leader.rs:287-299 handle_promise_prepare,
\* leader.rs:301-314 handle_promise_accept
HandlePromise(i, j, m, c) ==
  LET ls   == leader_state[i]
      meta == [n_accepted |-> m.n_accepted, accepted_idx |-> m.accepted_idx,
               decided_idx |-> m.decided_idx, pid |-> j]
  IN
  IF role[i] = "Leader" /\ phase[i] = "Prepare" /\ m.n = ls.n_leader
  THEN \E replace \in BOOLEAN :
         LET ls1 == SetPromise(ls, meta, m.log_sync, j, TRUE, replace) IN
         IF Cardinality(Promised(ls1)) >= Majority
         THEN HandleMajorityPromises(i, ls1, c)
         ELSE /\ leader_state' = [leader_state EXCEPT ![i] = ls1]
              /\ chan' = c
              /\ UNCHANGED <<durable, role, phase, accepted_idx, current_seq_num,
                             cached_promise_message, buffered_proposals>>
  ELSE IF role[i] = "Leader" /\ phase[i] = "Accept" /\ m.n = ls.n_leader
  THEN LET ls1 == SetPromise(ls, meta, m.log_sync, j, FALSE, FALSE) IN
       /\ leader_state' = [leader_state EXCEPT ![i] = WithAccSyncSeqs(ls1, {j})]
       /\ chan' = Send(c, i, One(j, AccSyncMsg(ls1, j, log[i], decided_idx[i])))
       /\ UNCHANGED <<durable, role, phase, accepted_idx, current_seq_num,
                      cached_promise_message, buffered_proposals>>
  ELSE /\ chan' = c
       /\ UNCHANGED <<durable, volatile>>

\* follower.rs:53-79 handle_acceptsync, follower.rs:81-86
\* forward_buffered_proposals. DecidedIdxMonotone = FALSE is the source:
\* sync_log writes SetDecidedIndex as given (internal_storage.rs:319-323).
\* TRUE is our variant: the follower keeps max(old, new).
HandleAcceptSync(i, j, m, c) ==
  LET cv == CheckValidBallot(i, m.n) IN
  IF ~cv.valid THEN ApplyRecover(i, cv, c)
  ELSE IF role[i] = "Follower" /\ phase[i] = "Prepare"
  THEN LET s   == m.log_sync
           bp  == buffered_proposals[i]
           ld  == BallotPid(promise[i])
           fwd == bp # <<>> /\ ld > 0 /\ ld # i
           acc == [type |-> "Accepted", n |-> m.n, accepted_idx |-> s.sync_idx + Len(s.suffix)]
           out == [k \in (IF fwd THEN {ld} ELSE {}) \cup (IF Votes(i) THEN {j} ELSE {}) |->
                     (IF fwd /\ k = ld THEN <<[type |-> "ProposalForward", entries |-> bp]>> ELSE <<>>)
                     \o (IF Votes(i) /\ k = j THEN <<acc>> ELSE <<>>)]
       IN /\ n_accepted' = [n_accepted EXCEPT ![i] = m.n]
          /\ decided_idx' = [decided_idx EXCEPT ![i] =
                               IF DecidedIdxMonotone THEN Max(@, m.decided_idx) ELSE m.decided_idx]
          /\ log' = [log EXCEPT ![i] = AppendOnPrefix(@, s.sync_idx, s.suffix)]
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
UpdateDecided(dec, acc, new) ==
  IF new <= dec THEN [dec |-> dec, flushed |-> FALSE]
  ELSE IF new > acc THEN [dec |-> Min(new, acc), flushed |-> TRUE]
  ELSE [dec |-> new, flushed |-> FALSE]

\* follower.rs:88-110 handle_acceptdecide, follower.rs:230-238
\* handle_sequence_num, follower.rs:160-175 reply_accepted
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
                              [type |-> "Accepted", n |-> m.n, accepted_idx |-> acc])
                                     ELSE NoOut)
               /\ UNCHANGED <<promise, n_accepted, role, phase,
                              cached_promise_message, buffered_proposals, leader_state>>
       ELSE IF st = "DroppedPreceding"
       THEN ApplyRecover(i, Reconnect(i, BallotPid(m.n)), c)
       ELSE /\ chan' = c
            /\ UNCHANGED <<durable, volatile>>

\* follower.rs:128-138 handle_decide
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
                              [type |-> "Accepted", n |-> m.n, accepted_idx |-> accepted_idx[i]])
                                     ELSE NoOut)
               /\ UNCHANGED <<promise, n_accepted, log, role, phase, accepted_idx,
                              cached_promise_message, buffered_proposals, leader_state>>
       ELSE IF st = "DroppedPreceding"
       THEN ApplyRecover(i, Reconnect(i, BallotPid(m.n)), c)
       ELSE /\ chan' = c
            /\ UNCHANGED <<durable, volatile>>

\* leader.rs:316-345 handle_accepted, leader.rs:240-255 send_decide
HandleAccepted(i, j, m, c) ==
  IF m.n = leader_state[i].n_leader /\ role[i] = "Leader" /\ phase[i] = "Accept"
  THEN LET ls1 == [leader_state[i] EXCEPT !.accepted_indexes[j] = m.accepted_idx] IN
       IF m.accepted_idx > decided_idx[i] /\ IsChosen(ls1, m.accepted_idx)
       THEN LET fs  == PromisedFollowers(ls1)
                ls2 == NextSeqNums(ls1, fs)
            IN /\ decided_idx' = [decided_idx EXCEPT ![i] = m.accepted_idx]
               /\ leader_state' = [leader_state EXCEPT ![i] = ls2]
               /\ chan' = Send(c, i, [k \in fs |->
                            <<[type |-> "Decide", n |-> ls2.n_leader,
                               seq_num |-> ls2.follower_seq_nums[k],
                               decided_idx |-> m.accepted_idx]>>])
               /\ UNCHANGED <<promise, n_accepted, log, role, phase, accepted_idx,
                              current_seq_num, cached_promise_message, buffered_proposals>>
       ELSE /\ leader_state' = [leader_state EXCEPT ![i] = ls1]
            /\ chan' = c
            /\ UNCHANGED <<durable, role, phase, accepted_idx, current_seq_num,
                           cached_promise_message, buffered_proposals>>
  ELSE /\ chan' = c
       /\ UNCHANGED <<durable, volatile>>

\* leader.rs:365-369 handle_notaccepted, util.rs:156-158 lost_promise
HandleNotAccepted(i, j, m, c) ==
  IF role[i] = "Leader" /\ leader_state[i].n_leader < m.n
  THEN /\ leader_state' = [leader_state EXCEPT ![i].promises_meta[j] = PromisedHigher]
       /\ chan' = c
       /\ UNCHANGED <<durable, role, phase, accepted_idx, current_seq_num,
                      cached_promise_message, buffered_proposals>>
  ELSE /\ chan' = c
       /\ UNCHANGED <<durable, volatile>>

\* leader.rs:76-84 handle_forwarded_proposal
HandleProposalForward(i, j, m, c) ==
  IF role[i] = "Leader" /\ phase[i] = "Prepare"
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
HandleRecovery(i, j, m, c) ==
  /\ chan' = Send(c, i, IfVotes(i, j, RecoveryResponseMsg(i, m.nonce)))
  /\ UNCHANGED <<durable, volatile, recovery>>

\* VR Revisited 4.3: the recovering node keeps one answer per sender with
\* its own nonce. When the answers are complete, it takes the state of the
\* primary, keeps the higher promise, and continues as a node that
\* restarts with that durable state (Phase::Recover, PrepareReq).
HandleRecoveryResponse(i, j, m, c) ==
  IF ~recovering[i] \/ m.nonce # nonce[i]
  THEN /\ chan' = c
       /\ UNCHANGED <<durable, volatile, recovery>>
  ELSE LET r  == [from |-> j, promise |-> m.promise, primary |-> m.primary,
                  n_accepted |-> m.n_accepted, log |-> m.log, decided_idx |-> m.decided_idx]
           rs == {x \in recovery_responses[i] : x.from # j} \cup {r}
       IN IF RecoveryComplete(rs)
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

----------------------------------------------------------------------------
(* Ghost: decided_history grows with every decided prefix of a node with a  *)
(* vote; decided_ballot[k] is n_accepted of the node that first decided k.  *)
DecidedPrefixP(i) == SubSeq(log'[i], 1, Min(decided_idx'[i], Len(log'[i])))
VotesP(i) == ~(RuleR /\ wiped'[i])

HistoryUpdate ==
  LET longer == {i \in Nodes : VotesP(i)
                   /\ Len(DecidedPrefixP(i)) > Len(decided_history)
                   /\ IsPrefix(decided_history, DecidedPrefixP(i))}
  IN IF longer = {}
     THEN UNCHANGED <<decided_history, decided_ballot>>
     ELSE LET i == CHOOSE x \in longer : TRUE
              p == DecidedPrefixP(i)
          IN /\ decided_history' = p
             /\ decided_ballot' = decided_ballot
                  \o [k \in 1..(Len(p) - Len(decided_history)) |-> n_accepted'[i]]

\* F6: past_durable[k] holds each earlier durable state of k until the
\* rollback has happened. Each step that changes the durable state of k
\* adds the state before the step.
DurableOfP(k) ==
  [promise |-> promise'[k], n_accepted |-> n_accepted'[k], log |-> log'[k],
   decided_idx |-> decided_idx'[k]]

PastUpdate ==
  past_durable' =
    [k \in Nodes |->
       IF MaxRollback > 0 /\ rolled_back' = {}
       THEN IF DurableOfP(k) # DurableOf(k) THEN past_durable[k] \cup {DurableOf(k)}
            ELSE past_durable[k]
       ELSE {}]

GhostUpdate == HistoryUpdate /\ PastUpdate

----------------------------------------------------------------------------
(* Actions *)

\* mod.rs:263-282 handle: deliver the head of the channel from j to i.
Receive(j, i) ==
  /\ chan[<<j, i>>] # <<>>
  /\ LET m == Head(chan[<<j, i>>])
         c == [chan EXCEPT ![<<j, i>>] = Tail(@)]
     IN IF m.type = "RecoveryResponse" THEN HandleRecoveryResponse(i, j, m, c)
        ELSE IF recovering[i]
        THEN /\ chan' = c
             /\ UNCHANGED <<durable, volatile, recovery>>
        ELSE IF m.type = "Recovery" THEN HandleRecovery(i, j, m, c)
        ELSE /\ CASE m.type = "PrepareReq"      -> HandlePrepareReq(i, j, m, c)
                  [] m.type = "Prepare"         -> HandlePrepare(i, j, m, c)
                  [] m.type = "Promise"         -> HandlePromise(i, j, m, c)
                  [] m.type = "AcceptSync"      -> HandleAcceptSync(i, j, m, c)
                  [] m.type = "AcceptDecide"    -> HandleAcceptDecide(i, j, m, c)
                  [] m.type = "Decide"          -> HandleDecide(i, j, m, c)
                  [] m.type = "Accepted"        -> HandleAccepted(i, j, m, c)
                  [] m.type = "NotAccepted"     -> HandleNotAccepted(i, j, m, c)
                  [] m.type = "ProposalForward" -> HandleProposalForward(i, j, m, c)
             /\ UNCHANGED recovery
  /\ UNCHANGED <<env, proposed, rolled_back>>
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
                  <<[type |-> "Prepare", n |-> n, decided_idx |-> dec,
                     n_accepted |-> na, accepted_idx |-> acc]>>])
     /\ UNCHANGED <<n_accepted, log, decided_idx, accepted_idx, current_seq_num,
                    cached_promise_message, buffered_proposals,
                    wiped, crashes, wipes, losses, duplicates, timeouts, proposed,
                    recovery, rolled_back>>
  /\ GhostUpdate

\* mod.rs:298-305 append, mod.rs:359-365 propose_entry,
\* leader.rs:111-121 accept_entry_leader
Propose(i, v) ==
  /\ ~recovering[i]
  /\ v \notin proposed
  /\ proposed' = proposed \cup {v}
  /\ IF role[i] = "Leader" /\ phase[i] = "Prepare"
     THEN /\ buffered_proposals' = [buffered_proposals EXCEPT ![i] = Append(@, v)]
          /\ UNCHANGED <<durable, role, phase, accepted_idx, current_seq_num,
                         cached_promise_message, leader_state, chan>>
     ELSE IF role[i] = "Leader" /\ phase[i] = "Accept"
     THEN /\ AcceptEntriesLeader(i, <<v>>, chan)
          /\ UNCHANGED <<promise, n_accepted, decided_idx, role, phase, current_seq_num,
                         cached_promise_message, buffered_proposals>>
     ELSE /\ ForwardProposals(i, <<v>>, chan)
          /\ UNCHANGED <<durable, role, phase, accepted_idx, current_seq_num,
                         cached_promise_message, leader_state>>
  /\ UNCHANGED <<env, recovery, rolled_back>>
  /\ GhostUpdate

\* Restart of each node i in R with the durable state d[i], through
\* mod.rs:53-128 `with`.
\* The promise is always set (bootstrap and `with` write it), so the node
\* restarts in Phase::Recover and sends PrepareReq to every peer.
\* internal_storage.rs:55-74 load_cache: accepted_idx = log length.
\* With Recovery, the node first sends Recovery with a fresh nonce and
\* takes part in nothing else until its answers are complete
\* (owner decision: after each start a node votes only after f + 1 answers).
Restart(R, d) ==
  LET At(f, g) == [k \in Nodes |-> IF k \in R THEN g[k] ELSE f[k]]
      msg(k)   == IF Recovery THEN RecoveryMsg(nonce[k] + 1) ELSE PrepareReqMsg(d[k].promise)
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
  /\ leader_state' = At(leader_state, [k \in R |-> LeaderStateWith(d[k].promise)])
  /\ chan' = [p \in Pairs |-> IF p[1] \in R THEN Enqueue(chan[p], <<msg(p[1])>>) ELSE chan[p]]
  /\ IF Recovery
     THEN /\ recovering' = At(recovering, [k \in R |-> TRUE])
          /\ nonce' = At(nonce, [k \in R |-> nonce[k] + 1])
          /\ recovery_responses' = At(recovery_responses, [k \in R |-> {}])
     ELSE UNCHANGED recovery

\* F3: crash keeping durable state.
Crash(i) ==
  /\ crashes < MaxCrash
  /\ crashes' = crashes + 1
  /\ Restart({i}, [k \in {i} |-> DurableOf(k)])
  /\ UNCHANGED <<wiped, wipes, losses, duplicates, timeouts, elections, proposed, rolled_back>>
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
       /\ Restart(R, d)
  /\ UNCHANGED <<wiped, crashes, wipes, losses, duplicates, timeouts, elections, proposed>>
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
  /\ leader_state' = [leader_state EXCEPT ![i] = LeaderStateWith(DefaultBallot)]
  /\ recovering' = [recovering EXCEPT ![i] = FALSE]
  /\ recovery_responses' = [recovery_responses EXCEPT ![i] = {}]
  /\ UNCHANGED <<chan, crashes, losses, duplicates, timeouts, elections, proposed,
                 nonce, rolled_back>>
  /\ GhostUpdate

\* mod.rs:232-237 resend_message_timeout, leader.rs:371-403
\* resend_messages_leader, follower.rs:240-282 resend_messages_follower.
\* util.rs:211-222 get_preparable_peers.
ResendMessageTimeout(i) ==
  /\ timeouts < MaxTimeout
  /\ timeouts' = timeouts + 1
  /\ IF recovering[i]
     THEN LET open == {k \in Peers(i) : \A r \in recovery_responses[i] : r.from # k}
          IN /\ chan' = Send(chan, i, [k \in open |-> <<RecoveryMsg(nonce[i])>>])
             /\ UNCHANGED <<durable, volatile>>
     ELSE IF role[i] = "Leader"
     THEN IF phase[i] \in {"Prepare", "Accept"}
          THEN LET pp == {k \in Peers(i) : leader_state[i].promises_meta[k].state = "NotPromised"}
               IN /\ chan' = Send(chan, i, [k \in pp |-> <<PrepareMsg(i)>>])
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
                 recovery, rolled_back>>
  /\ chan' # chan \/ phase' # phase
  /\ GhostUpdate

\* F1: loss of any message on a channel.
Lose(p, k) ==
  /\ losses < MaxLoss
  /\ losses' = losses + 1
  /\ chan' = [chan EXCEPT ![p] = SubSeq(@, 1, k - 1) \o SubSeq(@, k + 1, Len(@))]
  /\ UNCHANGED <<durable, volatile, wiped, crashes, wipes, duplicates, timeouts, elections, proposed,
                 recovery, rolled_back>>
  /\ GhostUpdate

\* F1: duplication; the copy is appended at the end of the channel.
Duplicate(p, k) ==
  /\ duplicates < MaxDuplicate
  /\ duplicates' = duplicates + 1
  /\ Len(chan[p]) < Cap
  /\ chan' = [chan EXCEPT ![p] = Append(@, @[k])]
  /\ UNCHANGED <<durable, volatile, wiped, crashes, wipes, losses, timeouts, elections, proposed,
                 recovery, rolled_back>>
  /\ GhostUpdate

----------------------------------------------------------------------------
Next ==
  \/ \E p \in Pairs : Receive(p[1], p[2])
  \/ \E i \in Nodes, k \in 1..MaxBallot : HandleLeader(i, k)
  \/ \E i \in Nodes, v \in Values : Propose(i, v)
  \/ \E i \in Nodes : Crash(i)
  \/ \E i \in Nodes : Wipe(i)
  \/ \E R \in SUBSET Nodes : Rollback(R)
  \/ \E i \in Nodes : ResendMessageTimeout(i)
  \/ \E p \in Pairs, k \in 1..Cap : k <= Len(chan[p]) /\ Lose(p, k)
  \/ \E p \in Pairs, k \in 1..Cap : k <= Len(chan[p]) /\ Duplicate(p, k)

Spec == Init /\ [][Next]_vars

ASSUME MaxRollback < Cardinality(Nodes)

----------------------------------------------------------------------------
(* Bounds of the model, used as a state constraint. *)
Bounds ==
  /\ \A i \in Nodes : Len(log[i]) <= MaxLog /\ Len(buffered_proposals[i]) <= MaxLog
  /\ \A i, k \in Nodes : leader_state[i].follower_seq_nums[k].counter <= MaxCounter
                         /\ leader_state[i].follower_seq_nums[k].session <= MaxSession

----------------------------------------------------------------------------
(* Properties *)
DecidedPrefix(i) == SubSeq(log[i], 1, Min(decided_idx[i], Len(log[i])))

\* internal_storage.rs:302-311: the cached accepted_idx is the log length,
\* follower.rs:140: decided index <= accepted index.
AcceptedIdxIsLogLength ==
  \A i \in Nodes : accepted_idx[i] = Len(log[i]) /\ decided_idx[i] <= accepted_idx[i]

S1 == \A i, j \in Nodes : Votes(i) /\ Votes(j) => Compatible(DecidedPrefix(i), DecidedPrefix(j))

S2 == \A i \in Nodes : Votes(i) => IsPrefix(DecidedPrefix(i), decided_history)

S2Grows == [][IsPrefix(decided_history, decided_history')]_vars

\* A leader in (Leader, Accept) with ballot b holds every entry of
\* decided_history that was decided at a ballot <= b.
S2b == \A i \in Nodes :
         role[i] = "Leader" /\ phase[i] = "Accept" =>
           \A k \in 1..Len(decided_history) :
             decided_ballot[k] <= leader_state[i].n_leader =>
               IsPrefix(SubSeq(decided_history, 1, k), log[i])

S3 == /\ \A i \in Nodes : \A k \in 1..Len(DecidedPrefix(i)) : DecidedPrefix(i)[k] \in proposed
      /\ \A k \in 1..Len(decided_history) : decided_history[k] \in proposed

S5 == \A i, j \in Nodes :
        n_accepted[i] = n_accepted[j] /\ n_accepted[i] # DefaultBallot
          => Compatible(log[i], log[j])

S7 == [][\A i \in Nodes : ~wiped'[i] => decided_idx'[i] >= decided_idx[i]]_vars

ValueSymmetry == Permutations(Values)

(* State view for TLC. It drops values that no action reads in the current  *)
(* role and phase: leader_state except n_leader of a Follower, the cached   *)
(* Promise outside (Follower, Prepare), current_seq_num outside            *)
(* (Follower, Accept).                                                      *)
View ==
  <<durable, role, phase, accepted_idx,
    [i \in Nodes |-> IF role[i] = "Follower" /\ phase[i] = "Accept"
                     THEN current_seq_num[i] ELSE DefaultSeq],
    [i \in Nodes |-> IF role[i] = "Follower" /\ phase[i] = "Prepare"
                     THEN cached_promise_message[i] ELSE <<>>],
    buffered_proposals,
    [i \in Nodes |-> IF role[i] = "Leader" THEN leader_state[i]
                     ELSE LeaderStateWith(leader_state[i].n_leader)],
    chan, env, ghost,
    recovering, nonce,
    [i \in Nodes |-> IF recovering[i] THEN recovery_responses[i] ELSE {}],
    rollback>>
=============================================================================
