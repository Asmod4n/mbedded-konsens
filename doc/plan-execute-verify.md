# Plan: Execute-Verify with the majority (stages D2 and D3 of plan-state-in-lmdb.md)

This plan gives the protocol and the model of sections 4 and 5 of
doc/plan-state-in-lmdb.md, in the form that the owner gave on
2026-10-25:

- A node that finds a fault in itself throws (09:26).
- A peer that delivers a wrong value is asked to try again. When the
  value is still wrong, the peer is told that it is broken (09:26).
- The majority shows which value is right (09:26).
- One repair runs automatically. Each further repair waits for the
  operator (09:30).
- The result comes from every node. The leader is one of many (10:20).

It replaces items 4.1 to 4.9 of plan-state-in-lmdb.md, the compare in
its item 5.2, and its tests "a follower whose result differs does not
accept and reports" and "a follower writes the write list of the
leader". Items 5.1, 5.3 to 5.5 stay.

History of this plan:

- Form 1 (cd919bb): the leader runs first, the followers check before
  the accept. Rejected: the veto in a new ballot, the own count of the
  leader, the run base, the throw without a durable proof.
- Forms 2 to 4 (e2eec1f, 90fd61d, 49ecc88): the leader runs first, every
  node checks after the decide. Rejected or withdrawn: a rejection of the
  leader's result voided every entry after it, and the void ranges and
  counts were hard to tie to the log. Form 4 put each verdict into the
  log; this form keeps that.
- This form: no node runs first. Every node runs each decided entry, and
  the result of the majority goes into the log.

Nothing of it is built before the model runs of section 4 show no
violation and a review approves this plan.

## 1. The protocol

### 1.1 The entries

1. A request entry (`register_client`, `client_request`,
   `expire_session`) carries the request only, as in D1. These kinds are
   the entries that change the state.
2. Two new kinds of entry (names provisional). Only the leader appends
   them, as normal proposals:
   - `results(from, to, list)`: for each index from `from` to `to`, the
     result that a majority agreed with: the reply and the mutations.
   - `rejected(k)`: no result for `k` has a majority.
3. The entry CRC and the chain CRC cover both kinds, as every entry.

### 1.2 The result and the agreement

1. A run of a request entry on a node gives a result: the reply and the
   mutations, in deterministic CBOR (RFC 8949 4.2).
2. Two results agree when the compare walks the CBOR items and each pair
   agrees. A float of any width is decoded to `double`, and two floats
   agree within 1 ULP of `double`: the distance of the bits in binary64,
   NaN agrees only with NaN, +0 and -0 are 1 ULP apart (owner,
   2026-10-17 12:43, 2026-10-25 09:22). Every other item, and every key of
   a mutation, agrees byte for byte.
3. The assumption: for each entry, the nodes whose run gives a wrong
   result are fewer than a majority, over all time; and the results of
   the nodes that work agree with each other. Then a result with a
   majority exists, and it agrees with each working node. Outside the
   assumption: a float whose results on working nodes lie more than 1
   ULP apart. Then no majority can exist, and the entry is rejected.

### 1.3 The verdict function

1. `V` walks a decided prefix in log order with a pointer `p`, which
   starts at 0, and a map `chosen` from index to result:
   - `results(from, to, list)` at index `m` with `from = p + 1` and
     `to < m`: `chosen` takes the list for the request entries in `from
     .. to`, and `p = to`. An index in that range that holds no request
     entry has no result in the list.
   - `rejected(k)` at index `m` with `k = p + 1` and `k < m`: `k` is
     void, and `p = k`.
   - Any other `results` or `rejected` entry is stale and changes
     nothing.
2. Every node computes `V` from its own decided prefix. Two nodes with
   the same decided prefix have the same `p` and the same `chosen`.
3. A node applies the request entries up to `p` that are not void, with
   the result in `chosen`. Each node writes the bytes of that result, so
   the state is equal on every node (owner, 2026-10-17 12:40). A void
   entry keeps its index and changes no state and no session; its client
   gets `client_errc::rejected` (provisional).
4. A leader proposes a StopSign only when `p` of its log covers every
   request entry.

### 1.4 The run

1. The base of the request entry at index `q` is the committed state
   plus the chosen results of the request entries from the applied index
   up to `q - 1` that are not void. For an entry above `p` that has no
   verdict yet, a node uses its own result in the base. That is a guess;
   1.4.4 and 1.5.3 deal with a wrong guess.
2. The open state transaction of D3 holds the base: the committed state,
   then the chosen results, then the own results above `p`.
3. A run happens in a nested transaction of that write transaction. The
   node keeps its result, aborts the nested transaction, and writes its
   result in the parent. The state environment has no `MDB_WRITEMAP`
   (plan-state-in-lmdb.md 1.1), so LMDB allows nested transactions there.
4. When a verdict entry for `q` becomes valid by `V`, and the chosen
   result is not byte for byte the own result, or `q` is void, the node
   aborts the open transaction. It writes the chosen results from the
   applied index again, and runs the entries above `p` again. A cut of
   the log, a crash, a recovery and a repair abort it as well
   (plan-state-in-lmdb.md 5.3, 5.4).
5. The open transaction holds at most `max_in_flight` request entries
   above the applied index. The leader proposes no more request entries
   while it is full.

### 1.5 The answers and the count

1. A node that votes, does not recover and does not repair runs each
   decided request entry above `p` in log order. When its result does not
   agree with the result of another node that it holds, it runs the entry
   a second time on the same base ("try again", question 5.1).
2. It sends `answer(ballot, from, base_crc, results)` (provisional
   message) to the leader of its promise, once in each step, for the
   indexes that it ran since its last answer. `base_crc` is a CRC over the
   results in its base above `p`. The leader is a node like every other:
   it runs, and its answer is counted as one.
3. The leader counts an answer for index `q` only when `base_crc` equals
   the CRC over the results that the leader selected for `p + 1 .. q - 1`.
   So an answer that ran on another base does not count.
4. For each index above `p`, the leader selects a result with a majority:
   it takes the answers in the order of the node id, and the first answer
   that a majority of the counted answers agrees with is selected.
5. With results selected for `p + 1 .. j`, and no verdict entry of its
   log above `p` that is not decided, the leader appends `results(p + 1,
   j, list)`. The list holds the bytes of the selected answers. When the
   counted answers for `p + 1` leave no result that a majority could
   still reach, the leader appends `rejected(p + 1)`. The leader appends
   at most one verdict entry in each step; group commit carries it with
   the other entries of the step.
6. The counts belong to one ballot and start at `p` of the leader's log.
   At each new promise, a node answers again from `p` of its decided
   prefix. The answers are sent again on the resend timeout until the
   ballot changes.

### 1.6 The minority

1. A node whose own result for `q` does not agree with `chosen[q]` knows
   that its own machine is wrong. It throws (section 2). A node whose
   answer for `q` ran on another base does not throw; it runs again from
   the new base (1.4.4).
2. A void `q` makes no node throw: no result had a majority, so no node
   knows that it is the wrong one. The proof goes to the operator.

### 1.7 What a client sees

1. A request gets its reply when its entry is applied: the reply in
   `chosen`. This is a second round after the decide of the request: the
   decide of its `results` entry.
2. A read through the read index waits until the applied index reaches
   the read index, as now.

## 2. A fault of the own node throws, with a proof that stays

1. A damage that a step finds (`error_is_a_damage`) and a throw of 1.6.1
   write a proof into the node storage: the kind, the index, the CRCs or
   the digests, and the ballot. The proof is one field of the
   `repair_record`. A failure of that write throws `sync_error`.
2. Then the node throws `node_fault` (provisional), a type derived from
   `std::system_error` with an error code of `paxos_errc`. Nothing catches
   it but `main` and the body of a thread.
3. The start reads the record. A proof that the open mark does not cover
   counts as a damage of plan-repair-from-peer.md 4.1: `count_a_repair`
   sets the count, sets the open mark with the index of the proof, and
   clears the proof, in one commit. While the count allows it, the node
   repairs; else it waits for the operator.
4. For a result proof the repair is the abort of the open transaction and
   the recovery. The node keeps its log. Its `p` covers the index of the
   proof, so it writes the chosen bytes and does not run that entry again.
5. The open mark closes when the applied index passes the index of the
   mark, or after the commit of a repaired log. A throw while the mark is
   open counts nothing more. A throw after the mark closed counts, and
   with the count at its limit the node waits for the operator.
6. The start also runs the scan of plan-repair-from-peer.md 2.3 over the
   whole log. A damage that it finds counts the same way (question 5.2).
7. `with` gives no stopped node for a damage any more; it gives a node
   that repairs, or one that waits for the operator.

## 3. The model

1. A constant `ExecuteVerify` (Boolean). FALSE keeps every present
   configuration as it is. The configurations of section 4 set
   `StateInLmdb`, `RepairFromPeer` and `StartDamageCounts`.
2. `Result(v, b)` gives a result for value `v` on base `b`, a sequence of
   (value, result) pairs. A ghost `wrong`, a set of pairs (node, value).
   A run of node `i` on `v` gives `Result(v, b)`, or a token `Bad(i)` when
   `<<i, v>> \in wrong`. A run may add a pair while fewer than a majority
   of the nodes hold a pair for `v` and the number of pairs is below
   `MaxWrong`. A second run may give the right result. A constant
   `Spread` (Boolean) lets two working nodes give results that do not
   agree, for the case outside 1.2.3.
3. Verdict entries are values `Results(from, to, list)` and
   `Rejected(k)`. S3 counts them as proposed.
4. `V(log)` is an operator, the same as 1.3.1.
5. The message `Answer` travels in `chan`, so Lose and Duplicate reach
   it.
6. Variables: `counts` (per leader), `own` (per node: its results above
   `p`), `proof` and `open_mark` (durable, per node), `result_throws`.
7. Actions: `RunEntry(i)`, `HandleAnswer(i, j, m)`, `AppendVerdict(i)`,
   `ApplyChosen(i)`, `ResultThrow(i)` (writes `proof`, then the start
   with the node's own durable state in one step, with the count of 2.3;
   bounded by `MaxResultThrow`).
8. A damage that a step finds also writes `proof` and takes the start in
   one step. `Detected` and the step check use one operator, and the
   start check applies it to the whole log. The high-water mark is lost at
   the throw, as at every restart.
9. The effective prefix of a node: its decided prefix up to `p`, each
   void entry kept at its index as a no-op. StateIsDecided, ClientTable,
   `Opened`, `Executed`, `ClientRegistered`, `ClientReplied`,
   `ClientSessionExpired`, the guard of `ExpireSession`, E1, ReadReply and
   R1 read the effective prefix. R1 compares with the history of entries
   that have a verdict.
10. A ghost `selections`: every selection a leader made, as (ballot,
    index, result, the nodes that agreed).
11. Invariants:
    - `ChosenHasMajority`: each result in `chosen` of a node agrees with
      a majority of the nodes in its selection.
    - `ChosenIsRight`: without `Spread`, each result in `chosen` equals
      `Result(v, b)` on its base.
    - `StatesAgree`: the applied states of two nodes are prefixes of one
      another.
    - `ThrowOnlyWrong`: without `Spread`, only a node with a wrong run of
      `q` takes a result throw for `q`.
    - `RepairsBounded`: a node takes at most `AutoRepairs` counted repairs
      before `OperatorAllowsRepair`.
    - S1 to S5, E1, R1, StateIsDecided and EntryRunsOnOnePrefix stay.
12. Covers, each must be violated by a trace: `NoResults`, `NoReject`
    (with `Spread`), `NoFollowerThrow`, `NoLeaderThrow`,
    `NoResultsAfterLeaderChange`, `NoStaleVerdictEntry`,
    `NoRunAgain` (1.4.4).
13. Mutations, each must break an invariant:
    - `ApplyOwnResult` (a node applies its own result, not `chosen`):
      `StatesAgree`.
    - `CountOtherBase` (the leader ignores `base_crc`): `ChosenIsRight`.
    - `SelectWithoutMajority` (the first answer is selected):
      `ChosenIsRight` or `ChosenHasMajority`.
    - `StaleVerdictCounts` (`V` takes a verdict entry with `from # p + 1`):
      `StatesAgree`.
    - `NoProofAtThrow` (the throw writes no proof): `RepairsBounded`.

## 4. The runs

Each configuration runs 15 minutes as a simulation first; a BFS runs only
where it can end (owner, 2026-10-19 05:55). A cover that a 15-minute
simulation does not reach counts as not shown. At most 3 TLC runs at a
time, with JOBS=1 for builds while they run. Each configuration holds one
class of fault:

1. `MC_ev_wrong`: 3 nodes, `MaxLog = 4`, `MaxWrong = 1`, no crash, no
   Lose, `MaxElection = 1`. Covers `NoResults`, `NoFollowerThrow`,
   `NoLeaderThrow`, `NoRunAgain`.
2. `MC_ev_change`: 3 nodes, `MaxLog = 5`, `MaxWrong = 1`, no Lose,
   `MaxElection = 3`. Covers `NoResultsAfterLeaderChange`,
   `NoStaleVerdictEntry`.
3. `MC_ev_loss`: 3 nodes, `MaxLog = 4`, `MaxWrong = 1`, Lose 1,
   Duplicate 1, one crash, `MaxElection = 2`.
4. `MC_ev_spread`: 3 nodes, `MaxLog = 3`, `Spread`, `MaxWrong = 0`. Cover
   `NoReject`.
5. `MC_ev_sessions`: 3 nodes, `MaxLog = 6`, `ClientSessions`,
   `MaxClientSend = 4`, `MaxWrong = 1`, `MaxElection = 1`.
6. `MC_ev_five`: 5 nodes, `MaxLog = 4`, `MaxWrong = 2`.
7. Each mutation of 3.13 gives a violation in one of these.

## 5. Questions for the owner

1. "Try again" is a second run on the node whose result does not agree
   (proposal).
2. The full scan of the log at each start grows with the log, and no
   compaction is planned yet. Proposal: measure it in section 5 of the
   parent plan, and plan compaction after that.
3. Each node sends its whole result to the leader, because results that
   agree within 1 ULP have different bytes, so a digest cannot show the
   agreement. A large result costs bandwidth on each node. Proposal:
   measure it; a digest first, and the bytes only when the digests
   differ, is the known remedy.
4. A write needs a second round before its reply: the decide of the
   request, then the decide of its `results` entry. One `results` entry
   covers every entry of a step. Section 5 of the parent plan measures
   writes per second and the p50 and the p99 of the reply latency.
