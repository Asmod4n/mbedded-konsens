# Plan: the write path of konsensd within half of raw SQLite

Source: the owner, 2026-10-30 05:42 (UTC+1): close the gap to raw
SQLite; the goal is to come within 50 % of raw SQLite, if that is
possible. This plan gets one review before the build.

## 1. Facts measured in this session

All on commit edfb54f, konsensd built with g++ 16 Release, 64 B values,
three nodes on one machine. Files: bench/results/2026-10-29-220701Z-distributed-logs.txt,
bench/results/2026-10-30-055400Z-write-path-beds.txt.

1. The bed changes the result. With the three data directories on one
   btrfs file system, konsensd gives 38 to 57 writes/s at 1 in flight
   and 1 355 to 2 335 writes/s at 64 in flight. With one btrfs file
   system for each node, it gives 99 to 115 writes/s and 3 389 to 3 451
   writes/s. One sample per cell, two rounds, no A/A floor.
2. Raw SQLite on the own bed (one process, WAL, synchronous=FULL):
   929 to 1 224 writes/s with 1 write per commit, 76 486 to 79 401
   writes/s with 64 writes per commit.
3. The goal of this plan is half of the raw SQLite median of the same
   run, on the own bed: at 1 in flight against 1 write per commit, at
   64 in flight against 64 writes per commit. On the numbers of item 2
   that is 465 to 612 writes/s and 38 243 to 39 701 writes/s.
4. On the shared bed, the median fdatasync of the WAL takes 7.52 ms on
   the leader and 2.53 ms and 3.64 ms on the followers. Each node makes
   2.1 fdatasync for each write at 1 in flight, and about 20 pwrite64.
   At 64 in flight, each fdatasync covers about 12 writes.
5. The fdatasync calls of one write follow each other: leader, both
   followers, leader, both followers (strace -ttt, 50 writes). The
   leader writes before it sends the Accept, and a follower writes
   before it sends the Accepted (doc/plan-group-commit.md section 2).
   Execute-Verify adds a second round: the `results` entry
   (doc/plan-execute-verify.md 2.5, 2.6.5).
6. CPU time of the three nodes per write, own bed: at 1 in flight 1.0 to
   1.2 ms user and 2.6 to 3.2 ms system; at 64 in flight 102 to 107 us
   user and 114 to 120 us system. At 64 in flight the three nodes use
   0.7 of one processor. So the CPU is not the limit at that load now.
7. Omni-Paxos `accept_entries_leader` (refs/omnipaxos leader.rs:123-133)
   appends to its storage before `send_acceptdecide`. The form where the
   leader writes in parallel with the Accept is in the Raft dissertation,
   section 10.2.1, from memory. The dissertation is not in refs/
   (refs/papers.md). Before the build of arm A, the claim is checked in
   the text, and the text goes into refs/ if its download works.

## 2. What is measured, and how

1. Bed: one btrfs file system with compress=zstd:1 for each node, each
   on its own loop image. Raw SQLite runs on the file system of node 1.
   The shared bed stays as a second column, because it shows the cost
   of three nodes on one disk.
2. Builds: konsensd with g++ 16, g++ 16 with -march=x86-64-v3 (g16aa as
   in the comparison), clang 22. Each arm is one binary for each build.
3. Each run: the A/A floor first (one binary built twice), then old arm
   and new arm alternately, 15 samples, 3 rounds, medians and spread.
   The run length is the shortest one at which the A/A pair agrees.
4. Cells: 64 B and 1 KiB; 1 and 64 in flight. Each cell also reports
   fdatasync per write per node, writes per commit, and user and system
   CPU per write.
5. Each run writes one file under bench/results/ with the full context
   (RULES.md "Each run of the bench is one file").

## 3. Measurements that find the cost (no change of code)

M1. Writes per commit at 64 in flight, per node: the count of entries
   that each commit appends, and the count of steps between two
   commits. Source: a counter that a Debug build gives in the log of
   konsensd (RULES.md: "What is there to debug is there in a debug
   build"), and strace -c for the Release build.
M2. The critical path of one write: strace -ttt -T of fdatasync on all
   three nodes, at 1 in flight, on the own bed. The question: does the
   order of item 1.5 hold there, and how long is each part.
M3. Pages per commit: the WAL frames of each commit, by table, read with
   the dbstat virtual table on a copy of the database and with the frame
   count that `sqlite3_wal_hook` reports. The question: which tables add
   the 20 page writes of item 1.4.
M4. CPU profile at 1 and at 64 in flight with valgrind callgrind (a
   package of the distribution, in the bench image only). Instruction
   counts find the hot functions. They do not say what a change gains;
   only arm runs say that.

## 4. Arms (each measured as section 2 says)

A. The leader sends the Accept before its own commit.
   - The rule of doc/plan-group-commit.md section 2 holds for every
     message that depends on the write of the step. An Accept of the
     leader does not: the leader counts its own accepted index only
     after its commit, so a decide never depends on a write that is not
     durable.
   - `node` gives the AcceptDecide messages of the leader before
     `commit`; every other message, every Accepted and every client
     reply leaves after the commit, as now.
   - The model gets the same split: the leader sends in one step and
     counts itself in a later step after its write. TLC checks the
     safety properties with it (section 7).
B. One round for Execute-Verify on the critical path. Owner question
   (section 5). No code before the answer.
C. Larger batches at 64 in flight. What limits the batch is found by
   M1. The arm changes only that limit. The plan is extended with the
   form after M1, and the review checks that part.
D. Less CPU per write. The arm takes the hot functions that M4 shows,
   one change per arm.
E. Fewer pages per commit. The arm follows M3. A change of the schema
   is an owner question.

An arm that is faster in the table as a whole stays. An arm that is
not faster is deleted, with its row in the result file.

## 5. Owner question (sent in one message after the review)

B. Execute-Verify makes two rounds before a client gets its reply: the
   request entry, then the `results` entry. Options:
   1. Keep two rounds. The reply waits for the decide of `results`.
   2. The leader replies when a majority of answers agrees (2.6.3),
      before `results` is decided. `results` follows off the critical
      path. The model must show that a later leader selects the same
      result or keeps the entry pending, and never rejects an entry
      whose client got a reply.
   Proposal: 2, after the model check of section 7 is green.

## 6. Tests

- Arm A: a crash of the leader after it sent the Accept and before its
  commit: the entry is decided only by the followers, and the restart of
  the leader takes a new identity (rejoin form B). A decide on the
  leader never counts its own accepted index before its commit.
- Arm B (if the owner takes 2): a leader change after the reply and
  before the decide of `results`: the new leader selects the same
  result.
- Each arm: the client tests, the konsensd tests (three_nodes), seeds 1
  to 300 of counter_stress, test_simulation. Release with g++ 15, g++ 16
  and clang 22. ASan and UBSan once before the merge
  .

## 7. Model

The model of this thread (/mnt/project-files/actor-raft/model/) gets
the split of arm A, and for arm B the reply at the selection. BFS with
small bounds, the same invariants as the current configs, and a
mutation that counts the own accepted index before the write, which
must fail.

## 8. Order

1. Plan, review, owner question B.
2. Bed scripts; M1 to M4; the results go into this plan as facts.
3. Arm A with its model check, then C, D, E in the order of what M1 to
   M4 show, then B if the owner takes it.
4. After each arm: one result file, push.
5. Merge only after the full test run and ASan and UBSan.

## 9. Data on the disk

- $S/zn1.img, $S/zn2.img, $S/zn3.img (8 GiB sparse each, btrfs) and the
  mounts /mnt/zn1 to /mnt/zn3: needed by open work (the own bed).
- $S/bench-logs/cpu.sh, $S/bench-logs/disks.sh: needed by open work;
  their digests stand in the result file.
- /mnt/zrun/b/cpu, /mnt/zrun/b/disks, /mnt/zrun/b/trace1, the data of
  /mnt/zn*/b/n: made again by the scripts. Deleted after the owner has
  seen this list.
- $S/wt-write: the worktree of this branch.

## 10. Changes after the review

The review (doc/review-plan-write-path.md: approve with changes, 17
findings, 6 blocking) is applied as follows.

- R1: fact 1.5 and arm A are wrong about the count. The leader counts
  its own accepted index before the commit (sequence_paxos.hpp:1330,
  1432, 1615-1616). Arm A is safe only while no message is handled
  between the release of the early messages and the commit. `node`
  throws `std::logic_error` on any call in that interval. The model
  mutation is "an Accepted is handled between the release and the
  write".
- R2: for each follower, only the prefix of its messages in the order
  of the step that holds only AcceptDecide and Decide leaves early. So
  no message with a sequence number overtakes a held one
  (message.hpp:65-74). Tests: a Decide then an AcceptDecide to one
  follower in one step; an AcceptSync then an AcceptDecide.
- R3: the release is just before `txn.commit()` (sequence_paxos.hpp:455),
  after `read_checked_meta`. konsensd queues the early messages and
  calls `io_uring_submit` before the commit. The error path of
  `serve_clients_in_the_step` after the release is checked in the code
  before the build, and the plan names its form then.
- R4, R5: option 2 of question B loses plan-execute-verify.md 2.7.7 (a
  fault in the count, compare or copy of the leader reaches the client)
  and, with floats, 2.9.1 (a later leader can choose other bytes than
  the reply). The question names both losses. The model runs with
  `Flip` and `Spread`.
- R6: the model is model/tla/SequencePaxos.tla of this repository, with
  `Flip` and the `MC_ev_*` configurations.
- Bed: each run also measures three raw SQLite processes at once, one
  on each file system. That is the bound for three nodes on this host.
- CPU: the three nodes use 221 to 222 us of CPU per write at 64 in
  flight. Half of raw SQLite needs at most 78 us on 3 processors. The
  profile (bench/results/2026-10-30-060200Z-write-path-callgrind.txt)
  shows 42.9 calls of `sqlite3_step` per write on the leader and 31.2 on
  a follower. Arm D starts with the statements per write.
- 1 in flight: half of raw SQLite allows at most two commits in
  sequence. M2 maps each fdatasync to the writes it holds and to the
  messages that wait for it.
- Smaller fixes: the share is 0.75 to 0.76 of one processor; both beds
  are on one host disk; g16aa is g++ 16 with -march=x86-64-v3; a g++ 15
  column is added; M1 counts in a Debug build only, not under strace;
  the frame count of `sqlite3_wal_hook` is checked in the SQLite
  documentation before M3 uses it; section 6 reads "ASan and UBSan once
  before the merge"; the run length is read from the state file where it
  is kept.

## 11. Wrong assumption

- Assumed: Execute-Verify makes two log rounds, the request entry and
  then a `results` entry (fact 1.5, section 5, review R4 and R5).
- True: the code (node.hpp:1150-1235) sends `work` to the followers
  first, collects the answers, and appends one done record that holds
  the request and the chosen result. One log round is on the path.
- Caused: owner question B named a round that does not exist. It was
  withdrawn on 2026-10-30. Question B and R4, R5 are void. M2 maps each
  fdatasync to its step before any new question about the path.
