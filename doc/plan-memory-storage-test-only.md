# Plan: memory_storage leaves the public headers

Source: the owner, 2026-10-24: memory_storage exists only to test
without a database; it is no public API. Task 7a193882 of Claude-state
led to it. This plan gets one review before the build.

## 1. Facts checked in this session, at e3e02ee

- include/konsens/memory_storage.hpp (75 lines) defines
  `konsens::memory_storage`. No file in include/ or src/ includes it.
- Users: examples/counter.cpp:20 and :196, examples/counter_stress.cpp:35
  and its memory_cluster, bench/konsens_bench.cpp, and the tests
  test_sequence_paxos, test_group_commit, test_simulation,
  test_simulation_lmdb, test_memory_storage, test_client,
  test_persistent_storage, test_read_index and test_node.
- README.md does not name memory_storage.
- examples/temporary_directories.hpp gives a directory for each node
  and removes it at the end (used by three_nodes.cpp and
  counter_stress.cpp).

## 2. Change

1. `git mv include/konsens/memory_storage.hpp test/memory_storage.hpp`.
   The namespace `konsens` and the name stay.
2. Each test includes `"memory_storage.hpp"`. The tests find it in their
   own directory.
3. CMakeLists.txt: counter_stress and the two bench targets get
   `${PROJECT_SOURCE_DIR}/test` as a PRIVATE include directory.
   counter_stress is a stress test of the library and keeps its memory
   variant.
4. examples/counter.cpp, the example that a user reads, runs its three
   nodes on persistent_storage in directories of temporary_directories,
   as three_nodes.cpp does. It no longer includes memory_storage.
5. Record in the state: the earlier handle_accept_decide figure of
   750 ns left out the free of the received messages; the bench case of
   today includes it, as a caller of handle_incoming pays it. The bench
   case stays as it is.

## 3. Checks

- Build in the image localhost/cxx-matrix, g++ 16, KONSENS_BENCH=ON:
  Release, and Debug with ASan and UBSan. All tests green.
- `grep -rn memory_storage include/` gives no hit.
- examples/counter runs to its end with exit status 0 as a user that is
  not root.

## 4. Not in this change

- No change to memory_storage itself.
- No change to the WASM demo plan, which named memory_storage as its
  storage. A demo without a database is a question for the owner when
  the demo starts.
