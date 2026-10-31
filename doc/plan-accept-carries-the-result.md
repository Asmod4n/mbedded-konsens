# Plan: the accept carries the result of the leader

Status: plan. Nothing of it is built before the model runs of section 5
show no violation, a second agent reviews this plan, and the owner gives
the go.

## 1. Facts

1. A write now takes three exchanges of messages before its sync: the
   leader sends `work`, each follower runs the request and sends
   `answer`, the leader selects the result of a majority and sends
   `accept_decide`. Then the nodes sync and send `accepted`, and the
   leader sends `decide`.
2. Measured at 64 writes in flight with arm E
   (bench/results/2026-10-30-223000Z-konsensd-timeline-of-a-write.txt):
   work to the first answer 983 us, second answer to the accept_decide
   758 us, accept_decide to the first accepted 2 525 us, first accepted
   to decide 629 us, work to decide 5 187 us.
3. A follower appends each entry of an accept and then checks it against
   its own run (`check_the_done_record`, node.hpp:1219-1222). The check
   gates only `accepted` through the verified index. It refuses with
   `result_differs` when its result does not agree after a second run,
   unless the entry lists it as a node that did not agree.
3a. `handle_decide` raises the decided index up to the log length, not up
   to the verified index (sequence_paxos.hpp:1765).
3b. The leader throws `refused_by_a_majority` when a majority refused an
   entry (node.hpp:1385-1389). The refusals are counted in the selection.
3c. The accept sync keeps the prefix of a follower up to `valid`
   (send_accsync, sequence_paxos.hpp:1655-1658): with the same accepted
   ballot as the best promise, `valid` is the shorter of the two logs.
4. The owner rule of plan-execute-verify.md: the result comes from every
   node, the leader is one of many, and a result enters the log when a
   majority has the same result (2026-10-25 10:20 to 10:22).
5. Form 1 of plan-execute-verify.md (cd919bb) had the leader run first
   and the followers check before the accept. A review rejected it for
   the veto in a new ballot, the own count of the leader, the run base
   and the throw without a durable proof.

## 2. Known form

1. Eve (Kapritsos et al., OSDI 2012) runs the requests before the
   agreement and then agrees on the output, not on the input. Not read
   again in this session.
2. Paxos stays safe when an acceptor refuses a value: a refusal is the
   same as a lost message. A value is chosen only when a majority
   accepted it in one ballot. If each acceptor accepts only a value whose
   result agrees with its own run, a chosen value has a majority that
   agrees with its result. That is rule 1.4.
3. The open point of every such form is the new ballot. Paxos makes the
   new leader propose again the value of the highest ballot in its
   promises, because that value can be chosen. With a check at the
   accept, that value can hold a result that only the old leader
   computed. Form 1 solved this with a veto. This plan solves it with
   the selection that the code already has (3.5).

## 3. The protocol (form 7, provisional)

1. Fast path. The leader runs a new request on its open state. Its base
   is its own log. It writes its result and the result CRC into the
   entry, appends the entry and sends `accept_decide`. There is no
   `work`, no `answer` and no selection on this path. The out list of
   the entry is empty.
2. A follower checks each entry of an `accept_decide` as now (1.3). When
   it agrees, it appends the entry and sends `accepted` after its sync.
   When it does not agree, it sends `refused(result_differs)` and appends
   nothing at or after that index in this ballot.
3. The leader counts itself after its own sync, as now. An entry is
   decided when a majority of the configuration accepted it.
4. The leader counts the refusals of the fast path for each index in
   its ballot, outside the selection. When a majority of the
   configuration refused index `k`, the leader takes a higher ballot. It
   does not throw `refused_by_a_majority` in the fast path. In the
   selection of 3.5 that throw stays as now.
5. New ballot. After a majority of promises, the leader takes the log of
   the promises as now, and `d`, the highest decided index of the
   promises. Before it sends `accept_sync`, it runs the present path
   (`work`, `answer`, selection) for each request entry of that log above
   `d`. The work names its base: `d`, the chain CRC of the log up to `d`,
   and the selected results above `d`. A node answers only when the chain
   CRC of its own log up to `d` is that CRC. It runs on its own log up to
   `d` and the selected results, not on its own entries above `d`; it
   aborts its open transaction for that. A node whose log is shorter than
   `d` does not answer. The `accept_sync` then carries the log up to `d` and
   the selected entries: the same request at the same index, and the
   result of a majority with its out list. An index with no result that a
   majority can reach gets a `rejected` entry, as now.
6. For each index, the leader first counts the answers that agree with
   the result of the taken entry by the float rule. When a majority
   agrees, it keeps the taken entry with its bytes. Only otherwise does it
   run the selection. The float rule is not transitive, so this order
   keeps a chosen entry with its bytes in every later ballot.
7. A follower checks each request entry of an `accept_sync` above the
   decided index of the sync, as in 3.2. It appends the entries up to the
   first entry that it does not agree with. This covers a follower that
   promises late, in the accept phase of the leader, whose sync holds
   entries of the fast path that are not decided.
8. A follower that refused index `k` and sees `k` decided knows that its
   own run is wrong. Before it applies `k`, it runs `k` again; when the
   result still does not agree, it writes the proof and throws, as with
   `listed_result_differs` now (fact 1.3a: today it applies `k` with no
   check).
9. The kept prefix of 1.3c is bounded: `valid` is at most the first index
   above `d` whose entry the selection changed, minus one. Without the
   bound, a follower with the old entry keeps it, votes for it in the new
   ballot, and decides it (model mutation `NoRepairBound`).

## 4. Why it is safe

1. A chosen entry has a majority that accepted it in one ballot. Each of
   these nodes ran it on the same base and agreed (3.2, 3.7), or the
   leader selected it from a majority of answers (3.5).
2. The assumption of plan-execute-verify.md 2.2.3: for each entry, the
   nodes whose run gives a wrong result are fewer than a majority. So
   the result of a chosen entry is the result of the nodes that work.
3. A later leader selects again on the same base (3.5). The nodes that
   work give the same result, so the selection agrees with the chosen
   entry, and 3.6 keeps its bytes. So the value at the index stays the
   same in each later ballot, which is what Paxos needs from a new
   leader.
4. The answers to the findings of form 1: the veto is replaced by 3.5;
   the leader counts itself as one node (3.3); the base is the log of
   the node (3.1, 3.5); the throw writes its proof first (3.8).

## 5. Model

1. A new module `AcceptWithResult` (provisional name) under model/tla,
   built from OneEnvironment.tla: the same commit, sync, hold, crash,
   restart and recovery. It leaves out the tombstones, the ticks and the
   store rollback, which OneEnvironment checks and which this plan does
   not change.
2. An entry is `[bal, req, res]`. The result of node `i` for request `r`
   on base `B` is `<<"ok", r, results of B>>`, or `<<"bad", i, r>>` when
   `<<i, r>>` is in `faulty`. `faulty` is chosen at the start, with at
   most `MaxFaulty` pairs and fewer than a majority of nodes for each
   request. Results are exact in the model; the float rule of 3.6 is not
   in it.
3. Actions: `Propose` (3.1), `HandleAccept` (3.2), `LeaderDecide` and
   `HandleDecide` as in OneEnvironment, `Prepare`, `HandlePrepare`,
   `HandlePromise`, the selection of 3.5 as `AnswerWork`, `Select` and
   `FinishSelection`, `HandleAcceptSync` with the kept prefix of 3.9 and
   the check of 3.7. `AnswerWork` holds the work message: a node answers
   while the leader works on the index, when its log up to `d` is the
   taken log up to `d` (the chain CRC of 3.5). A wrong result depends on
   its base. A new ballot can start at any time, so 3.4 is a liveness
   rule and is not needed for the safety runs.
4. Invariants: `ChosenOnce` (request and result), `DecidedAgree`,
   `DecidedIsChosen`, `ChosenIsTrue` (each chosen entry has the result
   of the nodes that work on the chosen prefix), `MajorityVotedOnce`
   (a value that a majority holds durable in one ballot is the only such
   value at its index, plan 4.3).
5. Mutations, each must break an invariant: `NoCheckAtAccept` (3.2 off),
   `NoCheckAtSync` (3.7 off), `NoSelectionAtSync` (3.5 off: the rule of
   Omni-Paxos), `NoRepairBound` (3.9 off), `LeaderCountsUnsynced`.
5a. Not in the model: the `rejected` entry, entries that are not
   requests (StopSign, sessions), the second run, the float rule (results
   are exact, so 3.6 is not checked), and 3.8.
6. Covers, each must be reached: a request first proposed with a wrong
   result is chosen with the right result; a follower refuses; an entry
   is chosen after a crash.
7. Runs: TLC breadth-first with small bounds (3 nodes, 2 requests, 3
   ballots, log of 2, at most one crash), one run per configuration;
   simulation only where breadth-first does not end. Results go to
   model/results.

## 6. Code (after the go)

1. `drive_the_tasks` (node.hpp) runs the request on the leader and
   proposes the entry with its result, in place of `send_work`.
2. The selection runs only between the majority of promises and the
   `accept_sync` (3.5). This is a new phase of the leader in
   sequence_paxos: `handle_majority_promises` now sets the accept phase,
   `accepted_stop_sign` and the read round at once
   (sequence_paxos.hpp:1605-1644), and a promise that arrives in the new
   phase must wait for the sync (`handle_promise_accept`, :1594). The work
   carries the base of 3.5, and the answer pointer is `d`
   (selection.cpp:122 needs the pointer of the leader).
2a. `send_accsync` takes the bound of 3.9.
2b. The refusals of the fast path are counted outside the selection
   (3.4).
2c. `reconnected` (node.hpp:671-676), `resend_work_`, `tasks_` and
   `try_again` change or go: the fast path has no work. `assign_the_tasks`
   keeps `refuse_if_reconfiguring`.
2d. A refused index is run again before it is applied (3.8).
3. `check_the_done_record` stays. Its check of the out list holds for an
   empty list.
4. Tests in test_execute_verify.cpp, one for each item of section 3, and
   the three simulation tests with 2 000 seeds.

## 7. Measurement

1. Arms: konsensd of linear-selection (7323c7a) and of this branch, and
   a copy of the base for the noise floor. Each in g++ 15, g++ 16 and
   clang 22. bench-logs/arms3.sh, 4 rounds, 64 bytes, 1 and 64 in flight.
2. The timeline arm (bench/arms/timestamps-of-a-write.patch) on both, at
   64 in flight, for the split of 1.2.

## 8. Review

1. Round 1 (second agent, 2026-10-31): reject. Blocking: the kept prefix
   of the sync keeps a repaired entry (now 3.9, mutation `NoRepairBound`
   breaks `DecidedAgree`); the base of a node in the selection does not
   exist in the code and the model gave it for free (now 3.5 and the
   precondition of `AnswerWork`). Should fix: the throw of 1.3b (now 3.4);
   3.8 had no code and fact 1.3 was wrong (now 1.3, 1.3a, 3.8); the float
   rule could repair a chosen entry (now 3.6); the model is weaker than
   the plan (now 5.4, 5.5a); no complete run checks the base (a run with
   a log of 2 and no crash). Notes: 3.7 alone (mutation `NoCheckAtSync`
   alone), the code of 6.2c.

## 9. Owner questions

1. The name `AcceptWithResult` of the module has no source.

## 10. Data on the disk

Listed at the end of the work.
