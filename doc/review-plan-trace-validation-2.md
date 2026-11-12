# Review round 2 of two plans

Plans under review:

- `doc/plan-trace-validation.md`, branch trace-validation, commit 4033383.
- `doc/plan-error-branch-coverage.md`, branch error-branch-coverage,
  commit 12af2be.

Scope: I checked whether the revision answers each finding of round 1
(`review-plans.md`), and whether it adds a new fault. I changed no file
in the repository. I did not build anything or use the web.

---

## Plan 1: trace validation

### Status of the findings of round 1

| Finding | Status | Where |
|---|---|---|
| T1 Retirement not in SequencePaxos.tla | Answered | 1.2; F6 ends phase 1 at the first crash that retires a node; Q2 (c) |
| T2 Step 0 cannot be written by hand | Answered | Step 0: a copy of simulation.hpp in the scratchpad writes the records |
| T3 No counter-check in the spike | Answered | Step 0: a decided index raised by one must be refused |
| T4 AcceptWithResult needs a second record form | Answered | Step 0, last paragraph |
| T5 Constants not mapped | Answered | Step 0, table of every constant |
| T6 "Full state" is not full | Answered | F2 names what stays open; R2 |
| T7 Message fields not mapped | Answered | Step 0, table of fields, chain hash against `last` |
| T8 BLE steps and the index of a drop | Answered | F5 |
| T9 Batch mode of the test | Answered | 1.2, step 0 |
| T10 No form for one record that needs several actions | Answered, with new faults N1 and N2 | F7 |
| T11 Community Modules | Answered for the jar (F3). Partly open: see below | F3, Q1 |
| T12 etcd as a model | Answered | 1.1 |
| T13 CCF "with no search" | Answered | 1.1 |
| T14 TTrace files and metadir | Answered | F8, section 7 |
| T15 Where the mutation lives | Answered | Step 4: branch `trace-validation-mutation`, never merged |
| T16 Read of a global | Answered | F1 |
| T17 Smaller points | Answered for F4 (n >= 1) and R1 (new Promise). Two points left: see N3 |

T11, open part: Q1 still says "TLC ... only to check the model". A trace
check uses TLC to check the code against the model. That is a wider use
than the owner decision names. Fix: add one sentence to Q1 that names
this use, so the owner's answer covers it.

### New findings

#### N1. A sub-step can be any action of the model, so the check can accept a wrong trace

- Place: F7.
- What is wrong: F7 allows up to `MaxSubSteps` actions that do not move
  `l`. It moves `l` "only when the state equals the record". It does not
  say which actions a sub-step may take. If a sub-step may take any
  disjunct of `ProtocolNext` (`SequencePaxos.tla:2681-2715`), TLC can put
  in a `Lose`, a `Duplicate`, a `HandleLeader` or a `Receive` of a node
  that the record does not touch, as long as the touched node ends in
  the recorded state. The paper's warning (1.1, "too imprecise logs ...")
  then applies to the sub-steps. The search also grows with each
  sub-step.
- Proposed fix: The record kind selects the allowed sub-steps. A
  delivery to node i allows `Receive(j, i)` for the messages of the
  batch, in order. A tick of node i allows `HandleLeader(i, _)` and
  `ResendMessageTimeout(i)`. A drop allows exactly one `Lose`, and a
  duplicate exactly one `Duplicate`. Nodes and channels that the record
  does not name stay `UNCHANGED`, except the channels that the step
  sends on.

#### N2. "The diameter equals the length of the trace" does not hold with sub-steps

- Place: 1.1 and F8 (`TraceAccepted`).
- What is wrong: The plan defines success as a behaviour whose diameter
  equals the length of the trace. With F7, one record can take several
  TLC steps, so the diameter of a correct behaviour is larger than
  `Len(Trace)`. A diameter check then refuses correct traces. A check of
  "at least the length" accepts a behaviour that stopped early, because
  sub-steps can make up the depth.
- Proposed fix: Test the end by `l`, not by the depth. One form: state
  the invariant `l <= Len(Trace)` and expect TLC to report that it is
  violated. Another form: a final check that a state with
  `l = Len(Trace) + 1` was reached. Test the chosen form in step 0. The
  counter-check of step 0 must then fail on this form.

#### N3. A record with no model action has no stated form

- Place: F5, F7.
- What is wrong: Many records change nothing in the model. Examples are
  a delivery of a BLE message, a tick between two resend timeouts
  (`resend_ticks = 2`), and a tick whose resend sends nothing
  (`ResendMessageTimeout` needs `chan' # chan \/ phase' # phase`,
  `SequencePaxos.tla:2116`). F7 describes only records that need one or
  more actions. Also, an election can occur inside a delivery
  (`node.hpp:1971-1972`), so a delivery record can need
  `HandleLeader`. F7 does not list it.
- Proposed fix: Add a step with zero actions: `l' = l + 1` with
  `UNCHANGED vars`, allowed only when the record says that no Paxos
  state changed and no Paxos message was sent. Add `HandleLeader(i, _)`
  to the allowed sub-steps of a delivery to node i.

### Verdict on plan 1: approve with changes

The revision answers T1 to T10 and T12 to T16. T11 needs one sentence in
Q1. N1, N2 and N3 change the trace spec, and step 0 tests the trace spec
first. So the plan can start step 0 once F7 and F8 hold the fixes of N1,
N2 and N3. A third review is not needed if the changes stay as stated.

---

## Plan 2: error-branch coverage

### Status of the findings of round 1

| Finding | Status | Where |
|---|---|---|
| C1 Count 808 | Correct, and "392 in template headers" is correct (`sequence_paxos.hpp:350-351` and `node.hpp:132` are class templates; 215 + 177 = 392) | 1 |
| C2 Template members and headers in many objects | Answered | Step 0, step 2 |
| C3 A marker covers several exits | Not fixed. The plan now states the limit. See below | Step 2 |
| C4 Error exits with no marker | Answered | 1, step 2 (second table) |
| C5 SIGKILL loses the counts | Answered for SIGKILL. A process that ends on an uncaught fault is not named | 1, step 2, section 3 |
| C6 List of tests, `--crash` | Answered | 1 |
| C7 gcov-16, python3 | Answered in step 0. The result file does not list the gcov version | Step 0, step 2 |
| C8 Where it runs, `/dev/fuse`, mounted data | Answered | Step 1, Q2 |
| C9 Five minutes | Open. See below | Step 0, step 1 |
| C10 Yuan et al. | Answered: the quote is given through Pressler, the paper is marked as not read, and the claim in Q1 is gone | Header, Q1 |
| C11 Smaller points | Answered (debug build and the tests that ran are in the result file) | Step 2 |

C3, open: the result can call a marker "reached" while one of its exits
never ran. The second table already reads every `throw` and
`std::unexpected` line. The same pass can give the line count of each
exit inside a marked block, at no extra cost. Proposed fix: one more
column, "exits not reached", in the table of each marker.

C9, open: step 1 says "The run is a measurement, not a test run of a
change". The rule "A run of build and tests takes five minutes" names
fuzzing as its one exception. So the plan makes a second exception
without the owner. The spike times one simulation test, and that does
not predict the time of the whole ctest run. Proposed fix: add Q3 for
the owner: "May this one coverage run take longer than five minutes?"
Give the time of the spike as the evidence.

Smaller, open: in step 2, list in the result file the tests that end on
an uncaught fault (C5) and the version of `gcov-16` (C7).

### New findings

None. The claim "This repository has no CI job that builds the tree" is
correct: `.github/workflows/` holds only `deps-upstream.yml`, which calls
a submodule notifier and builds nothing.

### Verdict on plan 2: approve with changes

The changes are C9 (a question for the owner) and C3 (one more column).
The two small additions to the result file (gcov version, tests that end
on a fault) are not blocking. No new review is needed.
