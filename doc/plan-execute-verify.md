# Plan: Execute-Verify with the majority (stages D2 and D3 of plan-state-in-lmdb.md)

This plan gives the protocol and the model of sections 4 and 5 of
doc/plan-state-in-lmdb.md, in the form that the owner gave:

- A node that finds a fault in itself throws (2026-10-25 09:26).
- A peer that delivers a wrong value is asked to try again. When the
  value is still wrong, the peer is told that it is broken (09:26).
- The majority shows which value is right (09:26).
- One repair runs automatically. Each further repair waits for the
  operator (09:30).
- The result comes from every node. The leader is one of many. When a
  majority has the same result, it is entered as the result of the
  request (10:20 to 10:22).
- The library runs on any consumer hardware, also without ECC (owner, a
  week before 2026-10-25; repeated 10:25). A single bit flip in RAM can
  hit the library on any node, not only the state machine.

## 0. What this plan replaces

- In plan-state-in-lmdb.md: items 4.1 to 4.9, the compare in item 5.2,
  item 7.2, and the tests "a result after the decide that differs from
  the entry stops the vote", "a follower whose result differs does not
  accept and reports" and "a follower writes the write list of the
  leader". Items 5.1 and 5.3 to 5.5 stay.
- In plan-repair-from-peer.md: item 4.1 (when the mark closes and what
  counts), item 4.3 (the repair after a damage in a step becomes a
  restart), and the close of the open mark at each start that votes
  (node.hpp, `with`).
- The format version of the file and of the wire goes up for the two
  new kinds of entry (plan-lmdb-only-source.md 2.9).
- The session record of a request is derived from the chosen reply.

## 1. History of this plan

- Form 1 (cd919bb): the leader runs first, the followers check before
  the accept. Rejected: the veto in a new ballot, the own count of the
  leader, the run base, the throw without a durable proof.
- Forms 2 to 4 (e2eec1f, 90fd61d, 49ecc88): the leader runs first, every
  node checks after the decide. Rejected or withdrawn: a rejection voided
  every entry after it, and the void ranges were hard to tie to the log.
  Form 4 put each verdict into the log; this form keeps that.
- Form 5 (d8b6a53): every node runs, and the result of the majority goes
  into the log. Approved with fixes: an idle cluster wrote verdicts
  without end; one bit flip on the leader could get a wrong result
  decided and make every working node throw; a working node could throw
  under the 1 ULP rule; the selection changed while answers arrived.
- This form (6) has those fixes.

Nothing of it is built before the model runs of section 5 show no
violation and a review approves the parts that changed from form 5.

## 2. The protocol

### 2.1 The entries

1. A request entry (`register_client`, `client_request`,
   `expire_session`) carries the request only, as in D1.
2. Two new kinds of entry (names provisional). Only the leader appends
   them, as normal proposals:
   - `results(from, to, list)`: for each request entry from `from` to
     `to`, the chosen result (the reply and the mutations), the CRC32C of
     that result as the node that ran it computed it, and the ids of the
     counted nodes whose result did not agree.
   - `rejected(k)`: no result for `k` has a majority.
3. A `results` entry covers at least one request entry. The entry CRC
   and the chain CRC cover both kinds, as every entry.

### 2.2 The result and the agreement

1. A run of a request entry gives a result: the reply and the mutations,
   in deterministic CBOR (RFC 8949 4.2). The run reads the state through
   the view and writes nothing (plan-state-in-lmdb.md 2.1), so it needs no
   nested transaction.
2. Two results agree when the compare walks the CBOR items and each pair
   agrees. A float of any width is decoded to `double`; two floats agree
   when they are equal to `float_digits` significant decimal digits
   (relative compare, provisional default 12); NaN agrees only with NaN;
   +0 and -0 agree (owner, 2026-10-25 10:54, replaces 1 ULP of
   2026-10-17 12:40; measured in
   actor-raft/floats/results/20261025T1045Z-ulp-spread.md: 1 ULP is too
   narrow, glibc 2.39 against 2.44 gives up to 4 ULP). Every other item,
   and every key of a mutation, agrees byte for byte.
2a. After the selection, each float of the chosen result is rounded to
   `float_denominator` (optional) with round half to even, before it is
   stored (owner, 2026-10-25 10:52-10:54). The rounding works on the one
   chosen value, so every node stores the same bytes. The form:
   `std::to_chars` in scientific form with the precision, then
   `std::from_chars` (both correctly rounded; one run measured 58 ns per
   value with g++ 16 and clang 22, noise floor not measured).
   `float_digits` and `float_denominator` are fields of the state machine
   (provisional).
3. The assumption: for each entry, the nodes whose run gives a wrong
   result are fewer than a majority of the configuration, over all time,
   and the results of the nodes that work agree with each other. Outside
   it: working nodes whose floats differ in more than `float_digits`
   digits. Then the entry can be rejected.

### 2.3 The verdict function

1. `V` walks a decided prefix in log order with a pointer `p`, which
   starts at 0, and a map `chosen` from index to result:
   - `results(from, to, list)` at index `m` with `from = p + 1`, `to <
     m`, and a request entry in `from .. to`: `chosen` takes the list,
     and `p = to`.
   - `rejected(k)` at index `m` with `k = p + 1`, `k < m`, and a request
     entry at `k`: `k` is void, and `p = k`.
   - Any other `results` or `rejected` entry is stale and changes
     nothing.
   An index of `from .. to` that holds no request entry has no result in
   the list.
2. Every node computes `V` from its own decided prefix. Two nodes with
   the same decided prefix have the same `p` and the same `chosen`.
3. The applied index of a node is the highest decided index such that
   every request entry at or below it is at or below `p`. So an idle log
   whose last entries are verdict entries is applied to its end, and a
   read through the read index (the log length of the leader) is
   answered.
4. A node applies the request entries up to `p` that are not void, with
   the bytes in `chosen`. The state is then equal on every node (owner,
   2026-10-17 12:40). A void entry keeps its index and changes no state
   and no session; its client gets `client_errc::rejected` (provisional).
5. Before the node writes a chosen result, it checks the CRC of 2.1.2
   over the bytes it holds. After it writes the mutations, it reads each
   value back in the same transaction and checks its CRC again
   (provisional, measured in section 6). A difference is a fault of the
   own node and throws.
6. A leader proposes a StopSign only when `p` of its log covers every
   request entry.

### 2.4 The base and the run

1. The base of the request entry at index `q` is the committed state
   plus the chosen results of the request entries from the applied index
   up to `q - 1` that are not void. For an entry above `p` that has no
   verdict yet, a node uses its own result in the base. That is a guess.
2. The open state transaction of D3 holds the base: the committed state,
   then the chosen results, then the own results above `p`.
3. When a verdict entry becomes valid by `V`, and a chosen result is not
   byte for byte the own result, or an entry is void, the node aborts
   the open transaction, writes the chosen results from the applied index
   again, and runs the entries above `p` again. A cut of the log, a
   crash, a recovery and a repair abort it as well (plan-state-in-lmdb.md
   5.3, 5.4).
4. Every node runs every decided request entry. The leader proposes no
   more request entries while its open transaction holds
   `max_in_flight` of them above its applied index.

### 2.5 The answers

1. A node that votes, does not recover and does not repair runs each
   decided request entry above `p` in log order.
2. It sends `answer(ballot, p, from, base_crc, results)` (provisional
   message) to the leader of its promise, once in each step, for the
   indexes that it ran since its last answer. `p` is the pointer of its
   decided prefix. `base_crc` is a CRC over its own results in `p + 1 ..
   from - 1`. Each result carries its CRC32C, computed on this node.
3. When `p` of the node rises, it sends its answer again at once, from
   the new `p`. At each new promise it answers again from `p`. Answers
   are sent again on the resend timeout until the ballot changes.

### 2.6 The selection

1. The leader is a node like every other: it runs, and its answer counts
   as one.
2. The leader counts an answer for index `q` only when the answer ran on
   the selected base: `base_crc` equals the CRC over the selected results
   in `p + 1 .. from - 1`, and the results of the answer in `from .. q -
   1` are the selected bytes. An answer whose base is not selected yet
   is kept and counted later.
3. For each index above `p`, in order, the leader selects the counted
   result that agrees with the most counted results, ties broken by the
   lowest node id, once at least a majority of the configuration agrees
   with it. A selection holds for the whole ballot.
4. Before it selects, the leader sends `try_again(q)` (provisional) once
   to each counted node whose result does not agree with the result it
   is about to select. Such a node runs `q` again on the same base and
   answers again. Its second answer counts in place of the first. The
   selection does not wait for the second answer. A node goes into the
   list only when its second answer also does not agree. A node with no
   second answer at the append is not counted and not listed.
5. With results selected for `p + 1 .. j`, `j` the last of them, and no
   verdict entry of its log above `p` that is not decided, the leader
   appends `results(p + 1, j, list)`: the selected bytes with the CRC of
   the node that sent them, and the counted nodes that did not agree.
   When the counted answers for `p + 1` leave no result that a majority
   could still reach, it appends `rejected(p + 1)`. It appends at most one
   verdict entry in each step; group commit carries it with the other
   entries of the step.

### 2.7 Accept means verify, for a verdict entry

1. A node sends `accepted` for a `results` entry only after it checked
   it: the CRC of each listed result holds, and for each index it compares
   its own result on the check base with the listed result. If its own
   result differs, it runs again first. A node that is not listed accepts
   when its result agrees. A listed node accepts only when its result
   still does not agree after the second run; when it agrees, the list is
   wrong and the node refuses.
2. The check base of a verdict entry at index `m` is `V` over the node's
   accepted log up to `m - 1`. In one ballot that log equals the log of
   the leader. A node does not wait for a decide to form it, and never
   refuses because its own `p` is lower.
3. A `rejected(k)` entry carries the counted answers for `k`. It is
   accepted only when the node's own answers for
   `k` would not change it (the node has no result for `k` that the
   counted answers and its own make a majority).
4. In `sequence_paxos`, a node sends `accepted` only up to its
   `verified_idx` (provisional), which the node raises after the check
   (plan-state-in-lmdb.md 4.5, kept for this use). A request entry needs
   no check, so `verified_idx` passes it at once. After a restart,
   `verified_idx` is the decided index; it is not durable. The leader counts its
   own accept also only up to its own `verified_idx`: it checks its own
   verdict entry after the append, as every node does.
5. A promise carries the log of the node only up to its `verified_idx`.
   So a new leader never takes over a verdict entry that no node checked.
6. A node that does not accept a verdict entry sends `refused(ballot,
   index)` (provisional) to the leader, with the reason, again on each
   resend timeout until the ballot changes. A leader that gets `refused`
   for a verdict entry that its own ballot appended, from a majority of
   the configuration, knows that its own selection or copy is wrong. It
   throws (section 3): "you are broken" from the majority. A leader does
   not throw for an entry that an earlier ballot appended.
7. So a decided verdict has a majority that checked it on their own
   nodes. A bit flip in the leader's count, compare or copy cannot reach
   the state.

### 2.8 The minority

1. A node listed in a `results` entry as a node that did not agree, at
   an index whose result is chosen, runs that index again on the check
   base. It throws (section 3) only when its own result still does not
   agree with the chosen result. When it agrees, the list is wrong: the
   node does not throw and reports the leader of that entry with a proof.
   The list makes a node run again; it does not make a node throw.
2. A node that was not counted, ran on another base, or is not listed
   does not throw.
3. A void `k` makes no node throw. The proof goes to the operator.

### 2.9 What a client sees

1. A request gets its reply when its entry is applied: the reply in
   `chosen`, with its CRC. This is a second round after the decide of the
   request: the decide of its `results` entry.
2. A read through the read index waits until the applied index (2.3.3)
   reaches the read index, as now.

## 3. A fault of the own node throws, with a proof that stays

1. A damage that a step finds (`error_is_a_damage`), a read error of LMDB
   (owner, 2026-10-24 14:39), and a throw of 2.3.5, 2.7.4 and 2.8.1 write
   a proof into the node storage: the kind, the index, the CRCs or the
   digests, and the ballot. The proof is one field of the `repair_record`.
   A failure of that write throws `sync_error`.
2. Then the node throws `node_fault` (provisional), a type derived from
   `std::system_error` with an error code of `paxos_errc`. Nothing
   catches it but `main` and the body of a thread.
3. The start reads the record:
   - A proof whose index is at or below the index of the open mark is
     covered: the start clears it, in one commit, and counts nothing.
   - Any other proof counts as a damage: `count_a_repair` sets the count,
     sets the open mark with the index of the proof, and clears the proof,
     in one commit. While the count allows it, the node repairs; else it
     waits for the operator.
4. For a result proof the repair is the abort of the open transaction and
   the recovery. The node keeps its log. Its `p` covers the index of the
   proof, so it writes the chosen bytes and does not run that entry
   again.
5. The open mark closes when the applied index passes its index, or
   after the commit of a repaired log; never at a start only because the
   log votes.
6. The start also runs the scan of plan-repair-from-peer.md 2.3 over the
   whole log. A damage that it finds counts as in 3.3 (question 7.2).
7. `with` gives no stopped node for a damage any more; it gives a node
   that repairs, or one that waits for the operator.

## 4. The model

1. A constant `ExecuteVerify` (Boolean). FALSE keeps every present
   configuration as it is. The configurations of section 5 set
   `StateInLmdb`, `RepairFromPeer` and `StartDamageCounts`.
2. `Result(v, b)` gives a result for value `v` on base `b`, a sequence of
   (value, result) pairs. A result is a pair (value, jitter), jitter in
   {0, 1, 2}; two results agree when their values are equal and their
   jitters differ by at most 1. A run of a working node gives jitter 0 or
   1; with `Spread`, also 2. A ghost `wrong`, a set of pairs (node,
   value): a run of node `i` on `v` gives `Bad(i)` when `<<i, v>> \in
   wrong`. A run may add a pair while fewer than a majority of the nodes
   hold a pair for `v` and the number of pairs is below `MaxWrong`. A
   second run may give the right result.
3. `Flip` (Boolean): one step of the leader may replace a selected result
   by `Bad(leader)`, or move a node into or out of the list, before it
   appends the `results` entry, at most `MaxFlip` times.
3a. The model has no CRC32C. A CRC that holds is the assumption of 2.3.5,
   and the model does not check it. A flip that a CRC finds is outside
   the model; the code tests of 2.3.5 cover it.
4. Verdict entries are values `Results(from, to, list)` and
   `Rejected(k)`. S3 counts them as proposed. `V(log)` is an operator, the
   same as 2.3.1.
5. `Answer`, `TryAgain` and `Refused` travel in `chan`, so Lose and
   Duplicate reach them.
6. Variables: `selected` and `counts` (per leader), `own` (per node),
   `verified_idx` (per node), `proof` and `open_mark` (durable, per
   node), `result_throws`.
7. Actions: `RunEntry(i)`, `HandleAnswer`, `HandleTryAgain`, `Select(i)`,
   `AppendVerdict(i)`, `CheckVerdict(i)` (2.7), `HandleRefused`,
   `ApplyChosen(i)`, `ResultThrow(i)` (writes `proof`, then the start with
   the node's own durable state in one step, with 3.3; bounded by
   `MaxResultThrow`).
8. A damage that a step finds also writes `proof` and takes the start in
   one step. `Detected` and the step check use one operator, and the
   start check applies it to the whole log. The high-water mark is lost at
   the throw, as at every restart.
9. The effective prefix of a node: its decided prefix up to its applied
   index (2.3.3), each void entry kept at its index as a no-op.
   StateIsDecided, ClientTable, `Opened`, `Executed`, `ClientRegistered`,
   `ClientReplied`, `ClientSessionExpired`, the guard of `ExpireSession`,
   E1, ReadReply and R1 read the effective prefix. R1 compares with the
   history of entries that have a verdict.
10. A ghost `runs`: every run of every node, as (node, index, base,
    result).
11. Invariants:
    - `ChosenByMajority`: each result in `chosen` of a node agrees with
      the results of runs on its base by a majority of the configuration
      (from `runs`).
    - `ChosenIsRight`: without `Spread`, each result in `chosen` agrees
      with `Result(v, b)` on its base.
    - `ChosenOnOwnBase`: each chosen result was selected on the base that
      the chosen results before it define.
    - `StatesAgree`: the applied states of two nodes are prefixes of one
      another.
    - `ThrowOnlyWrong`: only a node with a wrong run of `q` takes a result
      throw for `q`. With `Spread`, a run whose jitter is 2 away from the
      selected result counts as a wrong run (provisional, question 7.5).
    - `RepairsBounded`: `result_throws` that count stay at most
      `AutoRepairs` before `OperatorAllowsRepair`.
    - `VerdictCoversRequest`: each `results` entry covers at least one
      request entry.
    - `PromiseIsVerified` and `RejectedHasNoMajority` (4.13).
    - S1 to S5, E1, R1, StateIsDecided and EntryRunsOnOnePrefix stay.
12. Covers, each must be violated by a trace: `NoResults`, `NoReject`
    (with `Spread`), `NoFollowerThrow`, `NoLeaderThrow` (with `Flip`),
    `NoResultsAfterLeaderChange`, `NoRunAgain`,
    `NoTryAgain`, `NoIdleRead` (a read answered on a log that ends with a
    verdict entry).
13. Mutations, each must break an invariant:
    - `ApplyOwnResult` (a node applies its own result): `StatesAgree`.
    - `CountOtherBase` (the leader ignores the base of an answer):
      `ChosenOnOwnBase`.
    - `SelectWithoutMajority` (the first answer is selected):
      `ChosenByMajority`.
    - `AcceptWithoutCheck` (2.7 off, with `Flip`): `ChosenByMajority`.
    - `ThrowWithoutRerun` (a listed node throws without its second run,
      with `Flip`): `ThrowOnlyWrong`.
    - `CountProofTwice` (3.3 counts a covered proof): `RepairsBounded`.
    - `VerdictWithoutRequest` (2.1.3 off): the invariant
      `VerdictCoversRequest` (each `results` entry covers a request
      entry).
    - `ListedSkipsCheck` (a listed node accepts without its compare, with
      `Flip`, `MaxFlip = 2`: one flip makes the result bad, one lists a
      working node): `ChosenByMajority`.
    - `OwnAcceptUnchecked` (the leader counts its own accept at its log
      end, with `Flip`): the invariant `PromiseIsVerified` (a promise
      carries no entry above the `verified_idx` of its sender).
    - `StaleVerdictTaken` together with `VerdictWhileOpen` (`V` takes a
      verdict that does not start at `p + 1`, and the guard of 2.6.5 is
      off): `ChosenOnOwnBase`. With the guard on, no stale verdict is
      decided (reasoning of round 1, not proved), so there is no cover
      for it.
    - `RejectedUnchecked` (2.7.3 off, with `Flip`, where a flip turns a
      selection into `rejected(k)`): `RejectedHasNoMajority` (a decided
      `rejected(k)` has no result that a majority of runs on its base
      agree on). This invariant holds only for the provisional form of
      7.5, not for the option `rejected(k)` in place of a throw.
    - `AppliedIsPointer` (applied index = `p`): the cover `NoIdleRead`
      can not be violated.

## 5. The runs

Each configuration runs 15 minutes as a simulation first; a BFS runs only
where it can end (owner, 2026-10-19 05:55). A cover that a 15-minute
simulation does not reach counts as not shown. At most 3 TLC runs at a
time, with JOBS=1 for builds while they run. Each configuration holds one
class of fault:

1. `MC_ev_wrong`: 3 nodes, `MaxLog = 4`, `MaxWrong = 1`, no crash, no
   Lose, `MaxElection = 1`.
2. `MC_ev_flip`: as 1, `MaxWrong = 0`, `Flip`, `MaxFlip = 1`.
2a. `MC_ev_flip_change`: as 2, `MaxElection = 3`.
3. `MC_ev_change`: 3 nodes, `MaxLog = 5`, `MaxWrong = 1`, no Lose,
   `MaxElection = 3`.
4. `MC_ev_loss`: 3 nodes, `MaxLog = 4`, `MaxWrong = 1`, Lose 1,
   Duplicate 1, one crash, `MaxElection = 2`.
5. `MC_ev_spread`: 3 nodes, `MaxLog = 3`, `Spread`, `MaxWrong = 0`.
6. `MC_ev_sessions`: 3 nodes, `MaxLog = 6`, `ClientSessions`,
   `MaxClientSend = 4`, `MaxWrong = 1`, `MaxElection = 1`.
7. `MC_ev_five`: 5 nodes, `MaxLog = 4`, `MaxWrong = 2`.
8. Each mutation of 4.13 gives a violation in one of these, or in a
   configuration made for it.
9. `MC_ev_damage`: as 1, with a storage fault (4.8), so that the damage
   path and the proof run with Execute-Verify.
10. `CountProofTwice` may be shown by a simulated trace to a recovered
   node that is listed again, then a BFS from that state.

## 6. What section 5 of the parent plan measures

1. Writes per second, and the p50 and the p99 of the reply latency,
   with and without Execute-Verify, for a machine with integers only and
   for one with floats (where the bytes of the nodes differ).
2. How many runs happen again (2.4.3) per thousand entries, in both
   cases.
3. The cost of the read back of 2.3.5.
4. The time of the full scan at a start, against the log length.

## 7. Questions for the owner

1. "Try again" is a second run on the node whose result does not agree,
   on the request of the leader (2.6.4).
2. The full scan of the log at each start grows with the log, and no
   compaction is planned yet. Proposal: measure it (6.4), and plan
   compaction after that.
3. Each node sends its whole result to the leader, because results that
   agree within 1 ULP have different bytes. Proposal: measure it; a
   digest first and the bytes only when the digests differ is the known
   remedy.
4. A write needs a second round before its reply. With floats, the
   nodes whose bytes were not selected run again at each verdict. The
   known remedy: keep the run of each entry in its own nested transaction
   above a parent that holds the chosen part, so that only that entry is
   run again. Proposal: measure first (6.1, 6.2).
5. With `Spread`, a working node can give a result outside the tolerance
   of the contract. Proposal (provisional in 4.11): such a result counts
   as wrong, and the node throws when it is listed. Option: the leader
   appends `rejected(k)` in place of a throw.
