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

## 2. Part A: one read of the open log for each version of the log

Places that read the open part of the log in a step, each one from the
storage, in the open write transaction:

1. `check_the_done_records`: min(verified, applied) to log_len
   (node.hpp:1201), before the leader accept (node.hpp:1344) and again
   after it (node.hpp:1466).
2. `find_the_verified_idx` (sequence_paxos.hpp:1059, called at :1513
   and :1805).
3. the apply: applied_idx to decided_idx (node.hpp:1496).
4. `execute_verify`: applied_idx to log_len (node.hpp:1891).
5. `holds_the_request` (node.hpp:1178) and
   `log_or_buffered_proposals_contain` (sequence_paxos.hpp:993) read
   from a given index to log_len.

The read is a member of `sequence_paxos`, beside `txn_`: the entries
from one index to log_len and the count of log writes of the
transaction at the time of the read. Each INSERT, cut or rewrite of an
entry through the transaction raises the count. A place takes a span
of the read only through one function, `read_the_open_log(from)`. It
reads again when the count changed or when `from` lies before the
first entry it holds. A changed state of an entry (set_decided_index,
persistent_storage.cpp:762-795) does not raise the count, because
`entry` holds no state (storage.hpp:39-52).

A span of the read lives only until the next log write. `tail` in
check_the_done_records (node.hpp:1201-1243) and `done_log b` in
execute_verify each make no log write while they hold the span; the
build checks this for each place and keeps an own read where a place
writes the log while it holds the span.

The read is dropped together with `txn_`: at commit_phase_one, at
every roll back of the transaction and at stop_voting. So it never
outlives its transaction, and the rule "the storage is the only
source" holds. One transaction can hold several steps
(sequence_paxos.hpp:472-475); the read stays valid across them as
long as the count holds.

On the leader the first check comes before the accept, so a step of
the leader reads at least twice. That is expected.

Which place makes the most rows is not measured. The plan measures it
first: bench/sqlcount.cpp counts executions beside rows, and the run
of 203500Z is made again on the base.

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

Arms, each its own binary: base f2f2fc5, A, A+B, A+B+C, and a copy of
the base for A/A. Each arm is built with g++ 15, g++ 16 and clang 22,
one table with a column for each build. bench/sqlcount.cpp and the
change of arms.sh are committed before the runs; arms.sh stands
outside the tree, and each result file carries its digest.

- The A/A floor first: the base against its copy.
- The statement count of each arm (one run, a count, not a time).
- One callgrind run of the leader at 64 in flight for each arm
  (instructions, sqlite3_step).
- The bed run of arms.sh: 6 rounds, arms alternate in each round, 1
  and 64 in flight, medians compared, raw SQLite with the same
  statements (bench/raw_konsens.cpp) in each round. The pgrep -x fault
  of cpu_of (names over 15 characters) is fixed in the copy of arms.sh
  first, so the CPU per write is in the rows.
- Each run writes one result file with its context (rules "Each run of
  the bench is one file").

## 6. Tests

The changed code: test_node, test_execute_verify, test_simulation,
test_persistent_storage and test_sequence_paxos (part B;
entry_state at test_sequence_paxos.cpp:564-904), the start checks
(part B).

## Data that the plan leaves on the disk

The worktree wt-round (made again by git worktree), the build
directories (made again by a build command), the binaries of the arms
in bench-logs/bin (deleted after the result files), the callgrind
files (deleted after the result files).

Review: doc/review-plan-one-read-per-round.md, approved after fixes;
this text carries findings 1 to 5, 10 and 11 and the test list.
