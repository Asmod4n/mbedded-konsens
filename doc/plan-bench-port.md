# Plan: port the bench to the API of the branch, and the measurements it needs

The bench `bench/konsens_bench.cpp` does not compile at 4e11084 with
`-DKONSENS_BENCH=ON`. It uses the storage API of 06a52bf
(`write_atomically`, `storage_op`, `append_entries`, `set_promise`) and
`node::with(config, storage, machine, nonce)`. The checks of section 5 of
plan-lmdb-only-source.md and section 6 of plan-execute-verify.md need it.

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

1. `write_batch_lmdb` for batch sizes 1, 16 and 256 (plan-lmdb-only-source
   5).
2. `pipeline`: one leader and two followers in one process, threads as
   the tests use them, `max_in_flight` 1, 4, 16 and 64. Two works: one
   counter put, and a loop of 64 puts. It reports the runs per request,
   the processor time per request (`getrusage`) and the p50 and p99 of
   the reply latency.
3. `execute_verify`: writes per second and the p50 and p99 of the reply
   latency, for an integer machine and for a float machine with
   `float_digits`; runs again per 1000 entries.
4. `read_back`: the reads of `node::written_values_read_back`
   (node.hpp:1056 at 4e11084), done again in the bench on the same
   `state_storage` after a step. No library code changes for it.
5. `scan_the_whole_log`: `sequence_paxos::scan_the_whole_log` over logs
   of 10^3, 10^4 and 10^5 entries.

## 3. The arms

1. Cost of the branch (plan-lmdb-only-source 5): the cases of 1.3 and
   2.1. Before: 06a52bf with its own bench. After: the ported bench at
   the branch. A new case that does not compile at 06a52bf has no before
   arm, and the table says so.
2. Without and with Execute-Verify (plan-execute-verify 6): the arm
   without is 56c5f94 (D1 and stage E, the commit before 4bea5c6), with
   the ported bench; cases of 2.2 and 2.3 that need a new API there are
   reported as a cell with the error.
3. `max_in_flight`: four values in one binary of the branch; the default
   is the value that is fastest across the table as a whole.

## 4. The run

`bench/run.sh`: 31 samples times 7 rounds (the run length of the state
file), the A/A copy `konsens_bench_aa` as the noise floor, the bench user
uid 1001, not pinned, at most 3 of the 4 cpus. One result file per run.
The bench code is committed before the run.

## 5. What this plan does not do

It changes no library code. A fault that the bench finds is reported,
not fixed.
