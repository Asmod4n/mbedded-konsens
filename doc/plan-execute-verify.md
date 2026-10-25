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

It replaces items 4.1 to 4.9 of plan-state-in-lmdb.md, the compare in
its item 5.2, and its tests "a follower whose result differs does not
accept and reports" and "a follower writes the write list of the
leader". Items 5.1, 5.3 to 5.5 stay.

History of this plan:

- Form 1 (cd919bb) checked the result before the accept. Rejected: the
  veto in a new ballot, the own count of the leader, the run base, the
  throw without a durable proof.
- Form 2 (e2eec1f) checked the result after the decide. Rejected: no
  horizon on the mark, a cascade on a rejected base, a model that could
  not show either.
- Form 3 (90fd61d) added the horizon and the void range. Rejected: the
  void ranges and the counts depended on the node and the leader, not on
  the log alone; a wrong machine looped through its repairs; no verdict
  before a StopSign; a verdict could be lost at a leader change.
- This form puts each verdict into the log as an entry. The verdict
  state is then a pure function of the decided log, the same on every
  node and after every leader change. The check stays after the decide
  (provisional, card of 2026-10-25: "after the decide").

Nothing of it is built before the model runs of section 4 show no
violation and a review approves this plan.

## 1. The protocol

### 1.1 The result of an entry

1. Each entry of kind `register_client`, `client_request` or
   `expire_session` carries the result of its run on the leader that
   proposed it: the reply and the mutations. The entry CRC and the chain
   CRC cover it. These kinds are the entries that change the state.
2. A node agrees with an entry when the result of its own run equals the
   result in the entry. The compare walks the CBOR items. A float of any
   width is decoded to `double`, and two floats agree within 1 ULP of
   `double` (owner, 2026-10-25 09:22): the distance of the bits in
   binary64, NaN agrees only with NaN, and +0 and -0 are 1 ULP apart
   (owner, 2026-10-17 12:43). Every other item, and every key of a
   mutation, agrees byte for byte.
3. The assumption: for each entry, either every node that works agrees
   with the result in the entry, or none does. The nodes whose run of an
   entry gives a wrong result are fewer than a majority, over all time.
   Outside the assumption: a float whose true value lies within 1 ULP of
   the result in the entry on one working node and outside it on another.
   Two nodes can then answer differently, but the verdict is one entry of
   the log, so every node still applies the same; a working node in the
   minority throws and counts a repair.

### 1.2 The verdict entries

1. Two new kinds of entry (names provisional): `verified(j)` and
   `rejected(k)`. They change no state. Only the leader appends them, as
   normal proposals.
2. The verdict function `V` walks a decided prefix in log order with a
   pointer `p`, which starts at 0:
   - `verified(j)` at index `m` with `p < j < m`: `p = j`.
   - `rejected(k)` at index `m` with `p < k < m`: every entry in
     `k .. m - 1` that changes the state is void, and `p = m`.
   - Any other verdict entry is stale and changes nothing.
   `V` gives `p` and the void set. Every node computes `V` from its own
   decided prefix, so two nodes with the same decided prefix have the
   same `p` and the same void set.
3. A node applies the decided entries up to `p`, leaving out the void
   ones. A void entry keeps its index and changes no state and no
   session. The client of `k` gets `client_errc::rejected`
   (provisional); the clients of the other void entries get
   `client_errc::retry` and send again.
4. A leader proposes a StopSign only when `p` of its log covers every
   entry that changes the state.

### 1.3 The base and the run

1. The base of entry `e` at index `q` is the committed state plus the
   mutations in the entries (the bytes of the leader) of every entry
   before `q` that changes the state and is not void by `V` of the log
   before `q`. The own result of a node never enters a base.
2. The state transaction of D3 holds the base: it is the committed
   state plus the mutations of the decided entries above the applied
   index, on the assumption that no rejection follows.
3. A run of `e` happens in a nested transaction of that write
   transaction. The node compares the result and aborts the nested
   transaction. The parent then takes the mutations in the entry. So a
   later abort never needs a run again: the node writes the bytes again
   (the remedy of execute-order-validate systems with write sets). The
   state environment has no `MDB_WRITEMAP` (plan-state-in-lmdb.md 1.1),
   so LMDB allows nested transactions there.
4. The leader runs each proposal before it appends it, on the base of its
   log, and writes the result into the entry.
5. When `rejected(k)` becomes valid by `V`, a cut of the log happens, or
   a crash, a recovery or a repair happens, the node aborts the open
   transaction and writes the bytes of the entries from the applied index
   again (plan-state-in-lmdb.md 5.3, 5.4).
6. The open transaction holds at most `max_in_flight` entries above the
   applied index. The leader proposes no more while it is full.

### 1.4 The answers and the count

1. A node that votes, does not recover and does not repair runs each
   decided entry above `p` that changes the state and is not void. A node
   that does not agree runs the entry a second time on the same base
   ("try again", question 5.2).
2. It sends `verified(ballot, from, to, first_disagreed)` (provisional
   message) to the leader of its promise, once in each step: the range of
   decided indexes that it ran, ended at the first index that it does not
   agree with. It answers nothing after that index until a verdict entry
   for it is decided.
3. At each new promise, a node answers again from `p` of its decided
   prefix. The answers and the leader's mark are sent again on the resend
   timeout until the ballot changes.
4. The leader counts its own answer from its run after the decide, on the
   base of 1.3.1, never from the run at the proposal.
5. The leader counts, for each index above `p` of its log, the nodes that
   agree and the nodes that do not. The counts belong to one ballot and
   start at `p`, so a new leader needs no count of an earlier one.
6. When a majority agrees with every entry up to `j`, and the log of the
   leader holds no verdict entry above `p` that is not decided, the
   leader appends `verified(j)`. When a majority does not agree with `k`
   and agrees with every entry before it, the leader appends
   `rejected(k)`. The leader appends at most one verdict entry in each
   step; group commit carries it with the other entries of the step.

### 1.5 The minority

1. A node that did not agree with `k` sees `p` pass `k` with `k` not
   void: its own machine is wrong. A node that agreed with `k` sees `k`
   void by a `rejected(k)`: the same. Both throw (section 2). This holds
   only for the `k` of a `rejected(k)` and for the entries that the node
   did not agree with, never for the other void entries.
2. The leader is such a node when it agreed with `k` and `rejected(k)` is
   decided. It throws after that decide, so the Decide leaves first.
3. A wrong `rejected(k)`, from a fault of the library on the leader, makes
   every node that agreed with `k` throw. That lies outside the fault
   model of 1.1.3.

## 2. A fault of the own node throws, with a proof that stays

1. A damage that a step finds (`error_is_a_damage`) and a throw of 1.5
   write a proof into the node storage: the kind, the index `k`, the CRCs
   or the digests, and the ballot. The proof is one field of the
   `repair_record`. A failure of that write throws `sync_error`.
2. Then the node throws `node_fault` (provisional), a type derived from
   `std::system_error` with an error code of `paxos_errc`. Nothing
   catches it but `main` and the body of a thread.
3. The start reads the record. A proof that the open mark does not cover
   counts as a damage of plan 2.4 item 8: `count_a_repair` sets the
   count, sets the open mark with the proof's index, and clears the proof,
   in one commit. While the count allows it, the node repairs; else it
   waits for the operator.
4. For a result proof the repair is the abort of the open transaction and
   the recovery. The node keeps its log. Its `p` already covers `k` (the
   verdict was decided before the throw), so it applies the bytes of the
   leader for `k` and runs `k` no more.
5. The open mark closes when the applied index of the node passes the
   index of the mark, or after the commit of a repaired log. A throw while
   the mark is open counts nothing more (a crash during a repair). A throw
   after the mark closed counts, and with the count at its limit the node
   waits for the operator.
6. The start also runs the scan of plan-repair-from-peer.md 2.3 over the
   whole log. A damage that it finds counts as in 2.3. Its cost grows with
   the log length (question 5.3).
7. `with` gives no stopped node for a damage any more; it gives a node
   that repairs, or one that waits for the operator.

## 3. The model

1. A constant `ExecuteVerify` (Boolean). FALSE keeps every present
   configuration as it is. The configurations of section 4 set
   `StateInLmdb`, `RepairFromPeer` and `StartDamageCounts`.
2. The result. `Result(v, b)` gives a result for value `v` on base `b` (a
   sequence of values). A ghost `wrong`, a set of pairs (node, value). A
   run of node `i` on `v` gives `Result(v, b)`, or the token "bad" when
   `<<i, v>> \in wrong`. A run may add a pair while fewer than a majority
   of the nodes hold a pair for `v` and the number of pairs is below
   `MaxWrong`. A second run may give the right result.
3. Each entry that changes the state carries `res` and `base`, set at
   each append that runs it: `AcceptEntriesLeader`, the buffered
   proposals in `HandleMajorityPromises`, `HandleProposalForward` (at the
   leader) and `ExpireSession`. The verdict entries are values
   `Verified(j)` and `Rejected(k)`; S3 counts them as proposed.
4. `V(log)` is an operator, the same as 1.2.2.
5. The message `Verified` travels in `chan`, so Lose and Duplicate reach
   it.
6. Variables: `counts` (per leader), `answered` (per node), `proof`
   (durable, per node), `open_mark` (durable, per node), `result_throws`.
7. Actions: `Answer(i)`, `HandleVerified(i, j, m)`, `AppendVerdict(i)`,
   `ApplyVerified(i)` (1.2.3, from the bytes), `ResultThrow(i)` (writes
   `proof`, then the start with the node's own durable state in one step,
   and the count of 2.3; bounded by `MaxResultThrow`).
8. A damage that a step finds also writes `proof` and takes the start in
   one step. `Detected` and the step check use one operator, and the
   start check applies it to the whole log. The high-water mark is lost
   at the throw, as at every restart.
9. The effective prefix of a node: its decided prefix up to `p`, with
   each void entry kept at its index as a no-op. StateIsDecided,
   ClientTable, `Opened`, `Executed`, `ClientRegistered`, `ClientReplied`,
   `ClientSessionExpired`, the guard of `ExpireSession`, E1, ReadReply and
   R1 read the effective prefix. R1 compares with the history of entries
   that have a verdict.
10. A ghost `verdicts`: every count that reached a majority, as (ballot,
    index, agrees).
11. Invariants:
    - `AppliedHasVerdict`: each applied entry that is not void has an
      `agrees` majority in `verdicts`.
    - `OneVerdict`: no index has both an `agrees` and a `does not agree`
      majority in `verdicts`, over all ballots.
    - `StatesAgree`: the applied states of two nodes are prefixes of one
      another.
    - `AppliedAgrees`: each applied entry has `res = Result(v, base)` and
      `base` equal to the applied state before it.
    - `ThrowOnlyWrong`: only a node with a wrong run of `k` takes a result
      throw for `k`.
    - `RepairsBounded`: a node takes at most `AutoRepairs` counted repairs
      before `OperatorAllowsRepair`.
    - S1 to S5, E1, R1, StateIsDecided and EntryRunsOnOnePrefix stay.
12. Covers, each must be violated by a trace: `NoReject`,
    `NoLeaderThrow`, `NoFollowerThrow`, `NoVerify`,
    `NoRejectAfterLeaderChange`, `NoVoidEntry`, `NoStaleVerdictEntry`.
13. Mutations, each must break an invariant:
    - `ApplyWithoutVerdict` (apply at the decide): `AppliedHasVerdict`.
    - `OwnResultInBase` (the base holds the own result): `ThrowOnlyWrong`.
    - `NoVoidAfterReject` (only `k` is void): `AppliedAgrees`.
    - `LeaderAnswersFromProposal` (the leader counts its run at the
      proposal, on a base that a rejection voids later): `AppliedAgrees`.
    - `RejectWithOne` (one answer that does not agree rejects):
      `OneVerdict` or `ThrowOnlyWrong`.
    - `StaleVerdictCounts` (`V` takes a verdict entry with `j <= p`):
      `StatesAgree`.
    - `NoProofAtThrow` (the throw writes no proof): `RepairsBounded`.

## 4. The runs

Each configuration runs 15 minutes as a simulation first; a BFS runs
only where it can end (owner, 2026-10-19 05:55). A cover that a
15-minute simulation does not reach counts as not shown. At most 3 TLC
runs at a time, with JOBS=1 for builds while they run. Each
configuration holds one class of fault:

1. `MC_ev_reject`: 3 nodes, `MaxLog = 4`, `MaxWrong = 1`, no crash, no
   Lose, `MaxElection = 1`. Covers `NoReject`, `NoVoidEntry`,
   `NoFollowerThrow`, `NoLeaderThrow`.
2. `MC_ev_change`: 3 nodes, `MaxLog = 5`, `MaxWrong = 1`, no Lose,
   `MaxElection = 3`. Covers `NoRejectAfterLeaderChange`,
   `NoStaleVerdictEntry`.
3. `MC_ev_loss`: 3 nodes, `MaxLog = 4`, `MaxWrong = 1`, Lose 1,
   Duplicate 1, one crash, `MaxElection = 2`.
4. `MC_ev_sessions`: 3 nodes, `MaxLog = 6`, `ClientSessions`,
   `MaxClientSend = 4`, `MaxWrong = 1`, `MaxElection = 1`.
5. `MC_ev_five`: 5 nodes, `MaxLog = 4`, `MaxWrong = 2`.
6. Each mutation of 3.13 gives a violation in one of these.

## 5. Questions for the owner

1. Decided (owner, 2026-10-17 12:43): the ULP rule of 1.1.2.
2. "Try again" is a second run on the node that does not agree
   (proposal). The other reading: the node asks the leader to run again.
3. The full scan of the log at each start grows with the log, and no
   compaction is planned yet. Proposal: measure it in section 5 of the
   parent plan, and plan compaction after that.
4. A write now needs a second round before its reply: the decide of the
   entry, then the decide of its `verified` entry. One `verified` entry
   covers every entry of a step. Section 5 of the parent plan measures
   writes per second and the p50 and the p99 of the reply latency, with
   and without Execute-Verify.
