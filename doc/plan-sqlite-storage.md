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

Part A lists what LMDB does for the library now and which rule each
use carries. Part B is the form with SQLite. Section 9 lists the
decisions of the owner and the open points. Section 11 lists the
findings of review 1 and what changed.

Order of the work: this plan, review 1 (done), the decisions of the
owner, the model of section 10.1, then the code.

Marks: "card open" marks a provisional form that waits on a card of
the owner.

## 1. Facts checked in this session

The code is at c3645c4 (branch `one-environment`). SQLite is at
`version-3.53.4` (b09c88c), the version of the package `libsqlite3-dev`
of Ubuntu devel, read in a checkout outside the tree. "A C file outside
the tree" means a small test program against the package 3.53.4.

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
  (`wal.h:25-26`, `wal.c:4195-4215`). The setting holds for one
  connection (`pragma.c:1143`, `btree.c:3255`).
- A checkpoint syncs the WAL before it copies pages into the database
  file (`wal.c:2277`). It syncs the database file only when it reached
  the end of the WAL (`wal.c:2328-2338`). The WAL restarts only after
  such a full checkpoint.
- The default WAL hook calls the checkpoint and returns `SQLITE_OK`
  whatever its result (`main.c:2484-2496`). After a failed sync of the
  database file, the next checkpoint copies and syncs again
  (`wal.c:2333-2338`).
- A commit with `FULL` syncs the WAL before its frames enter the WAL
  index and its header (`wal.c:4212`, `wal.c:4238-4259`). A reader sees
  the commit only after the sync. With `NORMAL` the frames enter the
  index without a sync.
- The recovery of a WAL stops at the first frame whose checksum or salt
  does not match (`wal.c:1510-1511`), wherever it stands. A damaged
  frame in the middle drops that commit and every later one, and SQLite
  reports no error.
- The last frame of a commit is repeated up to the next sector boundary
  only when the file has no "powersafe overwrite" (`wal.c:1721`,
  `wal.c:1738-1740`, `wal.c:4185-4215`). The unix VFS sets that flag
  unless the URI says `psow=0` (`os_unix.c:4367-4370`,
  `os_unix.c:6104-6107`). So on Linux the default pads nothing, with
  `FULL` too.
- The unix VFS maps `ENOSPC` of a write to `SQLITE_FULL`
  (`os_unix.c:3710-3716`). A failed `fsync` gives `SQLITE_IOERR_FSYNC`
  for every errno (`os_unix.c:3930-3934`).
- `SQLITE_FULL` and `SQLITE_IOERR` in a statement roll back the whole
  transaction, close all savepoints and turn autocommit on
  (`vdbeaux.c:3355-3386`). A C file outside the tree: insert 1,
  `SAVEPOINT`, an insert fails with `SQLITE_FULL`, insert 3 then
  commits alone, `ROLLBACK TO` and `COMMIT` fail. Insert 1 is lost,
  insert 3 is durable.
- The pointer of `sqlite3_column_blob` is valid until the next step,
  reset or finalize of its statement. A C file outside the tree: two
  calls of one persistent statement gave the same address for two
  different values.
- `sqlite3_prepare_v3` takes `SQLITE_PREPARE_PERSISTENT`
  (`sqlite.h.in:4484`) and `SQLITE_PREPARE_NO_VTAB`
  (`sqlite.h.in:4486`).
- `SAVEPOINT` (`parse.y:195`), `VACUUM INTO` (`parse.y:1705`) and the
  backup interface `sqlite3_backup_init` (`sqlite.h.in:9749`) exist.
  `VACUUM INTO` refuses to run inside a transaction
  (`vacuum.c:169-172`). SQLite syncs a directory only for a new journal
  or WAL (`os_unix.c:6569-6573`, `:6777`).
- No checksum for each row exists. The WAL carries a checksum of two
  32-bit sums for each frame. `ext/misc/cksumvfs.c` computes the same
  kind of sum for each page (`cksumvfs.c:299-324`). It is in no package.
- `sha3()` is a function of the shell only. The package library
  answers "no such function: sha3" (a C file outside the tree).
- The session extension is in the package library: with
  `SQLITE_ENABLE_SESSION` and `SQLITE_ENABLE_PREUPDATE_HOOK` defined
  before `sqlite3.h`, `sqlite3session_create`, `_attach` and
  `_changeset` work (a C file outside the tree). `sqlite3session_diff`
  compares a table of two databases that one connection holds
  (`ext/session/sqlite3session.h:446`). A session ignores a table
  without an explicit primary key (`sqlite3session.h:192-200`).
- Value types, a C file outside the tree, `sqlite3_bind_double` and
  `sqlite3_column_double`: a `REAL` column gives -0.0 back as +0.0. An
  `ANY` column keeps the sign. NaN becomes `NULL` in both. A literal
  above 2^63-1 becomes `REAL` and loses bits; `sqlite3_bind_int64`
  takes a signed value, so a C++ value above 2^63-1 is negative before
  SQLite sees it.

Not checked: the behaviour of `.recover` on the damages of
plan-device-faults.md; the compile option `POWERSAFE_OVERWRITE` of the
package; whether a change inside a savepoint that is rolled back
appears in a changeset; a writable shared `mmap` of the `-shm` file on
`error_fs`.

## Part A: what LMDB does now

## 2. The uses of LMDB

| use | where | rule it carries |
|---|---|---|
| one writer, `MDB_NOTLS` reader kept | `src/persistent_storage.cpp` | one write transaction for each step (plan-one-environment.md 3.4) |
| `mdb_txn_prepare`, `MDB_NOSYNC`, `mdb_env_get_fd` | `persistent_storage.cpp:1236-1256`, `sequence_paxos.hpp:407,996,1949` | the library makes no sync; the caller syncs and then commits (3.5) |
| child transaction | `begin_nested` | the runs of the work roll back alone (5.2) |
| `throw_if_nested_is_open` | `persistent_storage.cpp:670-675` | the parent takes no operation while its child is open |
| `MDB_RESERVE` puts with a CRC tail | `put_checked`, `put_session` | CRC32C for each record (section 10 of that plan) |
| `MDB_APPEND` | `persistent_storage.cpp:557` | an index below the last one is refused |
| span into the map from `find` | `find_value`, `persistent_storage.cpp:725-738` | a value is read without a copy |
| `mdb_env_set_checksum` with a module | `src/lmdb_checksum.cpp`, `tools/lmdb_checksum_module.cpp` | a damaged page is found at read time |
| `MDB_MAP_FULL` as a value | `to_error_code`, `commit_or_throw` | a full store is an error value, not a crash |
| `map_size` | `persistent_storage.hpp:156`, `examples/counter.cpp:83`, `examples/three_nodes.cpp:78`, `examples/counter_stress.cpp:104` | the bound of the store |
| a failed commit throws `sync_error` | `commit_or_throw` | a failed sync ends the process (owner 2026-10-21) |
| `mdb_env_copy2` | `persistent_storage.cpp:1498` | a full copy for repair and a new node |
| a second environment | `node_storage` | the repair count, the proof, the hidden identity |
| file mode 0600 | `persistent_storage.cpp:29` | only the user reads the store and the hidden identity |
| format version, empty-store check | `persistent_storage.cpp:39`, `:694-722` | `storage_errc::unsupported_format` |
| destructor with an open transaction ends the process | `persistent_storage.cpp:1367-1372` | a wrong use fails loudly |
| `effective_user_id` check | `open` | no start as root |
| `lmdb_category` | `src/lmdb_category.hpp`, `bench/konsens_bench.cpp:41,955` | LMDB codes as `std::error_code` |
| submodules `deps/lmdb`, `deps/lmdb-0.9`, `mdb_dump`, `mdb_load` | `CMakeLists.txt:12-33`, `test/lsan-mdb_dump-0.9.txt` | the upgrade test |
| faults for `data.mdb` | `test/fuse/error_fs.cpp:17-28`, `:59` | the fault tests |
| `KONSENS_TEST_TMPDIR` | `CMakeLists.txt:157` | tests on tmpfs |

The interface does not name LMDB: the concept `storage`
(`include/konsens/storage.hpp:237-245`) is the boundary. The concept
changes in three places: `find` (section 5.3), the sync (section 6.2),
and `map_size` (section 6.6).

## 3. Older plans that this plan replaces

plan-hide-lmdb.md, plan-lmdb-converter.md, plan-lmdb-only-source.md,
plan-lmdb-submodule.md, plan-simulation-on-lmdb.md and
plan-state-in-lmdb.md describe the LMDB form. Their rules that do not
name LMDB stay: the CRCs, the start checks, the repair by entries, the
proof of a fault and the root check.

plan-one-environment.md stays, except these items. Each is replaced by
the section named.

| item of plan-one-environment.md | replaced by |
|---|---|
| 3.4 item 2, "sync only" | gone: no descriptor; an idle node has nothing to sync |
| 3.5 items 1, 3, 4 and 6 (io_uring, `mdb_txn_prepare`, the writer lock through the sync, slipstreamIO) | section 6 |
| 3.5 item 2, "done" while the leader syncs (owner decision 18 item 4) | section 6.3, card open |
| 3.6, the count of syncs | section 6.4 |
| 4.3 item 1, the decided index without a sync | each commit syncs (section 6.1) |
| 5.1 items 2 to 4 and 9 item 6 (the tombstone by file system, `MDB_NOSYNC`) | the tombstone rides in the commit of its step, which syncs |
| 8.2 item 3, the copy with `mdb_env_copy2` | section 7.3 |
| 15 items 6 to 8 | section 10 |
| owner decisions 18 items 5 and 6 | section 6, card open |

## Part B: the form with SQLite

## 4. Files and tables

1. One database file for the store, in WAL mode (owner card
   2026-10-26 17:30). The tables follow plan-one-environment.md 3.1:
   `meta` (the meta record, the format version, the done records, the
   client table, the tombstones), `data` (the state) and `auth`.
2. The node storage is a second file (owner card 2026-10-26 10:51).
3. The library creates each database file with mode 0600 before
   `sqlite3_open_v2`. The WAL and the `-shm` file take the mode of the
   database file.
4. Each table is `STRICT`. The key of the log and of the client table
   is an `INTEGER` (card open, section 4.2). Whether the log is a
   `WITHOUT ROWID` table or a rowid table with `INTEGER PRIMARY KEY` is
   an arm of the measurement (section 10); the log rows are large.
5. Each statement is prepared at the start with
   `SQLITE_PREPARE_PERSISTENT` and kept. A step binds and runs it, then
   calls `sqlite3_reset`. The statements are a fixed list in one place.
   The store opens with extended result codes
   (`SQLITE_OPEN_EXRESCODE`).
6. The format version stays in `meta`. A new store must be empty; a
   store with another version gives `storage_errc::unsupported_format`.
   SQLite has a field for this in its header (`PRAGMA user_version`);
   the record in `meta` keeps its CRC.

### 4.1 The columns and the CRC (card open)

1. Today a log entry is a kind byte and little-endian fields
   (`storage.cpp:78-97`), not CBOR. The meta record, the session record
   and the repair record are fixed little-endian forms with a CRC
   (`persistent_storage.cpp:235-310`, `:405-418`, `:762-805`). The tail
   of a log record holds the entry CRC, the chain CRC, the state and
   the state CRC (`persistent_storage.cpp:47-53`). The entry CRC
   travels on the wire.
2. Proposal: each field gets a native column (owner, 17:26). The
   entry CRC and the chain CRC stay columns and keep covering the
   current binary form, which the library builds to compute them. The
   wire does not change, and `format_version` goes to 5 only for the
   file. The other form, a new deterministic CBOR encoding of an entry,
   changes every CRC on the wire.
3. A field of the state whose value has an exact SQLite form takes that
   form: an integer from -2^63 to 2^63-1 as `INTEGER`, a text string as
   `TEXT`, a byte string as `BLOB`, a float as `ANY` (it keeps -0.0).
   NaN and the integers above 2^63-1 stay CBOR with their tag, in a
   `BLOB` (floats.md: -0 stays, NaN is f97e00).
4. A `STRICT` column checks the type and converts a value that it can
   convert without loss (text `'12'` into `INTEGER`). A prepared
   statement has no parameter types. An `ANY` column checks nothing;
   there the storage class of the value keeps `true` and `1` apart.
   So the library binds each parameter with the bind call of its C++
   type, and a column of a fixed type is `INTEGER`, `TEXT`, `BLOB` or
   `REAL`, never `ANY`, except for floats.

### 4.2 Integer keys (card open)

The log index, the session id, `client_id` and `pid` are `uint64_t`.
Proposal: one check for safety, with one named bound (2^63-1), at each
place where an index or an id enters the library. A value above it is
an error value. The key column stays `INTEGER`. The other form keeps a
big-endian `BLOB` key, as now (`persistent_storage.cpp:137-142`).
The insert of the log checks that the index is the next one, as
`MDB_APPEND` did.

### 4.2.1 Points from the thread on SQLite for clients (to weigh)

These points come from another thread. They are open until the
measurement of blob against columns (section 10).

1. The state column of an entry and its state CRC repeat
   `idx < decided` of the meta record, because both stand in one
   commit. If the column stays, `set_entry_states` needs a CRC function
   in SQL or a loop in C++.
2. With one CRC over the whole meta record, each setter reads, changes
   and writes the whole record. A
   `CHECK(applied<=decided AND decided<=log_len)` works with separate
   columns.
3. `holds_the_request` and `find_the_verified_idx` become one statement
   each when kind, session and request number are columns of their own.

### 4.3 Virtual tables, relations and types (owner, 17:29)

1. A `STRICT` table gives an error value when a value of a wrong type
   arrives (section 4.1 item 4).
2. The state may use relations and joins.
3. A virtual table may give data of another form (a CSV file, for
   example) and logic to the state. Its code stands in the binary or in
   the library, never in the log.
4. Execute-Verify compares the results of the nodes. So the same state
   must give the same result on each node. The owner decided on
   2026-10-26 18:22 and 18:34 that the cluster does not check the
   SQLite version (thread "SQLite functions for clients"). The owner
   decided on 2026-10-26 18:36 that a task may use all that SQLite
   offers, FTS5, R*Tree and the aggregates included. A computation on
   floats runs inside SQLite only. Excluded are the access to the
   engine (`ATTACH`, `PRAGMA`, `load_extension`, `sqlite_dbpage`),
   `random()` and `'now'`. Whether the libm functions through SQLite
   (`sin`, `pow`) are allowed is open. `sum(x ORDER BY pk)`
   (`parse.y:1238`) gives the same order of addition on each node. An
   authorizer
   (`sqlite3_set_authorizer`) allows a task only the functions of a
   list, and no `ATTACH`, `PRAGMA` or `ANALYZE`. Each query of a task
   orders its rows totally. A virtual table with a clock, a file
   outside the store or the network as its source is not readable from
   a task; its statements use `SQLITE_PREPARE_NO_VTAB`. Sources of a
   difference: row order without `ORDER BY`, `random()`, the date
   functions with `'now'`, `sum()` and `total()` by version
   (`func.c:1873-1896`), random rowids at the largest rowid
   (`vdbe.c:5672-5692`), `VACUUM` that renumbers rowids
   (`insert.c:3288-3289`).
5. Reads and writes allow the same functions (owner, 2026-10-26 18:34
   and 18:36).
6. A client sends a query as a CBOR structure, never as SQL (owner,
   2026-10-26 18:35). The library prepares the statement of a form at
   its first use and keeps it in a cache by form. The cache has one
   named bound. The rule of 17:24 (prepared at open) holds for the
   statements of the library only.
7. How a task reaches SQL is not in this plan. The `state_machine`
   concept offers `find` of a byte key (`state_machine.hpp:28-31`). A
   plan of its own gives the interface.

## 5. The transactions

### 5.1 The write transaction of a step

1. A step is `BEGIN IMMEDIATE` ... `COMMIT` on the writer connection.
2. The child of a run is `SAVEPOINT x`. Its commit is `RELEASE x`. Its
   abort is `ROLLBACK TO x` and then `RELEASE x`. The guard
   `throw_if_nested_is_open` stays on each method of the parent.
3. After each statement that fails, the store reads
   `sqlite3_get_autocommit`. When it is 1, SQLite has rolled back the
   whole step. The transaction object then enters a closed state: each
   later call returns the error without SQL, and `commit` returns it.
   The child does the same. So no part of a step becomes durable alone.
4. The destructor of the store with an open transaction ends the
   process, as now.

### 5.2 The reader

1. A second connection reads, in the same thread of the caller.
2. `begin_read_only` runs `BEGIN` and one read of the meta record at
   once, so that the snapshot starts there. The end of the reader runs
   `COMMIT`. So each `read_only_transaction` sees one state.
3. A reader that stays open stops the checkpoint at its snapshot. The
   library holds no reader between steps.

### 5.3 `find` (card open)

The pointer of `sqlite3_column_blob` dies at the next step or reset of
its statement (section 1). A span from `find` would point to memory
that is not alive after the next `find`. Proposal: `find` returns a
`std::vector<std::byte>`, a copy that the caller owns. Its cost goes
into the measurement. The other form: `find` calls a function with the
span, and the span does not leave that call.

## 6. The sync

1. Decided (owner card 2026-10-26 17:54): the first version lets
   SQLite sync. `journal_mode=WAL`, `synchronous=FULL`. The library
   sets `synchronous` on each connection that writes, the node storage
   included, reads it back at open and refuses another value. A commit
   returns after the sync of the WAL. A reader sees the commit only
   after the sync (section 1). Section 8.1 shows no difference above
   the noise floor to a sync of the caller that waits.
2. The library syncs inside `commit`, in the thread of the caller. A
   message that waits on a sync leaves after `commit` returns. The
   descriptor, `prepare` and `begin_without_sync` go from the concept
   `storage`. The form "the caller syncs through io_uring" is a later
   option; it is measured in the binary with the sends before it
   enters. In that form a reader sees the commit before the sync
   (section 1).
3. "done" from the leader (card open). Proposal: the leader sends
   "done" before its `commit`, so that its own sync runs while the
   followers write. It counts itself only after its `commit` returns.
   The model of section 10.1 shows that this keeps safety. The other
   form sends "done" after the commit, with one sync more in series
   before the reply.
4. The count of syncs: one sync of the WAL for each commit. A
   checkpoint adds one sync of the WAL and, when it reaches the end of
   the WAL, one sync of the database file.
5. The checkpoint (card open). Proposal: `PRAGMA
   wal_autocheckpoint=0`. The library calls
   `sqlite3_wal_checkpoint_v2` with `SQLITE_CHECKPOINT_PASSIVE` after
   the commit and after the sends of the step, when the WAL holds 1000
   frames or more (the WAL hook gives the count). `SQLITE_IOERR_FSYNC`
   from it throws `sync_error`, and nothing repeats it.
   `SQLITE_FULL` from it is the "full" value. No C++ exception crosses
   a callback that SQLite calls. The sync stays the work of SQLite.
6. The bound of the store: `map_size` goes. `PRAGMA max_page_count`
   gives the bound in pages; the parameter of `open` gives it in bytes,
   and the library converts it. Over the bound, SQLite gives
   `SQLITE_FULL`.
7. Torn sectors (card open). Proposal: each database file opens with
   the URI parameter `psow=0`, so that SQLite pads the last frame of a
   commit to the sector. The cost is measured.

### 6.1 The error codes

The rule "An error goes to whoever can repair it" decides each row.

| SQLite code | konsens |
|---|---|
| `SQLITE_FULL` | the "full" value (`std::errc::no_space_on_device`) |
| `SQLITE_IOERR_FSYNC`, `SQLITE_IOERR_DIR_FSYNC` | `sync_error`, whatever `sqlite3_system_errno` says |
| other `SQLITE_IOERR_*` on a write | `write_error` |
| other `SQLITE_IOERR_*` on a read | `read_error` |
| `SQLITE_CORRUPT*`, `SQLITE_NOTADB` | a damage code that `error_is_a_damage` (`sequence_paxos.hpp:83-88`) lists |
| `SQLITE_TOOBIG` | the value of section 6.2 |
| `SQLITE_BUSY`, `SQLITE_MISUSE`, `SQLITE_CONSTRAINT` on a key that the library computed | `std::logic_error` |

The limit X of free space stays a field, off by default.

### 6.2 The size of a value

The package has `MAX_LENGTH=1000000000`. A value or a row above
`SQLITE_LIMIT_LENGTH` gives `SQLITE_TOOBIG`. The plan names one bound
for a payload and one for a row, derived from `sqlite3_limit` at open,
each one named constant. The library checks a payload once where a
request enters, and gives an error value above the bound.

## 7. Damage, copy and recovery

1. The CRC32C of each record stays, on the disk and on the wire,
   because SQLite has no checksum for each row (section 1). The hash in
   idle time stays ours too (owner, 17:35).
2. A damage in a page of the B-tree gives `SQLITE_CORRUPT`, mapped as
   section 6.1 says.
3. A damaged frame in the middle of the WAL drops that commit and every
   later one without an error (section 1). The store then holds an
   older state that is whole in itself. With LMDB the page checksum
   gave `MDB_BAD_CHECKSUM`. The rule that keeps Paxos safe here is the
   vote after f + 1 answers after each start (plan-one-environment.md
   8.1). Not checked: that it covers a lost promise. The model of
   section 10.1 includes this fault.
4. A checksum on each page needs `cksumvfs.c`, which is in no package
   (section 9).
5. A full copy for repair and for a new node is `VACUUM INTO` a new
   file. It runs on a connection with no open transaction, in the
   thread of the caller. The receiver syncs the file and its directory
   before the switch in the node storage, and checks the copy as now
   (plan-one-environment.md 8.2 item 3).
6. A backup and an archive are the same copy. The hidden identity of
   the node storage is not in it, because the node storage is another
   file (card open).
7. A damaged store (owner card 2026-10-26 17:30): the operator copies
   the database file and its `-wal` file together to a clone, and runs
   `sqlite3 clone.db .recover | sqlite3 new.db` on the clone. The node
   then takes the repair by entries from its peers. The library does
   not call `.recover`.
8. The repair after a snapshot by a changeset of the session extension
   (owner, 17:35) has no mechanism yet: `sqlite3session_diff` needs
   both databases on one connection, and a session runs the preupdate
   hook on each write. Proposal: the first version repairs by entries
   and by the full copy; the changeset gets a plan of its own (card
   open).

## 8. Build and tests

1. SQLite comes from the package of the distribution
   (`libsqlite3-dev`). The submodules `deps/lmdb` and `deps/lmdb-0.9`,
   `mdb_dump`, `mdb_load`, the checksum module, `lmdb_category`,
   `test/lsan-mdb_dump-0.9.txt` and `test_lmdb_upgrade` go. RULES.md
   names LMDB as an exception to the rule on packages; the owner
   changes that text.
   The decimal extension of SQLite (`ext/misc/decimal.c`) comes as a
   submodule from the source of SQLite (owner, 2026-10-26 18:34). The
   package `libsqlite3` does not hold it; only the shell does
   (`shell.c.in:279` in 3.53.4). It is one more exception to the rule
   on packages, beside CRC32C and libhydrogen.
2. The interface tests of the concept `storage` change with the
   concept (sections 5.3 and 6.2). The tests that name LMDB
   (`lmdb_cluster.hpp`, `lmdb_damage.hpp`, `lmdb_storage.hpp`) get
   SQLite forms.
3. `error_fs` gets the SQLite files (store, `-wal`, `-shm`) and a switch
   that drops every write after the last sync of each file.
4. New tests:
   - a power cut during a commit, before its sync returns: the commit
     is lost and the store is whole;
   - a power cut during a checkpoint;
   - `SQLITE_FULL` inside a savepoint: no write of the step is in the
     file (section 5.1 item 3);
   - `ENOSPC` from `fdatasync`: `sync_error`, not the "full" value;
   - a failed sync in a checkpoint: `sync_error`, no second sync;
   - a reader keeps its snapshot while the writer commits a step;
   - a byte flipped in an early frame of a WAL with several commits,
     then a restart (section 7.3);
   - a damaged page gives `SQLITE_CORRUPT` and no crash;
   - `.recover` on a damaged clone gives back each record whose CRC
     holds;
   - the file mode is 0600 for the store, the WAL and the node storage.
   The VM of the session cannot cut its power and cannot tear a
   sector; these tests run through `error_fs`.
5. Tests keep `KONSENS_TEST_TMPDIR`.

### 8.1 Measured: who syncs

bench/results/2026-10-26-174119Z-sqlite-sync.txt (a C++ file outside
the tree, g++ 16 -O2, SQLite 3.53.4 of the package, ext4 on virtio, 64 B
values, 7 rounds, arms alternate). Median over the rounds of the median
time for each commit with its sync and its checkpoints:

| values per commit | full a | full b | caller a | caller b |
|---|---|---|---|---|
| 1 | 139.5 us | 147.2 us | 152.4 us | 140.4 us |
| 16 | 160.4 us | 162.6 us | 162.9 us | 167.7 us |
| 1024 | 854.7 us | 934.5 us | 873.8 us | 877.9 us |

The two builds of one arm differ by up to 9.3 %. The arms differ by
less, so there is no difference above the noise floor. The overlap of
the sync with the sends is not measured. Faults of this run against
the rules: nice 0 and a TLC run of one worker on the machine; one
build only; the arm "full" prints `checkpoints=0` because the bench
does not count the automatic checkpoint; the arm "caller" used
`RESTART`, the automatic checkpoint is `PASSIVE`; the bench does not
check the result of `sqlite3_prepare_v3`. The run of section 10
follows the rules.

## 9. Decisions of the owner and open points

Decided, 2026-10-26:

- 17:24: statements prepared once at open.
- 17:26: native column types, CBOR with tag where none fits.
- 17:29: virtual tables, relations, strict types.
- 17:30: one file with all tables; the node storage a second file.
- 17:30: `.recover` of the package `sqlite3`, on a clone.
- 17:35: the CRC32C of each record stays; a repair after a snapshot by
  a changeset.
- 17:54: the first version lets SQLite sync (`synchronous=FULL`).

Cards open (each with its proposal). The owner asked at 18:14 for the
practice of known systems on each; section 9.1 gives it, and the cards
are asked again:

1. `find` returns a copy (5.3).
2. The checkpoint is called by the library, not automatic (6.5).
3. `psow=0` (6.7).
4. The columns keep the current binary form under the CRC (4.1).
5. "done" before the commit of the leader (6.3).
6. One SQLite version in the cluster and an authorizer (4.3 item 4).
7. The bound 2^63-1 on integer keys (4.2).
8. The changeset repair after the first version (7.8).
9. The hidden identity stays out of a backup, so a restore has a new
   identity (7.6).

Open, not yet a card: a checksum on each page (`cksumvfs.c` as a
submodule, or none).

Not a question for the owner: the lowest SQLite version is the one
that the distribution gives.

Answers of the owner, 2026-10-26 18:22, to the cards of section 9.1:

1. `find` gives a pointer, no copy. The view lives until the next call
   of its statement, as `RawBytes` in Go. A debug build checks that a
   view is not read after that call (a generation count).
2. The checkpoint: the program calls it (owner card 18:23), section 6.5.
3. `psow=0` (owner card 18:23). Its cost is measured.
4. The log record as a blob or in columns: both are arms of the
   measurement of section 10; the numbers decide.
5. "done" before the commit of the leader: decided before (11:38),
   parallel.
6. Only the library writes SQL. No task sends SQL, so no authorizer and
   no version check are needed. Section 4.3 item 4 and 5 fall away.
7. Keys take what SQLite offers: `INTEGER`, with the bound 2^63-1 as
   one check for safety where a value enters.
8. The changeset repair: tried in a file outside the tree and modelled
   now, not deferred.
9. The identity after a restore: the effects of both forms are
   measured in a test before the decision.

### 9.0 The changeset, tried (2026-10-26)

A C file outside the tree, the package 3.53.4, g++ 16 container:

1. A change inside a savepoint that is rolled back (`ROLLBACK TO`,
   `RELEASE`) does not appear in the changeset of a session. Only the
   change outside the savepoint does.
2. Repair form: the live store and a snapshot file (`VACUUM INTO`) on
   one connection (`ATTACH`). `sqlite3session_diff` of a table gives the
   changeset from the snapshot to the live state. Applied to the
   snapshot with `sqlite3changeset_apply`, the two tables are equal
   (`EXCEPT` gives 0 rows), for 100,000 rows with 1,000 changed and for
   1,000,000 rows with 10,000 changed.
3. The changeset holds about 144 bytes for each changed row of 64 B
   values (144,007 bytes for 1,000 rows). No time is written here: the
   run was one build and one repetition, which is not a measurement.
4. The diff needs no session during normal work, so no preupdate hook
   runs on the path that succeeds. It needs the snapshot as a file.
5. For the model: a node that holds snapshot S and is behind asks a
   peer that holds S too. The peer attaches S, diffs, and sends the
   changeset with the log index of its live state. Each record in it
   keeps its CRC.

### 9.1 What known systems do, for each open card

Checked 2026-10-26 on the web. etcd and MongoDB are not cited (owner).

1. `find`: SQLite says the pointer of `sqlite3_column_blob` lives until
   the next step, reset or finalize
   (https://sqlite.org/c3ref/column_blob.html). Go `database/sql`
   copies into `[]byte` by default and offers `RawBytes` without a copy,
   valid only until the next row. Practice: a copy by default.
2. The checkpoint: Litestream recommends `PRAGMA wal_autocheckpoint = 0`
   and runs the checkpoint itself (https://litestream.io/tips/). The
   dqlite server runs a checkpoint when the WAL passes a fixed size
   (https://canonical.com/dqlite/docs/explanation/replication).
   Practice: the program, not the automatic checkpoint.
3. `psow`: the unix default is on since 3.7.10. SQLite says to assume
   that drive firmware is poorly written, and names `psow=0` for a safer
   database (https://sqlite.org/psow.html). No source found that a
   replicated SQLite system sets `psow=0`. Practice: the default; the
   documented safe form: `psow=0`.
4. The log record: hashicorp raft-boltdb stores an entry as opaque bytes
   under a big-endian 8-byte key (`util.go`, `uint64ToBytes`). dqlite
   keeps its Raft log in its own files, not in SQLite tables. Practice:
   the log entry is an opaque record; its form and its CRC do not
   change with the store.
5. "done" before the commit of the leader: the Raft dissertation 10.2.1
   says the leader may write to its disk in parallel with the
   replication, and counts itself only after its own sync. hashicorp
   raft writes first and replicates after; PR 579 adds the parallel form
   as work in progress (https://github.com/hashicorp/raft/pull/579).
   Practice in the theory: parallel; in that library: not yet.
6. Determinism: rqlite replicates statements and rewrites `RANDOM()`,
   `RANDOMBLOB()` and the time functions with `now` before the log; it
   does not cover `ORDER BY RANDOM()`, defaults, triggers and views
   (https://rqlite.io/docs/api/non-deterministic/). dqlite replicates
   pages, so it needs no determinism. SQLite lists `random()`,
   `changes()`, `last_insert_rowid()`, `sqlite_version()` and the date
   functions with `now`, `localtime` or `utc` as not deterministic
   (https://www.sqlite.org/deterministic.html). No source found for a
   check of the SQLite version across a cluster. Practice: forbid or
   rewrite the functions that are not deterministic.
7. Integer keys: raft-boltdb keeps the full `uint64` range as a
   big-endian key (item 4). Practice: the full range.
8. The changeset: SQLite describes sessions for recording changes and
   merging them between sites (https://sqlite.org/session.html). A
   session ignores a table without an explicit primary key. Raft systems
   catch a node up with log entries and a snapshot. Practice: log and
   snapshot; changesets in multi-site tools such as cr-sqlite.
9. The identity after a restore: Consul asks for unique node names when
   a snapshot is restored on new infrastructure
   (https://developer.hashicorp.com/consul/docs/manage/disaster-recovery/backup-restore).
   A Vault user restored a production snapshot into a test cluster; the
   snapshot carried the production members, and production nodes then
   refused that node id (https://discuss.hashicorp.com/t/restoring-raft-snapshot-on-test-cluster-broke-production-cluster/44473).
   Practice: a restore does not reuse a live identity.

## 10. What is measured

Both arms as bench/run.sh runs them: the LMDB store at fa14f44 and the
SQLite store, the same durability (each commit synced), stores on ext4.
The cases of plan-one-environment.md section 15, the batch sizes 1, 16
and 1024, the cost of a checkpoint, the copy in `find`, the log as a
`WITHOUT ROWID` table and as a rowid table, and `psow=0` against the
default. Each build of the rules, with priority. No number stands here
before the run.

### 10.1 The model

OneEnvironment.tla gets: the sync inside the commit; "done" sent
before the commit of the leader, which counts itself after its commit;
a store that goes back to an older whole state at a start (section
7.3). The runs come before the code.

## 11. Review 1 and changes

doc/review-plan-sqlite-storage-1.md (second agent, 2026-10-26): 28
findings, 6 blocking. Verdict: approve after the blocking fixes.

| finding | change |
|---|---|
| 1 blocking, `find` span | 5.3, card open |
| 2 blocking, a full disk in a savepoint | 5.1 item 3, test 8.4 |
| 3 blocking, the automatic checkpoint drops its error | 6.5, card open |
| 4 blocking, `ENOSPC` of an `fsync` | 6.1, test 8.4 |
| 5 blocking, no padding with `FULL` | section 1, 6.7, card open |
| 6 blocking, the record is not CBOR | 4.1, card open |
| 7, a damaged frame in the middle of the WAL | section 1, 7.3, 10.1, test 8.4 |
| 8, error codes | 6.1 |
| 9, the reader snapshot | 5.2 |
| 10, `RELEASE` | 5.1 item 2 |
| 11, the inventory | section 2 |
| 12, what `FULL` removes | section 3, 6.3, card open |
| 13, determinism | 4.3 item 4, card open |
| 14, the tests | 8.3, 8.4 |
| 15, integer keys | 4.2, card open |
| 16, the changeset | 7.8, card open |
| 17, the copy | 7.5 |
| 18, the size of a value | 6.2 |
| 19 to 28 | section 1, 4.1, 6.1, 6.2, 7.7, 8.1, 8.2, 9, 10 |
