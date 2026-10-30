# Plan: the storage on SQLite

Source: the owner, 2026-10-26 17:20. The task is
Claude-state `tasks/2ba55cf6-5a97-424a-ad89-8298662983c0.org` on the
branch `tasks/org-migration` (bc335a1).

1. SQLite holds all persistent data: the log, the meta record, the
   state, the client table, `auth`, the node storage, the backups and
   the archives. A mixed form is not taken.
2. The reason of the owner: SQLite has a recovery for a damaged
   database (`.recover`, section 1).
3. Each statement is prepared once when the store opens. A step only
   binds values and runs the statements (owner, 17:24). What this
   gains is measured, not estimated.

Part A lists the rules that the store before SQLite carried. Part B is the form with SQLite. Section 9 lists the
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

## Part A: the rules of the store before SQLite

## 2. The rules that the store carried

- One write transaction for each step (plan-one-environment.md 3.4).
- The library makes no sync; the caller syncs and then commits
  (plan-one-environment.md 3.5). Section 6 replaces this rule.
- The runs of the work roll back alone (plan-one-environment.md 5.2).
- The parent takes no operation while its child is open.
- CRC32C for each record (section 10 of plan-one-environment.md).
- An index below the last one is refused.
- A value is read without a copy.
- A damaged page is found at read time.
- A full store is an error value, not a crash.
- The store has a bound.
- A failed sync ends the process (owner, 2026-10-21).
- A full copy serves for a repair and for a new node.
- A store of its own holds the repair count, the proof and the hidden
  identity.
- Only the user reads the store and the hidden identity (file mode
  0600).
- A store of another format gives `storage_errc::unsupported_format`.
- A destructor with an open transaction ends the process: a wrong use
  fails loudly.
- No start as root.
- The fault tests and the tests on tmpfs (`KONSENS_TEST_TMPDIR`).

The LMDB form of this section is in the git history before branch remove-lmdb.

The interface does not name the engine of the store: the concept `storage`
(`include/konsens/storage.hpp:237-245`) is the boundary. The concept
changes in three places: `find` (section 5.3), the sync (section 6.2),
and `map_size` (section 6.6).

## 3. Older plans that this plan replaces

plan-hide-lmdb.md, plan-lmdb-converter.md, plan-lmdb-submodule.md and
plan-simulation-on-lmdb.md are deleted; they are in the git history.
plan-storage-only-source.md and plan-state-in-storage.md stay. These
rules of the six plans stay: the CRCs, the start checks, the repair by
entries, the proof of a fault and the root check.

plan-one-environment.md stays, except these items. Each is replaced by
the section named.

| item of plan-one-environment.md | replaced by |
|---|---|
| 3.4 item 2, "sync only" | gone: no descriptor; an idle node has nothing to sync |
| 3.5 items 1, 3, 4 and 6 (io_uring, the prepare of the commit, the writer lock through the sync, slipstreamIO) | section 6 |
| 3.5 item 2, "done" while the leader syncs (owner decision 18 item 4) | section 6.3, decided (section 9) |
| 3.6, the count of syncs | section 6.4 |
| 4.3 item 1, the decided index without a sync | each commit syncs (section 6.1) |
| 5.1 items 2 to 4 and 9 item 6 (the tombstone by file system) | the tombstone rides in the commit of its step, which syncs |
| 8.1 item 1, the vote after f + 1 answers as the only rule after a start | section 7.3 (`RecoveryWaitsForLeader`) |
| 8.2 item 3, the full copy | section 7.5 |
| 15 items 6 to 8 | section 10 |
| owner decisions 18 items 5 and 6 | section 6, decided (section 9) |

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
   is an `INTEGER` (section 4.2). The log is a rowid table with
   `INTEGER PRIMARY KEY` (measured, section 10.2).
5. Each statement is prepared at the start with
   `SQLITE_PREPARE_PERSISTENT` and kept. A step binds and runs it, then
   calls `sqlite3_reset`. The statements are a fixed list in one place.
   The store opens with extended result codes
   (`SQLITE_OPEN_EXRESCODE`).
6. The format version stays in `meta`. A new store must be empty; a
   store with another version gives `storage_errc::unsupported_format`.
   SQLite has a field for this in its header (`PRAGMA user_version`);
   the record in `meta` keeps its CRC.

### 4.1 The record and the CRC

1. Today a log entry is a kind byte and little-endian fields
   (`storage.cpp:78-97`), not CBOR. The meta record, the session record
   and the repair record are fixed little-endian forms with a CRC
   (`persistent_storage.cpp:235-310`, `:405-418`, `:762-805`). The tail
   of a log record holds the entry CRC, the chain CRC, the state and
   the state CRC (`persistent_storage.cpp:47-53`). The entry CRC
   travels on the wire.
2. A log entry is one `BLOB` record in the current binary form, with
   its entry CRC and chain CRC, in a rowid table (measured, section
   10.2; owner 18:22 item 4). The wire does not change, and
   `format_version` goes to 5 only for the file. The fields of the
   state take native columns (item 3).
3. A field of the state whose value has an exact SQLite form takes that
   form: an integer from -2^63 to 2^63-1 as `INTEGER`, a text string as
   `TEXT`, a byte string as `BLOB`, a float as `ANY` (it keeps -0.0).
   CBOR stands only on the wire. The store holds no CBOR; each value
   takes a form that SQLite gives, also through an extension or a
   virtual table (owner, 2026-10-26 18:40). CBOR is a type that an
   extension gives (owner, 19:05 and 19:08). SQLite has five storage
   classes, and an extension cannot add one: a `STRICT` table refuses
   every column type except `ANY`, `BLOB`, `INT`, `INTEGER`, `REAL` and
   `TEXT` (`build.c:2686-2698`, `global.c:394`). So the form is that of
   JSONB in SQLite: a `BLOB` column, the functions of the extension, and a virtual table that gives the
   fields as typed columns. Each CBOR tag on the wire
   has a typed form in SQLite (owner, 18:42). A tag with no form is
   refused at the wire with an RFC 9290 error, and tag 24 is decoded
   and stored in the form of its type (owner, 18:44, relayed). This
   replaces "every tag allowed" of 2026-10-17. The table of forms, the
   form of NaN and the form of an integer above 2^63-1 belong to the
   thread "user logic on virtual tables"
   (`/mnt/project-files/actor-raft/nutzer-logik/vorbereitung.md`
   section 10). Nothing of it is built or measured.
   The schema holds no `CHECK(cbor_valid(x))`. The library checks each
   CBOR value in C++ before it writes the value (owner, 2026-10-27
   08:33, review 2 B6). Reason: `.recover` of the distribution package
   builds the tables again without the extension, and a `CHECK` that
   calls a function it does not know fails there.
4. A `STRICT` column checks the type and converts a value that it can
   convert without loss (text `'12'` into `INTEGER`). A prepared
   statement has no parameter types. An `ANY` column checks nothing;
   there the storage class of the value keeps `true` and `1` apart.
   So the library binds each parameter with the bind call of its C++
   type, and a column of a fixed type is `INTEGER`, `TEXT`, `BLOB` or
   `REAL`, never `ANY`, except for floats.

### 4.2 Integer keys (owner, 2026-10-26 18:22 item 7)

The log index, the session id, `client_id` and `pid` are `uint64_t`.
Proposal: one check for safety, with one named bound (2^63-1), at each
place where an index or an id enters the library. A value above it is
an error value. The key column stays `INTEGER`. The other form keeps a
big-endian `BLOB` key, as now (`persistent_storage.cpp:137-142`).
The insert of the log checks that the index is the next one.

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
   `random()` and `'now'`. Mathematics, `sin` and `pow` included, runs
   only in SQL statements, never in code outside SQLite (owner,
   2026-10-26 18:37). These functions call the libm directly
   (`func.c:2575-2590`), and no CORE-MATH replaces it (owner, 18:38).
   A float from SQL follows the rule for floats: the results are
   compared on `float_digits`, then the chosen value is rounded half
   to even on `float_denominator`. `sum(x ORDER BY pk)`
   (`parse.y:1238`) gives the same order of addition on each node.
   Only the library writes SQL, so there is no authorizer (owner,
   18:22); the library builds no `ATTACH`, `PRAGMA` or `ANALYZE` from
   a query of a client. Each query of a task
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
7. A new schema is not compatible with an older version (owner,
   2026-10-26 18:57). Proposal of the thread "user logic on virtual
   tables" (`/mnt/project-files/actor-raft/nutzer-logik/vorbereitung.md`
   sections 9 to 18), not decided: each node first gets the new binary,
   which still works in the old schema. Then one entry of the log
   switches all nodes at the same index. From that index on, an older
   binary does not vote. This is Jepsen question 4 (mixed versions).
8. The schema of the library is fixed from release 1.0 on. The tables
   of a user are not part of it. The CBOR functions that the library
   gives to SQLite (`cbor_extract` with an index on an expression,
   `cbor_query`, `cbor_valid`) stand in `vorbereitung.md` sections 19
   and 20.
9. How a task reaches SQL is not in this plan. The `state_machine`
   concept offers `find` of a byte key (`state_machine.hpp:28-31`). A
   plan of its own gives the interface.
10. The nodes of a cluster may run different versions of SQLite, and
    no version check exists (measured,
    `bench/results/2026-10-27-083000Z-sqlite-versions.txt`, 3.37.2 to
    3.55.0). The stored values are the same bit for bit in every
    version. The bytes of the file differ by version and by build
    flags, and no mode of SQLite makes them equal. The text of a float
    differs: `cast(1.0/3 as text)` gives 15 digits before 3.53 and 17
    digits from 3.53 on, and `printf` of a float differs too. A new
    syntax, such as `sum(x ORDER BY y)` from 3.44 on, fails on an older
    version. So the digest and Execute-Verify read values, never the
    bytes of the file. A task that turns a float into text or uses a
    syntax that one node does not know gives different results on the
    nodes; Execute-Verify finds the difference.

## 5. The transactions

### 5.1 The write transaction of a step

1. A step is `BEGIN IMMEDIATE` ... `COMMIT` on the writer connection.
2. The child of a run is `SAVEPOINT x`. Its commit is `RELEASE x`. Its
   abort is `ROLLBACK TO x` and then `RELEASE x`. The guard
   `throw_if_nested_is_open` stays on each method of the parent.
3. After each statement that fails, the store reads
   `sqlite3_get_autocommit`. When it is 1, SQLite has rolled back the
   whole step. When it is 0, SQLite rolled back only the statement: a
   statement with a statement journal keeps the transaction open after
   `SQLITE_FULL` or `SQLITE_NOMEM` (`vdbeaux.c:3376-3377`). The store
   then runs `ROLLBACK` itself. In both cases the transaction object
   enters a closed state: each later call returns the error without
   SQL, and `commit` returns it. The child does the same. So no part of
   a step becomes durable alone.
4. The destructor of the store with an open transaction ends the
   process, as now.

### 5.2 The reader

1. A second connection reads, in the same thread of the caller.
2. `begin_read_only` runs `BEGIN` and one read of the meta record at
   once, so that the snapshot starts there. The end of the reader runs
   `COMMIT`. So each `read_only_transaction` sees one state.
3. A reader that stays open stops the checkpoint at its snapshot. The
   library holds no reader between steps.

### 5.3 `find` (owner, 2026-10-26 18:22 item 1)

`find` returns a view of the bytes, no copy, as `RawBytes` in Go. The
pointer of `sqlite3_column_blob` dies at the next step or reset of its
statement (section 1), so the view lives until the next call of its
statement. A debug build keeps a generation count for each statement
and throws `std::logic_error` when a view is read after that call. Not
checked: whether a caller holds two views of one statement at the same
time.

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
3. "done" from the leader (owner, 11:38): the leader sends "done"
   before its `commit`, so that its own sync runs while the followers
   write. It counts itself only after its `commit` returns. The model
   (`MC_oe_parallel_l1`) has no violation; the runs with the store
   rollback end at their limit. The model has no commit that fails;
   a failed commit ends the process (owner, 2026-10-24; section 6.1),
   so the leader proposes nothing after it (review 2 B4).
4. The count of syncs: one sync of the WAL for each commit. A
   checkpoint adds one sync of the WAL and, when it reaches the end of
   the WAL, one sync of the database file.
5. The checkpoint (owner card 18:23): `PRAGMA wal_autocheckpoint=0`
   runs first, because it removes any WAL hook (`main.c:2510-2524`);
   the library installs its own hook after it. The library calls
   `sqlite3_wal_checkpoint_v2` with `SQLITE_CHECKPOINT_PASSIVE` after
   the commit and after the sends of the step, when the WAL holds
   `checkpoint_frames` frames or more (one named constant; the WAL hook
   gives the count). Before `sqlite3_close` the library runs this
   checkpoint itself and checks its result, because the checkpoint
   inside `sqlite3_close` syncs the database file and does not raise a
   failure. `SQLITE_IOERR_FSYNC`
   from it throws `sync_error`, and nothing repeats it.
   `SQLITE_FULL` from it throws `write_error` (section 6.1). No C++ exception crosses
   a callback that SQLite calls. The sync stays the work of SQLite.
6. The bound of the store: `map_size` goes. `PRAGMA max_page_count`
   gives the bound in pages; the parameter of `open` gives it in bytes,
   and the library converts it. Over the bound, SQLite gives
   `SQLITE_FULL`. The node then ends, as the owner decided for a full
   store (2026-10-24); a switch of the binary, off by default, opens the store
   again with twice the bound. The stop below X % of free space (owner,
   2026-10-26 15:25) comes before that bound.
7. Torn sectors (owner card 18:23): each database file opens with the
   URI parameter `psow=0`, so that SQLite pads the last frame of a
   commit to the sector. Measured in section 10.2.

### 6.1 The error codes

The rule "An error goes to whoever can repair it" decides each row.

| SQLite code | konsens |
|---|---|
| `SQLITE_FULL` | `write_error`, and the node ends (owner, 2026-10-24: a failed commit and a full store end the process) |
| `SQLITE_IOERR_FSYNC`, `SQLITE_IOERR_DIR_FSYNC` | `sync_error`, whatever `sqlite3_system_errno` says |
| other `SQLITE_IOERR_*` on a write | `write_error` |
| other `SQLITE_IOERR_*` on a read | `read_error` |
| `SQLITE_CORRUPT*`, `SQLITE_NOTADB` | a damage code that `error_is_a_damage` (`sequence_paxos.hpp:83-88`) lists |
| `SQLITE_TOOBIG` | the value of section 6.2 |
| `SQLITE_BUSY` at open (another process holds the store) | an error value, the store is in use |
| `SQLITE_BUSY` after open, `SQLITE_MISUSE`, `SQLITE_CONSTRAINT` on a key that the library computed | `std::logic_error` |

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
   older state that is whole in itself. The older state is not always whole: a
   PASSIVE checkpoint copies frames into the database file and keeps
   them in the WAL (`wal.c:2254-2337`). After a damaged frame the
   recovery sets `nBackfill` to 0 (`wal.c:1579-1580`), and the
   database file keeps pages of commits after that frame. So the state
   can be a mix of commits, and each record CRC still holds (review 2
   B1). This happens only when a frame that was synced is damaged
   later; SQLite syncs the WAL before a checkpoint copies it
   (`wal.c:2360`). The recovery after each start compares the digest
   of the state at the applied index with the digest of the new
   leader at the same index. A difference asks for a changeset with
   the digest check of section 7.8, else the full copy (owner,
   2026-10-27 08:36). The vote after f + 1 answers after each
   start (plan-one-environment.md 8.1) does not keep Paxos safe here:
   TLC found a decided entry that is lost (`OneEnvironment` with
   `StoreRollback`, and `RecoveryRollback`). Taking the log of the
   answers is not enough either, because an answer can be older than
   an accept that is still on the way. The form that holds in the
   models: after the f + 1 answers the node does not count in a
   prepare. Its promise carries a mark, the leader counts it only in
   its accept phase and sends it the accept sync, and the node votes
   again after that accept sync, and only an accept sync of a ballot
   above the highest promise of its answers (`RecoveryWaitsForLeader`).
   A recovering or rejoining node gives no answers and does not lead.
   Each start enters the recovery again, so the mark needs no store.
   When a majority starts at the same time, no node gets f + 1 answers;
   the operator acts through the single-node mode, stages 1 and 2
   (owner, 2026-10-24: the repair needs a living majority). Results:
   `model/results/20261026T185400Z-one-environment-rollback.md`.
4. A checksum on each page needs `cksumvfs.c`, which is in no package
   (section 9).
5. A full copy for repair and for a new node is `VACUUM INTO` a new
   file. It runs on a connection with no open transaction, in the
   thread of the caller. The receiver syncs the file and its directory
   before the switch in the node storage, and checks the copy as now
   (plan-one-environment.md 8.2 item 3).
6. A backup and an archive are the same copy. The hidden identity of
   the node storage is not in it, because the node storage is another
   file. The effects of both forms are measured in a test before the
   decision (owner 18:22 item 9).
7. A damaged store (owner card 2026-10-26 17:30): the operator copies
   the database file and its `-wal` file together to a clone, and runs
   `sqlite3 clone.db .recover | sqlite3 new.db` on the clone. The node
   is stopped while its files are copied. The node
   then takes the repair by entries from its peers. The library does
   not call `.recover`.
8. The repair after a snapshot by a changeset of the session extension
   (owner, 17:35) has no mechanism yet: `sqlite3session_diff` needs
   both databases on one connection, and a session runs the preupdate
   hook on each write. The changeset was tried (section 9.0) and
   modelled (`model/tla/ChangesetRepair.tla`, results
   `model/results/20261027T060500Z-changeset-repair.md`). A damage in
   a row that the changeset does not touch stays after the repair,
   because `sqlite3changeset_apply` checks only the rows it changes. So
   after the changeset the node compares the digest of its whole state
   with the digest of the peer at the same index, and a difference asks
   for the full copy. With this check the model holds. The digest is
   the digest of the state that Execute-Verify compares. Each table of
   the state has an explicit primary key, because a session leaves out
   a table without one. The conflict handler of
   `sqlite3changeset_apply` returns `SQLITE_CHANGESET_ABORT`.

The LMDB form of this section is in the git history before branch remove-lmdb.

## 8. Build and tests

1. SQLite comes from the package of the distribution
   (`libsqlite3-dev`). The framework neither ships nor builds SQLite
   (owner, 2026-10-27 08:14 and 08:32, after the version comparison
   of section 4.3 item 10). The submodules, the tools and the tests of
   the store before SQLite go.
   The decimal extension of SQLite (`ext/misc/decimal.c`) comes as a
   submodule from the source of SQLite (owner, 2026-10-26 18:34). The
   package `libsqlite3` does not hold it; only the shell does
   (`shell.c.in:279` in 3.53.4). It is one more exception to the rule
   on packages, beside CRC32C and libhydrogen.
2. The interface tests of the concept `storage` change with the
   concept (sections 5.3 and 6.2). The test helpers of the store
   before SQLite get SQLite forms (`sqlite_cluster.hpp`,
   `sqlite_damage.hpp`, `sqlite_storage.hpp`).
3. `error_fs` gets the SQLite files (store, `-wal`, `-shm`) and a switch
   that drops every write after the last sync of each file.
4. New tests:
   - a power cut during a commit, before its sync returns: the commit
     is lost and the store is whole;
   - a power cut during a checkpoint;
   - `SQLITE_FULL` inside a savepoint: no write of the step is in the
     file (section 5.1 item 3);
   - `ENOSPC` from `fdatasync`: `sync_error`, not `write_error`;
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

The LMDB form of this section is in the git history before branch remove-lmdb.

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
- 17:26: native column types, CBOR with tag where none fits (replaced
  18:40: CBOR only on the wire, section 4.1 item 3).
- 17:29: virtual tables, relations, strict types.
- 17:30: one file with all tables; the node storage a second file.
- 17:30: `.recover` of the package `sqlite3`, on a clone.
- 17:35: the CRC32C of each record stays; a repair after a snapshot by
  a changeset.
- 17:54: the first version lets SQLite sync (`synchronous=FULL`).

The cards below were asked at 18:14 and are answered (18:22 and
18:23, further down). The list stays as the record of the questions:

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
   no version check are needed. The authorizer and the version check
   of section 4.3 item 4 fall away; its rules on deterministic queries
   stay.
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

Both arms as bench/run.sh runs them: the store before SQLite at fa14f44
and the SQLite store, the same durability (each commit synced), stores on ext4.
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

### 10.2 Measured: the form of the log table (2026-10-27)

File: `bench/results/2026-10-27-071421Z-sqlite-layout.txt`. One writer,
WAL, `synchronous=FULL`, entries of 89 B (kind, session, request, 64 B
payload, two CRCs). g++-16 -O2, SQLite 3.53.4, ext4 in the VM of the
session, 5 rounds, medians over rounds. Only the release build ran.

Noise floor (base_a against base_b, one binary): commit up to 3.5 %,
lookup up to 0.5 %. The spread of the commit time inside one arm is up
to 30 %, so a commit difference under about 10 % is no difference.

Lookup by idx, all fields read, ns, against `base` (BLOB, WITHOUT
ROWID):

| entries | base | columns | rowid |
|---|---|---|---|
| 1,000 | 280 | 376 | 251 |
| 16,000 | 378 | 475 | 303 |
| 102,400 | 1164 | 1240 | 1024 |

`columns_rowid`: 347, 398, 1124 ns.

File bytes per entry: base 113.9-118.8, columns 105.7-110.6, rowid
100.2-110.6, columns_rowid 93.7-102.4.

Commit time per commit, µs, medians: base 156-161, 172-175, 913-933;
rowid 167, 165, 821; columns 140, 180, 1099; columns_rowid 196, 153,
925. Only rowid at 1024 entries (-11 %) and columns at 1024 (+18 %)
are above the spread.

`psow=0` against the default: 158 against 230 µs at 1 entry per
commit, with no overlap of the rounds; the same within the noise at 16
and 1024. `psow=0` was not slower in any case. Why the default was
slower at 1 entry is not known.

Faults of this run against the rules: nice 0, the release build only,
one machine.

Result: the log is a rowid table (idx is the rowid) with one BLOB
record. It is the fastest arm for the lookup in each size, and the
fastest or equal for the commit. Columns cost 7 % to 34 % on the
lookup. The three points of section 4.2.1 (one statement for a query
by session) are not measured; a query by session needs its own index
in both forms.

## 11. Review 1 and changes

doc/review-plan-sqlite-storage-1.md (second agent, 2026-10-26): 28
findings, 6 blocking. Verdict: approve after the blocking fixes.

| finding | change |
|---|---|
| 1 blocking, `find` span | 5.3, decided (section 9) |
| 2 blocking, a full disk in a savepoint | 5.1 item 3, test 8.4 |
| 3 blocking, the automatic checkpoint drops its error | 6.5, decided (section 9) |
| 4 blocking, `ENOSPC` of an `fsync` | 6.1, test 8.4 |
| 5 blocking, no padding with `FULL` | section 1, 6.7, decided (section 9) |
| 6 blocking, the record is not CBOR | 4.1, decided (section 9) |
| 7, a damaged frame in the middle of the WAL | section 1, 7.3, 10.1, test 8.4 |
| 8, error codes | 6.1 |
| 9, the reader snapshot | 5.2 |
| 10, `RELEASE` | 5.1 item 2 |
| 11, the inventory | section 2 |
| 12, what `FULL` removes | section 3, 6.3, decided (section 9) |
| 13, determinism | 4.3 item 4, decided (section 9) |
| 14, the tests | 8.3, 8.4 |
| 15, integer keys | 4.2, decided (section 9) |
| 16, the changeset | 7.8, decided (section 9) |
| 17, the copy | 7.5 |
| 18, the size of a value | 6.2 |
| 19 to 28 | section 1, 4.1, 6.1, 6.2, 7.7, 8.1, 8.2, 9, 10 |

## 12. Review 2 and changes

doc/review-plan-sqlite-storage-2.md (second agent, 2026-10-27): 7
blocking, 9 others.

| finding | change |
|---|---|
| B1, a mixed state after a damaged WAL frame | 7.3; digest of the state against the new leader at each start (owner, 2026-10-27 08:36) |
| B2, the rejoining node took an old accept sync | model 803da6d: only a ballot above its answers; 7.3 |
| B3, the code recovers in the "answerlog" form; a majority that starts at once | section 3; 7.3: the single-node mode (owner, 2026-10-24) |
| B4, a failed commit of "done" at the leader | 6.1: `SQLITE_FULL` ends the process (owner, 2026-10-24), so the leader proposes nothing after it |
| B5, `SQLITE_FULL` rolls back only the statement | 5.1 item 3 |
| B6, `CHECK(cbor_valid(x))` against `.recover` | the check is in C++, the schema has no CHECK (owner, 2026-10-27 08:33) |
| B7, text against owner decisions | 4.1, 4.2, 5.3, 6.3, 6.5, 6.7, 7.6, 9, tables |
| O1, the checkpoint in `sqlite3_close` | 6.5 |
| O2, `wal_autocheckpoint=0` removes the hook; 1000 frames | 6.5 |
| O3, `SQLITE_BUSY` from another process | 6.1 |
| O4, "the model shows" | 6.3 |
| O5, the rules of the rejoining node | 7.3 |
| O6, the digest, primary keys, the conflict handler | 7.8 |
| O7, the faults of the run of 10.2, psow | 10.2 |
| O8, the node stops during the copy | 7.7 |
| O9, two citations | section 2 |

## 13. Changes during the build

The build of the first step (2026-10-27) changed three points of this plan.

1. The close (6.5). Each connection sets
   `SQLITE_DBCONFIG_NO_CKPT_ON_CLOSE`, so `sqlite3_close` makes no
   checkpoint and no sync. The library runs no checkpoint before the
   close. A destructor raises nothing, so a checkpoint in it could not
   give a failed sync to the caller. The WAL stays on the disk after the
   close, and the next open reads it.
2. The error codes (6.1). The store raises `write_error` for an
   `SQLITE_IOERR_*` other than a failed sync on the writer, and
   `read_error` for it on the reader. The code of the exception is the
   errno that `sqlite3_system_errno` gives, in `std::generic_category`.
   `SQLITE_FULL` stays an error value `no_space_on_device`, and the node
   raises it as `write_error`.
3. "done" before the commit of the leader (6.3) is not in the first
   step. The node gives every message and every reply after its commit.

## 14. The build of the rejoin (section 7.3)

The models check the rejoin for safety. They do not say which node raises
the ballot above the promises of the answers. This section gives the form
of the code after review 3 (findings F1 to F12). Items 1 and 8 are
provisional and wait on the owner.

1. Provisional (F5): `complete_recovery` keeps the log of the store and
   raises only the promise, as `RecoveryComplete` of the model does. The
   log of the leader comes with the accept sync of item 5. The copy of
   the log of a primary or of the best answer goes, because no model
   checks the copy followed by the wait, and the copy alone is unsafe
   (results, run 5). The comparison of the chain CRCs of the answers
   (`find_hardware_fault`) stays.
2. `complete_recovery` keeps the highest promise of the answers and of
   the store as `rejoin_above` in RAM. No store holds it: each start
   enters the recovery again (section 7.3).
3. While `rejoin_above` is set, the node does not lead: `handle_leader`
   does nothing, and Ballot Leader Election sends its heartbeats but
   never takes over (F1; a non-voting member of Raft starts no
   election). Its answer to a recovery says `recovering`. A client
   request gets `client_errc::recovering`. Execute-Verify runs no work
   and sends no answer until the node is in its accept phase (F4).
4. The node answers a prepare as now. Its promise and its `prepare_req`
   carry the new field `rejoin_above`, an optional ballot (model field
   `rejoin`; F2). A promise that the node sends again from
   `answered_prepare_` carries it too (F3). The wire format of the
   promise and of the prepare request changes.
5. A leader in its prepare phase drops a promise that carries
   `rejoin_above`: it neither counts nor records it, so the node is no
   promised follower and no source of `max_promise_sync` (F3; model
   `m.rejoin => phase = accept`). A leader in its accept phase takes it,
   resets `accepted_indexes` of that node, and sends the accept sync
   (F7).
6. The node takes an accept sync only when its ballot is above
   `rejoin_above` (model `HandleAcceptSync`). After it, the node clears
   `rejoin_above` and votes as any follower. Before it, the node accepts
   and decides nothing: `accept_decide` and `decide` need the phase
   `accept`, and only an accepted accept sync enters that phase (F6).
   A heartbeat ack of the node is true, because its raised promise is
   durable (F8).
7. Why the stale accepted index of item 5 does no harm (F7, inferred):
   a node that accepted in `n_leader` before its crash has
   `rejoin_above >= n_leader`, because f + 1 answers of 2f peers meet
   every majority that holds the node.
8. Provisional (owner card 14.7): a leader in its accept phase that gets
   a promise or a `prepare_req` whose `rejoin_above` is not below its
   own ballot starts a new ballot in the same step, with
   `n = max(n_leader.n, rejoin_above.n) + 1` and its own pid (F2, F9).
   Each rejoin gives at most one more ballot. With three nodes, two
   nodes that rejoin at the same time wait on each other; this is a
   case for the operator, as a majority that starts at the same time.
9. Tests: the TLC trace of `StoreRollback` (a decided entry lost) as a
   test of `sequence_paxos` that fails without items 5 and 6; a restart
   under a stable leader that votes again after item 8; a rejoining node
   that gets a prepare and an accept sync of the same ballot as its
   answers and stays out; a rejoining node that wins no takeover; a
   `rejoin_above` above `n_leader`; a promise of a rejoining node in the
   prepare phase that changes neither `max_promise_sync` nor the
   promised followers; a resent promise with `rejoin_above`; no work
   and no counted answer of a rejoining node; no `decide` and no
   `accept_decide` before the accept sync; a restart during the rejoin;
   two rejoining nodes; the new fields on the wire, and a truncated
   input that fails as a value.
10. The OneEnvironment runs end at depth 29 to 30; the trace of review 2
   needs about 33 steps. Only `MC_rr_newleader_b2` is a complete search
   (F10).
