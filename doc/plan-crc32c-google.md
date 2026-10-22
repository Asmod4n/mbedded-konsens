# Plan: CRC32C from Google crc32c

Source: the owner, 2026-10-22 06:14 to 06:17 (UTC+2): Google crc32c
comes as a submodule (RULES.md, Claude-state f19a415), and
`crc32c::Extend` computes every CRC32C, with the instruction and
without it. A warning at start says when the CPU has no instruction.
This plan gets one review before the build.

## 1. Facts checked in this session

- bench/results/2026-10-22-041648Z.txt (g++ 16 -O2 -march=x86-64, A/A
  1.1 %): ns per CRC of 64 B, 1 KiB, 64 KiB: table of src/crc32c.cpp
  93, 2106, 137283; Google `crc32c::Extend` 2.8, 34, 2662; Google
  `ExtendPortable` 22, 220, 14359.
- Google crc32c at 2bbb3be (tag 1.1.2-6): `include/crc32c/crc32c.h`
  declares `crc32c::Extend(uint32_t, const uint8_t*, size_t)` and
  `crc32c::Crc32c`. It chooses the SSE4.2 or the ARM64 form at run time
  when the compiler can build it (HAVE_SSE42, HAVE_ARM64_CRC32C in
  `crc32c_config.h`). The check of the CPU is internal
  (`crc32c_sse42_check.h`, `crc32c_arm64_check.h`).
- Users of `konsens::crc32c::extend`: src/storage.cpp:93,
  src/persistent_storage.cpp:229 and 249, test/test_crc32c.cpp.
- The Google namespace `crc32c` has the same name as
  `konsens::crc32c`.
- The library has no log sink yet (decision of 2026-10-17: records go
  to a sink of the user; not built).

## 2. Change

1. `deps/crc32c`: Google crc32c as a git submodule at 2bbb3be, built
   from CMake with `CRC32C_BUILD_TESTS`, `CRC32C_BUILD_BENCHMARKS`,
   `CRC32C_USE_GLOG` and `CRC32C_INSTALL` off, linked PRIVATE to
   `konsens`. No Google type or header leaves `src/`.
2. `include/konsens/crc32c.hpp` and `src/crc32c.cpp` go. The namespace
   `konsens::crc32c` goes, so no name hides the Google namespace.
3. `src/storage.cpp` and `src/persistent_storage.cpp` call
   `crc32c::Extend` with the bytes of a `std::span<const std::byte>`.
   Provisional: one private function in `src/` takes the span and calls
   `crc32c::Extend`, because the rule "No function wraps one library
   call" stands against it, and three call sites need the same cast
   from `std::byte` to `uint8_t`. The review decides between this
   function and the cast at each call site.
4. The warning: `konsens` gets one public function that tells whether
   the CPU has the CRC32C instruction (x86: `__builtin_cpu_supports
   ("sse4.2")`; ARM64: `getauxval(AT_HWCAP) & HWCAP_CRC32`). Its name
   has no source; provisional. The library does not print. The binary
   and the README tell the user to check it at start and to warn. When
   a log sink exists, the warning goes there.
5. The CRC values on disk do not change: CRC32C is the same function.

## 3. Tests

- test/test_crc32c.cpp keeps the check values of RFC 3720 B.4 (the
  vectors it has now) and runs them through the path the library uses.
- A test calls `ExtendPortable` and `Extend` on the same buffers of
  every length from 0 to 1024 and on 64 KiB, so the hardware form and
  the software form agree.
- The function of 2.4 gives true on this machine (`sse4_2` in
  /proc/cpuinfo).
- All tests, Debug with ASan and UBSan, and Release; seeds 1 to 300 of
  counter_stress, plain and `--cbor`.

## 4. Measurement

bench `crc32c` and `commit_lmdb`, `write_lmdb`: old (466f26e+) against
new, each in its own binary, A/A first, 31 samples, 7 rounds.

## 5. Not in this plan

The log sink. The browser and WASM build.

## 6. Order

1. Plan and review.
2. Submodule, CMake, calls; tests.
3. The function of 2.4; README.
4. Measurement.
