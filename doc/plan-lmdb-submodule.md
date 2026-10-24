# Plan: LMDB as a submodule at LMDB_1.0.2

Source: task 8f6f26ad of Claude-state. The owner, 2026-10-24: LMDB
comes as a submodule, because the distributions ship different versions
of LMDB. The owner chose the tag LMDB_1.0.2. This plan gets one review
before the build.

## 1. Facts checked in this session

- Origin git.openldap.org/openldap/lmdb is not reachable from this
  session (proxy 403). The mirror github.com/LMDB/lmdb has the same tags.
  LMDB_1.0.2 is the commit c279e04 "Prep for release (LMDB 1.0.2)".
- deps/lmdb/libraries/liblmdb/Makefile:86-87: liblmdb.a is mdb.o, midl.o
  and module.o. Makefile:23-25: `-pthread`, `-O2 -g`, warnings
  `-W -Wall -Wno-unused-parameter -Wbad-function-cast -Wuninitialized`.
  The programs link `-ldl` (Makefile:31, 96-105), for module.c.
- deps/lmdb/libraries/liblmdb/upgrading.doc:14-20: the file formats of
  0.9 and 1.0 are incompatible. The path is mdb_dump of 0.9 and
  mdb_load of 1.0. LMDB 1.0 does not open a 0.9 file.
- LMDB has no CMake file.
- CMakeLists.txt:9 finds LMDB with pkg-config, CMakeLists.txt:32 links
  `PkgConfig::LMDB` PUBLIC. The build image has the package 0.9.35.
- The library uses only the LMDB calls that 0.9 also has: env, txn,
  dbi, cursor, put, get, del, stat, strerror.
- bench/run.sh:90 records the package liblmdb0. README.md:125, 138 name
  `liblmdb-dev` and pkg-config.

## 2. Change

1. `deps/lmdb` at LMDB_1.0.2, from the mirror, as a submodule.
2. CMakeLists.txt: a static target `lmdb` from mdb.c, midl.c and
   module.c of deps/lmdb/libraries/liblmdb, with the include directory
   of lmdb.h, the warnings of its Makefile as PRIVATE options,
   `POSITION_INDEPENDENT_CODE ON`, and `Threads::Threads` and
   `${CMAKE_DL_LIBS}` as its links. No macro of the Makefile list is set:
   the Makefile says the defaults hold for most platforms.
3. The pkg-config lookup of lmdb goes away. konsens links `lmdb` PUBLIC,
   as it links `PkgConfig::LMDB` today. Hiding LMDB is task 25bfc73a.
4. bench/run.sh: the line of packages drops liblmdb0. A new line records
   `MDB_VERSION_STRING` from deps/lmdb/libraries/liblmdb/lmdb.h.
5. README.md: the LMDB row names the submodule `deps/lmdb` at
   LMDB_1.0.2 and the OpenLDAP Public License; `liblmdb-dev` and
   `pkg-config` leave the install line where nothing else needs them.
6. No mdb_dump or mdb_load target. They belong to the converter task
   aec7919a.

## 3. Checks

- Build in the image localhost/cxx-matrix, g++ 16, CMake 4.3: Debug with
  ASan and UBSan, and Release. All tests green. LMDB is now built with
  the sanitizers too.
- `ldd` of a test binary shows no liblmdb.so.
- CMakeCache.txt has no `LMDB_` entry of pkg-config.
- The compile command of mdb.c is read from ninja, with its flags.

## 4. Not in this change

- No use of the new features of 1.0 (page checksums, encryption,
  incremental backup, page size).
- No handshake field for the data version.
- No change to deps/crc32c, deps/libhydrogen or deps/mbedded-cbor.
