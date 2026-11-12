# Plan: trace validation of the simulation against the model

Source: the owner, 2026-11-12 21:31 (UTC): "okay, leg los" to the
proposal V1 of the evaluation of Ron Pressler, "Why Writing Correct
Software Is Hard" (/mnt/project-files/actor-raft/correctness-and-complexity.md).
The model checks the design. Today no check asks whether the C++ code
takes only steps that the model allows. This plan closes that gap for
the simulation tests. Review round 1 rejected the first form; this is
the second form, for a short second review.

## 1. Facts checked in this session

### 1.1 Sources

- Cirstea, Kuppe, Loillier, Merz, "Validating Traces of Distributed
  Programs Against TLA+ Specifications", arXiv 2404.16075 v2 (2024).
  Read: the abstract and the first part of the PDF. The program writes
  records. A trace spec has a line counter `l`; each action of the model
  is joined with a check of the record at `l`. Variables that the record
  does not hold stay open, and TLC searches them. The paper states:
  "too imprecise logs may even lead to erroneous traces being
  accepted." Success is a behaviour that reads the whole trace
  (`TraceAccepted`, the diameter equals the length of the trace).
  Deadlock and liveness checks are off. This plan takes its form from
  the paper.
- etcd-io/raft `tla/Traceetcdraft.tla` and `tla/README.md` (main, read
  2026-11-12) use the same method with NDJSON and the Community Modules.
  The README says: "It typically takes a few minutes to validate 3000
  traces." Read as a fact about cost only. etcd is not a model for this
  tree (owner).
- Microsoft CCF removed its TLA+ trace validation in commit 3bab46e,
  2026-11-05, "Remove TLA+ specifications and associated tooling
  (#8663)". It now replays traces of a test driver (`cft_driver`)
  against an executable Lean model (`lean/consensus/replay/README.md`):
  each record must match a named rule, "only top-level alternatives
  backtrack", the actions must be enabled in the model, the messages the
  model sent after a rule must equal the messages the program sent, and
  the replay "never repairs model state". The reason for the change
  from TLA+ to Lean is not in the commit message. Not read: the CCF
  paper (Howard et al.).

Result: the sources agree on the method. A program or a driver writes
each step with the state after it. A checker accepts the trace only if
the model can take the same steps, with a bounded choice where the
record leaves something open.

### 1.2 This tree (main 654c088)

- `test/simulation.hpp` drives three nodes in one process from one
  seed. A step is: deliver, drop, duplicate, crash, tick, append, a
  client call, rollback. `run` takes `batches sizes =
  batches::random` by default (line 951); `run_seeds` passes no batch
  size, so the seed tests run with random batches. test_simulation.cpp:32
  calls `run` with `batches::single`.
- A crash is one of two kinds (simulation.hpp:34-36, 557-576): a clean
  stop with a start, after which the node votes, or a crash, after which
  the node retires. `rollback::one` also retires the node.
- SequencePaxos.tla `Crash(i)` (line 1752) restarts the node with its
  durable state through `Restart`. The model has no retirement.
  ClusterStop.tla has retirement.
- After each step the harness reads the public state of the touched
  node: `view_of` gives the log, the decided index, `n_accepted`, role
  and phase, `n_leader`; `get_promise` gives the promise. The messages
  come out of `take_outgoing_messages` at the commit of the same step.
- The C++ ballot holds config_id, n, priority and pid. The simulation
  keeps config_id and priority constant. `handle_leader` is called only
  with the node's own pid.
- `model/tla/SequencePaxos.tla` (4074 lines, Omni-Paxos 532ff79, line 3)
  works at message level: `Receive(j, i)` takes the head of a FIFO
  channel; `Lose`, `Duplicate`, `Crash`, `HandleLeader(i, k)`,
  `Propose`, `ResendMessageTimeout`. It has 57 constants.
- `model/tla/AcceptWithResult.tla` (474 lines) holds form 7 of
  execute-verify. It has a set of messages, a separate sync action, and
  no ticks.
- No model has the message level of the code and form 7 at once (risk
  R1).

## 2. Form (provisional, marked for the owner)

F1. The records come from the simulation harness, not from the library.
The library gets no trace code. The test case reads the trace directory
once and passes it to `run` as an argument.

F2. One record for each step of the harness: the kind, its arguments,
the state of each touched node after the step (promise, `n_accepted`,
log as pairs of ballot and value, decided index, role, phase,
`n_leader`), and the messages the step sent. The record does not hold
everything: `leader_state`, the choice `replace` in `HandleLeader`, and
the ghost variables stay open. TLC searches them. Step 0 measures how
much.

F3. The trace is a TLA+ module, `Trace == << [...], [...] >>`, one per
seed. TLC reads it with no Community Modules and no JSON. The records
hold only integers and fixed names, so the test writes them with
`std::format`.

F4. A ballot maps to `n * PidBase + pid`. The trace spec refuses a
record whose config_id or priority differs from the first record, and a
ballot with n = 0 where the model needs n >= 1.

F5. A leader change of BLE becomes a record `leader i n` and maps to
`HandleLeader(i, k)`. Messages of BLE and of the cluster stop map to no
model action. The index of a drop or a duplicate is counted over the
Paxos messages of the channel only.

F6. Modes, one at a time. Phase 1: fifo, `batches::single`,
`clients::none`, `rollback::none`; the trace ends at the first crash
that retires a node, because a prefix of a behaviour is a behaviour.
Then batches, then clients. Retirement and rollback need a model that
has them (question Q2). `delivery::reorder` is not validated: the model
and the library assume FIFO (owner, 2026-10-20 06:22).

F7. One record can need several model actions, for example a batch of
messages, a tick that resends to two peers, or a `HandleLeader` inside a
delivery (node.hpp:1971-1972). The kind of the record fixes which
actions may run as its sub-steps, and only for the nodes it names;
every variable the record does not name stays unchanged. A `Lose` or a
`Duplicate` runs only for a record of that kind. Up to `MaxSubSteps`
such actions run without moving `l`; `l` moves only when the state
equals the record. A record with no model action (a BLE delivery, a
tick before the resend timeout, a resend that sends nothing) is one
step that moves `l` and changes nothing else. TLC does not evaluate
the composition operator, so this is the form.

F8. The trace spec `model/tla/TraceSimulation.tla` extends the chosen
model. Its bounds (MaxBallot, MaxCrash, Cap, MaxTimeout, MaxLog,
MaxElection, MaxLoss, MaxDuplicate, Values) come from the trace, so no
bound cuts a trace. TLC runs BFS with one worker. The end of the trace is
tested with the line counter: the invariant `l <= Len(Trace)` must be
reported as violated. The diameter of the paper does not work here,
because sub-steps make a behaviour longer than the trace. The
counter-check of step 0 shows that this form refuses a wrong trace. Its metadir and its trace files go to the
compressing disk, not into `model/tla`.

F9. The check runs in a script `model/trace/validate.sh`, outside
ctest. ctest writes the traces only when the test case gets a trace
directory. The build and the default test run stay free of Java
(question Q1).

## 3. Steps

0. Spike, outside the tree (rule "A hypothesis is tested in one file
   first"). A copy of `simulation.hpp` in the scratchpad gets the
   record writer of F2 and F3 and runs seed 1 of the fifo test with
   `batches::single`. It writes:
   - a table of every constant of SequencePaxos.tla with the value
     that the code has and the line of the code that decides it;
   - a table of each message field of the code and the field of the
     model, including the chain hash of the code against `last`;
   - the trace module.
   TLC checks the trace against SequencePaxos.tla. Measured: records per
   seed, TLC time per trace (s, wall clock, JVM start included),
   distinct states, the first refused record. Counter-check: the same
   trace with one decided index raised by one must be refused. The
   result file goes to `model/results/`.
   AcceptWithResult.tla is not checked in step 0: it needs a second
   record form (its own ballot owner, a set of messages, a separate
   sync).
1. The owner picks the model (Q2), with the result of step 0.
2. The harness writes the records when it gets a trace directory. The
   draws of the random generator do not change, so a seed runs the same
   steps with and without traces. A test checks that.
3. `TraceSimulation.tla`, its config, and `validate.sh`. The script runs
   TLC once per trace, as many at once as the machine has processors
   less one, and writes one result file.
4. Counter-check in the code: a mutation that the checks of the
   simulation do not see must make a trace fail. Candidate: a follower
   that takes an accept sync below its promise, guarded so that the
   safety checks stay green on the chosen seeds. The mutation lives on
   its own branch `trace-validation-mutation`, which is never merged.
5. The modes of F6, each with its own counter-check.
6. Seeds: the number that fits the five minutes of a test run, from the
   time of step 0. Before a release, as many seeds as the seed run of
   the simulation (owner, 2026-10-27 19:50).

## 4. Risks

- R1. The model is older than the code. A refused trace can be a fault
  of the code or of the model. Known before step 0: a tick of the code
  builds a new Promise, where `ResendMessageTimeout` resends the cached
  one. Each refusal gets a line in the state file with the record and
  the side that was wrong. A change of the model is its own plan with
  its own TLC run, before the trace is checked again.
- R2. The search of F2 grows with what the record leaves open. Step 0
  measures it.
- R3. The time of TLC per trace. Step 0 measures it before a seed count
  is promised.

## 5. What this plan does not do

- No trace code in the library, and none in konsensd.
- No validation of Jepsen runs. Their events have no total order across
  processes.
- No change to the model.
- No change from TLA+ to another checker.

## 6. Questions for the owner

- Q1. Where TLC runs. This use of TLC checks the code against the
  model, which the decision on Apalache and TLAPS (only to check the
  model) does not name. (a) A script outside ctest; TLC from its origin,
  for the model and for this check, and for nothing that builds the
  tree. (b) A ctest test that
  needs Java in the build image. Proposal: (a).
- Q2. Which model the trace checks. (a) SequencePaxos.tla, message
  level, older than form 7, no retirement. (b) AcceptWithResult.tla,
  form 7, no message level. (c) A new model at message level with form
  7 and retirement. Proposal: decide after step 0; (a) for phase 1 if
  step 0 shows few gaps, and (c) as its own plan for the rest.

## 7. Data this plan leaves on the disk

- The TLC jar in the scratchpad: made again by a download.
- The spike copy of simulation.hpp in the scratchpad: deleted after
  step 0.
- Traces and the TLC metadir (with TTrace files of a refused trace) on
  the compressing disk: deleted after the check, except a refused trace,
  which stays beside its result file.
- Result files in `model/results/`: recorded in the state file.
