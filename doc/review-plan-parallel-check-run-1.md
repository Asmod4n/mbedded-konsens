# Review: doc/plan-parallel-check-run.md

Worktree wt-acc, branch parallel-check-run, commit 1379543. Reviewer: a
second agent. Date 2026-10-31.

## Task

Goal: find the faults of the plan before it is built. Place: the plan and
the code it relies on in wt-acc, the TLA+ model, the liburing of the build,
the bench results of 2026-10-31 and the two rule files. Assumptions: the
decisions of the owner hold (threads allowed, MSG_RING only, no mutex or
lock, one writer thread, SQLite of the distribution, no network IO in the
library, C++ API, `step` blocks). Not done: no build, no commit, no push, no
change of any file but this one.

## Findings

### 1. The job holds the wrong range of results: the snapshot is older than the applied index of the loop (blocking)

Evidence. `commit_phase_one` keeps the writing transaction open across
steps while a sync is out:
`if (txn_->has_writes() && a_sync_is_out()) { the_transaction_holds_ended_steps_ = true; return {}; }`
(include/konsens/sequence_paxos.hpp:486-489). `apply_the_done_records`
writes the state and `set_applied` into that open transaction
(include/konsens/node.hpp:1589, 1644). At 64 in flight with
`sync_by_the_caller`, a sync is out almost all the time. So the
`applied_idx` that the loop reads (node.hpp:1205, 1209) is usually
higher than the `applied_idx` of the last commit. A read-only connection
sees only the last commit. The pool reads a < job.applied_idx. The plan
(§2.3) puts results from job.applied_idx + 1 into the job, and §2.4 needs
results from a + 1. The results a + 1 .. job.applied_idx are not in the
job. The entries can also be absent from the snapshot, because they can be
appended in the open transaction.

`high_water_mark` has no applied index (sequence_paxos.hpp:304-309), so the
loop does not know the committed applied index today.

Fix. The loop keeps the applied index of the last commit (take it from
`u.meta.applied_idx` in `commit_phase_two`, sequence_paxos.hpp:501-520).
The job holds the results from that index + 1, read with
`read_the_open_log(committed_applied_idx, log_len)`. The pool checks
`a >= job.from` and `a <= loop applied_idx`; a violation is a fault of
ours and raises. Add this case to the tests of §5: a check while a sync is
out and the transaction holds ended steps.

### 2. The input list of the check is not complete: `own_`, `nodes_` and `machine_` (blocking)

Evidence. The claim of §1 is true for the state: the state at a plus the
leader results a + 1 .. k - 1 is the base. `apply_the_done_records`
writes exactly the leader result (`write_the_result(e, k, *r, w)`,
node.hpp:1633) with the same skips as `write_the_done_results` (rejected,
undecodable, node.hpp:1611, 1836-1839). Sessions go through the same
write set (write_set.hpp:34-40, node.hpp:1752). Meta, data and session are
tables of one file (src/persistent_storage.cpp:50-53). The chain CRC is not
an input. `compute_result_crc(q, e.result)` is a pure function of q.

But the check reads three more members of the node:

- `find_the_own_result_on` reads `own_` (node.hpp:1272-1289, called at
  1301). When an own result exists, the check takes it and does not run
  the request. The loop thread erases from `own_` in many places
  (node.hpp:1441, 1473, 1647, 1883, 1886, 1896, 1912, 1950). A read on a
  pool thread is a data race. A pool that ignores `own_` runs the request
  where today the check does not, and for a machine with float rules the
  outcome can differ from `check_threads = 0`. The test of §5 ("the same
  refusals") then fails.
- `check_the_rejected_record` reads `nodes_.size()` (node.hpp:1336).
- `run_request` and `results_agree` read `machine_` (node.hpp:1757,
  1309-1311). The loop thread calls `machine_.apply` and `machine_.query`
  at the same time (node.hpp:1623, 2129).

Fix. The loop resolves `find_the_own_result_on` while it builds the jobs.
An entry with an own result is checked on the loop thread, as today (it
runs no request). The job holds the majority as a number, the pid and the
float rule. The plan states that `M::apply` and `M::query` are called on
several threads at the same time, and that the state machine must allow
it. The `state_machine` concept (include/konsens/state_machine.hpp:59-62)
cannot express this. This is a new requirement of the public API, so it is
a question for the owner.

### 3. A verdict can arrive for an entry that an accept sync replaced (blocking, safety)

Evidence. `handle_acceptsync` replaces the log from `sync_idx`
(`append_on_prefix`, sequence_paxos.hpp:1807) and lowers the verified
index (1809). A prepare moves the node to the prepare phase
(sequence_paxos.hpp:1596). `forget_the_runs` clears `refusal_` (node.hpp:1803-1819).
The plan sends jobs and takes the verdicts "at the next step" (§2.5). A
verdict "passed" for index k from the old suffix arrives after the sync.
If the loop passes it by index (`pass_the_checked_done_record`,
sequence_paxos.hpp:1934-1958), the follower sends accepted for an entry
that it did not check. The model forbids that: `HandleAccept` checks and
appends in one action (model/tla/AcceptWithResult.tla:335-347), and
`HandleAcceptSync` checks with `AgreedLen` (277-279, 296).

Fix. Each job carries a generation: the promise, the accepted round, and a
counter that the loop raises on each `append_on_prefix` below the job
index, on a reconfiguration and on `stop_voting`. The loop drops a verdict
whose generation differs. A test: an accept sync that replaces the
entries while jobs are out. Finding 9 removes this case.

### 4. The lifetime of the job memory and of the views (blocking)

Evidence.
- `read_the_open_log` returns a `shared_ptr` owner and a span
  (sequence_paxos.hpp:1162-1180). The loop replaces `open_log_` when the
  count of entry writes changes (1170-1176) and resets it at each commit
  and roll-back (491, 1288). The plan says "a view of the entry" (§2.3). A
  span without the owner dangles after the next accept of the loop.
- `node` has a defaulted move (node.hpp:621-622). `node::with` returns it
  by value (141), and the tests keep nodes in a `std::vector`
  (test/node_cluster.hpp:228, test/simulation.hpp:420). A pool that holds
  a pointer into the node (`machine_`, the job array, the loop ring) reads
  freed memory after a move.
- The plan does not say what happens to jobs in flight when the node
  stops voting, throws (`throw_on_a_damage`, `write_the_proof_and_throw`),
  is destroyed, or is restarted. A pool thread that still runs reads a
  destroyed `machine_` and a destroyed job.

Fix. The job holds the `shared_ptr` owner of the log vector. The pool and
its jobs live in one heap block that the node owns through a
`unique_ptr`, so a move of the node moves no address that a pool thread
holds. The destructor and every path that stops voting wait until all jobs
came back. Finding 9 removes most of this.

### 5. In library mode nothing drives the verdicts back to the node (blocking)

Evidence. The only io_uring loop is the ring of konsensd
(program/serve.cpp:917, 932-974). The library target does not link
liburing; only `konsensd_parts` does (CMakeLists.txt:165-170). The plan
sends the verdicts "to the loop ring" (§2.4) and the accepted goes out "at
the next step" (§2.5). It does not say which ring that is in library mode,
how a caller without konsensd learns that verdicts arrived, and which
public function takes them. The owner decided that `step` blocks and that
the library does no network IO. A library user (test/node_cluster.hpp, the
examples) has no fd to wait on. The test of §5 with `check_threads = 2`
has no way to wait for the verdicts.

In konsensd, a MSG_RING CQE whose user_data is a pointer collides with the
decoding of user_data: `op_shift = 56` (serve.cpp:51), and
`handle_a_completion` reads `cqe.user_data >> op_shift` as the op
(serve.cpp:766). A user-space pointer decodes as op 0.

Fix. See finding 9: the step sends the jobs and waits for them on a ring
that the library owns. Then no caller and no public API changes. If the
asynchronous form stays, the plan names the public function that takes
the verdicts, the fd that the caller waits on, and the user_data tag in
serve.cpp.

### 6. Faults on a pool thread change the error paths (blocking, question for the owner)

Evidence. §2.7: "a thrown exception goes back as a verdict with its error
code." On the pool path these can be thrown: `read_error` from
`transaction_view::find` (state_machine.hpp:44-50), `std::logic_error` from
`round_the_data_item` (node.hpp:1738-1741), `read_or_throw` in
`run_request` (node.hpp:1752), any exception of the state machine, and
`std::bad_alloc`. Today they leave `step` with their own type. A
`std::logic_error` has no error code. RULES.md, "An error goes to whoever
can repair it": "A catch names the exact type it can recover from. `main`
and the body of a thread catch what reaches them, record it, and end." A
pool thread that catches everything and goes on violates this. "A wrong
use raises `std::logic_error`" is also lost.

Fix. The pool thread catches nothing on the job path. It hands back a
`std::exception_ptr` in the job, and the loop rethrows it in `step`, so
the caller sees the same type as today. A SQLite error of the read is a
value (`std::expected`) and goes back as an error code, as the plan says.
The owner decides an error path (.claude/RULES.md, "Every question goes
to the owner at once"), so this goes into the questions.

### 7. The pool thread writes through a pointer it was given (blocking, rule)

Evidence. §2.4 and §2.6: the pool writes the verdict into the job that the
loop owns. RULES.md, "Every argument is const": no function writes
through an argument, with three exceptions (encoder, decoder, C
interface). This is none of them.

Fix. The verdict travels as a value in the CQE: MSG_RING gives `len` as
`res` (32 bits) and `data` as `user_data` (64 bits) (liburing man page
io_uring_prep_msg_ring.3; liburing.h:1415-1422). `user_data` names the job
(its index), `res` is the refusal reason or the error code value. The job
stays const for the pool. A `std::exception_ptr` (finding 6) does not fit
32 bits; it can stand in a result array that each pool thread owns and
that the loop reads only after the CQE. The owner judges that form.

### 8. MSG_RING mechanics that the plan leaves out (blocking for the CQ overflow, the rest not blocking)

Evidence and status of the ordering claim (§2.6):
- Sender: without SQPOLL, liburing stores the SQ tail plainly and enters
  the kernel (liburing src/queue.c:214), with SQPOLL it uses a store-release
  (queue.c:216). The writes of the job are sequenced before the syscall.
- Receiver: `__io_uring_peek_cqe` reads the CQ tail with
  `io_uring_smp_load_acquire` (liburing.h:1857). In C++ mode this is
  `std::atomic_load_explicit(..., memory_order_acquire)`
  (liburing/barrier.h:53-58).
- Kernel: the CQ tail is published with `smp_store_release` in
  `io_commit_cqring`. Not checked: no kernel source is in this container.
- So the order holds in practice through the kernel. It is not a
  happens-before edge of the C++ abstract machine. ThreadSanitizer cannot
  see it and reports a race. Not checked whether a TSan build runs in the
  CI: .github/workflows has only deps-upstream.yml.

Faults:
- The sender also gets a CQE for its own MSG_RING SQE. The plan does not
  reap it. `IOSQE_CQE_SKIP_SUCCESS` removes it on success.
- The target CQ can be full: the CQE of the sender then carries
  `-EOVERFLOW` (man page io_uring_prep_msg_ring.3, ERRORS). A lost job
  keeps the verified index where it is, and the follower stops sending
  accepted with no error. This is blocking. Fix: size each CQ to the
  maximum jobs in flight, and treat a negative `res` on the sender CQE as
  a fault of the machine that `step` raises.
- `-EBADFD`, `-ENOMEM`: the same path.

### 9. Proposed form: the step sends the jobs and waits for them (not blocking by itself, it removes findings 3, 4, 5 and most of 10)

The owner decided that `step` blocks. So `check_the_done_records`
(node.hpp:1192) can send the jobs, check its own share on the loop thread,
and wait on a ring that the library owns until every job came back, all
inside the same step. This is `dispatch_apply` of libdispatch (the source
of the names in §4): a concurrent queue that runs n iterations and returns
when all are done. Then:
- every view is valid: `open_log_`, `own_`, `nodes_` and the log do not
  change during the call;
- no verdict is stale, no generation is needed, and an accept sync
  cannot come between job and verdict;
- no node can move or be destroyed while a job runs;
- the library needs no new public function and no fd for the caller;
- the order of the verdicts does not change the result, so a simulation
  seed stays deterministic with `check_threads > 0`, and §3 can drop the
  `check_threads = 0` exception for the simulation;
- the accepted leaves at the end of the same step, as today, with a
  shorter step.

Finding 1 stays: the snapshot is the last commit. Finding 2, 6, 7 and the
CQ sizing of 8 stay.

### 10. Decided entries above the verified index run again on the loop thread (not blocking for the form of finding 9, blocking for the asynchronous form)

Evidence. `apply_the_done_records` runs the request again on the loop
thread for each decided entry with `k > verified_idx`
(node.hpp:1621-1631), and throws with a proof when the result differs.
In a cluster of three, the leader and the other follower decide without
this follower. With asynchronous verdicts, a decide can come before the
verdict, so the loop runs the request again and the gain goes. Then
`pass_the_checked_done_record(decided_idx)` (node.hpp:1649) raises the
verified index, and the late verdicts must be dropped. §3 says
`apply_the_done_records` stays the same; its cost changes.

Fix. Use the form of finding 9. Else, state the rule for late verdicts
and count the runs again in the bench.

### 11. The cost of each job grows with its index (not blocking, measure it)

Evidence. Today one base write set is built once and grows by the results
of each step (`base_below`, node.hpp:1216-1231). In the plan each job
builds its own base from a + 1 to index - 1 (§2.4). With n entries in a
step the work is about n²/2 calls of `write_the_result`, each with map
inserts and copies (write_set.hpp:42-52). With finding 1 the range starts
even lower. Each job also opens a read transaction and reads meta
(persistent_storage.cpp:1840-1856).

Fix. Give each pool thread one contiguous range of the request entries
of the step. The thread opens one read transaction and builds its base
once, as the loop does today. The bench of §6 reports cpu per write; add
the count of `write_the_result` calls per step to the record.

### 12. Readers on other threads can stop the reset of the WAL (not blocking, measure it)

Evidence. `wal_autocheckpoint = 0` (persistent_storage.cpp:1014). The
writer runs a PASSIVE checkpoint in `begin` when the WAL holds 1000 frames
or more (persistent_storage.cpp:1819-1827, `checkpoint_frames` at 42). The
frame count comes from the WAL hook (`count_frames`, 977-981), which
reports the frames in the WAL. A PASSIVE checkpoint does not copy frames
that an open reader needs, and SQLite resets the WAL only when no reader
uses it. A pool that keeps read transactions open most of the time can
keep the WAL from a reset. The count then stays above 1000, `begin` runs
a checkpoint at every step, and the WAL file grows.

A test in one file outside the tree (Python sqlite3 3.45.1, not 3.53.4)
showed that a read-only connection with `mode=ro` returns `wal` for
`PRAGMA journal_mode = WAL`, that a reader keeps its snapshot across a
commit of the writer and across a PASSIVE checkpoint (the checkpoint left
one frame), and that a new read transaction sees the new commit. The
snapshot part of question 2 is correct.

Fix. Record the size of the WAL file and the number of checkpoints per
second in the bench of §6.

### 13. A new API of the storage and of `S` is not in the plan (not blocking)

Evidence. `persistent_storage` has one reader connection
(`environment::reader_db`, persistent_storage.cpp:1107) and
`begin_read_only` raises `std::logic_error` when it is open (1842-1844).
The pool needs a connection per thread, opened on the same file with the
same checks (user id, format). `node` is a template over `S`; the tests
also use `disk_storage` (concept `storage`, include/konsens/storage.hpp:243).

Fix. Name the function that opens a reader for another thread, and the
requirement on `S`. A `check_threads > 0` with an `S` that cannot do it
does not compile, or raises `std::logic_error`.

### 14. The leader also runs `check_the_done_records` (not blocking)

Evidence. The guard is `votes()` and the accept phase, for both roles
(node.hpp:1194). The plan says the leader keeps its form (§2, last
paragraph) but does not say where the code chooses.

Fix. State that the pool runs only when the role is follower.

### 15. Wording and smaller rule points (not blocking)

- §2.2 "It opens no other file": SQLite also opens the `-wal` and `-shm`
  files. Say "no file other than the store".
- §1: the step time of 40 to 70 us per entry comes from a run with the
  record on, at 4026 to 4079 writes/s, with the client at the leader
  (2026-10-31-172600Z, lines 5, 9, 27). That file says: "Not checked: the
  split of the 40 to 70 us per entry between the check run, the apply and
  SQLite" (line 31). The 39 % is instructions, not time
  (2026-10-31-2045Z, line 17, 28). The plan builds on a time share that no
  run measured. RULES.md, "A gain is measured, never estimated". Propose:
  measure the time of `check_the_done_records` per step with the
  timestamp patch before the build.
- .claude/RULES.md, "A hypothesis is tested in one file first": the
  MSG_RING round trip of a job pointer between two threads, and a reader
  on another thread against a writer that keeps its transaction open,
  go into one file outside the tree first.
- §6: the bed runs with `podman --cpus=3` on 4 vCPU (2026-10-31-2010Z,
  line 6). Three nodes with two pool threads each and the load generator
  can exceed 3 runnable threads. The bench records the cpu of each thread.
- §5, "The suites of plan-promise-again §5": RULES.md, "A test run checks
  the changed code only". Name the tests that touch the changed lines.
- §7: mark the result file as "recorded in the state file"
  (.claude/RULES.md, "A plan ends with a list of its data").
- The library gains a dependency on liburing, which today only konsensd
  links (CMakeLists.txt:143-170).

## Answers to the seven questions

1. True for the state, with the leader results, the sessions and the
   meta. Not complete: `own_`, `nodes_.size()`, `machine_` (finding 2). The
   range of results starts at the committed applied index, not at the
   applied index of the loop (finding 1). The chain CRC is not an input.
   `refusal_`, the verified index and stop signs stay on the loop; stop
   signs and normal entries are not checked (node.hpp:1238-1243,
   1293-1295).
2. Yes for the snapshot: meta, data and session are in one file, BEGIN is
   deferred and the first read is the meta (persistent_storage.cpp:50-53,
   1846-1850). PASSIVE does not break a snapshot. The plan's "read a, use
   a + 1 .. k - 1" is correct only if the job holds results from the
   committed applied index (finding 1). WAL growth: finding 12.
3. Faults: findings 3, 4, 10.
4. The model needs no change only if a verdict is never used after the
   prefix of its entry changed (finding 3). With that, a later accepted is
   the same action at a later time, and the model has it already as a
   held message. The form of finding 9 keeps the action as it is.
5. Finding 8.
6. Not said. Finding 5, proposal in finding 9.
7. Findings 6, 7 and 15.

## What I did not check

- The kernel source of MSG_RING and `io_commit_cqring`.
- SQLite 3.53.4 itself: the test ran with Python sqlite3 3.45.1.
- Whether a ThreadSanitizer build runs anywhere.
- I did not build or run any test of the tree.
- I read the parts of sequence_paxos.hpp and node.hpp named above, not the
  whole files. I did not read EVReconfigure.tla or the mutation
  StaleVerdictTaken of SequencePaxos.tla (line 595) in full.

## Verdict

Reject in this form. Findings 1 to 8 are blocking. Rework the plan to
the form of finding 9 (the step sends the jobs and waits for them), with
the fixes of findings 1, 2, 6, 7 and 8. Then review it again. Findings 2
(state machine called on several threads) and 6 (error path) go to the
owner.
