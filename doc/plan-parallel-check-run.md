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

A follower checks the entries of a step on a pool of threads. The
loop thread keeps the protocol, the writing connection and every
write.

1. The pool has `check_threads` threads, a field of the configuration
   struct. 0 keeps the form of today: the loop thread checks. The
   binary reads the value from the TOML file.
2. Each pool thread has its own io_uring and its own read-only SQLite
   connection (SQLITE_OPEN_READONLY, SQLITE_OPEN_NOMUTEX). It opens no
   other file.
3. After the step that appended entries, the loop thread builds one
   job for each entry that `check_the_done_records` would check. A job
   holds the index, a view of the entry, applied_idx at the time of the
   job, and a view of the results of the leader for the entries from
   applied_idx + 1 to index - 1. The loop thread owns the job memory.
   It sends a pointer to each job with IORING_OP_MSG_RING to a pool
   ring, in turn.
4. A pool thread takes the job from its completion queue. It begins a
   read transaction, reads applied_idx of that snapshot (a), and builds
   the write set over its read transaction with the results of the
   entries from a + 1 to index - 1. If a >= index, the verdict is
   "applied". It then runs `run_request`, encodes the result and calls
   `results_agree`. It ends the read transaction. It sends the pointer
   back to the loop ring with MSG_RING, with the verdict in the job.
5. The loop thread collects the verdicts. It passes the verified index
   only over a run of consecutive passed entries, as today. A refused
   entry gives the refusal of today. The accepted of the follower goes
   out at the next step after the verdicts arrive.
6. A job is never shared: the loop thread writes it before the send,
   the pool thread writes the verdict after it took the job, and the
   loop thread reads it after it took the job back. MSG_RING orders
   these accesses; there is no atomic and no lock.
7. A pool thread does not write. A fault on a pool thread (a SQLite
   error of the read, a thrown exception) goes back as a verdict with
   its error code. The loop thread handles it as today.

The leader keeps its form in this plan. It executes in the writing
transaction.

## 3. What stays the same

- The protocol and its messages. The model does not change: the
  follower sends accepted later, with the same content.
- `apply_the_done_records` and the commit on the loop thread.
- A simulation test runs with `check_threads = 0`, so that a seed stays
  deterministic.

## 4. Names

Provisional, from libdispatch (the owner named it): the pool is a
concurrent dispatch queue, a job is a dispatch work item. Not built
before the owner has seen this form.

## 5. Tests

- test_node: a follower with `check_threads = 2` gives the same
  verified index, the same refusals and the same applied state as with
  0, for a log of mixed entries (requests, rejected entries, a stop
  sign, an entry with a result that differs).
- A test of a job where the snapshot is newer than the job (a >= index).
- A test of a fault on a pool thread: the read fails, the loop thread
  stops voting as today.
- The suites of plan-promise-again §5 with g++ 16.

## 6. Measurement

Arms `check_threads` = 0, 1 and 2, each its own binary, plus A/A of 0,
in g++ 16, g++ 15 and clang 22. bench/arms/cap-of-the-step.sh with
cluster3.sh, 64 and 1 in flight, 4 rounds. It measures writes/s,
latency and cpu per write. The number comes after the measurement.

## 7. Data on the disk

Build directories under bench-logs/kbuild (made again by a build
command), the bed under /mnt/zrun/b (deleted after each run), one
result file in bench/results.
