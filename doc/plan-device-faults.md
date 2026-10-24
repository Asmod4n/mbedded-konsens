# Plan: a failed fsync and a failed read on a real file system

Source: the owner, 2026-10-24. Card "Echtes Gerät": a failed fsync and a
failed read are tested on a real device, not with a wrapper of the
storage. Card "FUSE": this VM has no device mapper, so a FUSE file system
is the device under LMDB. This plan gets one review before the build.

## 1. Facts checked in this session

- This VM has no CONFIG_BLK_DEV_DM. CONFIG_FUSE_FS=y and /dev/fuse
  exist.
- Ubuntu in the image localhost/cxx-matrix has libfuse3-dev and fuse3,
  version 3.18.3-1. No fault file system is a package of the
  distribution.
- A probe outside the tree, g++/gcc 16, LMDB 1.0.2, libfuse 3.18.3: a
  passthrough file system of the high-level API in its own process,
  mounted with `fuse_main` without `-f`, under podman with
  `--device /dev/fuse --cap-add SYS_ADMIN`:
  - 100 commits on a clean mount pass.
  - A mount whose fsync returns EIO: mdb_txn_commit returns EIO.
  - A mount whose reads from byte 8192 on return EIO: mdb_env_open in a
    child passes, and the read of the entries ends the child with
    SIGBUS.
- A first probe that ran the FUSE server as a thread in the process that
  maps the file hung at the first fault of a page. The processes stay in
  state D. The server must run in a process of its own.
- fuse_main without `-f` returns in the parent only after the mount is
  done (fuse_daemonize). waitpid of the parent therefore waits for the
  mount, with no loop.
- persistent_storage throws sync_error when mdb_txn_commit fails
  (src/persistent_storage.cpp:131). LMDB writes the meta page only after
  the sync of the data pages, so a failed sync leaves the last committed
  state on the disk.

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
   gets the path of error_fs as a compile definition, as
   test_lmdb_upgrade does.
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
  covers a failed write of LMDB.

## 5. Provisional names

`error_fs`, `error_fsync`, `error_reads_from`, `mounted_error_fs`,
`test_device_faults`, `KONSENS_FUSE_TESTS`.
