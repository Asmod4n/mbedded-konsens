# Plan: hide LMDB from the public library interface

Source: task 25bfc73a of Claude-state. The owner decided that the API
of the library shows no LMDB name (Claude-state mbedded-konsens.md). A
module owns the types of its dependency and shows them to nobody
(RULES.md). This plan gets one review before the build.

## 1. Facts checked in this session, at 40fdbbe

- include/konsens/persistent_storage.hpp:18 declares `struct MDB_env;`.
  Lines 56-64 hold `std::unique_ptr<MDB_env, env_close> env_` and a
  private constructor that takes it. Line 27 declares
  `lmdb_category()`. The header does not include lmdb.h.
- CMakeLists.txt:32 links LMDB PUBLIC to konsens. So every target that
  links konsens gets the include directory of lmdb.h.
- Outside src/, only bench/konsens_bench.cpp includes lmdb.h (line 33).
  It measures LMDB alone as a baseline. Its `error_code_of` (line 859)
  calls `konsens::lmdb_category()`.
- No test and no example names `lmdb_category`, `MDB_` or `mdb_` in
  code. The tests name them only in comments.
- src/persistent_storage.cpp:72-80 `to_error_code` maps MDB_MAP_FULL
  to `std::errc::no_space_on_device`, a positive status to
  `std::generic_category()`, and every other status to the LMDB
  category.

## 2. Change

1. persistent_storage.hpp: the forward declaration `struct MDB_env;`,
   the type `env_close` and `lmdb_category()` leave the header. The
   class holds `std::unique_ptr<environment> env_`, where
   `struct environment;` is declared in the class and defined in
   src/persistent_storage.cpp. The name comes from LMDB: MDB_env is
   the LMDB environment. `environment` holds the `MDB_env*` and closes
   it in its destructor.
2. Because `environment` is incomplete in the header, the destructor
   and the move constructor and move assignment of
   `persistent_storage` are declared in the header and defaulted in
   the .cpp. Copy stays deleted, as today through unique_ptr.
3. The dbi values stay as `unsigned int` members. They carry no LMDB
   name.
4. `lmdb_category()` moves to a new internal header
   src/lmdb_category.hpp. src/persistent_storage.cpp includes it.
   The error codes that the library returns keep the same category,
   name "lmdb", and the same messages.
5. CMakeLists.txt: konsens links LMDB PRIVATE. A static library still
   gives LMDB to the link of each user, as a link-only dependency.
6. The bench target links LMDB itself, because it includes lmdb.h, and
   gets src/ as a PRIVATE include directory for lmdb_category.hpp.
7. No change to the behaviour, to the error values or to the README
   example. The README has no line about lmdb_category.

## 3. Checks

- Build in the image localhost/cxx-matrix, g++ 16: Debug with ASan and
  UBSan, and Release, with KONSENS_BENCH=ON. All tests green.
- A new test translation unit is not needed. The check that the header
  shows no LMDB name is a grep in this session: `MDB_`, `mdb_` and
  `lmdb` in include/ give no hit outside comments.
- An example target that links konsens compiles without the include
  directory of lmdb.h: read from the ninja compile command of
  examples/three_nodes.cpp.
- Seeds 1 to 50 of counter_stress (from the branch counter-stress-checks
  if merged, else of this base) are not needed: the storage code paths
  do not change.

## 4. Not in this change

- No measurement. The change adds one pointer dereference per storage
  call and one allocation per open. No claim about speed is made.
- `open` keeps its parameter `map_size`. It is the LMDB map size. A
  different name or form is a question for the owner (provisional, see
  section 5).
- The mapping of LMDB statuses to error values does not change. Which
  LMDB status a caller must be able to compare (for example
  MDB_VERSION_MISMATCH for the converter task aec7919a) is an error
  path and the owner decides it.
- No change to the branch lmdb-submodule. The two branches touch
  CMakeLists.txt:32 both. The second merge resolves that line by hand.

## 5. Open points for the owner

- The parameter `map_size` of `persistent_storage::open` is an LMDB
  term in the API.
- A caller can no longer name the LMDB category to compare an error
  code with an LMDB status.
