# Plan: the write path of konsensd within half of raw SQLite

Source: the owner, 2026-10-30 05:42 (UTC+1): close the gap to raw
SQLite; the goal is to come within 50 % of raw SQLite, if that is
possible. Review: doc/review-plan-write-path.md, rounds 1 and 2, both
"approve with changes". This text has the changes of both rounds.

## 1. Facts measured in this session

All on commit edfb54f or later with the same konsensd code, g++ 16
Release, 64 B values, three nodes on one machine. Result files:
bench/results/2026-10-29-220701Z-distributed-logs.txt,
2026-10-30-055400Z-write-path-beds.txt,
2026-10-30-060200Z-write-path-callgrind.txt,
2026-10-30-061200Z-write-path-steps.txt.

1. The bed changes the result. With the three data directories on one
   btrfs file system, konsensd gives 38 to 57 writes/s at 1 in flight
   and 1 355 to 2 335 writes/s at 64 in flight. With one btrfs file
   system for each node, it gives 99 to 115 and 3 389 to 3 451 writes/s.
   Both beds lie on one host disk. One sample per cell, two rounds, no
   A/A floor.
2. Raw SQLite on the own bed (one process, WAL, synchronous=FULL): 929
   to 1 224 writes/s with 1 write per commit, 76 486 to 79 401 writes/s
   with 64 writes per commit.
3. The goal is half of the raw SQLite median of the same run, on the
   own bed: at 1 in flight against 1 write per commit, at 64 in flight
   against 64 writes per commit. On the numbers of item 2 that is 465 to
   612 writes/s and 38 243 to 39 701 writes/s.
4. On the shared bed, each node makes 2.1 fdatasync and about 20
   pwrite64 for each write at 1 in flight. At 64 in flight each
   fdatasync covers about 12 writes.
5. The path of one write at 1 in flight (M2, steps file):
   1. The leader sends `work` to the followers. No sync.
   2. Each follower runs the request and sends its `answer`. No sync.
   3. The leader appends one done record (request and chosen result,
      node.hpp:1208-1238), syncs, and sends the AcceptDecide.
   4. Each follower syncs and sends the Accepted.
   5. The leader decides, applies, syncs, and then releases the reply.
   6. Each follower applies on the Decide and syncs.
   konsensd is one thread (serve.cpp:737-777), so the syncs of steps 5
   and 6 also delay the next request at 1 in flight.
6. CPU of the three nodes per write at 64 in flight, own bed: 102 to
   107 us user and 114 to 120 us system, 221 to 222 us in total, 0.75
   to 0.76 of one processor. The goal needs at most 78 us per write on
   the 3 processors of the bench, a cut of about 2.8 times.
7. Instruction profile at 64 in flight (callgrind, start included): the
   leader calls `sqlite3_step` 42.9 times per write, a follower 31.2
   times. Raw SQLite calls it once.
8. The leader counts its own accepted index before its commit
   (sequence_paxos.hpp:1330, 1432, 1615-1616). This is safe only
   because no message leaves before the commit
   (doc/plan-group-commit.md, R8).
9. SQLite 3.53.4, pragma.html of the package sqlite3-doc: "In WAL mode
   when synchronous is NORMAL (1), the WAL file is synchronized before
   each checkpoint ... but no sync operations occur during most
   transactions." "WAL mode is always consistent with
   synchronous=NORMAL, but WAL mode does lose durability. A transaction
   committed in WAL mode with synchronous=NORMAL might roll back
   following a power loss or system crash." The text goes into
   refs/sqlite/ (public domain) with the first function that reads it.

## 2. What is measured, and how

1. Bed: one btrfs file system with compress=zstd:1 for each node, each
   on its own loop image. Raw SQLite runs as one process on the file
   system of node 1, and as three processes at once, one on each file
   system. The three processes are the bound for three nodes on this
   host. The shared bed stays as a second column.
2. Builds: g++ 15, g++ 16, g16aa (g++ 16 with -march=x86-64-v3), clang
   22. Each arm is one binary for each build.
3. Each run: the A/A floor first (one binary built twice), then old arm
   and new arm alternately, 15 samples, 3 rounds, medians and spread.
   The run length is read from the state file where it is kept, else it
   is measured once and kept there.
4. Cells: 64 B and 1 KiB; 1 and 64 in flight. Each cell also reports
   fdatasync per write per node and user and system CPU per write.
5. Each run writes one file under bench/results/ with the full context.

## 3. Measurements that find the cost (no change of code)

M1. Writes per commit and `sqlite3_step` calls per commit at 64 in
   flight, per node, by the kind of the step. Source: a counter in the
   Debug build, printed to the log of konsensd. Not under strace.
M2. Done (fact 1.5). It is repeated after each arm.
M3. Pages per commit, by table: dbstat on a copy of the database, and
   the frame count that the WAL hook gives. The meaning of that count is
   checked in the SQLite text before M3 uses it.
M4. Done (fact 7). It is repeated after arm D.

## 4. Arms (each measured as section 2 says)

A. The leader sends the AcceptDecide before its own sync.
   - The release point is just before `txn.commit()`
     (sequence_paxos.hpp:455), after `read_checked_meta`. konsensd
     queues the early messages and calls `io_uring_submit` before it
     calls the commit.
   - Only the prefix of the messages to each follower, in the order of
     the step, that holds only AcceptDecide and Decide leaves early. So
     no message with a sequence number overtakes a held one
     (message.hpp:65-74).
   - Invariant: no message is handled between the release and the end
     of the commit. `node` throws `std::logic_error` on any call in that
     interval. With fact 8, the invariant keeps every decide on durable
     writes of a majority.
   - Every other message, every Accepted, and every client reply that F1
     does not name leaves after the commit.
   - The release needs a new call of `node`. Its name has no source; it
     is provisional and goes to the owner with the result.
F1. The reply to a client leaves before the commit of its apply.
   - Only the replies that `reply_to_the_client` makes in
     `apply_the_done_records` leave early, at the release point of arm A
     and inside its interval. Retry, read and reconfiguration replies
     (node.hpp:661) leave after the commit.
   - The reply depends on the decided done record, which is durable on
     a majority. A follower accepts only up to its verified index
     (sequence_paxos.hpp:1617-1628), so the result was checked by a
     majority. A read still waits until the committed state reaches the
     read index. A crash between the reply and the commit retires the
     identity (rejoin form B).
F2. A commit that holds only the decided index and the apply does not
   sync.
   - Such a commit runs with synchronous=NORMAL (fact 9); every other
     commit runs with synchronous=FULL. The next synced commit makes it
     durable, because the WAL keeps the order of its frames.
   - A clean stop syncs the log database before it writes the mark.
   - After a crash or a power loss, the node retires its identity (form
     B), so it never serves from an apply that the loss rolled back.
   - Known practice: a replicated state machine keeps its log durable
     and applies without a sync, because the state follows from the log.
     CockroachDB and TiKV apply this way (from memory, not checked).
     For this project the practice fits, because form B already treats
     every crash as a loss of the local state. F2 is a new design; it is
     built as an arm, and the owner sees the arm with its numbers before
     it stays.
C. Larger batches at 64 in flight. M1 shows what limits a batch. The
   arm changes only that limit, and the plan gets the form before the
   build.
D. Fewer SQL statements per write. M1 names the statements; one change
   per arm.
E. Fewer pages per commit, after M3. A change of the schema goes to the
   owner.

An arm that is faster in the table as a whole stays. An arm that is
not faster is deleted, and its row stays in the result file.

## 5. Owner questions

Sent in one message when the first arms are measured: the name of the
release call of arm A, F2 with its numbers, and any change of the
schema from arm E.

## 6. Tests

- A: a Decide and then an AcceptDecide to one follower in one step; an
  AcceptSync and then an AcceptDecide: the early prefix holds no
  message after a held one. A call of `node` between the release and the
  commit throws. A crash of the leader after the release and before its
  sync: the entry is decided by the followers, and the restart takes a
  new identity.
- F1: a crash between the reply and the apply commit: after the
  restart the client gets the same reply for its session and number.
  A retry reply is not released early.
- F2: a pure apply step makes no fdatasync (a store that counts
  syncs); the next step with an append syncs both; a clean stop syncs
  before the mark; a power cut after a NORMAL commit leaves a consistent
  database whose applied index is at or below the decided index.
- Each arm: the client tests, the konsensd tests (three_nodes), seeds 1
  to 300 of counter_stress, test_simulation, Release with g++ 15, g++ 16
  and clang 22. ASan and UBSan once before the merge.

## 7. Model

model/tla/SequencePaxos.tla of this repository, with `Flip` and the
`MC_ev_*` configurations. Arm A: the leader releases the AcceptDecide
in one step and syncs in a later step. Arm F1: the reply is released in
the same step. Mutation that must fail: an Accepted is handled between
the release and the sync, and a reply is released on it. F2 needs no
model change: an apply that is lost in a crash is the same as a crash
before the apply. BFS with small bounds.

## 8. Order

1. M1 and M3 (Debug counter, dbstat).
2. Arm A with F1, model check, tests, measurement.
3. Arm F2, tests, measurement.
4. Arm D, then C and E in the order that M1 and M3 show.
5. After each arm: one result file, push, state file.
6. Merge only after the full test run and ASan and UBSan.

## 9. Data on the disk

- $S/zn1.img, $S/zn2.img, $S/zn3.img (8 GiB sparse each, btrfs) and
  /mnt/zn1 to /mnt/zn3: needed by open work (the own bed).
- $S/bench-logs/*.sh, $S/vg-image, image localhost/bench-vg: needed by
  open work; their digests stand in the result files.
- $S/diag-steps.patch and $S/wt-diag: the diagnostic build of M2; needed
  until M2 is repeated after arm A.
- $S/sqlitedoc: the package sqlite3-doc; its text goes into refs/.
- /mnt/zrun/b/{cpu,disks,trace1,trace-own,trace-diag,vg-c64} and
  /mnt/zn*/b/n: made again by the scripts. Deleted after the owner has
  seen this list.
- $S/wt-write: the worktree of this branch.

## 10. Wrong assumption

- Assumed: Execute-Verify makes two log rounds, the request entry and
  then a `results` entry (from doc/plan-execute-verify.md).
- True: the code (node.hpp:1150-1238) sends `work` first, collects the
  answers, and appends one done record that holds the request and the
  chosen result. One log round is on the path.
- Caused: an owner question about a second round was sent and withdrawn
  on 2026-10-30. Review round 1, findings 4, 5 and 8, rested on it and
  are void.

## 11. Result of M1

bench/results/2026-10-30-062000Z-write-path-statements.txt (a
diagnostic patch outside the tree, not the Debug counter of M1; the
counts come from `sqlite3_stmt_status`). At 64 in flight, per write:

- The leader upserts a `data` row 5.43 times and a `session` row 5.56
  times; a follower 5.07 and 5.20 times. The apply needs one of each.
  The rest come from the base of Execute-Verify, which each check
  writes again in a SAVEPOINT and rolls back
  (`write_the_done_results`, node.hpp:993 and 1481).
- The leader reads a `log` row 4.66 times and a `meta` row 4.39 times.
- The leader makes 1 840 write transactions for 8 193 writes, so 4.45
  writes per commit.

So arm D starts with the base: it is kept between steps, or the run
reads the chosen results of the base from memory. The form goes into
this plan before the build.

## 12. Form of F2 as built

- The writer connection of konsens.db opens with synchronous=NORMAL.
  The reader and node.db keep synchronous=FULL.
- `commit()` commits and then syncs the WAL through `xSync` of the
  file that `SQLITE_FCNTL_JOURNAL_POINTER` gives
  (refs/sqlite/c3ref/c_fcntl_begin_atomic_write.html). So a commit is
  durable as with FULL.
- `commit_without_a_sync()` only commits. Sequence Paxos calls it when
  the transaction wrote nothing of the log and of the votes
  (`has_log_writes()`).
- The open syncs the WAL after it writes the format.
- `stop()` syncs the last commit before it writes the clean-stop mark.
- pragma.html: "Transactions are durable across application crashes
  regardless". Only a power loss or a crash of the system can roll a
  commit without a sync back.

## 13. Wrong assumptions of F2

- Assumed: `PRAGMA synchronous` can change the level for one
  transaction, inside that transaction.
- True: SQLite returns "Safety level may not be changed inside a
  transaction". Every test failed with "SQL logic error".
- Caused: one build and one test round. The form of section 12
  replaced it.

- Assumed: a first test in one file showed that the pragma inside the
  transaction works.
- True: the helper of that test ended the process on the first error,
  and the output did not show the error.
- Caused: the wrong form above was built in the tree.

## 14. Arm D1: the meta record of a write transaction stays in memory

- Source: M1 (section 11): the leader reads the `meta` row 4.39 times
  per write; one write transaction reads it many times.
- Form: the environment of the storage keeps the meta record of the
  open write transaction after its first read. A write of the meta
  record through the transaction replaces it. The begin, the commit and
  the rollback of a write transaction drop it. The reader connection
  does not use it.
- The CRC of the meta record is checked on the first read of each
  transaction, as before.
- Test: a rolled back transaction does not leave its meta record for
  the next transaction; a write is seen by the next read of the same
  transaction.
- Measured as section 2 says, against F2.

## 15. Arm S: the caller syncs the WAL through io_uring

Source: the owner chose "build it as an arm" on 2026-10-30. Measured
before (2026-10-30-080500Z-write-path-waits.txt): at 64 in flight the
one thread of a node waits 31 to 42 % of the time for fdatasync and
computes 26 to 40 %, one after the other.

Practice: hashicorp/raft PR 579 (draft, not merged, no numbers) keeps
appends in memory and flushes them in batches from a background task.
No consensus was found. The form here keeps one thread in the library
and in konsensd.

### Form

1. A node option `sync_by_the_caller` (provisional name, default off).
   Off: everything stays as now.
2. On: `commit_phase_two` commits every step without a sync. A step that
   wrote the log or the votes gets a sync number n (one more than the
   last). The node keeps n as `last_sync_number`.
3. Output of a step with sync number n is held until the caller reports
   a sync that covers n. Exceptions, as with arm A and F1: the prefix of
   AcceptDecide and Decide per peer, and the F1 replies.
4. Order: a later step adds its output behind the held output of each
   peer. A message of a later step never overtakes a held message to
   the same peer. A later step's AcceptDecide or Decide leaves early only
   when nothing is held for that peer.
5. A step without log writes that comes after a held step: its output is
   held behind the held output too, with the same number as the last
   held step. So no message depends on a state that is not durable.
6. The caller asks `take_the_sync_request()` (provisional). It gives
   {file descriptor of the WAL, n} when n is above the last reported
   sync and no sync is out. The caller submits fdatasync of that file
   descriptor through io_uring (IORING_FSYNC_DATASYNC) and goes on with
   other steps.
7. The caller reports the end with `the_log_is_synced(n)`
   (provisional). The node releases the held output of every step up to
   n. A failed fdatasync is reported as an error and ends the node with
   a sync error. The node never repeats a sync.
8. The leader counts its own accepted index only up to the log length of
   the last synced step. So `set_accepted_idx(pid_, ...)` takes
   min(length now, synced length). The report of a sync raises the
   synced length and is a step: it may decide (decide_if_chosen) and
   send Decide.
9. The storage opens the WAL file a second time, read only, after the
   open, and keeps that descriptor. fdatasync of any descriptor of a
   file syncs the data of the file. SQLite keeps the WAL file while its
   connection is open; a checkpoint resets it in place. A checkpoint is
   synced by SQLite itself (pragma.html, NORMAL).
10. `stop()` waits for no sync: it syncs the last commit itself
    (sync_the_last_commit), as F2 does.
11. konsensd: one fsync in flight at a time. All steps that commit while
    it runs join the next sync (group commit). A step that waits on
    nothing is not delayed.

### Safety

- A follower sends Accepted and Promise only after its own sync covers
  the step (item 3).
- The leader decides only on a majority of synced accepts (item 8).
- A client reply of F1 depends on a decided record, durable on a
  majority.
- A crash loses only steps above the last sync. Their output did not
  leave, except AcceptDecide and Decide (as with arm A) and F1 replies
  (durable elsewhere).

### Tests

- A unit test with a storage that counts syncs: with the option on, a
  step with log writes holds its Accepted until `the_log_is_synced`.
- The leader does not decide on its own unsynced append: three nodes,
  one follower down, the other follower accepts; no decide before the
  leader reports its sync.
- Order: a held Accepted and a later AcceptDecide to the same peer leave
  in step order.
- A failed sync ends the node with a sync error.
- test_simulation with the option on for every node, seeds as before.
- three_nodes with konsensd and the option on.

### Model

model/tla/SequencePaxos.tla: the leader's own accept counts only after a
separate `Sync` action. Mutation that must fail: the leader counts its
own accept before `Sync`. BFS with small bounds.

### Measurement

As section 2, arm S against D1, with an A/A pair. Cells 1 and 64 in
flight.

### Changes after review (arm S, round 1)

R1. Durable prefix. The node keeps `synced_len`: the length of the log
    prefix that is durable. `append_on_prefix` and a change of the
    accepted round lower it to the first changed index (0 for a change of
    the round). A sync report for n raises it to the log length recorded
    with step n, never above a lowering of a later step. Item 8 uses
    `synced_len`. Test: a new leader adopts a suffix that overwrites
    synced entries; its sync is delayed; it does not decide.
R2. On a sync report the leader decides the highest chosen index: the
    majority-th largest value of the accepted indexes.
R3. Item 5 becomes: each Sequence Paxos message gets the highest sync
    number assigned so far. Ballot Leader Election messages and the
    read replies of `serve_reads` are not held.
R4. The received and message batches are freed at the commit; only the
    outgoing Sequence Paxos messages and the replies are held.
R5. With the option on, every commit sets the flag that `stop()` uses to
    sync the last commit. konsensd drops a sync completion after
    `stop()`.
R6. A failed sync report puts the storage in a failure state. Every
    later `begin()` throws, as after a write that did not finish.
    konsensd ends and answers the waiting calls.
R7. Test with a small checkpoint threshold, so that checkpoints and WAL
    resets run while a sync is out.
R8. The storage opens the -wal file after the open sync, with O_CLOEXEC,
    checks st_dev and st_ino against the path, and closes it only after
    both connections are closed.
R9. Safety: under form B a single crash retires the node. The sync per
    step protects against a crash of a majority at the same time, and it
    keeps the log that a retired node gives to a build.
R10. `the_log_is_synced(n)` opens a step and returns a transaction, as
    `tick()` does. A call between the two commit phases is a wrong use.

## 16. Wrong assumption: the bench runs konsensd on io_uring

- Assumed: konsensd in the bench container uses io_uring of the kernel.
- True: the default seccomp profile of podman refuses every io_uring
  system call with ENOSYS. slipstreamIO then uses its own engine. A probe
  gives -95 for IORING_OP_FSYNC in the container and 0 on the host
  (bench/results/2026-10-30-090000Z-io-uring-in-podman.txt).
- Caused: every konsensd number of the write path so far, the CPU per
  write included, is a number of the engine of slipstreamIO. Arm S
  cannot run in the bench container as it is.

### Change after the build (arm S)

R11. The messages of Execute-Verify (`work`, `answer`, `try_again`,
     `refused`) are not held. They carry no sequence number
     (message.hpp:65-74 applies to Sequence Paxos messages). None of
     them states a durable write: `work` asks a follower to run a
     request, `answer` gives a result that the done record and the check
     of each follower decide on later, and a refusal only stops an
     accept. A `work` that reaches a follower before its promise is
     dropped by `keep_the_tasks` and sent again. Without R11 every
     AcceptDecide of a leader with a sync out waits behind a held
     `work`, and the early path of arm A is lost. Provisional; it goes to
     the owner with the numbers of arm S.

## 17. Arm S2: one write transaction for each sync

Source: arm S gave +21 % at 1 in flight and no gain shown at 64 in
flight, with 1.6 to 2 times the CPU per write
(2026-10-30-094500Z-konsensd-arm-s.txt). S makes about 1.7 times the
pwrite64 of D1 per write (2026-10-30-100000Z-arm-s-system-calls.txt):
it commits each turn of the loop, and each commit writes its own WAL
frames.

Form:
1. With `sync_by_the_caller` on, a step that ends while a sync is out
   does not commit. The write transaction stays open, and the next
   step continues in it (`begin_step` reuses an open transaction).
2. The step after the report of the sync commits the open transaction,
   and the caller then asks for the next sync. So each sync covers
   exactly one commit, and each commit holds every step since the last
   sync.
3. The output of a step in the open transaction is held as in arm S,
   with the sync number that the commit of the transaction gets. The
   early prefix of AcceptDecide and Decide (arm A), the F1 replies, the
   messages of Execute-Verify (R11) and the messages of Ballot Leader
   Election leave as in arm S.
4. A read-only transaction sees only committed state. A read waits
   until the committed state reaches its read index, as now.
5. `stop()` commits the open transaction and syncs it before the mark.
6. A failure inside a step rolls back the open transaction, so it rolls
   back every step since the last commit. Their output is held and is
   dropped with it.

Questions for the review: whether a step reads its own uncommitted
writes in every path (the reader connection serves only reads of
clients); whether item 6 can drop output that already left (the early
prefix); whether a nested transaction (Execute-Verify base) inside a
long transaction is safe; whether a checkpoint is delayed without a
bound.

### Changes after review (arm S2, round 1)

Q1. The output of a step in the open transaction is tagged as pending.
    The commit resolves the tag: the new sync number when the
    transaction has log writes, else the last sync number. Test: an
    Accepted of a step in the open transaction does not leave on the
    report of the sync that was out.
Q2. Every rollback of the open transaction calls `stop_voting` and
    clears the held output; the node then never votes again, and a
    restart is form B. This covers the reset after a failure of
    `serve_clients_in_the_step` and a failure inside the storage.
Q3. A flag marks the open step apart from the open transaction.
    `throw_if_a_step_is_open` and the calls that use it test the flag.
    `stop()` commits the open transaction, syncs it, then writes the
    mark.
Q4. `expire_session`, `find_client_session` and `get_applied_idx` read
    through the open write transaction when one is open. Test: a session
    registered in the open transaction does not expire as unknown.
Q5. The path that keeps the transaction open does not raise the mark.
    The share of F1 replies that leave early is measured.
Q6. "A sync is out" is: last sync number > synced sync number.
Q7. The R7 test runs with S2 too.
Q8. The nested transaction of the Execute-Verify base is a savepoint in
    the open transaction (no change; lang_savepoint.html is not in
    refs/sqlite/ and was not checked).
Q9. The latency of reads is measured with S2.

## 18. Wrong assumption: the arm results at 64 in flight before 2a8cd3b

- Assumed: the samples of one cell measure the same thing, and their
  spread is noise between runs.
- True: the rate of one cluster fell with each sample. The leader
  walked each done record of its ballot in each step
  (`find_the_entries_that_a_majority_refused`), and that set has no
  bound (2026-10-30-123000Z-write-path-slows-with-each-sample.txt,
  2026-10-30-131500Z-refusal-check-reads-the-refusals.txt).
- Caused: the medians at 64 in flight of F2, D1, S and S2 before
  2a8cd3b hold the drop. A faster arm wrote more and lost more. The
  D1 and S "gains" and the spread of S and S2 came from it.
- Still open: the set of appended done records and the set of
  refusals keep growing in memory for the length of one ballot.

## 19. Arm P: the promise between checks comes from memory

Source: callgrind at 1 in flight on all three nodes, form S2 on top of
2a8cd3b. The leader runs 697k instructions per write and 6.4 steps per
write. `sequence_paxos::get_promise` takes 25 % of them: about 13.7k
instructions per call, two calls per step (`finish_the_step` and
`execute_verify`). Each call runs `read_checked_meta`: the meta row,
the last entry and the entry state of `decided_idx`. Outside a write
transaction it also begins a read-only transaction.

The checks now run 4 times per step: `begin_step`, the two calls of
`get_promise`, and `commit_phase_one`.

Form:
1. `sequence_paxos` keeps the meta record of the last check that ended
   a transaction: the record that `commit_phase_one` checked, once
   `commit_phase_two` committed it or once the transaction had no
   writes. A rollback, `stop_voting` and an error drop it.
2. `read_meta_between_steps` returns, in this order: inside a write
   transaction, the meta of that transaction (`txn_->get_meta()`; the
   transaction checked it in `begin_step` and holds only its own writes
   since); outside, the kept record; with no kept record, the checked
   read as now.
3. `begin_step` and `commit_phase_one` keep their checks. Each write
   transaction is checked at its begin and at its end, as now.
4. The four getters (`get_promise`, `get_accepted_round`,
   `get_decided_idx`, `get_accepted_idx`) use item 2 without a change
   of their declarations.

Tests:
- After a commit, `get_promise` returns the promise that the step
  wrote, with no read of the storage (a storage that counts its reads).
- After a rollback, the next `get_promise` makes the checked read.
- A damage of the meta row between two steps is found by the next
  `begin_step`, and the node stops voting.

Measured as section 2 says, against R1 with S2 and against R1.

Question for the review: whether a value in memory between two steps
misses a damage that a checked read finds now, and whether any path
writes the storage outside a transaction of `sequence_paxos`.

### Changes after review (arm P, round 1)

The review is in doc/review-plan-write-path.md ("Review of arm P").

- P4 is built first as its own arm, P4: `execute_verify` takes the
  promise from the meta record that it reads next (`read_meta()`), and
  does not call `get_promise`. No check is removed: the step checked
  the meta in `begin_step`.
- P3 replaces items 1 and 2. Outside a write transaction, with no error
  and no unfinished write, the getters return the values of `mark_`.
  Inside a write transaction they return `txn_->get_meta()`. Else they
  make the checked read. No new member. This fixes P1.
- P2: P3 reverses item 3 of 2.1 of plan-lmdb-only-source.md (each
  getter makes the checked read). The test at
  test_sequence_paxos.cpp:668-689 changes as the review says. P3 is
  built as an arm, marked provisional, and is kept only after the owner
  decides.
- P5: `answer_indexed_reads` makes the checked read on its read-only
  transaction, so a client read keeps a check in every step.
- P6: the tests follow the review.
- P7: with S2, a step that continues an open transaction runs three
  checks.
- P8: the name `read_meta_between_steps` stays until the owner decides.

## 20. Arms U and M: Unix sockets and multishot receive in konsensd

Source: the owner asked for Unix sockets and multishot io_uring
operations (2026-10-30). konsensd now uses single-shot accept and
recv, and arms a new recv after each completion
(program/serve.cpp:159-172, 580-600). Each node is one process with one
thread; one `io_uring_submit_and_wait_timeout` per turn of the loop.

Two arms, each its own binary, so each part is measured alone:
- U: the addresses in the configuration may name a Unix socket. The
  form is `unix:<path>`, as gRPC and nginx write it (provisional; the
  owner decides the name). `resolve` returns a `sockaddr_un` for that
  form. `listener_on` removes nothing on the disk; a socket file that
  exists makes the bind fail, as now for a port in use. TCP_NODELAY is
  set only on TCP sockets. Nothing else changes.
- M: U, plus multishot accept (`io_uring_prep_multishot_accept`) for
  both listeners and multishot recv (`io_uring_prep_recv_multishot`)
  with one provided buffer ring (`io_uring_setup_buf_ring`) for the
  connections that receive. A completion with IORING_CQE_F_BUFFER
  names its buffer; the bytes go to the frame or HTTP buffer of the
  connection as now, and the buffer goes back to the ring at once. A
  completion without IORING_CQE_F_MORE re-arms the operation. ENOBUFS
  re-arms after the buffers are back. `connection::in` goes away in M.
  Names of the buffer ring and its sizes are named constants.
- Both work on TCP too. M is measured on Unix sockets.

Bench: cluster.sh writes `unix:` addresses under the run directory when
UNIX=1. The driver dials a `unix:` target through `net.Dial("unix")`.
The probe uses `curl --unix-socket`.

Tests: test_config and test_address (or the test that covers
`resolve`) check the `unix:` form: a path, an empty path (malformed),
a path longer than `sun_path` (malformed). The program tests (ptest.sh,
three_nodes) run once with TCP and once with `unix:` addresses, for U
and for M. The address check covers every member of the set of forms:
host:port, [v6]:port, unix:path.

Measured as section 2 says: arms R1 with S2 (TCP), U with S2, M with
S2, and an A/A copy of M.

Questions for the review: the order of a multishot recv completion and
a close of the connection; the bound of buffers per connection when a
peer sends faster than the node reads; whether a provided buffer ring
works on the slipstream engine (if not, M refuses to start there as S
does).

## 21. Arm C: messages between nodes as cbor::schema

Source: the owner (2026-10-30): `cbor::schema` is much faster than
`cbor::structs`. The decision of 2026-10-20 names packed CBOR through
`cbor::schema` for the messages between nodes. The code uses
`cbor::structs` (src/wire.cpp:109-236, src/verdict.cpp:30-43,
program/frame.cpp:72-86, program/key_value.cpp:17-34). callgrind at 64
in flight: `cbor::generic::generic_read` takes about 17k instructions
per write on the leader, about 5 %.

Facts that bound the form (deps/mbedded-cbor 55418aa, doc/schema.md):
- `cbor::schema` needs C++26 reflection, g++ 16 with -freflection. A
  clang build has no `cbor::schema`.
- Every struct in a message carries `[[=cbor::tag(n)]]` with its own
  number.

Steps:
1. A file outside the tree measures, for the messages that a write
   sends (work, answer, accept_decide with entries, accepted, decide),
   encode and decode with `structs` and with `schema`, each in its own
   binary, g++ 16 Release. Medians, A/A first.
2. If step 1 shows a gain above its floor, the wire of src/wire.cpp
   uses `schema`. The log entries on disk follow in a later arm.
3. konsensd is measured as section 2 says.

Open for the owner before step 2: the tag numbers (the provisional block
of 2026-10-20 in the IANA first-come range), and whether a clang build
of the library without a wire is acceptable.
