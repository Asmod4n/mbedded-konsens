# Plan: Execute-Verify with the majority (stages D2 and D3 of plan-state-in-lmdb.md)

This plan gives the protocol and the model of sections 4 and 5 of
doc/plan-state-in-lmdb.md, in the form that the owner gave on
2026-10-25 09:26 to 09:30:

- A node that finds a fault in itself throws.
- A peer that delivers a wrong value is asked to try again. When the
  value is still wrong, the peer is told that it is broken.
- The majority shows which value is right.
- One repair runs automatically. Each further repair waits for the
  operator.

History of this plan:

- Form 1 (cd919bb) checked the result before the accept. The review
  rejected it: the veto in a new ballot, the own count of the leader, the
  run base, and the throw without a durable proof.
- Form 2 (e2eec1f) checked the result after the decide. The review
  rejected it: a node could apply `k` before it saw `rejected(k)` (no
  horizon); answers and proposals that ran on top of an entry that is
  later rejected stayed valid; the model could not show either fault.
- This form keeps the check after the decide (provisional, card of
  2026-10-25: "after the decide"). A rejection voids the rejected entry
  and every entry after it up to the rejection, and `verified_idx` has a
  horizon. D2 and D3 are built together, because every run after the
  first one in flight uses the open transaction.

Nothing of it is built before the model runs of section 4 show no
violation and a review approves this plan.

## 1. The protocol

### 1.1 The result of an entry

1. Each entry of kind `register_client`, `client_request` or
   `expire_session` carries the result of its run on the leader that
   proposed it: the reply and the mutations. The entry CRC and the chain
   CRC cover the result. This replaces item 4.1 of plan-state-in-lmdb.md
   (no `error` field: a user error is a reply, section 7.4).
2. A node agrees with an entry when the result of its own run equals the
   result in the entry. The compare walks the CBOR items. A float of any
   width is decoded to `double`, and two floats agree within 1 ULP of
   `double` (owner, 2026-10-25 09:22). NaN agrees with NaN, and +0 agrees
   with -0 (provisional, question 7.1). Every other item, and every key
   of a mutation, agrees byte for byte.
3. The assumption: for each entry, either every node that works agrees
   with the result in the entry, or none does. The nodes whose run of an
   entry gives a wrong result are fewer than a majority, over all time.
   Then each majority holds a node that works, and two nodes that work
   come to the same verdict. Outside the assumption: a float whose true
   value lies within 1 ULP of the leader's result on one working node and
   outside it on another (the tolerance is not transitive). The plan
   gives no guarantee there.

### 1.2 The run and its base

1. The base of entry `k` is the state after every entry before `k`, on
   the assumption that none of them is rejected. Every run of `k`, on the
   leader and on each node, uses that base: the open state transaction
   holds the entries from the applied index up to `k - 1` (D3,
   plan-state-in-lmdb.md 5.1).
2. A rejection of `k` (1.4) voids `k` and every entry after it that
   changes the state, up
   to the entry `rejected(k)`. So no entry that stays valid ran on a base
   that holds a rejected entry. The base of 1.2.1 is then exact for every
   entry that the state applies.
3. The leader runs each proposal before it appends it, on its base, and
   writes the result into the entry. A new leader runs every entry of its
   sync above the decided index, on the base, before it answers for it.
4. A node runs each entry when it is decided, on the base, if its open
   transaction does not hold that run yet. It may run an entry that is
   accepted but not decided, and keep the run in the open transaction;
   the answer of 1.3.1 leaves only for a decided entry.
5. A cut of the log aborts the open transaction (plan-state-in-lmdb.md
   5.3), and the node runs again from the applied index. A crash, a
   recovery and a repair abort it as well (model `StateFollows`).
6. The open transaction holds at most `max_in_flight` entries above the
   applied index (plan-state-in-lmdb.md 5.1). The leader proposes no
   more while it is full.

### 1.3 The answer and the verdict

1. A node that ran decided entries sends one `verified(ballot, from,
   to, first_disagreed)` (provisional) to the leader of its promise in
   each step, for the range of decided indexes that it ran since its last
   answer. `first_disagreed` is the first index in the range that it does
   not agree with, or none. A node that does not agree with an entry runs
   it a second time on the same base first ("try again", question 7.2).
   Only a node that votes, does not recover and does not repair answers.
2. The leader counts its own answer from its run after the decide, on the
   base of 1.2.1. For its own proposals that run is the run of 1.2.3.
3. The leader counts, for each decided index above its `verified_idx`,
   the nodes that agree and the nodes that do not. The counts belong to
   one ballot.
4. When a majority agrees with every entry up to `k`, the leader raises
   `verified_idx` (provisional) to `k`. The pair (`verified_idx`, the
   decided index of the leader in that step) is the verdict mark. The
   leader sends a Decide with the mark at the end of that step. A node
   takes a mark only from a Decide or an AcceptDecide whose ballot
   equals its promise.
5. A node applies its decided entries up to `verified_idx` of the mark
   only when its own decided index is at or above the decided index of
   the mark. So it has every `rejected` entry that the leader decided
   below the mark. Each node writes the mutations of the entry, which are
   the bytes of the leader (owner, 2026-10-17 12:40).
6. The answers and the mark are sent again on the resend timeout until
   the ballot changes.

### 1.4 The rejection

1. When a majority does not agree with the entry at `k`, and agrees with
   every entry before it, the leader appends `rejected(k)` (provisional)
   as a normal proposal at an index `m > k`.
2. `rejected(k)` at `m` voids every entry in `k .. m - 1` that changes
   the state (`register_client`, `client_request`, `expire_session`). The
   client of `k` gets `client_errc::rejected` (provisional). The clients
   of the others get `client_errc::retry`, and resend: a request with the
   same request number, a registration or an expiry again. A void entry
   changes no state and no session.
3. After `rejected(k)` is decided, the verdict mark passes `m`. The
   entries after `m` have the base of the state before `k`.
4. A `rejected(k)` with `k` at or below the applied index of a node, or
   for a `k` that already has a `rejected` entry, is a fault of the
   leader. The node reports it, applies nothing for it, and does not
   throw.
5. Only one verdict exists for each index. An `agrees` majority and a
   `does not agree` majority for `k` share a node, and by 1.1.3 the
   working nodes of both come to the same verdict.

### 1.5 The minority

1. A node that did not agree with `k` and applies `k` (the mark passed
   `k` with no `rejected(k)` in its decided prefix) knows that its own
   machine does not work. A node that agreed with `k` and sees
   `rejected(k)` decided knows the same. Both throw (section 2). This
   holds only for `k` itself, never for the entries that 1.4.2 voids.
2. A follower that does not agree with a decided entry that has no
   verdict yet answers the heartbeats of the ballot leader election with
   `happy = false` (message.hpp `heartbeat_reply`), so a node whose
   machine works can take over when the leader holds the count back.
3. A leader that counts a `does not agree` majority against its own
   answer appends `rejected(k)` and then throws.

### 1.6 A leader change

1. Each promise carries the applied index of the node. The new leader
   starts `verified_idx` at the largest applied index among the promises:
   a node with applied index `a` proves that every entry up to `a` has
   its verdict, and the decided prefix of the leader covers `a`.
2. Above it, the new leader counts again. Its own answers come from its
   runs of 1.2.3. The answers in a promise are not needed, so the promise
   grows by one number.

### 1.7 What a client sees

1. A request gets its reply when its entry is applied, after its verdict.
2. A read through the read index waits until the applied index reaches
   the read index, as now.
3. Section 5 of the parent plan measures, with and without
   Execute-Verify: writes per second, and the p50 and the p99 of the
   reply latency.

## 2. A fault of the own node throws, with a proof that stays

1. A damage that a step finds (`error_is_a_damage`) and a throw of 1.5
   write a proof into the node storage: the kind, the index, the CRCs or
   the digests, and the ballot. The proof is one field of the
   `repair_record`. A failure of that write throws `sync_error`.
2. Then the node throws `node_fault` (provisional), a type derived from
   `std::system_error` with an error code of `paxos_errc`. Nothing
   catches it but `main` and the body of a thread.
3. The start reads the record. A proof counts as a damage of plan 2.4
   item 8: `count_a_repair` sets the count, the open mark and clears the
   proof in one commit. While the count allows it, the node repairs from
   a peer; else it waits for the operator. A crash during the repair
   finds the open mark and does not count again.
4. After a proof of a result throw, the log search of the repair finds no
   difference. The repair is then the count, the abort of the open
   transaction, and the recovery.
5. The start also runs the scan of plan-repair-from-peer.md 2.3 over the
   whole log. A damage that it finds counts the same way. Its cost grows
   with the log length (question 7.3).
6. `with` gives no stopped node for a damage any more; it gives a node
   that repairs, or one that waits for the operator.

## 3. The model

1. A constant `ExecuteVerify` (Boolean). FALSE keeps every present
   configuration as it is. The configurations of section 4 set
   `StateInLmdb`, `RepairFromPeer` and `StartDamageCounts`.
2. The result. `Result(v, b)` is the result of value `v` on base `b` (a
   sequence of values). A ghost `wrong`, a set of pairs (node, value), and
   a run of node `i` on `v` gives `Result(v, b)` or, when `<<i, v>> \in
   wrong`, a token "bad". A run may add a pair to `wrong` while the
   constraint of 1.1.3 holds (fewer than a majority for each value), and a
   second run may give the right result, so the retry is in the model.
3. Each entry of the log carries the fields `res` (the result) and `base`
   (the base it ran on), set by the leader at the append. Session entries
   carry them too.
4. `verified`, the mark and `rejected(k)` travel in `chan`, so Lose and
   Duplicate reach them.
5. Variables: `verified_idx` and `mark` (per node), `counts` (per leader),
   `answered` (per node), `proof` (durable, per node), `result_throws`.
6. Actions: `RunEntry(i)` (a run in the open transaction, on the base of
   1.2.1), `Answer(i)`, `HandleVerified`, `RaiseMark`, `Reject`,
   `ApplyVerified(i)` (1.3.5, with 1.4.2), `ResultThrow(i)` (writes
   `proof` and takes the start in one step, as the start branch of
   `StorageFault` does; it does not spend `MaxCrash`).
7. A damage that a step finds also writes `proof` and takes the start in
   one step. `Detected` and the step check use one operator, and the start
   check applies it to the whole log. The high-water mark is lost at the
   throw, as at every restart.
8. The effective prefix of a node: its decided prefix up to the mark,
   without the void entries of 1.4.2. StateIsDecided, ClientTable, E1,
   ReadReply and R1 read the effective prefix.
9. Invariants:
   - `OneVerdict`: no index has both an `agrees` majority and a `does not
     agree` majority.
   - `StatesAgree`: the applied states of two nodes are prefixes of one
     another.
   - `AppliedAgrees`: each entry of an applied state has `res =
     Result(v, base)` and `base` equal to the applied state before it.
   - `ThrowOnlyWrong`: only a node with a wrong run of `k` takes a result
     throw for `k`.
   - `CountsStay`: every result throw and every damage throw counts
     toward the repairs of the node.
   - S1 to S5, E1, R1, StateIsDecided and EntryRunsOnOnePrefix stay.
10. Covers, each must be violated: `NoReject`, `NoLeaderThrow`,
    `NoFollowerThrow`, `NoVerify`, `NoRejectAfterLeaderChange`,
    `NoVerdictWithFollowerBehind`, `NoVoidEntry`.
11. Mutations, each must break an invariant:
    - `ApplyWithoutVerdict` (apply at the decide): `AppliedAgrees`.
    - `ApplyBeyondHorizon` (apply without the decided index of the mark):
      `StatesAgree`.
    - `NoVoidAfterReject` (only `k` is void): `AppliedAgrees`.
    - `LeaderAnswersFromProposal` with a wrong base: `AppliedAgrees` or
      `ThrowOnlyWrong`.
    - `RejectWithOne`: `OneVerdict` or `ThrowOnlyWrong`.
    - `StaleMark` (a mark from another ballot): `StatesAgree`.
    - `RecountWithoutApplied` (the new leader starts below the largest
      applied index): a cover of a stall, `NoProgressAfterChange`.
    - `NoProofAtThrow`: `CountsStay`.

## 4. The runs

Each configuration runs 15 minutes as a simulation first; a BFS runs
only where it can end (owner, 2026-10-19 05:55). At most 3 TLC runs at a
time, with JOBS=1 for builds while they run.

1. `MC_ev_three`: 3 nodes, one wrong pair, one crash, `MaxInFlight = 2`,
   `MaxElection = 3`, Lose and Duplicate 1.
2. `MC_ev_sessions`: as 1, with `ClientSessions`.
3. `MC_ev_five`: 5 nodes, two wrong pairs, simulation only.
4. Each mutation of 3.11 gives a violation, and each cover of 3.10 gives
   a trace.

## 5. Questions for the owner

1. NaN agrees with NaN, and +0 agrees with -0 (proposal).
2. "Try again" is a second run on the node that does not agree
   (proposal). The other reading: the node asks the leader to run again.
3. The full scan of the log at each start grows with the log, and no
   compaction is planned yet. A restart of a node with a large log is
   slow. Proposal: measure it in section 5 of the parent plan, and plan
   compaction after that.
