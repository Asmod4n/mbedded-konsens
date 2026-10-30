# Plan: a bench of each part of a write

Source: the owner, 2026-10-21 16:29 (UTC+2): a bench harness that shows
how fast each part is and where the time goes. This plan gets one
review before the build.

## 1. Facts checked in this session

- `bench/` holds only `results/`. A bench of 2026-10-20 measured
  decode plus handle of a follower; its code was deleted after the run,
  as the rules say for a bench outside the tree.
- `CMakeLists.txt:26-45`: tests and examples come from a glob; there is
  no bench target.
- `storage.hpp:107-113`: `write_atomically(std::span<const storage_op>)`
  is the one write of a store. `persistent_storage` commits it with a
  synchronous commit.
- `wire.hpp`: `encode(const message&)` and `decode(std::string)`.
- `node.hpp`: `tick`, `handle`, `client_request`, `client_query`,
  `take_outgoing_messages`, `take_client_replies`.
- `bench/results/2026-10-21-092557Z.txt`: the form of a result file with
  its context (user bench, setpriv, A/A pair, rounds, raw lines).

## 2. Change

1. `bench/konsens_bench.cpp`, one binary in the tree, built only with
   `-DKONSENS_BENCH=ON`. It takes the name of one case, the number of
   samples and the size of the payload, and prints one line: the case,
   the time of one operation as median, minimum and maximum of the
   samples, and the number of operations in a sample.
2. Cases, each one part of a write:
   - `crc32c`: CRC32C of one payload.
   - `encode_accept_decide`, `decode_accept_decide`: one AcceptDecide
     with 64 entries of the payload size; `encode_decide`,
     `decode_decide`.
   - `handle_accept_decide`, `handle_decide`: a follower in
     (Follower, Accept) on a store that counts the bytes.
   - `commit_memory`, `commit`: `write_atomically` of one
     `append_entries` with one entry, on `memory_storage` and on
     `persistent_storage` in a directory that the run names.
   - `write_memory`, `write`: three nodes in one process, the
     network a queue; one `client_request` at a follower until its
     reply.
   - `read_memory`, `read`: one `client_query` at a follower until
     its reply.
   - `write_batch_memory`, `write_batch`: 64 requests sent before
     the first round, the time until the last reply, divided by 64.
3. `bench/run.sh`: builds the bench in podman (image
   localhost/cxx-matrix, g++ 16, Release `-O2 -DNDEBUG`), runs each case
   as the user bench through setpriv, 7 rounds with the order of the
   cases turned around each second round, and writes one result file
   `bench/results/<UTC>.txt` with the context of the rules and the raw
   lines. The binary that the run builds is its own A/A pair: the first
   case of each round runs twice.
4. The run length is 31 samples, as measured for this machine
   (state file, 2026-10-20). The A/A difference of the run is its noise
   floor and stands in the file.
5. The disk of the cases on `persistent_storage` is the disk of the
   session. The file
   names it (file system and device from `/proc/mounts`).

## 3. Tests

The bench checks its own result where a result exists: the CRC of the
check value, a decoded message equal to the encoded one, a counter
reply equal to the number of writes. A case whose check fails prints
the failure and exits with 1.

## 4. Not in this plan

Several processes or machines, a real network, the IO of the binary.

## 5. Order

1. Plan and review.
2. `konsens_bench.cpp` and the CMake option; one run of each case.
3. `run.sh`; one full run; the result file.
4. README: the table of the section Performance from that file.

## 6. Changes after the review

The review (approve with changes, 13 findings) is applied as follows.

- R1: the source is built twice into two binaries, `konsens_bench` and
  `konsens_bench_aa`. Every case runs in both, and the file gives the
  A/A difference of each case. The A/A is measured first.
- R2: each case has a fixed number of operations per sample, chosen so
  that a sample lasts about 10 ms. The run length of this bench is
  measured (31 and 101 samples, A/A of each) and goes into the state
  file.
- R3: mbedded-konsens builds only with g++ 16 (README, state file), so
  the table has one column: g++ 16 Release. A second compiler gets a
  column when the floor changes.
- R4, R5: the setup of each case is outside the timed part: the
  election, the registration of the client, `persistent_storage::open`,
  and the follower brought to (Follower, Accept). A write uses a request
  number that rises by one. The time runs from the call to its reply.
- R6: the cluster cases send each message through `encode` and
  `decode`. They count the rounds and the messages of each write. The
  file says that the parts do not add up to a write, because a round
  also carries heartbeats and resends.
- R7: the decode cases copy the bytes outside the timed part.
- R8: the store directory is on a mount of the host directory under the
  scratchpad, owned by user bench. The file gives the file system and
  the mount options seen inside the container.
- R9: the commit cases open a fresh store for each sample outside the
  timed part; the cluster cases record the log length at the first and
  the last sample.
- R10: `crc32c` times `crc32c::extend`, the form that a write uses
  today, and its number is throughput over independent payloads.
- R11: payloads of 64 B, 1 KiB and 64 KiB for `crc32c`, `commit_*` and
  `write_*`; the AcceptDecide cases take 64 entries of 1 KiB.
- R12: the bench is committed before the run, and the file names that
  commit with its dirty mark, the instruction set, the libraries read
  off the binary, the scheduler, steal ticks and other processes of the
  user per row, and the free cpus.
- R13: `handle_*` compares the bytes counted with the bytes sent;
  `commit_*` compares `get_log_len` after each commit.

## 7. The store alone as the baseline

Source: the owner, 2026-10-22 06:19 (UTC+2): write one value and read
one value with the store alone, then the same through the library.

The file says that `write` runs three nodes in one process, so it holds
the commits of all three nodes one after the other, and that a cluster
on three machines commits on the followers at the same time.

The bench has no baseline case for SQLite (bench/konsens_bench.cpp,
2026-10-30). The LMDB form of this section is in the git history before branch remove-lmdb.

## 8. Changes after the review

The review (needs changes, 8 findings) is applied as follows.

- R1, R2 and R5: removed.
- R3: `read` answers from the state machine in memory after one
  read-index round, so the table puts it beside `read_memory`.
- R4: case `get_entries` calls `persistent_storage::get_entries(i,
  i + 1)` on an entry written before the timer. It runs with 64 B,
  1 KiB and 64 KiB. Check: the bytes equal the payload.
- R6: the bench counts the calls of `write_atomically` per op. The file
  writes "N commits in series" with N as measured.
- R7: `commit_and_collect` times each `n.commit(*t)` and sums the time
  per node. The rows give `leader_ns_per_op` and `follower_ns_per_op`.
  The file says that the latency on three machines is about the leader
  time plus the slowest follower time plus the network, and that this
  bench does not measure that latency.
- R8: removed.

The LMDB form of this section is in the git history before branch remove-lmdb.

## 9. Three nodes in parallel

Source: the owner, 2026-10-22 06:22 and 06:23 (UTC+2): the three nodes
run in parallel in one process, and each node has its own store.

1. A second harness `threaded_cluster` beside `cluster`. Each node runs
   on its own `std::jthread`. The node, its storage and its inbox
   belong to that thread alone. Each node opens its own store in its
   own directory.
2. The inbox of a node is a `std::mutex`, a `std::condition_variable`
   and a vector of encoded messages and client calls. A thread waits on
   the condition variable until its inbox is not empty or a stop is
   requested. It takes everything that waits, decodes it, calls
   `handle_incoming` once, calls `commit`, encodes the released
   messages and puts each into the inbox of its target. Replies go to
   the inbox of the main thread. The messages stay encoded between the
   threads, as in `cluster`.
3. The election runs before the threads start, on the same nodes, with
   `cluster::elect`. No `tick` runs in the timed part. The case fails
   when the leader changes in the timed part.
4. Cases, beside the serial ones: `write_threads`,
   `write_batch_threads`, `read_threads`, and the same three
   on memory. The client sends to a follower, as in the serial cases.
   The main thread starts the clock, puts the call into the inbox of
   that follower, waits on its own condition variable for the reply,
   and stops the clock. Names provisional.
5. The serial cases stay as arms until the owner decides.
6. The timers of 8 (R7) run in each thread, so `leader_ns_per_op` and
   `follower_ns_per_op` hold in both harnesses.
7. Checks: each reply has the counter value that the serial case
   expects. After the run each node has the same log length and the
   same decided index.
8. Threads: the main thread and three node threads. The machine has 4
   processors and the bench leaves one free; the main thread waits
   while the nodes work. The result file records the number of
   threads.
9. Tests: the threaded cases run in a Debug build with ThreadSanitizer
   once, with no report.

## 10. Changes after the review of 9

The review (needs changes, 15 findings) is applied as follows.

- The serial cases already open one store per node, in its own
  directory (bench:765-767).
- R1: the node thread waits with `std::condition_variable_any::wait`
  with the stop token and the predicate "inbox not empty".
- R2: the main thread waits with a predicate under its inbox mutex:
  every reply with a client id in the range of the calls has arrived,
  or a failure has arrived.
- R3: a thread swaps its inbox out under its own lock, releases the
  lock, and only then decodes, handles, commits and pushes. No thread
  holds two inbox locks.
- R4: an inbox element is a `std::variant` of an encoded message and a
  client call. The node thread runs the client call and joins its
  transaction with `+=`. The main thread never touches a node while the
  threads run.
- R5: the thread body catches what reaches it, puts a failure into the
  main inbox and notifies.
- R6: a thread takes at most `konsens::max_batch_size` elements per
  commit.
- R7: the main thread puts all calls of a sample under one lock with
  one notify. A node thread puts all its messages for one target under
  one lock.
- R8: each thread keeps its own counters and timers: messages,
  `write_atomically` calls and commit time. The main thread reads them
  and the leader state only after the join. The rows give messages
  per op and commits per node per op in both harnesses, and no rounds.
- R9: the serial harness ticks in its timed part and the threaded one
  does not. The file states it.
- R10: a count of the elements in flight, under one mutex with a
  condition variable. The main thread waits until it is zero, then
  stops and joins, then checks log length, decided index and leader.
- R11: the jthreads are the last members of `threaded_cluster`, so they
  join before the nodes close their stores. Nodes stay in a vector that
  does not grow after the start.
- R12: the file says that the three stores share one disk.
- R13: the file records 4 threads, of which at most 3 are runnable. It
  says that the memory cases mostly measure the wakeup of threads.
- R14: one Debug build of its own with `-fsanitize=thread`. The bench
  links `Threads::Threads`.
- R15: `threaded_cluster` and the `*_threads` names go to the owner as
  questions.

The LMDB form of this section is in the git history before branch remove-lmdb.
