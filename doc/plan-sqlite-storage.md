# Plan: SQLite replaces LMDB

Source: the owner, 2026-10-26 17:20. The task is
Claude-state `tasks/2ba55cf6-5a97-424a-ad89-8298662983c0.org` on the
branch `tasks/org-migration` (bc335a1).

1. SQLite holds all persistent data: the log, the meta record, the
   state, the client table, `auth`, the node storage, the backups and
   the archives. No LMDB stays. A mixed form is not taken.
2. The reason of the owner: LMDB has no recovery for a damaged
   database. `mdb_dump` stops at the first damaged data.
3. Each statement is prepared once when the store opens. A step only
   binds values and runs the statements (owner, 17:24). What this
   gains is measured, not estimated.

This file is in two parts. Part A lists what LMDB does for the library
now and which safety rule each use carries. Part B is the proposed
form with SQLite. The open points of section 9 go to the owner before
the model and the code.

Order of the work: this plan, a review by a second agent, the
decisions of the owner, the model where the order of sync and send
changes, then the code.

## 1. Facts checked in this session

The code is at c3645c4 (branch `one-environment`). SQLite is at
`version-3.53.4` (b09c88c), the version of the package `libsqlite3-dev`
of Ubuntu devel, read in a checkout outside the tree.

- The package `libsqlite3-dev` 3.53.4 gives `libsqlite3.so` and
  `libsqlite3.a`. Its compile options include `ENABLE_DBPAGE_VTAB`,
  `SECURE_DELETE`, `DEFAULT_WAL_SYNCHRONOUS=2`,
  `DEFAULT_WAL_AUTOCHECKPOINT=1000` and `MAX_LENGTH=1000000000`
  (`pragma compile_options` in a container of Ubuntu devel).
- The package `sqlite3` 3.53.4 has the command `.recover`. The header
  `sqlite3.h` of `libsqlite3-dev` has no `sqlite3_recover` interface
  (`grep -c` gives 0). The interface is in `ext/recover` of the source
  and in no package.
- `synchronous=OFF` sets no sync flag for the commit and none for the
  checkpoint (`pager.c:3662-3693`). `NORMAL` syncs only in a
  checkpoint. `FULL` also syncs the WAL at each commit
  (`wal.h:25-26`, `wal.c:4195-4215`).
- A checkpoint syncs the WAL before it copies pages into the database
  file, and syncs the database file after it (`wal.c:2277`,
  `wal.c:2333`).
- The recovery of a WAL stops at the first frame whose checksum or salt
  does not match (`wal.c:1510-1511`). A torn tail of the WAL is lost,
  and the frames before it stay.
- With `FULL`, the last frame of a commit is repeated up to the next
  sector boundary when the file system does not give "powersafe
  overwrite" (`wal.c:4185-4215`). With `NORMAL` there is no such
  padding.
- `SQLITE_FCNTL_JOURNAL_POINTER` gives the `sqlite3_file` of the WAL
  (`sqlite.h.in:928-932`). The descriptor inside it is not a public
  field.
- `sqlite3_prepare_v3` takes `SQLITE_PREPARE_PERSISTENT`
  (`sqlite.h.in:4484`).
- `SAVEPOINT` (`parse.y:195`), `VACUUM INTO` (`parse.y:1705`) and the
  backup interface `sqlite3_backup_init` (`sqlite.h.in:9546`) exist.
- `ext/misc/cksumvfs.c` writes a checksum of 8 bytes on each page. It is
  in no package.

- No checksum for each row exists. The WAL carries a checksum of two
  32-bit sums for each frame; it finds a torn tail at recovery and does
  not leave the WAL (`wal.c:1510-1511`). `cksumvfs.c` computes the same
  kind of sum for each page (`cksumvfs.c:299-324`).
- `sha3()` is a function of the shell only. The package library
  answers "no such function: sha3" (a C file outside the tree).
- The session extension is in the package library: with
  `SQLITE_ENABLE_SESSION` and `SQLITE_ENABLE_PREUPDATE_HOOK` defined
  before `sqlite3.h`, `sqlite3session_create`, `_attach` and
  `_changeset` work (a C file outside the tree, a changeset of 20 bytes
  for one insert). `sqlite3session_diff` compares a table of two
  databases (`ext/session/sqlite3session.h:446`).

Not checked: the behaviour of `.recover` on the damages of
plan-device-faults.md; the speed of anything in this plan.

## Part A: what LMDB does now

## 2. The uses of LMDB

| use | where | rule it carries |
|---|---|---|
| one writer, `MDB_NOTLS` reader kept | `src/persistent_storage.cpp` | one write transaction for each step (plan-one-environment.md 3.4) |
| `mdb_txn_prepare`, `MDB_NOSYNC`, `mdb_env_get_fd` | `persistent_storage.cpp:1236-1256`, `sequence_paxos.hpp:407,996,1949` | the library makes no sync; the caller syncs and then commits (3.5) |
| child transaction | `begin_nested` | the runs of the work roll back alone (5.2) |
| `MDB_RESERVE` puts with a CRC tail | `put_checked`, `put_session` | CRC32C for each record (section 10 of that plan) |
| `mdb_env_set_checksum` with a module | `src/lmdb_checksum.cpp`, `tools/lmdb_checksum_module.cpp` | a damaged page is found at read time |
| `MDB_MAP_FULL` as a value | `to_error_code`, `commit_or_throw` | a full store is an error value, not a crash |
| a failed commit throws `sync_error` | `commit_or_throw` | a failed sync ends the process (owner 2026-10-21) |
| `mdb_env_copyfd2` | `persistent_storage::copy` | a full copy for repair and a new node |
| a second environment | `node_storage` | the repair count, the proof, the hidden identity |
| `effective_user_id` check | `open` | no start as root |
| submodules `deps/lmdb`, `deps/lmdb-0.9` | `CMakeLists.txt:12-33` | `mdb_dump` and `mdb_load` for the upgrade test |
| `KONSENS_TEST_TMPDIR` | `CMakeLists.txt:157` | tests on tmpfs |

The interface does not name LMDB: the concept `storage`
(`include/konsens/storage.hpp:237-245`) is the boundary. A second
implementation fits behind it.

## 3. Older plans that this plan replaces

plan-hide-lmdb.md, plan-lmdb-converter.md, plan-lmdb-only-source.md,
plan-lmdb-submodule.md, plan-simulation-on-lmdb.md and
plan-state-in-lmdb.md describe the LMDB form. Their rules that do not
name LMDB stay: the CRCs, the start checks, the repair by entries, the
proof of a fault and the root check. plan-one-environment.md stays,
except 3.5 item 3 and 3.6 (the commit with LMDB) and 8.2 item 3 (the
copy with `mdb_env_copyfd2`).

## Part B: the form with SQLite

## 4. Files and tables

1. One database file for the store, in WAL mode. The tables follow
   plan-one-environment.md 3.1: `meta` (the meta record, the format
   version, the done records, the client table, the tombstones) and
   `data` (the state). `auth` is a table of the same file. The number
   of files is open (section 9, Q5).
2. The node storage is a second file (owner card 2026-10-26 10:51, the
   rule is the same for SQLite).
3. Each table is `WITHOUT ROWID` with the key of today as its primary
   key, and stays `STRICT`. Values are `BLOB`. The encoding of a record
   does not change: CBOR and its CRC tail.
4. Each statement is prepared at the start with
   `SQLITE_PREPARE_PERSISTENT` and kept. A step binds and runs it, then
   calls `sqlite3_reset`. The statements are a fixed list in one place.
5. A field whose CBOR value has an exact SQLite form is stored in that
   form (owner, 17:26): an integer from -2^63 to 2^63-1 as `INTEGER`,
   a text string as `TEXT`, a byte string as `BLOB`, a float as a
   float. Each other value stays CBOR with its tag, in a `BLOB`. The
   type of the column decides how a value is read back, so `true` and
   `1` stay apart.
6. Measured with the package 3.53.4 (a small C file outside the tree,
   `sqlite3_bind_double` and `sqlite3_column_double`): a `REAL` column
   gives -0.0 back as +0.0. An `ANY` column keeps the sign. NaN becomes
   `NULL` in both. An integer above 2^63-1 becomes a `REAL` and loses
   bits. So -0.0 needs a column of type `ANY` or CBOR, and NaN and the
   large integers need CBOR (floats.md: -0 stays, NaN is f97e00).
7. The CRC of a record covers its deterministic CBOR encoding (RFC 8949
   4.2), as now. The library encodes the row to compute it.

### 4.1 Virtual tables, relations and types (owner, 17:29)

1. A `STRICT` table and the types of the prepared statements give an
   error value when a value of a wrong type arrives.
2. The state may use relations and joins.
3. A virtual table may give data of another form (a CSV file, for
   example) and logic to the state. Its code stands in the binary or in
   the library, never in the log. A virtual table that the state reads
   gives the same rows on each node for the same state, because
   Execute-Verify compares the results. A clock, a file outside the
   store or the network as its source breaks this; such a table is not
   readable from a task.

## 5. The transaction of a step

1. A step is `BEGIN IMMEDIATE` ... `COMMIT` on one connection. The
   runs of the work use `SAVEPOINT` and `ROLLBACK TO` in place of the
   child transaction.
2. One connection writes. A second connection reads, as the kept LMDB
   reader does now. Both open in the same thread of the caller.

## 6. The sync, outside the library

1. Proposal: `journal_mode=WAL`, `synchronous=NORMAL`,
   `wal_autocheckpoint=0`. A commit writes its frames to the WAL and
   makes no sync (section 1).
2. The group "sync" of plan-one-environment.md 3.5 is one
   `fdatasync` of the WAL. The caller makes it, as now with io_uring.
   The WAL replaces the data file of LMDB there. The two forms Q1 and
   Q2 fall away: a commit of SQLite has no separate meta page, so one
   sync covers the whole commit.
3. The descriptor: the library opens the WAL file a second time,
   read-only, and gives this descriptor out. A sync through it flushes
   the same file. Section 9 Q3 asks about this.
4. A difference to LMDB: a commit is visible to the reader at once,
   before the sync. LMDB kept it invisible until the commit after the
   sync. The library sends nothing that depends on a commit before the
   caller reports the sync, as now. A read of the state reads only
   decided entries, which a majority holds. The model checks this.
5. The checkpoint is a step of its own: `PRAGMA wal_checkpoint` in the
   form that section 9 Q4 decides. It syncs the WAL and the database
   file by itself (section 1). The library starts it; the binary may
   run it in idle time.
6. A failed sync throws `sync_error`, and nothing repeats it, as now.
   A failed checkpoint throws `sync_error` too.
7. `SQLITE_FULL` and `SQLITE_IOERR` with `ENOSPC` are the "full" error
   value, as `MDB_MAP_FULL` now. The limit X of free space stays a
   field, off by default.

## 7. Damage, copy and recovery

1. The CRC of each record stays. It finds a damage in a value. A
   damage in a page of the B-tree gives `SQLITE_CORRUPT`, which is a
   damage as `MDB_BAD_CHECKSUM` now.
2. A checksum on each page needs `cksumvfs.c`, which is in no package
   (section 9 Q6).
3. A full copy for repair and for a new node is `VACUUM INTO` a new
   file, or the backup interface. Both read one snapshot. The receiver
   checks the copy as now (plan-one-environment.md 8.2 item 3).
4. A backup and an archive are the same copy. The hidden identity of
   the node storage is not in it, because the node storage is another
   file (section 9 Q1).
5. A damaged store: the operator runs `sqlite3 store .recover` on a
   clone (owner 2026-10-17 20:53: work only on a clone). The node then
   takes the repair by entries from its peers. The library does not
   call `.recover` itself (section 9 Q7).

6. The CRC32C of each record stays, on the disk and on the wire,
   because SQLite has no checksum for each row. The hash in idle time
   stays ours too.
7. Proposal: a repair after a snapshot takes a changeset of the session
   extension in place of single entries. The changeset carries no
   checksum; each record in it keeps its CRC tail, and the receiver
   checks each one.

## 8. Build and tests

1. SQLite comes from the package of the distribution (`libsqlite3-dev`).
   The submodules `deps/lmdb` and `deps/lmdb-0.9`, the checksum module
   and `test_lmdb_upgrade` go.
2. The interface tests of the concept `storage` run on the new store
   unchanged. The tests that name LMDB (`lmdb_cluster.hpp`,
   `lmdb_damage.hpp`, `lmdb_storage.hpp`) get SQLite forms.
3. New tests: a power cut between commit and sync loses the commit
   and keeps the store whole (fuse, as `test/fuse` now); a power cut in
   a checkpoint; a full disk; a failed `fdatasync` of the WAL; a damaged
   page gives `SQLITE_CORRUPT` and no crash; `.recover` on a damaged
   clone gives back each record whose CRC holds.
4. Tests keep `KONSENS_TEST_TMPDIR`.

## 9. Open points for the owner

- Q1. The hidden identity in a backup. The task leaves it open. This
  plan keeps it out of the backup, because it is in the node storage
  file. A restored node then has a new identity.
- Q2. How a restored node and an old node never use one identity.
- Q3 and Q4. Decided (owner card 2026-10-26 17:31): measure first. Arm
  "SQLite syncs": `synchronous=FULL` and the automatic checkpoint. Arm
  "caller syncs": section 6. The simple arm is built first. The form
  of section 6 stays a proposal until the measurement.
- Q5. Decided (owner card 2026-10-26 17:30): one file with all tables.
  The node storage stays a second file.
- Q6. A checksum on each page: `cksumvfs.c` as a submodule from its
  origin, or no page checksum (the CRC of each record stays).
- Q7. Decided (owner card 2026-10-26 17:30): the `.recover` of the
  package `sqlite3`, on a clone. The binary does not hold
  `sqlite3_recover`.
- Q8. The lowest SQLite version is the one that the distribution gives. This is not a question for the owner.
- Q9. Without the padding of `FULL`, a later write in the same sector
  can tear a frame that a sync already made durable, where the device
  has no powersafe overwrite (section 1). Proposal: test it with the
  power cut of section 8, and decide after.

## 10. What is measured

Both arms as bench/run.sh runs them: the LMDB store at fa14f44 and the
SQLite store, the same durability (one `fdatasync` of the store for
each step), stores on ext4. The cases of plan-one-environment.md
section 15, the batch sizes 1, 16 and 1024, and the cost of one
checkpoint. No number stands here before the run.
