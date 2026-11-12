# Plan: I/O error testing in the form of SQLite

Source: the owner, 2026-11-12 22:41 (UTC), card "Ja, SQLite-Art
(Praxis)". The list of branch error-branch-coverage
(test/coverage/results/20261112T225500Z.md) finds 430 error branches of
the kind "failure of the machine" that no test reaches. This plan gives
them a test in the form that SQLite uses for itself.

## 1. Facts checked in this session (main 654c088)

- SQLite, https://www.sqlite.org/testing.html section 3.2 "I/O Error
  Testing": a VFS is rigged to fail after a set number of I/O
  operations. The test moves the failure point one step later each time,
  until the operation ends with no failure. It runs twice: once with one
  failure, once with every I/O after the first failure failing too.
  After that, `PRAGMA integrity_check` looks for damage.
- SQLite names these counters `sqlite3_io_error_pending`,
  `sqlite3_io_error_persist`, `sqlite3_io_error_hit` and
  `sqlite3_diskfull_pending` (src/os_common.h). They exist only in a
  build with `SQLITE_TEST`. The package of the distribution has no such
  build, so this plan cannot use them.
- os_unix.c fails these operations in a test build: the check of the
  reserved lock, read, write, write with a full disk (`SQLITE_FULL`),
  sync, truncate, the file size, delete and access. The shared memory of
  the WAL is not failed.
- The library opens SQLite with `sqlite3_open_v2(uri, ..., nullptr)`
  (src/persistent_storage.cpp:1057), so it uses the default VFS. It does
  not use memory-mapped I/O. A VFS that a test registers as the default
  is used by the library with no change to the library.
- The 430 branches lie in persistent_storage.cpp (157),
  sequence_paxos.hpp (153), node.hpp (100) and seven other files.
- test_device_faults (FUSE) times out in the Ubuntu 26.10 image with
  SQLite 3.53.4. Its cause is open. This plan does not replace it: FUSE
  tests the kernel under SQLite, the VFS tests SQLite and the library.

## 2. Steps

0. Spike, in one file outside the tree. A VFS that wraps the default
   VFS of the distribution's SQLite, registered as the default, counts
   the operations of section 1 and fails the one at `io_error_pending`.
   The spike answers: does the wrapper work with WAL mode; how many
   operations one open, one commit and one read take; how long one sweep
   over them takes. It also reads how the simulation of this tree
   handles a node that throws `sync_error` or `write_error`, and a node
   that stops voting.
1. `test/io_error_vfs.hpp`: the VFS of step 0. It fails only files below
   one directory that the test names, so that one node of a cluster in
   one process fails and the others do not. The counters keep the names
   of SQLite: `io_error_pending`, `io_error_persist`, `io_error_hit`,
   `diskfull_pending`. Each failed operation returns the code that
   os_unix.c returns for it.
2. `test/test_io_errors.cpp`, one test case for each scenario, each
   swept over N from 1 until a run has no hit, once with
   `io_error_persist` off and once on, and once with `diskfull_pending`:
   - a. open a new storage, append entries, commit, read them back;
   - b. the same on a storage that holds entries;
   - c. three nodes in the simulation, the failure at one of them, with
     writes from clients.
   Each run of a scenario checks:
   - the failure ends as the rules say: an error value, `write_error` or
     `sync_error` (the test catches them as the body of a thread does),
     or the node stops voting; no other exception, no crash;
   - after the failure is turned off, the storage opens again,
     `PRAGMA integrity_check` answers "ok", and every entry whose commit
     was confirmed is there;
   - in scenario c, the checks the simulation already makes hold.
3. The run of step 1 of plan-error-branch-coverage again, with the new
   test. The result file gives the count of "machine" branches reached,
   before and after.

## 3. What this plan does not do

- No change to the library. A fault the test finds goes to the owner
  first.
- No test of a failed allocation (`SQLITE_CONFIG_MALLOC`). That is the
  second half of the SQLite form and the next plan, after this one.
- No fix for the timeout of test_device_faults.

## 4. Open question for the owner

- Q1. Only if step 0 shows that the sweep of scenario c takes longer
  than the five minutes of a test run: (a) scenario c with a fixed seed
  and every N, behind a CMake option and run nightly, as the trace
  check; (b) scenario c with a sample of N. Proposal: (a).

## 5. Data this plan leaves on the disk

- The spike file and its binary: deleted after step 0.
- The storage directories of the test: removed by the test program.
- The coverage build and its gcov JSON: deleted after the result file is
  written.
- The result file in test/coverage/results/: recorded in the state
  file.
