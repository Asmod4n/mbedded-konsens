# Plan: Execute-Verify with the majority (stage D2 of plan-state-in-lmdb.md)

This plan gives the protocol and the model of section 4 of
doc/plan-state-in-lmdb.md, in the form that the owner gave on
2026-10-25 09:26 to 09:30:

- A node that finds a fault in itself throws.
- A peer that delivers a wrong value is asked to try again. When the
  value is still wrong, the peer is told that it is broken.
- The majority shows which value is right.
- One repair runs automatically. Each further repair waits for the
  operator.

The first form of this plan (cd919bb) checked the result before the
accept, and a review rejected it with five blocking findings (the veto
in a new ballot, the own count of the leader, the run base, the throw
without a durable proof). This form checks the result after the
decide, and leaves Sequence Paxos as it is (provisional, card of
2026-10-25: "after the decide"). Nothing of it is built before the
model runs of section 4 show no violation and a second review approves
this plan.

## 1. The protocol

### 1.1 The result of an entry

1. Each entry of kind `register_client`, `client_request` or
   `expire_session` carries the result of its run on the leader that
   proposed it: the reply and the mutations. The entry CRC and the chain
   CRC cover the result. This replaces item 4.1 of plan-state-in-lmdb.md
   (it has no `error` field: a user error is a reply, section 7.4).
2. A node agrees with an entry when the result of its own run equals the
   result in the entry. The compare walks the CBOR items: a float agrees
   within 1 ULP in any width, NaN agrees with NaN, +0 and -0 agree; every
   other item, and every key of a mutation, agrees byte for byte (owner,
   2026-10-25 09:22).
3. The assumption: the machine of a node that works gives the same
   result for the same entry on the same state. For each entry, the nodes
   whose run gives a wrong result are fewer than a majority, over all
   time. Then each majority holds a node that works, and two nodes that
   work agree.

### 1.2 The run

1. The leader runs each proposal before it appends it, on the state of
   its log before the proposal, and writes the result into the entry.
   The run uses the open state transaction of D3.
2. Every node runs each decided entry, in log order, on the state of the
   prefix before it. That state is the committed state after the
   verdicts of the earlier entries (1.3). A node may run ahead in its
   open transaction on the assumption that every earlier entry is
   applied; a verdict `rejected` on an earlier entry aborts the open
   transaction, and the node runs again from the applied index (D3,
   plan-state-in-lmdb.md 5.2).
3. Sequence Paxos does not change: an entry is accepted, chosen and
   decided as bytes, as now. Agreement is a second step after the decide.

### 1.3 The verdict

1. A node that ran a decided entry sends `verified(ballot, index,
   agrees)` (provisional) to the leader of its promise, for each index in
   order. A node that does not agree with an entry first runs it a second
   time on the same state ("try again"); when it still does not agree, it
   sends `agrees = false` ("you are broken"). The leader runs the same
   step for its own run and counts itself.
2. The leader counts, for each decided index, the nodes that agree and
   the nodes that do not. The counts belong to one ballot.
3. When a majority agrees with the entry at `k`, the leader raises its
   `verified_idx` (provisional) to `k` and sends it in the next Decide,
   AcceptDecide or heartbeat. A node applies the decided entries up to
   `verified_idx`: it writes the mutations of the entry, which are the
   bytes of the leader (owner, 2026-10-17 12:40).
4. When a majority does not agree with the entry at `k`, the leader
   appends the entry `rejected(k)` (provisional) to its log, as a normal
   proposal. When `rejected(k)` is decided, every node applies nothing for
   `k`, and the client of `k` gets the error `client_errc::rejected`
   (provisional). `verified_idx` then passes `k`.
5. One verdict exists for each index. The reason: an `agrees` majority
   and a `does not agree` majority share a node, and by 1.1.3 that node
   and a working node of the other majority give the same result. So the
   leader never counts both majorities for one index, and a later leader
   that counts again finds the same verdict.
6. A node in the minority knows that its own machine does not work: a
   node that did not agree with `k` and sees `verified_idx >= k` without
   `rejected(k)`, or a node that agreed with `k` and sees `rejected(k)`
   decided. It throws (section 2). The leader is such a node when its own
   run is in the minority.
7. A new leader collects the verdict state from the promises: each
   promise carries the applied index of the node and its last `verified`
   answers above it. The leader counts again from there. Until it has a
   majority for an index, nothing above that index is applied.
8. A `verified` message and `verified_idx` are sent again on the resend
   timeout until the ballot changes, so a lost message does not block the
   count.

### 1.4 What a client sees

1. A request gets its reply after the entry is applied, that is after
   its verdict. This adds one exchange of messages to each write.
2. A read through the read index waits until `verified_idx` reaches the
   read index.

## 2. A fault of the own node throws, with a proof that stays

1. A damage that a step finds (`error_is_a_damage`) and a result throw of
   1.3.6 write a proof into the node storage before the throw: the kind,
   the index, the CRCs or the digests, and the ballot. A failure of that
   write throws `sync_error`, as for every commit.
2. Then the node throws `node_fault` (provisional), a type derived from
   `std::system_error` with an error code of `paxos_errc`. Nothing
   catches it but `main` and the body of a thread.
3. The start reads the proof. A proof counts as a damage of plan
   2.4 item 8: the node repairs from a peer while its count allows it,
   and else waits for the operator. The mark of the open repair stays, so
   a crash during a repair does not count again.
4. The start also runs the scan of plan-repair-from-peer.md 2.3 over the
   whole log. A damage that the scan finds counts the same way. The cost
   of the scan grows with the log length; section 5 of the parent plan
   measures it.
5. `with` gives no stopped node for a damage any more; it gives a node
   that repairs, or one that waits for the operator.

## 3. The model

1. A constant `ExecuteVerify` (Boolean). FALSE keeps every present
   configuration as it is.
2. A constant `MaxWrong` and a ghost `wrong`, a set of pairs (node,
   value): the runs that give a wrong result. A run of a node adds a pair
   or not (nondeterministic), so a second run can give the right result,
   and the retry of 1.3.1 is in the model. The constraint: for each value,
   fewer than a majority of the nodes are in `wrong`.
3. Each entry of the log carries the field `res \in {"ok", "bad"}`, set
   by the leader at the append from its own run. Session entries carry it
   too; one configuration runs with `ClientSessions`.
4. Variables: `verified_idx` (per node), `verdicts` (per leader: index to
   the sets of nodes that agree and that do not), `answered` (per node:
   the last index it answered).
5. Actions: `RunDecided(i)` (a node runs the next decided entry and sends
   `verified`), `HandleVerified`, `LeaderRaisesVerified`,
   `LeaderRejects` (appends `rejected(k)`), `ApplyVerified(i)` (the
   state takes the entries up to `verified_idx`, with the rejected ones
   left out), `ResultThrow(i)` (1.3.6, through `Crash` with a proof, and
   the start counts the proof as a damage).
6. A throw of a damage becomes `Crash` with a proof plus the detection
   at the start, in place of the atomic `DamageFound`. `Detected` and the
   step check use one shared operator, and the start check uses it over
   the whole log. The high-water mark is lost at the throw, as at every
   restart.
7. New invariants:
   - `OneVerdict`: no index has both an `agrees` majority and a `does not
     agree` majority, and no node applies `k` while another applies
     `rejected(k)`.
   - `AppliedAgrees`: the state of each node holds only entries with
     `res = "ok"` for a run of a node that works.
   - `ThrowOnlyWrong`: only a node with a wrong run of the entry takes a
     result throw.
   - `CountsStay`: a result throw of a node counts toward its repairs.
   - S1 to S5, E1, StateIsDecided and EntryRunsOnOnePrefix stay.
8. Covers that must be violated, so that each path is reached: `NoReject`,
   `NoLeaderThrow`, `NoFollowerThrow`, `NoVerify`.
9. Mutations, each must break an invariant:
   - `ApplyWithoutVerdict` (a node applies at the decide): `AppliedAgrees`.
   - `LeaderCountsDisagreed` (the leader counts itself as agreeing):
     `AppliedAgrees` or `OneVerdict`.
   - `RejectWithOne` (one `does not agree` rejects): `OneVerdict` with a
     wrong follower.
   - `NoProofAtThrow` (the throw writes no proof): `CountsStay`.

## 4. The runs

Each configuration runs 15 minutes as a simulation first; a BFS runs
only where it can end (owner, 2026-10-19 05:55). At most 3 TLC runs at a
time, with JOBS=1 for builds while they run.

1. `MC_ev_three`: 3 nodes, `MaxWrong = 1`, one crash, `StateInLmdb`,
   `MaxInFlight = 2`, `MaxElection = 3`.
2. `MC_ev_sessions`: as 1, with `ClientSessions`.
3. `MC_ev_five`: 5 nodes, `MaxWrong = 2`, simulation only.
4. The four mutations of 3.9 each give a violation, and the four covers
   of 3.8 each give a trace.
