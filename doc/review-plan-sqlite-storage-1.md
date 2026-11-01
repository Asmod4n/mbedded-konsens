# Review 1 of doc/plan-sqlite-storage.md

Branch `sqlite-storage` of mbedded-konsens, plan commit a966150.
Reviewer: a second agent. No file of the repository was changed.

## Task, as I understood it

Goal: find the faults in the plan "the storage on SQLite". Check each
claim against the store before SQLite, plan-one-environment.md, the SQLite source
at `version-3.53.4` (b09c88c), the measurement and the rules.

Place: the plan, `src/persistent_storage.cpp`,
`include/konsens/{storage,persistent_storage,sequence_paxos,state_machine,node}.hpp`,
the SQLite checkout outside the tree, `bench/results/2026-10-26-174119Z-sqlite-sync.txt`
and its source.

Assumptions: the owner decisions of 2026-10-26 hold. I check only that
the plan writes them correctly.

Not done: no change in a repository, no commit, no push. No speed
measurement.

## What I checked and found correct

- The tag `version-3.53.4` is commit b09c88c.
- `pager.c:3659-3693`: `OFF` sets no sync flag. `wal.h:25-26`: the two
  macros. `wal.c:4195-4215`: the commit sync under `FULL`.
- `wal.c:2277`: the WAL sync before the backfill. `wal.c:2333`: the
  database sync after it. See finding 3 for the condition on it.
- `wal.c:1510-1511`: recovery stops at the first frame that does not
  decode.
- `wal.c:4212` is the commit sync. `wal.c:4238-4259` append the frames
  to the WAL index and write its header after that sync. So under
  `FULL` a second connection sees a commit only after its sync (6.4).
- `sqlite.h.in:928-932` (`SQLITE_FCNTL_JOURNAL_POINTER`), `:4484`
  (`SQLITE_PREPARE_PERSISTENT`), `parse.y:195` (`SAVEPOINT`),
  `parse.y:1705` (`VACUUM INTO`), `cksumvfs.c:299-324`,
  `sqlite3session.h:446` (`sqlite3session_diff`).
- `DEFAULT_WAL_SYNCHRONOUS=2` is `FULL` in the zero-based numbers of
  the pragma (`sqliteInt.h:1460-1472`).
- -0.0 in a `REAL` column comes back as +0.0; an `ANY` column keeps the
  sign. I repeated this with a C file outside the tree (libsqlite3
  3.45.1 of the host, not 3.53.4).
- `VACUUM INTO` keeps the rowids (`insert.c:3288-3292`, the flag
  `DBFLAG_VacuumInto`). It uses the safety level of the source for the
  output (`vacuum.c:248`).
- The session extension records changes of `WITHOUT ROWID` tables: the
  preupdate hook fires for them (`sqlite.h.in:10841-10845`).
- The table of section 8.1 agrees with the summary lines of the result
  file. 934.5 / 854.7 is 9.3 %.

## Findings

The former form of the findings below is in the git history at ebd9f42.

### 1. Blocking: `find` returns a span that the next step of the statement frees

Section 4.4, 5. The concept returns
`std::optional<std::span<const std::byte>>` from `find`
(`storage.hpp:179`, `state_machine.hpp:37-45`). With SQLite, the pointer of
`sqlite3_column_blob` is valid only until the next `sqlite3_step`,
`sqlite3_reset` or `sqlite3_finalize` of that statement. Section 4.4
calls `sqlite3_reset` after each step, and one prepared statement
serves every `find`. A state machine that holds two results of `find`
reads memory that is not alive, or the bytes of the second key.
`node.hpp:1175` holds one span and is safe today; a user state machine
need not be.

Evidence: a C file outside the tree. Two calls of one persistent
statement gave the same address for two different values.

Proposal: `find` copies the value into storage that the caller owns
(for example `std::vector<std::byte>`), or the plan names another form
that a wrong use cannot misuse. The concept changes; the owner decides
the form. The copy cost goes into the measurement of section 10.

### 2. Blocking: `SQLITE_FULL` or `SQLITE_IOERR` inside a savepoint rolls back the whole step, and later statements commit alone

Section 5.1, 6.6. `sqlite3VdbeHalt` treats `SQLITE_NOMEM`,
`SQLITE_IOERR`, `SQLITE_INTERRUPT` and `SQLITE_FULL` as special
errors. Unless the statement has a statement journal, it calls
`sqlite3RollbackAll`, closes all savepoints and sets `autoCommit = 1`
(`vdbeaux.c:3355-3386`). Each later statement of the step then runs in
autocommit mode: one transaction with its own sync for each statement.
A part of a step becomes durable. This breaks "one write transaction
for each step" (plan-one-environment.md 3.4). The owner rule says that
a full store is an error value and the server keeps serving, so the
step goes on after the error.

Evidence (C file outside the tree, `max_page_count` to force
`SQLITE_FULL`): `BEGIN IMMEDIATE`, insert 1, `SAVEPOINT run`, an
insert fails with code 13, `sqlite3_get_autocommit` is 1, insert 3
commits alone, `ROLLBACK TO run` gives "no such savepoint", `COMMIT`
gives "no transaction is active". Insert 1 is lost, insert 3 is
durable.

Proposal: after each statement that fails, the store reads
`sqlite3_get_autocommit`. When the transaction is gone, the
transaction object enters a closed state. Each later call returns the
error without SQL, and `commit` returns it. The nested transaction
does the same. A test forces `SQLITE_FULL` in a savepoint and checks
that no write of the step is in the file.

### 3. Blocking: the automatic checkpoint drops its own error, and the next checkpoint repeats a failed sync

Section 6.1, 6.5. The default WAL hook calls `sqlite3_wal_checkpoint`
and returns `SQLITE_OK` whatever the result (`main.c:2484-2496`). So
the sentence "A failed checkpoint throws `sync_error` too" cannot hold
with "the automatic checkpoint at its default".

The consequence: when the sync of the database file fails in a
checkpoint, `nBackfill` stays (`wal.c:2333-2338`). The next automatic
checkpoint copies the pages again and syncs again. If that sync
succeeds, the WAL restarts and its frames are overwritten. After a
failed `fsync`, the kernel can drop the dirty pages and report success
on the next `fsync`. The pages of acknowledged commits can then be
missing from the database file, and their frames are gone. The owner
rule (2026-10-21) says that nothing repeats a failed sync.

Also: the database sync at `wal.c:2333` runs only when the checkpoint
reached the end of the WAL (`wal.c:2328`). A partial checkpoint does not
sync the file. This is safe, because the WAL restarts only after a full
checkpoint. Section 1 should name the condition.

Proposal: `PRAGMA wal_autocheckpoint=0`. The library calls
`sqlite3_wal_checkpoint_v2(..., SQLITE_CHECKPOINT_PASSIVE, ...)` after
the commit, and after the sends of the step, so that its time is not in
the path of the reply. `SQLITE_IOERR_FSYNC` from it throws `sync_error`.
`SQLITE_FULL` from it is the "full" value (the database file grows in a
checkpoint), not a `sync_error`; otherwise a full disk ends the process
on each start. No C++ exception crosses a callback that SQLite calls.
This is still "SQLite syncs itself"; the owner should confirm that the
explicit checkpoint call fits the decision of 17:54.

### 4. Blocking: "`SQLITE_IOERR` with `ENOSPC`" as the full value takes in a failed fsync

Section 6.6. The unix VFS maps `ENOSPC` of a write to `SQLITE_FULL`
already (`os_unix.c:3710-3716`). A failed `fsync` gives
`SQLITE_IOERR_FSYNC` for every errno, `ENOSPC` included
(`os_unix.c:3930-3934`). Some file systems report `ENOSPC` from
`fsync` (delayed allocation, thin storage). Under 6.6 such a failed
sync becomes a value, the server keeps serving, and a later sync can
report success over lost pages.

Proposal: only `SQLITE_FULL` is the "full" value. Every
`SQLITE_IOERR_FSYNC` and `SQLITE_IOERR_DIR_FSYNC` throws `sync_error`,
whatever `sqlite3_system_errno` says. The store opens with extended
result codes (`SQLITE_OPEN_EXRESCODE`). Section 6.6 and a test of
`error_fs` with `ENOSPC` from `fdatasync` say so.

### 5. Blocking: `FULL` does not pad the commit frame on Linux by default; section 1 and Q9 say it does

Section 1 (bullet on padding), section 9 Q9. `walOpen` turns padding
off when the file has `SQLITE_IOCAP_POWERSAFE_OVERWRITE`
(`wal.c:1721`, `wal.c:1738-1740`). The unix VFS sets that flag unless
the URI parameter `psow=0` or the compile option
`SQLITE_POWERSAFE_OVERWRITE=0` says otherwise
(`os_unix.c:6104-6107`, `os_unix.c:4367-4370`, `sqliteInt.h:373-374`;
the comment at `os_unix.c:4470-4478`). The VFS decides, not the file
system. So the decided form (`FULL`) has the same exposure as Q9
describes for `NORMAL`: the next commit rewrites the sector that holds
the end of the last synced commit frame. A torn write of that sector
makes recovery stop before that frame (`wal.c:1510-1511`), and an
acknowledged commit is lost.

Not checked: the compile options of the package for
`POWERSAFE_OVERWRITE`. `pragma compile_options` lists it only when it
differs from the default.

Proposal: correct the bullet of section 1 and Q9. Q9 becomes: open the
store and the node storage with `psow=0` (padding to the sector, cost
measured), or keep the default and accept the exposure. The VM of the
session cannot cut its power (plan-one-environment.md 15 item 9), and
`error_fs` cannot tear a sector (finding 14). The owner decides.

### 6. Blocking: the record is not CBOR now, and sections 4.3 and 4.5 contradict each other

Section 4.3, 4.5, 4.7, 7.7. Section 4.3 says "The encoding of a record
does not change: CBOR and its CRC tail". Section 4.7 says the CRC
covers the deterministic CBOR encoding "as now". The code encodes an
entry as a kind byte and little-endian fields (`storage.cpp:78-97`),
not as CBOR. The meta record, the session record and the repair record
are fixed little-endian forms with a CRC
(`persistent_storage.cpp:235-310`, `:405-418`, `:762-805`). The tail of
a log record holds the entry CRC, the chain CRC, the state and the
state CRC (`persistent_storage.cpp:47-53`).

Section 4.3 also says "Values are `BLOB`", while 4.5 (owner, 17:26)
puts each field in its native column type. Section 7.7 speaks of "its
CRC tail" in a changeset, which exists only in the `BLOB` form.

The entry CRC travels on the wire ("done" carries it,
plan-one-environment.md 10 item 2). A change of the encoded form
changes every CRC on the wire.

Proposal: section 4 states the current forms as they are. It then
states, for each table: the columns and their types, which bytes each
CRC covers (the current binary form or a new CBOR form), where the CRC
and the state CRC stand (columns), and whether the wire format and
`format_version` (now 4, `persistent_storage.cpp:39`) change. The
owner confirms whether "CBOR" in 17:26 means a change of the entry
encoding.

### 7. Should fix: a damaged frame in the middle of the WAL drops every later commit without an error

Section 1, 7.1. Section 1 says "A torn tail of the WAL is lost, and the
frames before it stay." Recovery stops at the first frame that does not
decode (`wal.c:1510-1511`), wherever it stands. A bit flip in a synced
frame drops that commit and all later commits until the next
checkpoint. SQLite reports no error. The start check of
plan-one-environment.md 9 sees an older, self-consistent store.

Proposal: section 1 and 7 name this. The plan states which rule keeps
Paxos safe when a store silently goes back to an older state (the vote
after f + 1 answers of plan-one-environment.md 8.1 is the candidate;
not checked that it covers a lost promise). The model of the plan
includes this fault. A test flips a byte in an early frame of a WAL
that holds several commits and restarts the node.

### 8. Should fix: the error codes of SQLite have no table, and the damage claim was not checked

Section 6.5, 6.6, 7.1. Section 7.1 says that `SQLITE_CORRUPT` is a
damage. `error_is_a_damage` (`sequence_paxos.hpp:83-88`) lists no code
of the store. So the claim is not shown by the code.

Proposal: one table in the plan, from SQLite code to konsens error:
`SQLITE_CORRUPT*` and `SQLITE_NOTADB` to a damage code that
`error_is_a_damage` lists; `SQLITE_FULL` to `no_space_on_device`;
`SQLITE_IOERR_FSYNC` and `_DIR_FSYNC` to `sync_error`; other
`SQLITE_IOERR_*` to `write_error` or `read_error`; `SQLITE_TOOBIG` to a
value (finding 18); `SQLITE_BUSY`, `SQLITE_MISUSE`, `SQLITE_CONSTRAINT`
on a key that the library computed to `std::logic_error`. The rule "An
error goes to whoever can repair it" decides each row.

### 9. Should fix: the read connection takes a new snapshot for each statement

Section 5.2. A SQLite
connection in autocommit mode takes a snapshot for each statement.
`get_meta` and `get_entries` of one `read_only_transaction` can then see
two states. A deferred `BEGIN` takes its snapshot at the first read,
not at `BEGIN`. A reader that stays open also stops each checkpoint at
its snapshot, and the WAL grows.

Proposal: `begin_read_only` runs `BEGIN` and one read (the meta record)
at once. The destructor runs `COMMIT`. A test opens a reader, commits a
step through the writer, and checks that the reader still sees the old
meta record.

### 10. Should fix: `SAVEPOINT` and `ROLLBACK TO` alone do not end the child

Section 5.1. `ROLLBACK TO x` leaves the savepoint open. The abort of
the child is `ROLLBACK TO x` and then `RELEASE x`; its commit is
`RELEASE x`. On one connection, a write of the parent while the
savepoint is open lands in the savepoint. The guard
`throw_if_nested_is_open` (`persistent_storage.cpp:670-675`) must stay
on every method of the parent.

Proposal: section 5.1 names the three statements and keeps the guard.

### 11. Should fix: items of the inventory are missing

Section 2. Not listed:

- File mode 0600 (`persistent_storage.cpp:29`). SQLite creates the file
  with `SQLITE_DEFAULT_FILE_PERMISSIONS` (0644 without the umask). The
  WAL and the shared memory file take the mode of the database file.
  The node storage holds the hidden identity. Proposal: the library
  creates each database file with mode 0600 before `sqlite3_open_v2`.
- The format version and the check that a new store is empty
  (`persistent_storage.cpp:39`, `:694-722`, `storage_errc::unsupported_format`).
- `put_entries` (`persistent_storage.cpp:557`) refuses an index below
  the last one. An `INSERT` accepts it.
- The parameter `map_size` of `open` (`persistent_storage.hpp:156`) and
  its callers (`examples/counter.cpp:83`, `examples/three_nodes.cpp:78`,
  `examples/counter_stress.cpp:104`). A full store comes from this
  bound, not from the disk. Section 6.6 names "the limit X of free
  space" but not what replaces `map_size` (`PRAGMA max_page_count` is
  the SQLite form).
- `test/fuse/error_fs.cpp` injects write and read faults only for the
  one data file of the store (`error_fs.cpp:17-28`, `:59`).
- The destructor of the store ends the process when a transaction is
  open (`persistent_storage.cpp:1367-1372`).

### 12. Should fix: the plan does not list what `FULL` removes from plan-one-environment.md

Section 3, 6. Section 3 keeps plan-one-environment.md except 3.5 item
3, 3.6 and 8.2 item 3. With a sync inside `commit` these parts also no
longer apply: 3.4 item 2 ("sync only", a bare `fdatasync` of a
descriptor that no longer exists); 3.5 items 1, 4 and 6 (io_uring, the
writer lock through the sync, slipstreamIO); 4.3 item 1 (the decided
index now has a sync); 5.1 items 2 to 4 and 9 item 6 (the tombstone by
file system); 15 items 6 to 8; and the
owner decisions 18 items 4, 5 and 6.

Decision 18 item 4 (the leader sends "done" while its own sync runs)
needs a statement: with the sync inside `commit` in the thread of the
caller, the leader can send "done" only before `commit`, that is
before the write. The model must show that this keeps safety.

Proposal: section 3 lists each of these items and says what replaces
it. The owner confirms the change of decisions 18 items 4 to 6.

### 13. Should fix: determinism of Execute-Verify with SQL state

Section 4.1, 9 Q8. Section 4.1 allows relations, joins and virtual
tables in the state. plan-one-environment.md 11 item 4 says the
`state_machine` concept does not change, and that concept offers only
`find` of a byte key (`state_machine.hpp:28-31`). The plan does not
say how a task reaches SQL. When it does, these sources make honest
nodes give different results:

- Row order without a total `ORDER BY` depends on the query plan, the
  version and `sqlite_stat1`.
- Functions that are not deterministic: `random()`, `randomblob()`,
  `changes()`, `last_insert_rowid()`, `sqlite_version()`, the date
  functions with `'now'`.
- Results that depend on the version: `sum()` and `total()` use the
  Kahan-Babuska-Neumaier sum (`func.c:1873-1896`), which older versions
  do not use. Q8 lets each node take the version of its distribution.
- A rowid table whose largest rowid is the maximum gets random rowids
  (`vdbe.c:5672-5692`).
- A plain `VACUUM` can renumber rowids of a table without an
  `INTEGER PRIMARY KEY` (`insert.c:3288-3289`; `VACUUM INTO` keeps
  them).
- A virtual table or a function of the binary differs between two
  versions of the binary.

Proposal: the plan names the interface of a task to SQL. It forbids
these sources: an authorizer (`sqlite3_set_authorizer`) with a list of
allowed functions and no `ATTACH`, `PRAGMA` or `ANALYZE` from a task;
`SQLITE_PREPARE_NO_VTAB` (`sqlite.h.in:4486`) for the statements of a
task where 4.1 item 3 forbids a table; a rule that every query of a
task orders its rows totally. All nodes of one cluster run one SQLite
version and one binary version, and the handshake compares them.

### 14. Should fix: the tests of section 8.3 do not fit the form or the tools

Section 8.3. "A power cut between commit and sync" has no "between"
when SQLite syncs inside `commit`. `error_fs` gives `EIO` for `fsync`,
`ENOSPC` for writes of the data file, and `EIO` for reads in a range
(`error_fs.cpp:17-28`). It cannot drop writes after the last sync, and
it cannot tear a sector. plan-one-environment.md 15 item 7 already
marks this as not checked.

Proposal: the test says "a power cut during the commit, before its
sync returns". `error_fs` gets a switch that drops every write after
the last sync of each file, and its faults name the SQLite files
(store, `-wal`, `-shm`). Not checked: whether a writable shared `mmap`
of the `-shm` file works on `error_fs`.

### 15. Should fix: integer keys above 2^63-1

Section 4.5. The log index, the session id (the index of its
registration, `node.hpp:1284`), `client_id` and `pid` are `uint64_t`.
A `STRICT` `INTEGER` key holds only -2^63 to 2^63-1. Section 4.5 puts a
larger value in CBOR, but one key column cannot hold `INTEGER` and
`BLOB` in a `STRICT` table. A cast to `int64_t` makes large values
negative, and a range scan of the log (`get_entries`, the delete from
an index) then returns them in the wrong order. An index comes from a
peer on the wire.

Proposal: one check for safety, one named bound (2^63-1), at the place
where an index or a session id enters the library; a value above it is
an error value. Or the key stays a big-endian `BLOB` as now
(`persistent_storage.cpp:137-142`). The owner decides.

### 16. Should fix: the changeset repair has no mechanism

Section 7.7. `sqlite3session_diff` compares two databases that one
connection holds (`sqlite3session.h:446`). The peer does not hold the
store of the node that repairs. A session that records every write
since a snapshot runs the preupdate hook on the path that succeeds,
for every write, the rolled-back writes of the child included
(plan-one-environment.md 5.2). A session ignores a table without an
explicit `PRIMARY KEY` (`sqlite3session.h:192-200`) unless
`SQLITE_SESSION_OBJCONFIG_ROWID` is set (`sqlite3session.h:113-126`).
A changeset carries no `CREATE TABLE`. Not checked: whether a change
inside a savepoint that is rolled back appears in a changeset.

Proposal: the plan says who makes the changeset, from which two
states, and when. Every table of the state has an explicit primary key,
or the session sets `SQLITE_SESSION_OBJCONFIG_ROWID`. A test checks
that a rolled-back savepoint leaves no change in the changeset. The
cost of the hook goes into section 10.

### 17. Should fix: the copy needs a sync of its directory and its own connection

Section 7.3. `VACUUM INTO` refuses to run inside a transaction
(`vacuum.c:169-172`), so it needs a connection with no open
transaction. It runs in the thread of the caller for the whole file.
SQLite syncs the directory only for a new journal or WAL
(`os_unix.c:6569-6573`, `:6777`), not for the new output file. The
backup interface gives one snapshot only when one
`sqlite3_backup_step(-1)` copies all pages; with smaller steps, a
commit of the writer connection restarts the copy.

Proposal: section 7.3 names the connection and the thread. The
receiver syncs the file and its directory before the switch in the
node storage. The backup interface, if used, runs with `-1`.

### 18. Should fix: the size limit of a value has no name

Section 1, 4. The package has `MAX_LENGTH=1000000000` (section 1). A
value or a row above `SQLITE_LIMIT_LENGTH` gives `SQLITE_TOOBIG`. A
client payload, a result or a log row can reach it. The rule "Each
limit has one name, and its hard bound is one named constant" applies.

Proposal: the plan names the bound of a payload and of a row, derives
it from the limit of SQLite, checks it once where a request enters, and
gives an error value above it.

### 19. Minor: one line citation is wrong

Section 1. `sqlite.h.in:9546` is the comment of the backup object. The
declaration of `sqlite3_backup_init` is at `sqlite.h.in:9749`.

### 20. Minor: statements have no parameter types; an `ANY` column checks nothing

Section 4.1 item 1, 4.5. A prepared statement accepts each type for
each parameter. Only a `STRICT` column checks, and it converts a value
that it can convert without loss (text `'12'` into `INTEGER`). An `ANY`
column accepts all values; there, the storage class of the value, not
the column type, keeps `true` and `1` apart.

Proposal: correct the two sentences.

### 21. Minor: the claim on integers above 2^63-1 depends on the bind call

Section 4.6. `sqlite3_bind_int64` takes `sqlite3_int64`; a C++ value
above 2^63-1 wraps to a negative value before SQLite sees it. A value
becomes `REAL` only through a literal, text or `sqlite3_bind_double`.

Proposal: name the bind call that the measurement used.

### 22. Minor: stale text after the decisions

Section 4 item 1 says "The number of files is open (section 9, Q5)";
Q5 is decided. Q3/Q4 says "The form of section 6 stays a proposal until
the measurement"; section 6.1 is decided.

Proposal: update both sentences.

### 23. Minor: `WITHOUT ROWID` for the log

Section 4.3. A log row holds the payload and the result and can be
large. SQLite documents `WITHOUT ROWID` for small rows. A rowid table
with `index INTEGER PRIMARY KEY` keeps large values out of the key
B-tree.

Proposal: both forms are arms of the measurement in section 10. No
choice before the numbers.

### 24. Minor: the measurement of section 8.1 and the rules for a bench

Section 8.1, 10. The run had nice 0, and "a TLC run of 1 worker may
share the machine" (result file header). The rule says the run gets
priority over everything else of this user and records whether it got
it. One build (g++ 16 -O2) only; the rule asks every build. The arm
"full" prints `checkpoints=0` because the bench does not count the
automatic checkpoint; the file should say so. The arm "caller" uses
`RESTART`, the automatic checkpoint is `PASSIVE`. The bench does not
check the result of `sqlite3_prepare_v3`. Section 8.1 should say "no
difference above the noise floor of 9.3 %" in place of "gains
nothing". Section 10 says "one `fdatasync` of the store for each step";
with `FULL` each checkpoint adds a sync of the WAL and of the database
file.

Proposal: correct the wording now. Apply the rules in the run of
section 10.

### 25. Minor: `.recover` on a clone needs the WAL

Section 7.5. A clone of the database file without its `-wal` file
lacks every commit since the last checkpoint. The command
`sqlite3 clone .recover` writes SQL text, which a second `sqlite3`
reads into a new file. When `sqlite3` opens the original and closes it
as the last connection, it checkpoints and deletes the WAL.

Proposal: section 7.5 says that the clone holds both files, and gives
the full pipeline.

### 26. Minor: visibility before the sync in the later form

Section 6.2, 6.4. The statement of 6.4 holds for `FULL` only. In the
later form "the caller syncs" (`NORMAL`), the WAL index takes the frames
at the commit, before the sync of the caller (`wal.c:4195`,
`wal.c:4238-4259`). A second connection then reads data that is not
durable.

Proposal: one sentence in 6.2 for the later form.

### 27. Minor: the rule for packages names the store before SQLite

Section 8.1. RULES.md lists the store before SQLite as an exception to
"What builds the tree is the distribution's package". After the change the exception
has no object. Section 8.2 says the interface tests run "unchanged",
but the concept changes (findings 1 and 12).

Proposal: the plan names both for the owner. The owner changes the
rule text.

### 28. Minor: `synchronous` is a setting of each connection

Section 6.1. `PRAGMA synchronous` holds for one connection
(`pragma.c:1143`, `btree.c:3255`). The node storage is another
connection. The package default for WAL is `FULL`, so today the result
is the same.

Proposal: the library sets `synchronous=FULL` on every connection that
writes, reads it back at open, and refuses another value.

## Count

| severity | count |
|---|---|
| blocking | 6 (1 to 6) |
| should fix | 12 (7 to 18) |
| minor | 10 (19 to 28) |

## Verdict

Approve after the blocking fixes 1 to 6. Findings 1 to 4 concern memory
safety and the durability of acknowledged entries. Findings 5 and 6
correct facts that decide the format and an open question. The
findings "should fix" can enter the plan in the same pass. None of them
needs a new design from the reviewer; findings 1, 5, 6, 13, 15 and 16
need a decision of the owner.
