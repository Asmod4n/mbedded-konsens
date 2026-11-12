# Plan: which error branches no test reaches

Source: the owner, 2026-11-12 21:31 (UTC): "okay, leg los" to the
proposal V2 of the evaluation of Ron Pressler, "Why Writing Correct
Software Is Hard" (/mnt/project-files/actor-raft/correctness-and-complexity.md).
That evaluation quotes Yuan et al., "Simple Testing Can Prevent Most
Critical Failures", OSDI 2014, through Pressler: "Almost all (92%) of
the catastrophic system failures are the result of incorrect handling of
non-fatal errors". The paper itself was not read. This plan measures
which error branches of this tree no test reaches. It writes no new
test. This is the second form, after review round 1.

## 1. Facts checked in this session (main 654c088)

- `[[unlikely]]` stands 808 times in `src/`, `include/` and `program/`.
  The most: persistent_storage.cpp 219, sequence_paxos.hpp 215,
  node.hpp 177, program/serve.cpp 46, program/config.cpp 44. 392 of
  them stand in template headers.
- The rule "Every error branch is marked unlikely" makes the marker the
  list of the error branches. A rough count of the review finds error
  exits with no marker: 3 of 107 `throw`, 17 of 416 `std::unexpected`,
  and the 3 `catch` blocks (a `catch` cannot carry the marker).
- The tests that reach error paths on purpose: test/fuse/test_device_faults
  (a failed fsync and a failed read, `KONSENS_FUSE_TESTS`, off by
  default, needs `/dev/fuse` and `fusermount3`), sqlite_damage.hpp,
  test_rest and three_nodes.sh of konsensd (`KONSENS_PROGRAM`, off by
  default). The power loss of counter_stress does not run in ctest,
  because `--crash` is 0 by default.
- libgcov writes its counts when a process exits normally.
  three_nodes.sh ends konsensd with SIGKILL, so those counts are lost.
- This repository has no CI job that builds the tree. This session has
  no podman and no g++ 16.

## 2. Steps

0. Spike, in one file outside the tree. One small file with each form
   that the tree uses: `if (c) [[unlikely]] return ...;` on one line,
   `if (c) [[unlikely]] {` with a block, a condition with `&&` and
   `||`, a marker inside a `switch`, a template member that is called
   and one that is not, and one function that two objects both include.
   Build with g++ 16 `--coverage -O0`, run each branch once or never,
   read `gcov-16 --json-format`. The spike answers how a branch that was
   never taken shows, how an uncalled template member shows, and how
   the counts of one header line from two objects add up. It also
   measures the time of one simulation test with and without
   `--coverage`.
1. A debug build with g++ 16 and `--coverage -fprofile-update=atomic`
   (the library has a thread pool), with `KONSENS_FUSE_TESTS=ON` and
   `KONSENS_PROGRAM=ON`, in the build image, with `/dev/fuse` given to
   the container. The build directory and the results lie in a
   directory that the container mounts. ctest runs every test once,
   with the default seed counts. The run is a measurement, not a test
   run of a change; its time is written into the result file.
2. A script `test/coverage/unreached_error_branches.py` (python3 of the
   build image) reads the gcov JSON of every object, adds the counts of
   each source line over all objects, and finds each marker. It writes
   one result file `test/coverage/results/<UTC time stamp>.md`: the
   commit with its dirty mark, the compiler, the flags, the debug
   build, the tests that ran, the tests that end by SIGKILL, and a
   table per file. Each marker is "reached", "not reached", "not
   instantiated" (a template member with no line in any object) or "not
   decided" (a form the spike cannot map). A marker that covers several
   exits counts as reached when one of them ran. A second table lists
   the error exits with no marker.
3. The session sorts each branch that no test reached into one of four
   kinds: a failure of the machine (disk, memory, kernel), input from
   outside (wire, client, config file), a wrong use (`logic_error`),
   an impossible state. It gives the owner the count of each kind and
   the list of the first kind.

## 3. What this plan does not do

- No new test, and no new marker. Which branches get a test is the
  owner's decision after the list (Q1).
- No signal handler that writes the counts of konsensd before SIGKILL.
  Those branches show as "not reached", and the result says why.
- No coverage gate in the CI.
- No Jepsen run with coverage.
- No measurement of speed.

## 4. Questions for the owner

- Q1. After the list: (a) each unreached branch of the kind "failure of
  the machine" gets a test, through the FUSE file system or a storage
  that fails on request; (b) a rule that each new error branch has a
  test that reaches it; (c) nothing more than the list. Proposal: (a)
  now, and (b) only if (a) finds a fault.
- Q2. Where the run takes place, because this session has no podman:
  (a) a session with the build image, as the earlier build sessions;
  (b) a Remote Control session on the owner's machine. Proposal: (a).

## 5. Data this plan leaves on the disk

- The build directory with the `.gcda` and `.gcno` files and the gcov
  JSON: made again by the build command; deleted after the result file
  is written.
- The spike file and its binary: deleted after step 0.
- The result file in `test/coverage/results/`: recorded in the state
  file.
