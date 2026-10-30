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
