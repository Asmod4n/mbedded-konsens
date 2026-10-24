# Plan: every test runs on LMDB, and memory_storage goes away

Source: the owner, 2026-10-24: memory_storage exists only to test without
a database and is no public API; a simulation that needs it shows that
the abstraction over LMDB is broken, and it is to be repaired. The owner
chose "Echtes LMDB" on the card: the simulation runs on persistent_storage
with LMDB on tmpfs, the faults happen below LMDB, and memory_storage goes
away. Batching is allowed if it is needed. This plan replaces
doc/plan-memory-storage-test-only.md. It gets one review before the
build.

## 1. Facts checked in this session, at 9867109

- memory_storage is used in: test_sequence_paxos (14 lines),
  test_memory_storage (14), test_node (9), test_simulation (5),
  test_persistent_storage (5), test_client (4), test_read_index (3),
  test_group_commit (2), bench/konsens_bench.cpp (6), examples/counter.cpp
  (3), examples/counter_stress.cpp (4). No file in include/ or src/ uses
  it.
- test/simulation.hpp:31-33 and :913-919: the rollback fault needs a
  storage that is copyable. memory_storage is; persistent_storage is not,
  so test_simulation_lmdb runs without rollback.
- A probe, not a measurement by the rules (two passes, Release, one
  binary): 30 seeds of counter_stress took 0.70 and 0.95 s on
  memory_storage, 2.17 and 2.15 s on LMDB in a tmpfs, 96.9 and 96.1 s on
  LMDB on the disk of the VM.

## 2. Change

1. Each test and example that uses memory_storage uses
   persistent_storage. The tests open it in a temporary_directory.
2. The tests run with their temporary directories on a tmpfs.
   CMakeLists.txt sets the test property `ENVIRONMENT TMPDIR=/dev/shm`
   for each test. The README part Testing says that /dev/shm must be a
   tmpfs with room for the tests. The build container runs with
   `--shm-size` large enough; the size is measured as the peak during
   ctest. A fsync on tmpfs returns at once, so the library stays the
   same and no flag turns the sync off.
3. Rollback below LMDB: a node that stops closes its environment; the
   simulation copies data.mdb of its directory with
   std::filesystem::copy_file to a second directory, and on rollback
   starts the node from that copy. This is a lost fsync or a backup at
   the level of the file. test_simulation and test_simulation_lmdb become
   one test with rollback::one. The storage_type of a cluster stays a
   type parameter only where a test needs the spy of the bench.
4. Tests that compare persistent_storage with memory_storage (the same
   CRC32C of the last entry, test_persistent_storage.cpp:191; batched
   against single writes, :437-440) compare with values computed from
   the specification instead: the CRC32C of the bytes that RFC 7143 and
   the record form give, computed with crc32c of Google.
5. test_memory_storage.cpp goes away. Each of its cases that checks
   behaviour of the storage concept moves to test_persistent_storage if
   that file lacks it.
6. examples/counter.cpp runs on persistent_storage as three_nodes.cpp
   does (geteuid, the refusal of root, --allow-root-for-test, a
   temporary directory per node).
7. examples/counter_stress.cpp: the memory variant and the flag --lmdb
   go away; LMDB is the only form. memory_cluster and lmdb_cluster become
   one cluster.
8. bench/konsens_bench.cpp: the cases *_memory go away, with their
   lines in bench/run.sh. counting_storage, the spy of the bench, stays.
   The README table loses no row: it shows only LMDB cases.
9. include/konsens/memory_storage.hpp goes away.
10. The seed counts stay where the run time allows it. Each test file
    that lowers its count says so in a comment with the measured time.

## 3. Checks

- Build in the image localhost/cxx-matrix, g++ 16, KONSENS_BENCH=ON:
  Release, and Debug with ASan and UBSan. All tests green.
- The time of ctest, Release and ASan, before and after, in one table.
- The rollback fault: a mutation run once. With the copy restored to
  the wrong node or skipped, the run that needs it must fail or the
  coverage count of rollbacks must be zero; the test checks that count.
- counter_stress: seeds 1 to 300 with --cbor and --power-loss 50 green.
- `grep -rn memory_storage include src test examples bench` gives no
  hit outside bench/results.

## 4. Not in this change

- No change to the library code.
- No batching. Item 10 decides by measurement whether it is needed.
- The browser demo (WASM), which named memory_storage as its storage
  (owner, 2026-10-17), needs a new decision when it starts.

## 5. Review (second Claude agent, 2026-10-24) and changes

Result: changes needed. The build takes these changes:

1. The rollback copy is taken while the node runs (simulation.hpp:517-520,
   :914-917), not at a stop. The copy is data.mdb of the open environment
   between two write_atomically calls: the run has one thread, each
   write_atomically is one committed transaction, open uses no WRITEMAP,
   no read transaction stays open, and LMDB 1.0.2 locks only lock.mdb.
   lock.mdb is not copied. The state equals the copy of memory_storage:
   the state after the last write_atomically. Open for the owner: this
   file copy in the tests, or a copy method of the library.
2. Restore: destroy node i, copy the saved data.mdb over
   directories[i]/data.mdb, then open. A later crash then reopens the
   rolled-back state.
3. Unit tests that copy a storage (test_sequence_paxos.cpp:1157,
   test_node.cpp:74, :114, :151, :155) use one shared helper that opens a
   storage and copies data.mdb. The wrappers failing_storage
   (test_sequence_paxos.cpp:195), recording_storage (test_node.cpp:336)
   and the disk of test_group_commit.cpp:48-60 wrap persistent_storage.
   A failed write is a real MDB_MAP_FULL with a tiny map. Open for the
   owner: fail_sync and fail_reads have no fault below LMDB on tmpfs.
4. The counter_stress tests (CMakeLists.txt:106-107) get the TMPDIR
   property and --allow-root-for-test.
5. TMPDIR comes from a cache variable KONSENS_TEST_TMPDIR, empty by
   default; the build of this session sets it to /dev/shm. The peak of
   /dev/shm during ctest is measured.
6. test_persistent_storage.cpp:437 loses only its memory_storage
   subcase. :191 checks each of the five entry kinds against record bytes
   from the specification and against compute_entry_crc.
7. Bench: only memory_spy (:842), memory_nodes (:1101) and the seven
   *_memory cases (:1758-1774) go away; counting_storage and storage_spy
   stay. README.md:245-250 and bench/run.sh:264 change their text.
8. temp_directory_path in temporary_directory.hpp is the one effect of
   the fixture. No change.
9. Three steps on three branches, each merged when green: (a) TMPDIR,
   rollback on LMDB, one simulation test, the timing table; (b) the unit
   tests with the helper of item 3; (c) examples, bench, removal of the
   header, README.

## 6. Result of step (a)

The owner chose on 2026-10-24: ASan runs once before each merge, not at
each step; the ASan build uses `-O1`; test_simulation is split so that
ctest runs it in parallel; a build with ASan runs fewer seeds.

- persistent_storage::copy makes the rollback copy with mdb_env_copy2
  and MDB_CP_COMPACT while the node runs. Restore opens the copy.
- test_simulation is three programs: test_simulation,
  test_simulation_rollback and test_simulation_clients. The cluster is in
  test/lmdb_cluster.hpp. test_simulation_lmdb is gone.
- A rollback counts when the restored log is shorter. Mutation, run once:
  roll_back that does not restore gives `c.rollbacks == 0`, and both
  rollback cases fail.
- g++ 16, image localhost/cxx-matrix, KONSENS_TEST_TMPDIR=/dev/shm:
  19 of 19 tests green in Release and in Debug with
  `-O1 -fno-omit-frame-pointer -fsanitize=address,undefined`. Both pass
  with `--shm-size=64m`.
- Without frame pointers at `-O1`, the LSan suppression of `dumpit` in
  test_lmdb_upgrade does not match, because the stack has no frame of
  dumpit.

Times, g++ 16, test_simulation with all its cases in one process, before
the split, 2000 seeds, Release:

| Storage | Median of 3, alternate runs |
|---|---|
| memory_storage | 8.0 s |
| LMDB on tmpfs | 27.9 s |

Times of ctest, one run each:

| Build | Before | After |
|---|---|---|
| Release | 26.1 s, one process | 16.1 s, split |
| ASan | 479-493 s, -O0, memory, 2000 seeds | 26.1 s, -O1, LMDB, 200 seeds, split |

The ASan figure before comes from six ctest logs of this session on
memory_storage. test_simulation_clients takes the longest part: 16.0 s
in Release, 26.1 s under ASan.

## 7. Result of step (b)

The owner chose on 2026-10-24 that the node tests with a write fault in
the middle of a run use error_fs, switched through a control file.

- test/lmdb_storage.hpp opens each storage of a unit test in a
  directory of its own below one directory of the program, and copies a
  storage with persistent_storage::copy.
- test_sequence_paxos: a failed write is a full LMDB map from the start,
  and a failed read is a flipped byte of the last entry in data.mdb.
  failing_storage is gone.
- test_persistent_storage: the CRC32C of each of the five kinds is
  checked against a record written by hand from the format and against
  compute_entry_crc.
- The cluster of test_group_commit is test/node_cluster.hpp, on LMDB. Its
  two tests of a failed write and a failed sync moved to
  test_device_faults: error_fs fails each write of data.mdb with ENOSPC
  or each fsync with EIO after a write to /.error_fs. A failed write at
  the commit is a sync_error with no_space_on_device, a write_error.
- Mutations, run once: a map that does not fill and no flipped byte make
  the three tests of test_sequence_paxos fail; a control file that sets
  no fault makes both node tests of test_device_faults fail.
- g++ 16, image localhost/cxx-matrix with libfuse3-dev and fuse3,
  KONSENS_FUSE_TESTS=ON: 20 of 20 tests green in Release and with ASan.
