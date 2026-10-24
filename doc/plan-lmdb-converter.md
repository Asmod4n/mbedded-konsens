# Plan: convert an LMDB 0.9 database to LMDB 1.0

Source: task aec7919a of Claude-state. The owner, 2026-10-24: a release
that changes the LMDB data version ships a dump program built with the
previous LMDB, and a batch import writes the new format. The owner chose
mdb_dump and mdb_load of LMDB for this (card "mdb_dump/load"). This plan
gets one review before the build.

## 1. Facts checked in this session

- deps/lmdb/libraries/liblmdb/upgrading.doc:14-20 (LMDB_1.0.2): the
  formats of 0.9 and 1.0 are incompatible. The path is mdb_dump of 0.9
  and mdb_load of 1.0.
- A check outside the tree, g++/gcc 16: two named databases "log" and
  "meta" of 2000 entries each, written by mdb_load of LMDB_0.9.36,
  dumped by mdb_dump -a of 0.9.36 and loaded by mdb_load of 1.0.2. The
  dump of 1.0.2 has the same keys and values. Its header has one more
  line, `mapaddr`.
- 1.0.2 refuses a data file of 0.9.36, and 0.9.36 a data file of 1.0.2,
  both with MDB_INVALID, "File is not an LMDB file".
- The tags LMDB_0.9.35 and LMDB_0.9.36 exist in the mirror
  github.com/LMDB/lmdb. LMDB_0.9.36 (the newest 0.9) is the commit of
  the tag; the plan pins it.
- Makefile of both versions: mdb_dump and mdb_load link liblmdb.a and
  `-ldl` (0.9 needs no -ldl; it has no module.c).
- persistent_storage keeps the log in the named database "log" and the
  meta values in "meta" (src/persistent_storage.cpp:28-29).

## 2. Change

1. A second submodule `deps/lmdb-0.9`, the same mirror, at the tag
   LMDB_0.9.36.
2. CMakeLists.txt, in a file cmake/lmdb.cmake that the top level
   includes, so the two versions stand side by side:
   - the static target `lmdb` stays as it is (1.0.2);
   - a static target `lmdb-0.9` from mdb.c and midl.c of
     deps/lmdb-0.9/libraries/liblmdb, with the same warnings;
   - the programs `mdb_dump` and `mdb_load` from 1.0.2, and
     `mdb_dump-0.9` and `mdb_load-0.9` from 0.9.36. Each links only its
     own library. No program links both.
   Program names come from LMDB; the suffix `-0.9` names the version.
3. A test test/test_lmdb_converter.cpp:
   - writes entries and meta values with persistent_storage (1.0.2);
   - runs `mdb_dump -a` of 1.0.2 and `mdb_load-0.9` into a new
     directory: a database in the 0.9 format with the same data;
   - checks that persistent_storage::open of that directory fails;
   - runs `mdb_dump-0.9 -a` and `mdb_load` of 1.0.2 into a third
     directory: this is the conversion of the task;
   - opens it with persistent_storage, and compares each entry, its
     CRC32C (read_entries verifies it), and each meta value with the
     values it wrote.
   The test starts the programs with posix_spawn, with stdin and
   stdout redirected to files through posix_spawn_file_actions. C++
   has no form to start a process. The paths of the programs come in
   as compile definitions from `$<TARGET_FILE:...>`.
4. README: a section "Upgrade from LMDB 0.9" with the two commands
   (`mdb_dump-0.9 -a old > dump`, `mdb_load -f dump new`), and the
   sentence that the new directory replaces the old one after the
   check.
5. Claude-state: RULES.md names LMDB once as an exception. A second
   pin of the same origin needs no new exception.

## 3. Checks

- Build in the image localhost/cxx-matrix, g++ 16, Release and Debug
  with ASan and UBSan. All tests green, the new test included.
- The new test fails when the second load is skipped (the 0.9 directory
  is opened instead): a mutation run once in this session.

## 4. Not in this change

- No handshake field and no check of the data version at open.
- No own dump format. The CBOR form of compaction stays task 9d5cb561.
- No conversion from 1.0 to a later format. That needs a later LMDB.

## 5. Provisional names

`lmdb-0.9`, `mdb_dump-0.9`, `mdb_load-0.9`, `cmake/lmdb.cmake`,
`test_lmdb_converter`.
