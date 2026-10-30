# Plan: port the bench to the API of the branch, and the measurements it needs

The bench `bench/konsens_bench.cpp` does not compile at 4e11084 with
`-DKONSENS_BENCH=ON`. It uses the storage API of 06a52bf
(`write_atomically`, `storage_op`, `append_entries`, `set_promise`) and
`node::with(config, storage, machine, nonce)`. The checks of section 5 of
plan-storage-only-source.md and section 6 of plan-execute-verify.md need it.

## 1. The port

1. The bench builds a node as test/node_cluster.hpp does (line 197 at
   4e11084): `node::with(config, S, node_storage, state_storage, M,
   nonce)`.
2. The state machines of the bench follow the concept of
   include/konsens/state_machine.hpp: `apply(payload, write_view) ->
   applied` and `query(payload, read_view)`.
3. Every case that 06a52bf has keeps its name, its input and what it
   measures. A case whose operation no longer exists at the branch is
   removed, and the result file says so.
4. A ctest test builds the bench with `KONSENS_BENCH=ON` and runs each
   case once with one sample, so that the bench cannot stop compiling
   again without a red test.

## 2. New cases

1. `write_batch_size_lmdb` for batch sizes 1, 16 and 256
   (plan-storage-only-source 5). The name `write_batch_lmdb` stays with its
   meaning of 06a52bf (value sizes 64, 1024, 65536 at batch size 64).
2. `pipeline`: one leader and two followers in one process, threads as
   the tests use them, `max_in_flight` 1, 4, 16 and 64. Two works: one
   counter put, and a loop of 64 puts. A fixed delay of each message is a
   parameter, 0 ms and 20 ms (bench/results/2026-10-24-231534Z used 20
   ms); the result file names it. It reports the runs per request, the
   processor time per request (`getrusage`) and the p50 and p99 of the
   reply latency.
3. `execute_verify`: writes per second and the p50 and p99 of the reply
   latency, for an integer machine and for a float machine with
   `float_digits`; runs again per 1000 entries. The state machines of the
   bench count their own calls of `apply`; the runs again are the calls
   minus one call per node and entry. No library code changes for it.
4. `read_back`: a copy of the reads of the private
   `node::written_values_read_back` (node.hpp:1056 at 4e11084), done
   again in the bench on the same `state_storage` after a step. The case
   measures that copy; a test of the bench compares the result of the
   copy with the result of a step. No library code changes for it.
5. `scan_the_whole_log`: `sequence_paxos::scan_the_whole_log` over logs
   of 10^3, 10^4 and 10^5 entries.

## 3. The arms

1. Cost of the branch (plan-storage-only-source 5): the cases of 1.3 and
   2.1. Before: 06a52bf with its own bench. After: the ported bench at
   the branch. For `write_batch_size_lmdb` the before arm is 06a52bf with
   the same case added against the old storage API; that patch is
   committed on the branch under bench/arms/ so that the run can be
   repeated. Another new case that does not compile at 06a52bf has no
   before arm, and the table says so.
2. Without and with Execute-Verify (plan-execute-verify 6): the arm
   without is 56c5f94 (D1 and stage E, the commit before 4bea5c6), with
   the ported bench. At 56c5f94 `node_config` has no `max_in_flight` and
   no `float_digits`: the bench sets each field only where it exists (a
   `requires` check), and the float machine of that arm has the same
   `apply` and input without the `float_digits` member. The compare is
   at `max_in_flight` = 1 in the arm with; the other values are a
   separate table of the branch only.
3. `max_in_flight`: four values in one binary of the branch; the default
   is the value that is fastest across the table as a whole.

## 4. The run

`bench/run.sh`: 31 samples times 7 rounds (the run length of the state
file), the A/A copy `konsens_bench_aa` as the noise floor, the bench user
uid 1001, not pinned, at most 3 of the 4 cpus. One run builds one binary
per arm commit and the A/A copy, and alternates them in each round; so
`run.sh` takes the arm commits as arguments. The run gets priority over
every other process of this user, and the row records whether it got it.
The build matrix of mbedded-konsens holds one build, g++ 16 Release
(clang 22 was dropped, owner 2026-10-20); each table has its column. The
result file holds the context that RULES.md names: the commits with the
dirty mark, the flags and the resolved instruction set, the compiler, the
C++ and C library read from the binary, the kernel and its scheduler, the
VM and the container, the cpu and a digest of its flags, the user, the
priority, the threads, each repetition, the median and the spread. One
result file per run. The bench code is committed before the run.

## 5. What this plan does not do

It changes no library code. A fault that the bench finds is reported,
not fixed.
