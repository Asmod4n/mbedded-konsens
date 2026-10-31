# Plan: the check run of a follower on several threads

Owner 2026-10-31 17:52 to 17:55: threads in the library are allowed.
Only io_uring coordinates the threads, with IORING_OP_MSG_RING. No
mutex and no lock. One thread owns the writing SQLite connection and
writes in strict order.

## 1. The facts this plan builds on

- At 64 writes in flight with a stable leader, konsensd gives 14112
  writes/s (median of 12 starts) at a median latency of about 4.1 ms
  (bench/results/2026-10-31-1920Z-konsensd-spread-of-the-bed.txt).
  64 / 4.1 ms is 15600 writes/s. The rate follows the latency of one
  write.
- No node is busy all the time: 99 us of CPU per write over three
  nodes (2026-10-31-2010Z). The loop thread of a node is not saturated.
- A follower sends accepted for the entries of a step only after
  `check_the_done_records` (node.hpp:1192-1250) has checked them. The
  step of a follower grows by about 40 to 70 us per entry
  (2026-10-31-172600Z, with the record on).
- On a follower, `check_the_done_records` holds 39 % of the
  instructions and `sqlite3_step` 50 % (2026-10-31-2045Z, Ir, not
  time).
- The check of entry k runs `run_request` on a write set over the
  writing transaction. That write set holds the results of the entries
  from applied_idx + 1 to k - 1 (`write_the_done_results`). These
  results are the results of the leader, and they stand in the entries
  of the log. So the input of the check of entry k is known before the
  check of entry k - 1 ends: the committed state at applied_idx, plus
  the results of the leader for the entries before k.
- Hence the checks of the entries of one step do not depend on each
  other. They depend on the committed state and on data that the
  follower already holds.

## 2. The change

Review 1 (doc/review-plan-parallel-check-run-1.md) rejected the first
form, in which the verdicts came back after the step. This form follows
its finding 9: `dispatch_apply` of libdispatch. The step sends the work,
does its own share, and returns when all work is done. `step` blocks, as
the owner decided.

1. `check_threads` is a field of the configuration struct. 0 keeps the
   form of today. The binary reads it from the TOML file.
2. The library owns the pool: `check_threads` threads, each with its own
   io_uring and its own read-only SQLite connection to the same file.
   The node owns one ring of its own for the answers. The library links
   liburing. The storage gets a read-only connection per pool thread
   (a new function of `S`, finding 13).
3. `check_the_done_records` first takes every entry that has an own
   result (`find_the_own_result_on`, node.hpp:1272-1289) on the loop
   thread, as today, because `own_` belongs to the loop (finding 2).
4. It splits the remaining request entries of the step into contiguous
   ranges, one per thread plus one for itself (finding 11). For each
   range it sends one job by IORING_OP_MSG_RING with
   IOSQE_CQE_SKIP_SUCCESS. A job is const for the pool. It holds: the
   range of indexes, a view of the entries, the applied index of the last
   commit (finding 1; `commit_phase_two` keeps it), a view of the leader
   results from that index + 1, the majority, the pid and the float rule
   (finding 2).
5. A pool thread opens one read transaction, reads its applied index a,
   and checks a against the job: a below the applied index of the last
   commit, or above the applied index of the loop, is a fault of ours and
   raises. It builds one base from the results a + 1 to the start of its
   range, then checks its entries in order, as the loop does today. It
   writes the verdicts into an array that the thread owns (finding 7),
   and answers with MSG_RING: `user_data` names the job, `res` is 0.
6. An exception on a pool thread is caught as `std::exception_ptr` in
   the result array of the thread. The loop rethrows it in `step`, so the
   caller sees the same type as today. This is the form of
   std::promise and std::future ([futures.state]): a value or an
   exception crosses threads as a shared state that the receiver reads
   once. A SQLite error of a read is a value and goes back as an error
   code (finding 6).
7. The loop waits on its answer ring until every job came back. It then
   reads the result arrays in index order and does what it does today:
   it passes the verified index over the run of passed entries and sends
   the first refusal.
8. Each answer ring and each pool ring has room for every job that can
   be in flight. A negative `res` on a CQE of a send is a fault of the
   machine and raises (finding 8).
9. `M::apply` and `M::query` are const. The library calls them on
   several threads at the same time. RULES.md already requires it: a
   function reads no global, writes no global and keeps no static that
   changes. The start comment of state_machine.hpp states it. The pool
   reads through its own view type, which the concept accepts
   (finding 2, review 2 N5).
10. The leader takes the same path (finding 14).
11. The range of the loop thread also writes its verdicts and an
    exception into a result array. The loop waits for every answer
    before it reads a result or rethrows, so no job holds a view into a
    frame that unwinds (review 2, N1).
12. The pool lives in a heap block that the node owns through a
    unique_ptr, so a move of the node does not move it. The destructor
    sends a stop job to each pool ring and joins each thread. The
    committed applied index starts with the applied index read at open
    (review 2, N2, N9).
13. A pool thread that cannot send its answer (a negative res on its own
    send) records the fault and ends the process. This is the rule for
    the body of a thread in RULES.md ("main and the body of a thread
    catch what reaches them, record it, and end") (review 2, N4).
14. The loop reads the results in index order and stops at the first
    refusal, at refusal_->index or at the first exception. A job leaves
    out the entries from refusal_->index on (review 2, N3).
15. An entry whose own result disagrees runs again, as today
    (node.hpp:1309-1311), so the loop keeps its base for those entries
    (review 2, N6).
16. Each pool read transaction ends before the answer (review 2, N7).
17. Provisional: the library takes liburing the way konsensd takes it
    today, through the pinned slipstreamIO. The build image has no
    liburing package. This is a question to the owner (review 2, N8).

## 3. What stays the same

- The protocol, the messages and the model: the accepted leaves at the
  end of the same step, as today, after the same checks.
- `apply_the_done_records` and every write on the loop thread.
- A simulation seed stays deterministic, because the result does not
  depend on the order of the answers.

## 4. A test in one file first

Before the tree changes: one file outside the tree with two threads,
each with a ring, that sends n jobs by MSG_RING and waits for n answers,
with one read-only SQLite reader per thread in WAL. It measures the time
of one round trip and of a read transaction on a second thread. g++ 16,
g++ 15 and clang 22. The result goes into this plan before the build.

Result (bench/results/2026-10-31-1935Z-msg-ring-round-trip.txt): one
round to one pool thread and back takes 23.8 to 24.3 us (median of 4
p50), to two pool threads 30.0 to 31.4 us, in all three builds. A read
transaction with one SELECT on the pool thread adds about 1 us. A first
form of the file closed the writing connection before the readers
opened; then a reader of two at the same time sometimes found no row.
With the writing connection open, as in konsensd, no run failed.

## 5. Names

Provisional, from libdispatch (the owner named it): `dispatch_apply`
for the step that sends and waits, and a concurrent queue for the pool.

## 6. Tests

- test_node: a follower with `check_threads = 2` gives the same
  verified index, the same refusals and the same applied state as with
  0, for a log of mixed entries (requests, rejected entries, a stop
  sign, an entry with a result that differs).
- A check while a sync is out and the transaction holds ended steps
  (finding 1).
- A fault on a pool thread: a failed read stops voting as today; a
  thrown std::logic_error leaves `step` as std::logic_error.
- A full answer ring raises.
- The suites of plan-promise-again §5 with g++ 16.

## 7. Measurement

Arms `check_threads` = 0, 1 and 2, each its own binary, plus A/A of 0,
in g++ 16, g++ 15 and clang 22. bench/arms/cap-of-the-step.sh with
cluster3.sh, 64 and 1 in flight, 4 rounds. It measures writes/s,
latency, cpu per write, the WAL size and the checkpoints per second. The number comes after the measurement.

## 8. Data on the disk

- Build directories under bench-logs/kbuild: made again by a build
  command.
- The bed under /mnt/zrun/b: deleted after each run.
- One result file in bench/results: recorded in the state file.
- The file of §4 and its binaries in the scratchpad: deleted after the
  owner has seen the list.
