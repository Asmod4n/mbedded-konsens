# Trace check as a ctest test: seeds 1 to 10

Steps 2 and 3 of doc/plan-trace-validation.md, on branch
trace-validation with model-buffer-after-sync merged (76f4d47).

## What was built

- `test/simulation.hpp`: `run` takes a trace file. With a file, the
  world writes one record for each step: the events, the state of each
  node, and the Paxos messages in flight. The records end before the
  first crash that is not clean (F6, phase 1).
- `model/tla/TraceSimulation.tla` and `TraceSimulation.cfg`: the trace
  spec of the spike of 2026-11-13, with `DecidedIdxMonotone = TRUE`.
- `test/trace_validation/test_trace_records.cpp`: writes the traces of
  seeds 1 to 10 (FIFO, one message for each batch, no clients, no
  rollback), and checks that a seed with records ends as the same seed
  without them.
- `test/trace_validation/check_trace.cmake`: runs TLC on one trace. The
  test passes when TLC reports `NotDone` as violated.
- CMake option `KONSENS_TRACE_VALIDATION`, default ON (owner, 2026-11-12
  21:52 UTC). It needs Java 21. `KONSENS_TLA2TOOLS_JAR` names the jar;
  empty downloads v1.8.0 from its origin. The SHA256 of the jar is
  pinned. v1.8.0 is a pre-release that its origin builds again, so a new
  build of it stops the configure step with the two hashes.

## Run

Image: Ubuntu 26.10, g++ 16, SQLite 3.53.4, OpenJDK 21.0.13, debug
build, podman, 4 cpus. `ctest -j 3 -R trace`:

| test | time (s) | end |
|---|---|---|
| test_trace_records | 2.2 | passed |
| trace_check_seed_1 | 14.5 | 887 of 887 records |
| trace_check_seed_2 | 10.2 | 807 of 807 |
| trace_check_seed_3 | 4.8 | 25 of 25 (ends before an unclean crash) |
| trace_check_seed_4 | 4.5 | 43 of 43 (same) |
| trace_check_seed_5 | 9.4 | 839 of 839 |
| trace_check_seed_6 | 9.2 | 796 of 796 |
| trace_check_seed_7 | 4.0 | 55 of 55 (same) |
| trace_check_seed_8 | 4.4 | 57 of 57 (same) |
| trace_check_seed_9 | 4.6 | 148 of 148 (same) |
| trace_check_seed_10 | 10.2 | 872 of 872 |

Total: 31.9 s wall clock. test_simulation stays green (69 s).

Counter-check: the trace of seed 1 with the decided index of node 1
raised by one at record 50. check_trace.cmake ends with an error at
line 51 of the trace.

## Not done

- Step 4 of the plan: a mutation in the code that only the trace check
  finds, on its own branch.
- The modes of F6 after phase 1: batches, clients, retirement, rollback.
- Seeds 3, 4, 7, 8 and 9 end early at an unclean crash. They check few
  records.
