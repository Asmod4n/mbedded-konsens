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
- The library opens SQLite in one place, `open_connection`, with
  `sqlite3_open_v2(uri, ..., nullptr)` (src/persistent_storage.cpp:1057):
  the writer, the readers and node_storage (node.db). So it uses the
  default VFS, and a VFS that a test registers as the default is used
  with no change to the library. `copy()` writes a second database
  through the VFS with `VACUUM INTO`.
- Some calls bypass the VFS: `::open` in create_with_file_mode,
  create_new_with_file_mode and open_the_wal_read_only
  (persistent_storage.cpp:940-993), and the sync of the WAL by the
  caller through io_uring (program/serve.cpp:945, node.hpp:374).
- The library sets no `mmap_size`. The default of the distribution's
  build is not checked; the spike reads it.
- A failed sync of the WAL at a commit comes back as a value, not as
  `sync_error`. `sync_the_wal` returns `to_error_code` of the failed
  `xSync` (persistent_storage.cpp:1097-1098), and `transaction::commit`
  returns it (1756-1763) after `commit_without_a_sync` has set
  `write_unfinished` to false (1786). sequence_paxos.hpp:544-545 raises
  it as `write_error`. A direct user of persistent_storage can commit
  again, and that repeats the sync. The sync in `open` has the same form
  (1945-1947). This is a finding for the owner. It is the probable cause
  of the failed fsync cases of test_device_faults, not checked.
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
1. `test/io_error_vfs.hpp`: the VFS of step 0, the default VFS of its
   own test binary only. It fails only files below the directories that
   the test names: both directories of one node (its store and its
   node.db), and it follows a node that a restore moves. A file with no
   name (a temporary file) is not failed. The state lies in `pAppData`,
   not in a static, and the counters are atomic. The counters keep the
   names of SQLite: `io_error_pending`, `io_error_persist`,
   `io_error_hit`, `diskfull_pending`. Each failed operation returns the
   code that os_unix.c returns for it: a read fails with
   `SQLITE_IOERR_READ`, never with `SQLITE_IOERR_SHORT_READ`.
   `xGetLastError` gives EIO, or ENOSPC for a full disk. The shared
   memory of the WAL is not failed, as in os_unix.c. The operations of a
   close are counted too.
2. `test/test_io_errors.cpp`, one test case for each scenario, each
   swept over N from 1 until a run has no hit, once with
   `io_error_persist` off and once on, and once with `diskfull_pending`:
   - a. open a new storage, append entries, commit, read them back;
   - b. the same on a storage that holds entries;
   - c. a checkpoint: 999 frames are written before the failure is
     armed;
   - d. node_storage::open and its writes;
   - e. three nodes in the simulation, the failure at one of them, with
     writes from clients, a fixed seed and no check threads.
   Each run checks the exact outcome:
   - a failed sync gives `sync_error`; a failed write, or a failed read
     in a write transaction, gives `write_error`; a failed read in a read
     transaction gives `read_error`; a full disk gives
     `no_space_on_device` as a value, and `write_error` in a checkpoint;
   - after the first raise, the test uses that storage no more: it
     closes it, turns the failure off and opens it again. A second use
     gives `std::logic_error`, and that is right;
   - an I/O error never makes a node stop voting or write a proof of a
     fault; an I/O error is not damage;
   - after the failure is turned off, `PRAGMA integrity_check` answers
     "ok", and confirmed is a part of present, and present is a part of
     attempted. Each transaction is there whole or not at all.
     Confirmed means: `commit()` returned a value, for the storage
     alone; decided by a majority or answered to a client, in the
     cluster. That a failed sync loses data stays the work of the FUSE
     test.
   - in scenario e, a node that throws is handled as a crash: the
     simulation destroys it, turns the failure off and starts it again
     through `cluster.start`. This needs a new branch in
     test/simulation.hpp. The checks it already makes hold.
3. The run of step 1 of plan-error-branch-coverage again, with the new
   test, after the owner allows a second long run. The result file gives
   the count of "machine" branches reached, before and after.

## 2a. What the sweep can reach

An I/O error is raised inside persistent_storage, so an `if (!r)` value
branch above it is reached only by a value: a full disk on a write
path. Expected reach, before the measurement of step 3:

- persistent_storage.cpp: the I/O paths of open, commit, read and
  checkpoint.
- sequence_paxos.hpp: the write paths, for example 545, 565, 634-644,
  1183-1191, 1471, 2329-2334.
- node.hpp: the node_storage writes, 283-284, 504-505, 774-775,
  1211-1220, 1352-1353, 1371-1372.

Not reached by this plan: the read value branches of node.hpp (1559,
1564, 1655, 1659, 2172, 2179, 2587-2616); the sync by the caller (385,
395-401); the ring and the check pool (1494-1531, 1789-1820);
chain_hash.cpp (OpenSSL); the bind and prepare failures, which need a
failed allocation; the size, CRC and decode branches, which need damage
or input; the `::open` calls of persistent_storage.cpp:940-991. So the
plan does not cover the 430.

## 2b. Result of step 0 (2026-11-12)

Spike in the scratchpad, built in the image (g++-16, SQLite 3.53.4,
`-O1`), raw SQLite and no library:

- The wrapper works as the default VFS over `unix`, in WAL mode.
- `PRAGMA mmap_size` is 0 by default in this build, and `xFetch` was
  not called.
- One open, one commit of one row with `synchronous=FULL` and one read
  take 23 counted operations. N runs from 1 to 24.
- One sweep: 22.1 ms with one failure, 15.0 ms with persistent failure.
  16 of 23 runs failed, and `PRAGMA integrity_check` answered "ok" after
  each of them.
- The time of a sweep over scenario e is not measured yet. It is
  measured in step 2 before Q1 is asked.

## 3. What this plan does not do

- No change to the library. A fault the test finds goes to the owner
  first.
- No test of a failed allocation (`SQLITE_CONFIG_MALLOC`). That is the
  second half of the SQLite form and the next plan, after this one.
- No fix for the timeout of test_device_faults.
- No fix of the finding of section 1 inside this plan. It goes to the
  owner, and its fix is a branch of its own.

## 4. Open question for the owner

- Q1. Only if step 0 shows that the sweep of scenario e takes longer
  than the five minutes of a test run: (a) scenario e with every N,
  behind a CMake option and run nightly, as the trace check; (b)
  scenario e with a sample of N. Proposal: (a).
- Q2. Step 3 is a second run longer than five minutes.

## 5. Data this plan leaves on the disk

- The spike file and its binary: deleted after step 0.
- The storage directories of the test: removed by the test program.
- The coverage build and its gcov JSON: deleted after the result file is
  written.
- The result file in test/coverage/results/: recorded in the state
  file.
