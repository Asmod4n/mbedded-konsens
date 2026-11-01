# Review 2: doc/plan-parallel-check-run.md, §2 to §8

Commit dbcbed5, branch parallel-check-run. Scope: the blocking findings
1 to 8 of review 1, and new faults in §2 to §8. No build, no commit.

## Status of the blocking findings of review 1

| # | Finding | Status |
|---|---------|--------|
| 1 | results from the committed applied index | closed (§2.4, §2.5, §6) |
| 2 | `own_`, `nodes_`, `machine_` | closed, but see N5 and N6 |
| 3 | stale verdicts | closed by dispatch_apply |
| 4 | lifetime | in-flight part closed; idle pool and unwinding open (N1, N2) |
| 5 | library mode | closed (§2.2, §2.7) |
| 6 | error path | closed (§2.6); ordering open (N3) |
| 7 | write through an argument | closed (§2.5) |
| 8 | MSG_RING overflow | closed for the loop; open for the answer (N4) |

## New or remaining faults

### N1. The loop can unwind while pool jobs still run (blocking)

§2.4 gives the loop its own range. §2.7 says the loop waits after its
share. If the share of the loop throws, the stack unwinds before the
answers come back. Examples are `read_error` from `transaction_view::find`
(state_machine.hpp:44-50) and the `logic_error` of `round_the_data_item`
(node.hpp:1738-1741). The jobs then hold views into freed locals and the
open log. Fix: the share of the loop also goes into a result array with an
`std::exception_ptr`, the same form as §2.6. The loop always waits for
every answer before it reads or rethrows.

### N2. The lifetime of the pool between steps is not stated (blocking, one sentence)

`node` has a defaulted move (node.hpp:621-622), and tests keep nodes in a
`std::vector` (test/node_cluster.hpp:228). Between steps the pool threads
wait on their rings. If a ring, a result array or a connection is a direct
member of the node, a move changes its address under a waiting thread.
The plan also does not say how the node stops and joins the pool when it
is destroyed. Fix: the pool lives in one heap block that the node owns
through a `unique_ptr`. The destructor sends a stop message to each pool
ring and joins the threads.

### N3. Which fault counts when several ranges answer (not blocking, state it)

Today the loop stops at the first refusal and at `refusal_->index`
(node.hpp:1221, 1233). An entry above that is never run, so it cannot
throw. With ranges, an entry above the first refusal also runs.
`check_the_done_records` also passes nothing when it throws, because
`pass_the_checked_done_record` comes after the loop (node.hpp:1246-1250).
Fix: the loop reads the arrays in index order. It stops at the first
refusal, at `refusal_->index`, or at the first fault, whichever comes
first. It ignores everything above that point, and it rethrows a fault
only if the fault comes first. Jobs leave out the entries from
`refusal_->index` on.

### N4. A failed answer of a pool thread leaves the loop waiting forever (blocking)

§2.8 raises on a negative `res` of a send. With IOSQE_CQE_SKIP_SUCCESS,
the CQE of a failed answer from a pool thread arrives on the ring of that
pool thread, and the loop never sees it. Example: `-ENOMEM` (man page
io_uring_prep_msg_ring.3). An exception on the pool thread does not help,
because the answer is what failed. Fix: a pool thread whose answer fails
records the fault and ends the process. This is the rule for a fault that
nothing can catch ("`main` and the body of a thread ... record it, and
end", RULES.md). The plan states it as an error path for the owner.

### N5. A user type is not bound by [res.on.data.races] (not blocking, question for the owner)

[res.on.data.races]/3 binds the functions of the standard library, not
the types of a user. §2.9 makes "a const call changes no shared state" a
requirement of the public API. That is the known convention, and the
choice of the owner. The plan decides it and does not ask. A header
comment on the concept conflicts with RULES.md, "Comments live in tests":
only the start comment of the header is allowed. Put the requirement in
the start comment of state_machine.hpp and in the README. Also: the
`state_machine` concept checks `apply` only on `write_view`
(state_machine.hpp:59-62). Add the view type of the pool to the concept,
so that a machine with a non-template `apply` fails at the concept and
not deep in node.hpp.

### N6. Entries with an own result can still run (not blocking)

§2.3 says these entries are checked on the loop thread "as today". When
the own result disagrees, `check_the_done_record` runs the request again
on the base (node.hpp:1309-1311). So the loop still needs its base
over the writing transaction for these entries, as it has today. State
this, so that nobody builds that path without a base.

### N7. The read transaction of a pool thread must end before the answer (not blocking)

§2.5 opens one read transaction per job and does not say when it ends.
A transaction that stays open until the next step makes that step see a
stale snapshot: then a is below the committed index, the check of §2.5
raises, and the WAL cannot reset (review 1, finding 12). Fix: the pool
thread ends the transaction before it answers. §7 records the size of the
WAL file and the checkpoints per second.

### N8. The source of liburing for the library (not blocking, question for the owner)

§2.2 links liburing into the library. Today only konsensd links it, from a
carried copy of slipstreamIO, built only with KONSENS_PROGRAM
(CMakeLists.txt:136-170). RULES.md, "What builds the tree is the
distribution's package", lists CRC32C, libhydrogen and LMDB as the only
exceptions. Name the distribution package of liburing for the library, or
ask the owner for an exception. Not checked: whether the build image has
the liburing package.

### N9. Smaller points (not blocking)

- §2.4: the committed applied index needs a start value at the open of
  the node (from the meta, next to `sp.mark_`, sequence_paxos.hpp:362), and
  `commit_the_open_transaction` also passes through `commit_phase_two`
  (sequence_paxos.hpp:522-535). Say both.
- §4: the time of a round trip is a number about speed. RULES.md
  ("Measure before, measure after"): a noise floor, a binary for each arm,
  alternate runs, medians.
- §6, "The suites of plan-promise-again §5": RULES.md, "A test run checks
  the changed code only". This point from review 1 is still open.
- Review 1, finding 15: the time share of the check in a step is still
  not measured (§1 cites instructions and a run with the record on). The
  test of §4 does not answer it.

## Verdict

Approve after the blocking fixes N1, N2 and N4. All three are short
additions to §2. N4, N5 and N8 go to the owner with the other questions.

Not checked: kernel source, SQLite 3.53.4, the liburing package of the
build image.
