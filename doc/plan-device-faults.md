# Plan: a failed fsync and a failed read on a real file system

Source: the owner, 2026-10-24. Card "Echtes Gerät": a failed fsync and a
failed read are tested on a real device, not with a wrapper of the
storage. Card "FUSE": this VM has no device mapper, so a FUSE file system
is the device under the storage. This plan gets one review before the build.

## 1. Facts checked in this session

- This VM has no CONFIG_BLK_DEV_DM. CONFIG_FUSE_FS=y and /dev/fuse
  exist.
- Ubuntu in the image localhost/cxx-matrix has libfuse3-dev and fuse3,
  version 3.18.3-1. No fault file system is a package of the
  distribution.
- A first probe that ran the FUSE server as a thread in the process that
  maps the file hung at the first fault of a page. The processes stay in
  state D. The server must run in a process of its own.
- fuse_main without `-f` returns in the parent only after the mount is
  done (fuse_daemonize). waitpid of the parent therefore waits for the
  mount, with no loop.

The former form of this section is in the git history at ebd9f42.

## 2. Change

1. A program `test/fuse/error_fs.cpp`, a passthrough file system of the
   high-level API of libfuse 3. It serves a backing directory and takes
   two faults as mount options:
   - `error_fsync`: each fsync and fdatasync returns EIO.
   - `error_reads_from=<offset>`: each read that reaches the offset
     returns EIO.
   The name follows the target `error` of the device mapper, which
   fails I/O on a range.
2. A fixture `test/fuse/mounted_error_fs.hpp`: the constructor starts
   error_fs with posix_spawn and waits for it with waitpid; the
   destructor runs `fusermount3 -u`. A failed mount fails the test.
3. A test `test/fuse/test_device_faults.cpp`:
   - "a failed fsync at commit throws sync_error and keeps the last
     commit": entries are written on a clean mount, the mount is
     replaced by one with error_fsync, write_atomically throws
     sync_error with io_error, and a clean mount shows the log of
     before.
   - "a failed read of an entry ends the process with SIGBUS": one
     small entry, then one large entry as the last write, on a clean
     mount. The new mount fails reads from the start of the overflow
     pages of the large entry. A child process opens the storage, writes
     one byte to a pipe, and reads the large entry. The parent reads the
     byte and requires that the child ended with SIGBUS.
4. CMakeLists.txt: an option `KONSENS_FUSE_TESTS`, off by default. When
   it is on, pkg-config must find fuse3, else the configure fails. The
   option builds error_fs and the test, and registers the test. The test
   gets the path of error_fs as a compile definition.
5. README, part Testing: the packages libfuse3-dev and fuse3, the
   option, and that the test needs /dev/fuse and the right to mount.
   In podman: `--device /dev/fuse --cap-add SYS_ADMIN`.

## 3. Checks

- g++ 16, image localhost/cxx-matrix with libfuse3-dev and fuse3,
  KONSENS_FUSE_TESTS=ON, Release: all tests green. ASan once before the
  merge.
- Mutation, run once: error_fs without its fault. Both tests fail.
- Each podman run with a FUSE mount runs under `timeout`, so that a
  hung mount ends with its container.

## 4. Not in this change

- No change to the library.
- No lost write and no torn write. The simulation covers a lost write
  with its rollback.
- No test for a failed write(2): the existing test with RLIMIT_FSIZE
  covers a failed write of the storage.

## 5. Provisional names

`error_fs`, `error_fsync`, `error_reads_from`, `mounted_error_fs`,
`test_device_faults`, `KONSENS_FUSE_TESTS`.

## 6. Review (second Claude agent, 2026-10-24) and changes

Result: changes needed. The build takes these changes:

1. The read fault covers a range, as a line "start length error" of a
   device-mapper table: `error_reads_offset` and `error_reads_length`,
   and only for the data file of the store.
2. The test finds the range: the large entry carries a pattern; after
   the unmount the test finds the pattern in the backing data file. The
   range is the whole pages inside the payload.
3. The kernel can read ahead past the range. The test runs at the real
   offsets of a persistent_storage file, so the build shows whether a
   page outside the range stays readable.
4. error_fs runs with `-f`, so its stderr stays and the test can reap
   it. Its init callback writes one byte to a pipe that the fixture
   gives it; the fixture blocks on the read of that byte.
5. The child of the SIGBUS test sets SIGBUS to SIG_DFL (ASan has its own
   handler), sets RLIMIT_CORE to 0 and ends with _exit.
6. A second mutation, outside the tree and run once: a store that
   ignores the failed sync of its commit. The check "a clean mount shows the
   log of before" must then fail.
7. error_fs and the test get KONSENS_WARNINGS. fuse3 comes from
   pkg_check_modules with IMPORTED_TARGET, so its headers are system
   headers. FUSE_USE_VERSION is a compile definition of error_fs.
8. The test closes each storage before the unmount, and the unmount is
   a call of the test that REQUIREs exit status 0. The destructor of the
   fixture unmounts only a mount that is still there. The path of
   fusermount3 comes from find_program.
9. Removed.
10. The test gets a ctest TIMEOUT.

The former form of this section is in the git history at ebd9f42.

## 7. Result

- g++ 16, image localhost/cxx-matrix with libfuse3-dev and fuse3,
  KONSENS_FUSE_TESTS=ON, KONSENS_TEST_TMPDIR=/dev/shm, podman with
  `--device /dev/fuse --cap-add SYS_ADMIN`: 20 of 20 tests green in
  Release and in Debug with
  `-O1 -fno-omit-frame-pointer -fsanitize=address,undefined`.
  test_device_faults takes 0.04 s in Release and 0.10 s with ASan.
- The read fault covers only the whole pages inside the payload of the
  large entry. open passes, and the read of the entry ends the child
  with SIGBUS.
- Mutation, run once: error_fs without its faults. Both tests fail: no
  sync_error, and the child does not end by a signal.
The former form of this section is in the git history at ebd9f42.
