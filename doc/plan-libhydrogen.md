# Plan: libhydrogen as a submodule, hydro_hash in the bench

Source: task 0602e4ac of Claude-state. The owner, 2026-10-24: hashes
come from libhydrogen, not from SHA-256 of OpenSSL. A cryptographic hash
is not part of the consensus path. libhydrogen comes as a submodule.
This plan gets one review before the build.

## 1. Facts checked in this session

- Upstream github.com/jedisct1/libhydrogen, head 264bf42 (2026-09-28),
  ISC license, no tags.
- deps/libhydrogen/CMakeLists.txt:1 asks for `cmake_minimum_required(VERSION 3.1)`.
  CMake 4 refuses a minimum below 3.5 unless `CMAKE_POLICY_VERSION_MINIMUM`
  is set.
- deps/libhydrogen/CMakeLists.txt:43-46: without `HYDROGEN_TARGET_ARCH`
  and without cross-compiling, the library builds with `-march=native`.
- deps/libhydrogen/CMakeLists.txt:56: the library always builds with `-Os`,
  after the flags of the build type.
- deps/libhydrogen/CMakeLists.txt:142-172: a test executable and a target
  in ALL that runs it. Under `EXCLUDE_FROM_ALL` neither is built, but
  its `add_test` still reaches the CTest file of the parent.
- The target is `hydrogen::hydrogen`, in C. The project of mbedded-konsens
  declares `LANGUAGES CXX` only.
- bench/hash_bench.cpp (40fdbbe) has no CMake target. It is the only
  user of OpenSSL in the tree.
- bench/konsens_bench.cpp:486-505 `measure_crc32c` and the case table at
  :1749; bench/run.sh:13 lists the cases and builds two binaries, A and A/A.

## 2. Change

1. `deps/libhydrogen` at 264bf42 as a submodule.
2. CMakeLists.txt: `project(... LANGUAGES C CXX)`. libhydrogen is added in
   a `block()` like crc32c, with `EXCLUDE_FROM_ALL`,
   `CMAKE_POLICY_VERSION_MINIMUM 3.10`, and `HYDROGEN_TARGET_ARCH` set
   from `CMAKE_SYSTEM_PROCESSOR`: `x86-64` on x86_64, `armv8-a` on
   aarch64, else an error at configure time. Provisional: the baseline
   of the distribution, so that a binary runs on every machine of its
   architecture. The `-Os` of upstream stays.
3. No library target links libhydrogen yet. Only the bench links it.
4. bench/konsens_bench.cpp: a case `hydro_hash` beside `crc32c`, with the
   same buffers and the same sizes 64, 1024 and 65536 bytes. It calls
   `hydro_init` once, then `hydro_hash_hash` with a fixed context and no
   key, 32 bytes out. Before the time, it checks the third vector of
   deps/libhydrogen/tests/tests.c:78-120 (`7dfa45ce...a463`), made the
   way that test makes it.
5. bench/run.sh: the three `hydro_hash` cases in the list, and an optional
   sixth argument, a prefix of case names. Only the cases whose name
   starts with one of the given prefixes run. Without it, every case runs.
6. bench/hash_bench.cpp is deleted. Its question now goes through
   konsens_bench, with the A/A binary and the context of run.sh. This
   removes OpenSSL from the tree.

## 3. Checks

- A build with the CMake of the distribution in the image localhost/cxx-matrix,
  g++ 16: Debug with ASan and UBSan, and Release. All tests green.
- `grep -rni openssl` over the CMake files and the sources outside deps/ finds nothing.
- One run of bench/run.sh with the prefixes `crc32c,hydro_hash`. The
  result file is committed. The answer gives the ratio of hydro_hash to
  CRC32C for each size, with the noise floor of the A/A binary.

## 4. Not in this change

- No hash in the consensus path and no hash of the log for recovery.
- No change to deps/crc32c or deps/mbedded-cbor.
- No measurement of `-O2` against the `-Os` of upstream.

## 5. Review (second Claude agent, 2026-10-24) and changes

Result: changes needed. Each finding is taken into the build:

1. `hydrogen-tests` is built (`EXCLUDE_FROM_ALL OFF` on the target), so its
   test runs in ctest and does not show as "Not Run".
2. `CMAKE_POLICY_DEFAULT_CMP0126 NEW` in the block, so the normal variable
   `HYDROGEN_TARGET_ARCH` holds on the first configure.
3. `CMAKE_EXPORT_NO_PACKAGE_REGISTRY ON` and `BUILD_SHARED_LIBS OFF` in the block.
4. `CMAKE_POLICY_VERSION_MINIMUM` is 3.10, not 3.5: CMake 4.3 warns below 3.10.
5. The timed hash uses the key and the context of the test vector.
6. bench/hash_bench.cpp asked about 64-byte chunks. The cases
   `crc32c_chunks` and `hydro_hash_chunks` keep that question. The names
   are provisional.
7. run.sh: sixth argument, passed to `--inside`; header; C flags set to
   `-O2 -DNDEBUG` and recorded with the compile command of hydrogen.c.
8. The `ldd` check is replaced by a grep of the CMake files for OpenSSL.

## 6. Change after the owner, 2026-10-24

The owner: the measurement of libhydrogen does not matter. hydro_hash
does not replace CRC32C. It is only additive, for a rebuild. The bench
cases crc32c_chunks, hydro_hash and hydro_hash_chunks are removed, and
the run was discarded. The submodule, the build of libhydrogen with its
tests, the prefixes of run.sh and the removal of bench/hash_bench.cpp stay.
