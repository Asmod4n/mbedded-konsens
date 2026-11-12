# Plan: which error branches no test reaches

Source: the owner, 2026-11-12 21:31 (UTC): "okay, leg los" to the
proposal V2 of the evaluation of Ron Pressler, "Why Writing Correct
Software Is Hard" (/mnt/project-files/actor-raft/correctness-and-complexity.md).
That evaluation cites Yuan et al., "Simple Testing Can Prevent Most
Critical Failures", OSDI 2014: 92 % of the catastrophic failures in the
systems they studied came from the handling of non-fatal errors. This
plan measures which error branches of this tree no test reaches. It
writes no new test. It gets one review before the run.

## 1. Facts checked in this session (main 654c088)

- `[[unlikely]]` stands 808 times in `src/`, `include/` and `program/`.
  The most: persistent_storage.cpp 219, sequence_paxos.hpp 215,
  node.hpp 177, program/serve.cpp 46, program/config.cpp 44.
- The rule "Every error branch is marked unlikely" makes the marker the
  list of the error branches. A branch with no marker is not in this
  measurement, and the result says so.
- The tests that reach error paths on purpose: test/fuse (a failed
  fsync and a failed read, `KONSENS_FUSE_TESTS`, off by default),
  test_device_faults, sqlite_damage.hpp, the power loss of
  counter_stress, test_rest of konsensd (`KONSENS_PROGRAM`, off by
  default).
- gcov is part of the gcc package. No other tool is needed.
- This session has no podman and no g++ 16. The run needs the build
  image of the tree.

## 2. Steps

0. Spike, in one file outside the tree. One small file with each form
   that the tree uses: `if (c) [[unlikely]] return ...;` on one line,
   `if (c) [[unlikely]] {` with a block, a condition with `&&` and
   `||`, a `[[unlikely]]` inside a `switch`. Build with g++ 16
   `--coverage -O0`, run each branch once or never, read
   `gcov --json-format`. The spike answers how a branch that was never
   taken shows: a line count of zero on the body, or a branch count of
   zero on the line of the condition. The script of step 2 reads it the
   way the spike shows.
1. A debug build with g++ 16 and `--coverage -fprofile-update=atomic`
   (the library has a thread pool), with `KONSENS_FUSE_TESTS=ON` and
   `KONSENS_PROGRAM=ON`, in the build image. ctest runs every test once,
   with the default seed counts.
2. A script `test/coverage/unreached_error_branches.py` (python3 of the
   distribution) finds each `[[unlikely]]` in the sources and reads the
   gcov JSON of its line. It writes one result file
   `test/coverage/results/<UTC time stamp>.md`: the commit with its
   dirty mark, the compiler, the flags, the tests that ran, and a table
   per file of each marker with "reached" or "not reached". A marker
   that the spike cannot map stays "not decided" with its line.
3. The session sorts each branch that no test reached into one of four
   kinds: a failure of the machine (disk, memory, kernel), input from
   outside (wire, client, config file), a wrong use (`logic_error`),
   an impossible state. It gives the owner the count of each kind and
   the list of the first kind.

## 3. What this plan does not do

- No new test. Which branches get a test is the owner's decision after
  the list (question Q1).
- No coverage gate in the CI.
- No Jepsen run with coverage.
- No measurement of speed, so no bench rule applies.

## 4. Questions for the owner

- Q1. After the list: (a) each unreached branch of the kind "failure of
  the machine" gets a test, through the FUSE file system or a storage
  that fails on request; (b) a rule that each new error branch has a
  test that reaches it; (c) nothing more than the list. Proposal: (a)
  now, and (b) only if (a) finds a fault. Yuan et al. found most faults
  in the branches of the first kind.

## 5. Data this plan leaves on the disk

- The build directory with the `.gcda` and `.gcno` files: made again by
  the build command; deleted after the result file is written.
- The result file in `test/coverage/results/`: recorded in the state
  file.
