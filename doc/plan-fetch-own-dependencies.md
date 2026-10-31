# Plan: the libraries of the owner come through FetchContent

Branch `fetch-own-dependencies`, from linear-selection 2a1d12b.
Owner, 2026-10-31 13:15: the libraries of the owner leave `deps/` as
submodules. CMake fetches them. Owner 13:16 and 13:17: the pins stand
in one file, like a lockfile.

## 1. Facts checked in this session

- Three submodules come from the owner: `deps/mbedded-cbor` (7c245f3),
  `deps/slipstreamIO` (50fea67) and `deps/mruby-webmachine` (8c76f51,
  `branch = integration` in `.gitmodules`).
- `deps/crc32c` and `deps/libhydrogen` come from others. RULES.md names
  them as exceptions that come as submodules. They stay.
- mbedded-cbor has a CMakeLists.txt. It defines the target
  `mbedded-cbor` and the alias `mbedded-cbor::mbedded-cbor`
  (CMakeLists.txt:8-9 at 9126904). With `CBOR_INSTALL` it installs a
  CMake package `mbedded-cbor-config.cmake` with the namespace
  `mbedded-cbor::` (lines 29-39).
- slipstreamIO and mruby-webmachine have no CMakeLists.txt. konsens
  reads their sources: CMakeLists.txt:139-157 builds liburing from
  `deps/slipstreamIO/deps/liburing` and the library `slipstream`;
  line 165 reads the headers of `deps/mruby-webmachine/src`. Both are
  used only with `KONSENS_PROGRAM=ON`.
- CMake 3.28, ExternalProject.cmake:299-310: "specifying a commit hash
  should be preferred". A branch name gives no repeatable build.
- FetchContent.cmake:185-196 (CMake 3.24 and later):
  `FIND_PACKAGE_ARGS` makes `FetchContent_MakeAvailable` call
  `find_package` first. konsens asks for CMake 3.25.
- `GIT_SUBMODULES_RECURSE` is on by default since CMake 3.17. A
  probe project in the scratchpad fetched mbedded-cbor at 9126904 and
  slipstreamIO at 50fea67 by hash. slipstreamIO came with its
  submodule `deps/liburing` (liburing-2.13). A file that includes
  `<cbor/structs.hpp>` built with `-Wall -Wextra -Wshadow -Werror` and
  g++ 16. `SOURCE_SUBDIR` with a directory that does not exist fetches
  a tree with no CMakeLists.txt and adds nothing.
- The workflow `deps-upstream` watches submodules only. It no longer
  sees the three libraries after this change.
- containers/konsensd/Containerfile copies the tree and runs CMake. It
  installs no git.

## 2. Change

1. New file `cmake/dependencies.cmake`. It holds one
   `FetchContent_Declare` for each library, with `GIT_REPOSITORY` and
   `GIT_TAG` set to the full commit hash, and nothing else:

       FetchContent_Declare(mbedded-cbor
           GIT_REPOSITORY https://github.com/Asmod4n/mbedded-cbor
           GIT_TAG 9126904f11c51cd791d7ef80b76ba982fc66caa1
           SYSTEM
           FIND_PACKAGE_ARGS CONFIG)
       FetchContent_Declare(slipstreamio
           GIT_REPOSITORY https://github.com/Asmod4n/slipstreamIO
           GIT_TAG 50fea675302c1029f6036743a9c9c6a6d9cbc659
           SOURCE_SUBDIR none)
       FetchContent_Declare(mruby-webmachine
           GIT_REPOSITORY https://github.com/Asmod4n/mruby-webmachine
           GIT_TAG 8c76f5166244caeaf85f08859bd2abf572747e7e
           SOURCE_SUBDIR none)

   mbedded-cbor moves to 9126904, the head of its `main` today.
   slipstreamIO and mruby-webmachine keep their pins.
2. CMakeLists.txt includes FetchContent and the file.
   `FetchContent_MakeAvailable(mbedded-cbor)` replaces
   `add_subdirectory(deps/mbedded-cbor EXCLUDE_FROM_ALL)`, with
   `CBOR_INSTALL OFF` as before. Every use of the target `mbedded-cbor`
   becomes `mbedded-cbor::mbedded-cbor`, because the installed package
   has only that name. `SYSTEM` in the declaration gives the headers
   `-isystem`; line 55 stays as it is.
3. Inside `if(KONSENS_PROGRAM)`:
   `FetchContent_MakeAvailable(slipstreamio mruby-webmachine)`.
   `KONSENS_SLIPSTREAM` becomes `${slipstreamio_SOURCE_DIR}`, and line
   165 reads `${mruby-webmachine_SOURCE_DIR}/src`.
4. `.gitmodules` loses the three entries. `git rm` removes the three
   gitlinks.
5. README.md: the table of requirements names
   `cmake/dependencies.cmake` for mbedded-cbor. The section
   Installation says that the configure step fetches it, that
   `-DFETCHCONTENT_SOURCE_DIR_MBEDDED-CBOR=<dir>` takes a local tree,
   and that an installed mbedded-cbor is used first.
   `git submodule update --init --recursive` stays for crc32c and
   libhydrogen.
6. Containerfile: `git` and `ca-certificates` join the packages of the
   build stage.

## 3. Checks

- Release, g++ 16, a new build directory: configure, build, ctest.
  Expected as on linear-selection here: every test passes except the
  `.recover` case, which the sqlite3 shell of Ubuntu cannot run.
- The same with `KONSENS_PROGRAM=ON`, if toml++ and ada are packages
  of this machine; otherwise this check is marked as not run.
- mbedded-cbor installed into a prefix in the scratchpad, then
  konsens configured with `CMAKE_PREFIX_PATH` on it: the configure log
  says that the package was found, and the build passes.
- `-DFETCHCONTENT_SOURCE_DIR_MBEDDED-CBOR=/home/user/mbedded-cbor`:
  the configure log names that directory.
- `git grep -n "deps/mbedded-cbor\|deps/slipstreamIO\|deps/mruby-webmachine"`
  finds nothing outside doc/ history files.

## 4. Not in this change

- No watch of the three pins. A bump is one edit of
  `cmake/dependencies.cmake`.
- crc32c and libhydrogen stay submodules.
- No change to the code of konsens.

## 5. After the merge

Issue #2 is closed with a link to the commit: mbedded-cbor is no
longer a submodule. The owner can delete the branch
`mbedded-cbor-head`, `cbor-includes-system` and
`counter-stress-warning`.
