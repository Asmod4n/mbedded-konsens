# Review 2 of doc/plan-sqlite-storage.md

## Task, as I understood it

Goal: find faults in the plan at dd2c16c: wrong claims, safety gaps,
model against text, open points with no owner, rule violations. Place:
/home/claude/mk-sqlite, SQLite 3.53.4 in the scratchpad, the named
models and results. The owner decisions of the task hold; I check only
how the plan states them. I edit, commit and push nothing, propose no
new feature, and ran no TLC.

## Blocking

### B1. 7.3 (l. 432-434): "an older state that is whole in itself" is false after a partial checkpoint

- What is wrong: a PASSIVE checkpoint copies frames up to `mxSafeFrame`
  into the database file (`wal.c:2254-2321`). It sets `nBackfill`
  (`wal.c:2337`) and syncs the database file only at the end of the WAL
  (`wal.c:2328-2334`). The WAL stays. Suppose a frame at or below
  `mxSafeFrame` is damaged later. At the next start, recovery stops at
  that frame (`wal.c:1510-1511`) and sets `nBackfill = 0`
  (`wal.c:1579-1580`). The database file still holds the pages of the
  commits after the cut. A reader then sees WAL frames before the cut
  and pages of the database file from after it. The result is a mix of
  commits, not an older state. Example: the `meta` page comes from
  commit k and a `data` page from commit k+5. Each record's CRC holds,
  because each record is whole.
- The models: both models roll back to one earlier whole state
  (`OneEnvironment.tla:335-350`, `RecoveryRollback.tla:110-119`). So
  neither model has this fault.
- Fix: 7.3 states the mixed case. The owner decides whether the start
  checks must find a mix (meta against log against state) or whether
  the fault model leaves it out. The fault test of 8.4 ("a byte flipped
  in an early frame") also runs after a partial checkpoint.

### B2. 7.3 (l. 440-445) against OneEnvironment: the model of the fix takes an old accept sync

- What is wrong: in `RecoveryRollback` "newleader", the node votes
  again only at an accept with `m.b > p`, where `p` is the highest
  promise, its own included (`RecoveryRollback.tla:132,136-139`). That
  is the only complete run (MC_rr_newleader_b2). In OneEnvironment,
  `RecoveryWaitsForLeader` takes any accept sync whose ballot equals
  the recovered promise (`OneEnvironment.tla:190-200`, with
  `:387-388`). That includes an accept sync that the leader sent before
  the rollback.
- A trace by hand, not run in TLC (3 nodes, MaxBallot 2, MaxLog 1,
  MaxCrash 1): n1 leads ballot 1. n2 takes the accept sync and the done
  of r1 in one sync, and r1 is chosen. RollbackCrash takes n2 back to
  "promise 1, acc 0". n2 recovers with promise 1 ("rejoining"). The old
  accept sync (b 1, empty log) is still in `msgs`, and `acc 0 # 1`, so
  n2 takes it and is "running" with an empty log. n2 leads ballot 2
  with n3, `Best` gives the empty log, and r2 is chosen at index 1.
  ChosenOnce fails at about 33 steps. The runs stopped at depth 29
  (`20261026T185400Z-one-environment-rollback.md:268-276`).
- The models also roll back only one sync (`prevDur`). Plan 7.3 says
  that the damage drops "that commit and every later one". With FULL,
  each commit is one sync.
- Fix: the model takes the rule that was checked (ballot above `p`, or
  an accept sync that answers the rejoin promise). The rollback goes to
  any earlier durable state. Then rerun, and give the result of
  MC_rr_newleader_b3_c2, which has a config and no result. Until then,
  7.3 says "holds in one complete small run", not "holds in the models".

### B3. 7.3 and section 3: the current recovery is not the form that was modelled, and the new form has no path when several nodes start at once

- What is wrong: 7.3 calls the current form "the vote after f + 1
  answers" (plan-one-environment.md 8.1 item 1, "as now"). The code has
  two other paths (`sequence_paxos.hpp:1610-1665`, `:1756-1768`):
  1. A majority of answers that are not "recovering", plus the primary
     of the highest promise: the node takes the primary's log.
  2. All peers answered and none is a primary: the node takes the best
     of all the logs. Path 2 is the "answerlog" form that TLC broke.
  Neither path is modelled as coded.
- The section 3 table does not replace plan-one-environment.md 8.1
  item 1. So wrong assumption (1) still stands in a plan in force.
- What is missing: the model lets only "running" nodes answer
  (`OneEnvironment.tla:377-382`), and it needs f + 1 answers from
  peers. Take N=3 and a power cut of two or three nodes. The cut drops
  no frame. Each node that starts needs two running peers, and the
  others are "recovering", so no node ever votes again. SQLite gives
  no signal of a rollback (section 1), so every start goes through
  recovery. The models check no liveness, so TLC cannot show this.
- Fix: section 3 lists 8.1 item 1 as replaced by 7.3. 7.3 names the
  code paths that it removes. The owner decides the form for a start
  where fewer than f + 1 peers are running. This is a new design and
  an error path, so it is a card.

### B4. 6.3 with 6.1: "done" before the leader's commit, and a commit that fails with the "full" value

- What is wrong: `SQLITE_FULL` is a value, and the server keeps serving
  (6.1). Suppose the leader sent "done" for index i and its `commit`
  then gives `SQLITE_FULL`. SQLite rolls the step back
  (`vdbeaux.c:3378-3384`). The followers hold the entry at i. The
  leader's log ends at i-1, and the leader can propose another request
  at i in the same ballot. The model counts "synced" by `idx >= i`
  (`OneEnvironment.tla:288`), so the old "synced" of the followers
  counts for the new entry. The model has no commit that fails: Commit
  always succeeds (`:77`).
- Fix: the plan names the path of a leader whose commit fails after its
  "done" left. One example is "the node leaves the ballot and stops
  voting until it recovers". This is an error path for the owner. The
  model then gets a commit that fails (with DoneBeforeOwnSync).

### B5. Section 1 (l. 73-74) and 5.1 item 3: `SQLITE_FULL` does not always roll back the whole step

- What is wrong: with a statement journal, `SQLITE_FULL` and
  `SQLITE_NOMEM` roll back only the statement (`vdbeaux.c:3376-3377`).
  Autocommit then stays 0. The C file of section 1 used single-row
  inserts, which use no statement journal. A multi-row statement uses
  one, for example the SQL of a task or `set_entry_states`. 5.1 item 3
  says only what happens when autocommit is 1. When autocommit is 0,
  the step goes on without the failed statement, and it can commit.
- Fix: section 1 states both cases with the line. 5.1 item 3: every
  statement that fails closes the transaction object, whatever
  `sqlite3_get_autocommit` gives. A test runs a multi-row statement
  into a full disk.

### B6. 4.1 item 3 and 4.3 item 8 against 7.7: `.recover` cannot rebuild a table with `CHECK(cbor_valid(x))`

- What is wrong: the distro `sqlite3` does not have the library's
  functions. A test with Python, SQLite 3.45.1 (not 3.53.4, not
  checked there): `CREATE TABLE t(k INTEGER PRIMARY KEY, x BLOB
  CHECK(cbor_valid(x))) STRICT` on a connection without the function
  gives "no such function: cbor_valid". The index on `cbor_extract`
  fails too. So `sqlite3 clone.db .recover | sqlite3 new.db` loses each
  user table that has a CBOR column.
- Two owner decisions conflict here: "CBOR in JSONB form with a CHECK"
  and ".recover of the distro sqlite3".
- Fix: a card for the owner. The plan states the conflict in 7.7 and
  4.1 item 3.

### B7. Text that contradicts the owner decisions (the build reads it)

- 5.3: "Proposal: `find` returns a `std::vector`". The owner decided on
  a pointer (9, answer 1).
- 4.1 item 2: "each field gets a native column". 10.2 decided on one
  BLOB in a rowid table. 4 item 4 still calls the layout an arm.
- These sections still carry "card open" after the owner decided:
  4.1, 4.2, 5.3, 6.3, 6.5 and 6.7. The same holds for the section 3
  table rows and the section 11 rows 1, 3, 5, 6, 12, 13, 15 and 16.
- Section 9: "Cards open" 1-9 are all answered. "17:26 ... CBOR with
  tag where none fits" is replaced by 18:40 and 19:05.
- Section 9, answer 6, says "Section 4.3 item 4 and 5 fall away". But
  4.3 item 4 holds rules that stay: the excluded functions,
  `SQLITE_PREPARE_NO_VTAB`, and the total order of rows.
- Fix: rewrite these places to the decided form, and keep each card
  only in section 9 as history.

## Others

### O1. 6.4 and 6.5: a checkpoint at close is not counted, and its failed sync is not raised

The last connection to close runs a PASSIVE checkpoint, and that
checkpoint syncs the database file (`wal.c` in `sqlite3WalClose`, the
call `sqlite3WalCheckpoint(..., SQLITE_CHECKPOINT_PASSIVE, ...)`). When
it fails, the WAL stays, and no error reaches the library. The rule
says that a failed sync is raised. Fix: the plan names the close. The
option `SQLITE_DBCONFIG_NO_CKPT_ON_CLOSE` exists (`main.c:992`), or the
library calls the checkpoint before the close.

### O2. 6.5: `PRAGMA wal_autocheckpoint=0` removes the library's own WAL hook

The pragma calls `sqlite3_wal_hook(db, 0, 0)` (`main.c:2510-2524`). A
connection has one hook. If the pragma runs after the library sets its
hook, the count of frames is lost. Fix: name the order, or leave out the
pragma, because the library's hook replaces the default one. The
number "1000 frames" needs a name (rule "A number has a name").
"`SQLITE_FULL` from it is the 'full' value": the step is already
committed, so the plan says who gets that value.

### O3. 6.1: `SQLITE_BUSY` as `std::logic_error`

`SQLITE_BUSY` also comes from another process that holds a lock, for
example the operator's `sqlite3` shell. A PASSIVE checkpoint gives
`SQLITE_BUSY` when another connection runs a checkpoint. Both come from
outside the library, so by the rule each is a value. Fix: the row says
for which source each mapping holds.

### O4. 6.3 (l. 379): "The model of section 10.1 shows that this keeps safety"

10.1 is still to do. The model does not yet have the sync inside the
commit. MC_oe_rollback_waits_parallel_l1 (DoneBeforeOwnSync TRUE) ended
at its time limit. Fix: "the bounded runs found no violation; the model
of 10.1 is to do".

### O5. The rejoin mark across a second crash, and the open details of 7.3

- In the model, the mark is not stored. Each Restart enters
  "recovering" again (`OneEnvironment.tla:354-359`), so a second crash
  does not lose it. This was never checked: MaxCrash is 1 in all the OE
  and RR configs that have results.
- The plan does not say that the mark is not stored, or that recovery
  runs at every start.
- The model has rules that the plan does not state:
  - A recovering or rejoining node gives no recovery answer (`:379`).
  - Such a node does not lead (`:133`).
  - It becomes "running" at the commit of the accept sync, before the
    sync (`:200`). The plan says "after that accept sync".
- Fix: state these rules in 7.3, and run an OE config with MaxCrash 2.

### O6. 7.8: the digest has no definition

The model compares the whole states exactly, and it takes the peer as
whole (`ChangesetRepair.tla:33,74`). The plan names no function, no row
order and no set of tables. `sha3()` is absent from the package (section
1). A session leaves out a table without an explicit primary key
(`sqlite3session.h:192-200`), so such a user table always needs the
full copy. The conflict handler must abort, as the model does
(`:60-65`), and the plan does not say so. Fix: state these three points.

### O7. 10.2: a decision on a run that breaks the bench rules, and a psow result that the text does not show

- The result file says: release build only, nice 0, "a TLC run of 1
  worker may share the machine", automatic checkpoint (header lines).
  These break the rules "Every speed question is asked of every
  build" and "The bench takes the machine". 10.2 lists only the first.
  The table of 8.1 lists its faults; 10.2 does not.
- psow, 1 entry per commit, commit time per commit:

  | arm | median | min to max |
  |---|---|---|
  | default | 230.2 µs | 200.9-237.9 µs |
  | base_a (psow=0) | 161.3 µs | 145.2-186.4 µs |
  | base_b (psow=0) | 155.9 µs | 144.2-176.9 µs |

  The ranges do not overlap. The difference is above the 10 %
  threshold of 10.2 itself. psow1 always ran first or last in its
  round. "costs no measurable time" is not what was measured.
- Fix: write the measured difference with "cause not known", and
  repeat the run with every build and with priority before the layout
  is final.

### O8. 7.7: the node is not stopped for the copy

A copy of the database file and the `-wal` file made during a
checkpoint is not one state. Fix: one sentence saying the node is
stopped.

### O9. Section 2: file mode is `persistent_storage.cpp:28`, format version `:38`

## Checked and correct

All other SQLite lines of section 1 match 3.53.4. Section 2 rows `:557`,
`:670-675`, `:725-738`, `:1367-1372` and `:1498` match the code. 7.3 and
7.8 do not rest on wrong assumptions (2) and (3). Assumption (1) still
stands through plan-one-environment.md 8.1 (B3). Not checked: the trace
of B2 in TLC; B6 on 3.53.4; whether a caller of `find` holds two spans
at the same time.
