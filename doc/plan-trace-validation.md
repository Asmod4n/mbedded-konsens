# Plan: trace validation of the simulation against the model

Source: the owner, 2026-11-12 21:31 (UTC): "okay, leg los" to the
proposal V1 of the evaluation of Ron Pressler, "Why Writing Correct
Software Is Hard" (/mnt/project-files/actor-raft/correctness-and-complexity.md).
The model checks the design. Today no check asks whether the C++ code
takes only steps that the model allows. This plan closes that gap for
the simulation tests. It gets one review before the build.

## 1. Facts checked in this session

### 1.1 Sources

- Cirstea, Kuppe, Loillier, Merz, "Validating Traces of Distributed
  Programs Against TLA+ Specifications", arXiv 2404.16075 v2 (2024).
  Read: the abstract and the first part of the PDF. The program writes
  JSON records. A trace spec has a line counter `l`; each action of the
  model is joined with a check of the record at `l`. Variables that the
  record does not hold stay open, and TLC searches them. The paper
  states: "too imprecise logs may even lead to erroneous traces being
  accepted." Success is a behaviour that reads the whole trace.
  Deadlock and liveness checks are off.
- etcd-io/raft `tla/README.md` and `tla/Traceetcdraft.tla` (main,
  read 2026-11-12). The program writes NDJSON through a `TraceLogger`
  at each state change. The spec reads it with `ndJsonDeserialize` of
  the Community Modules. TLC runs BFS with one worker. `TraceView` is
  `<<vars, l>>`, so a state that repeats at two lines stays two states.
  `TraceMatched` is a temporal property: the queue is empty only when
  `l > Len(TraceLog)`. The README names two known problems: a log that
  persists in part, and a long check: "It typically takes a few minutes
  to validate 3000 traces."
- Microsoft CCF. Its TLA+ trace validation is gone:
  commit 3bab46e, 2026-11-05, "Remove TLA+ specifications and
  associated tooling (#8663)". CCF now replays traces against an
  executable Lean model (`lean/consensus/replay/README.md`). The form of
  the replay: each record must match a named rule; the actions must be
  enabled in the model; after each rule the messages that the model
  sent must equal the messages that the program sent; the replay "never
  repairs model state". The traces come from a test driver
  (`cft_driver`), not from the production code path.
- Not read: the CCF paper (Howard et al.). The reason for the change
  from TLA+ to Lean is not in the commit message.

Result: the sources agree on the method. The program, or a driver
around it, writes each step with the state after it. A checker accepts
the trace only if the model can take the same steps. They differ in the
checker (TLC with search, or a replay with no search) and in the place
of the records (the library, or the driver).

### 1.2 This tree (main 654c088)

- `test/simulation.hpp` drives three nodes in one process from one
  seed. Each step is one of: deliver (fifo or reorder), drop,
  duplicate, crash (clean stop with start, or crash after which the
  node retires), tick, append, the client calls, rollback. With
  `detailed` it writes a text line for each step (`note`).
- After each step the harness reads the public state of the touched
  node: `view_of` gives the log, the decided index, `n_accepted`, the
  role and phase, `n_leader`; `get_promise` gives the promise. The
  messages a node sends come out of `take_outgoing_messages`.
- `model/tla/SequencePaxos.tla` (4074 lines) has actions at message
  level: `Receive(j, i)` on the head of a FIFO channel, `Lose`,
  `Duplicate`, `Crash`, `Wipe`, `HandleLeader(i, k)` (own pid, no BLE),
  `Propose`, `ResendMessageTimeout`. The comment at line 3 names
  commit 532ff79 of the Rust origin.
- `model/tla/AcceptWithResult.tla` (474 lines) holds the form
  "the accept carries the result" (plan accept-carries-the-result).
  It is abstract: commit, sync and crash come from OneEnvironment.tla.
  The state file says it lacks the rejected entries of stage 2.
- So no single model has the message level of the code and form 7 of
  execute-verify at once. This is the first risk of the plan (R1).

## 2. Form (provisional, marked for the owner)

F1. The records come from the simulation harness, not from the
library. The library gets no trace code. This is the place CCF uses
(a driver) and it keeps the rule that an effect happens in one place.

F2. One record for each step of the harness. A record holds the kind
of step, its arguments, the full state of each node that the step
touched after the step (promise, `n_accepted`, log as pairs of ballot
and value, decided index, role, phase, `n_leader`), and the messages
the step sent. A full state leaves TLC nearly no search. The paper
warns that a thin record can accept a wrong trace.

F3. NDJSON, one file per seed, read by `ndJsonDeserialize`. The
records hold only integers and fixed names, so the test writes them
with `std::format` and needs no escaping and no JSON library.

F4. A ballot of the code (config_id, n, priority, pid) maps to the
integer `n * PidBase + pid` of the model. This holds only while
config_id and priority stay constant (SequencePaxos.tla:318-320). The
trace spec checks that with an ASSUME on the trace.

F5. A leader change of BLE becomes a record `leader i n`. It maps to
`HandleLeader(i, k)`. BLE itself is checked by
BallotLeaderElection.tla and is not in this trace.

F6. Modes, one at a time: first fifo, `batches::single`,
`clients::none`, `rollback::none`. Then batches (one record, several
`Receive` in sequence, the composition of the paper), then clients,
then rollback. `delivery::reorder` is not validated: the model and the
library assume FIFO (owner, 2026-10-20 06:22).

F7. The trace spec `model/tla/TraceSimulation.tla` extends the chosen
model. The bounds of the model (MaxBallot, MaxCrash, Cap, MaxTimeout,
MaxLog, MaxElection) come from the trace, so no bound cuts a trace.
TLC runs BFS, one worker, `TraceView == <<vars, l>>`, and the
`TraceMatched` property of etcd.

F8. The check runs in a script `model/trace/validate.sh`, outside
ctest. ctest only writes the traces when `KONSENS_TRACE_DIR` is set.
The build and the default test run stay free of Java (question Q1).

## 3. Steps

0. Spike, in one file outside the tree (rule "A hypothesis is tested in
   one file first"). Write the trace of seed 1 of `test_simulation`
   (fifo) by hand from the `detailed` text output into NDJSON. Check it
   against SequencePaxos.tla and against AcceptWithResult.tla. Measure:
   records per seed, TLC time per trace (s, wall clock, JVM start
   included), distinct states. Find the first record that each model
   refuses. Result file in `model/results/`.
1. The owner picks the model (question Q2), with the result of step 0.
2. The harness writes the records (F2, F3) when `KONSENS_TRACE_DIR` is
   set. No change to the draws of the random generator, so a seed runs
   the same steps with and without traces. A test checks that.
3. `TraceSimulation.tla`, its config, and `validate.sh`. The script
   runs TLC once per trace, as many at once as the machine has
   processors less one, and writes one result file.
4. Counter-check: a mutation of the code that the simulation's own
   checks do not see must make the trace fail. Candidate: a follower
   that takes an accept sync of a lower ballot than its promise, guarded
   so that the safety checks of the run stay green on the chosen seeds.
   A trace check that finds no mutation proves nothing.
5. The modes of F6, each as its own step with its own counter-check.
6. Seeds: the number that fits the five minutes of a test run, from
   the time of step 0. Before a release, as many seeds as the seed run
   of the simulation (owner, 2026-10-27 19:50).

## 4. Risks

- R1. The model is older than the code. A refused trace can be a fault
  of the code or a fault of the model. The etcd README gives the order:
  change the model, check it with TLC, then validate the trace again.
  Each refusal gets a line in the state file with the record and the
  side that was wrong.
- R2. The code and the model cut steps in other places, for example a
  commit after a batch, or a resend inside a tick. The trace spec then
  maps one record to a sequence of actions.
- R3. TLC time. etcd needs minutes for thousands of records. Step 0
  measures it before a seed count is promised.

## 5. What this plan does not do

- No trace code in the library, and none in konsensd.
- No validation of Jepsen runs. Their events have no total order across
  processes.
- No change to the model in this plan. A change that R1 needs is its own
  plan with its own TLC run.
- No change from TLA+ to another checker.

## 6. Questions for the owner

- Q1. Where TLC runs. (a) A script outside ctest, TLC from its origin
  only to check the model, as Apalache and TLAPS. (b) A ctest test that
  needs Java in the build image. Proposal: (a).
- Q2. Which model the trace checks. (a) SequencePaxos.tla, message
  level, maybe older than form 7. (b) AcceptWithResult.tla, form 7, no
  messages. (c) A new model at message level with form 7. Proposal:
  decide after step 0; (a) if step 0 shows that the gaps are few.

## 7. Data this plan leaves on the disk

- The jars of TLC and the Community Modules in the scratchpad: made
  again by a download.
- Traces: one NDJSON file per seed, in `KONSENS_TRACE_DIR` on the
  compressing disk; deleted after the check, except a refused trace,
  which stays with its result file.
- Result files in `model/results/`: recorded in the state file.
