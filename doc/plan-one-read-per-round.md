# Plan: each round reads the open part of the log once

Source: the owner, 2026-10-30 20:24-20:46 ("eine transaktion für
alles", "gleiches nur mit gleichen vergleichen", "weitersuchen").
Base: write-path f2f2fc5. Branch: one-transaction-per-round.

## 1. What is measured

bench/results/2026-10-30-203500Z-konsensd-sql-statements.txt counts,
for each write at 64 in flight on the leader: 31.4 rows of
`SELECT idx, record FROM log WHERE idx >= ?1`, 3.4 point reads of the
log, one log INSERT and one log UPDATE, a session read and upsert, a
data read and upsert, 0.08 commits. The code already has one write
transaction for each period of a sync (sequence_paxos.hpp:470-473,
program/serve.cpp:950-970). Every statement is prepared once
(persistent_storage.cpp:46, :352). Every lookup uses a primary key
(EXPLAIN QUERY PLAN, SQLite 3.45.1).

## 2. Part A: one read of the open log in each round

Three places read the open part of the log again in each round, each
one from the storage, in the open write transaction:

1. `execute_verify`: applied_idx to log_len (node.hpp:1891).
2. `check_the_done_records`: min(verified, applied) to log_len
   (node.hpp:1201).
3. the apply: applied_idx to decided_idx (node.hpp:1496).

A round reads the range from min(verified, applied) to log_len once,
in the open write transaction, after the last write of the log in that
round. The three places take a span of that one read. The read is
dropped at the end of the round. It never outlives its transaction,
so the rule "the storage is the only source" holds. A write to the log
inside the round after the read (accept of the leader, append on a
prefix) drops the read; the next user reads again.

Which of the three makes the most rows is not measured. The plan
measures it first: bench/sqlcount.cpp gets a count of executions
beside the count of rows, and the run of 203500Z is made again.

## 3. Part B: the state of an entry (waits for the owner card)

`set_entry_states` (persistent_storage.cpp:762-795) reads each decided
entry and writes it again with the state `committed`. The meta record
holds decided_idx in the same commit. Raft (commitIndex), TigerBeetle
(commit_max) and FoundationDB (knownCommittedVersion) keep one index
and write no entry again. With the card "Nur Index": the record loses
the state byte and its CRC; the start check (sequence_paxos.hpp:
1175-1190) loses its two entry-state checks; format_version becomes 6.

## 4. Part C: the read back of written values (waits for the owner card)

`written_values_read_back` (node.hpp:1545-1558) reads each written key
in the same write transaction. That read comes from the page cache of
SQLite. With the card "Nur Debug-Build" it runs only without NDEBUG.

## 5. Measurement

Arms, each its own binary, g++ 16 Release: base f2f2fc5, A, A+B,
A+B+C, and a copy of the base for A/A. Each arm: the statement count,
one callgrind run of the leader at 64 in flight (instructions,
sqlite3_step), and the bed run of arms.sh (6 rounds, 1 and 64 in
flight). The pgrep -x fault of cpu_of (names over 15 characters) is
fixed in the copy of arms.sh first, so the CPU per write is in the
rows.

## 6. Tests

The changed code: test_node, test_execute_verify, test_simulation,
test_persistent_storage (part B), the start checks (part B).

## Data that the plan leaves on the disk

The worktree wt-round (made again by git worktree), the build
directories (made again by a build command), the binaries of the arms
in bench-logs/bin (deleted after the result files), the callgrind
files (deleted after the result files).
