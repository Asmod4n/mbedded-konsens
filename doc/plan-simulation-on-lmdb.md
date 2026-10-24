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
