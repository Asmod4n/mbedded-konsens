# Review of two plans

Plans under review:

- `doc/plan-trace-validation.md`, branch trace-validation, commit 7c074f3.
- `doc/plan-error-branch-coverage.md`, branch error-branch-coverage,
  commit 38b2799.

Both plans start from main 654c088. That commit is origin/main. The local
branch `main` stands at 0060c64, but neither plan depends on it.

Method: I read both plans, RULES.md, .claude/RULES.md, and the state file
`mbedded-konsens.md`. I checked each claim against the cited files at
654c088. I checked the external claims against the local copies in
`scratchpad/refs/`. I did not fetch the web. I did not build or run
anything, except a grep and one read-only Python count over the sources.

Where I did not check a claim, the finding says "not checked".

---

## Plan 1: trace validation

### T1. The simulation retires a node after a crash, and SequencePaxos.tla has no retirement

- Place: plan 1.2 (list of actions), F6, step 0, Q2.
- What is wrong: The plan treats `Crash` as the model form of a crash in
  the simulation. There are two kinds of crash in the simulation
  (`test/simulation.hpp:34-36`, `crash()` at 557-576):
  - A clean stop with a start. The node starts as `start_kind::clean`.
    It goes to (follower, recover) and sends `prepare_req` to all peers
    (`sequence_paxos.hpp:431-433`). With `Recovery = FALSE`, this
    agrees with `Crash(i)` -> `RestartInPhase(..., "Recover")`, which
    sends `PrepareReqMsg` (`SequencePaxos.tla:1715-1752`).
  - A crash with no clean-stop mark. `kind_of_the_start` gives
    `start_kind::retired` (`node.hpp:70-93`), and `retired_ = true`
    (`sequence_paxos.hpp:436`). After that, `handle` consumes each
    message and does nothing (`sequence_paxos.hpp:676-678`), and
    `resend_message_timeout` returns at once (`sequence_paxos.hpp:844-848`).
- SequencePaxos.tla has no retirement. `grep -n retire` finds it only in
  ClusterStop.tla ("A crash retires the node (form B)", line 8, action at
  232-237) and RecoveryRollback.tla. `Halt` is the nearest action
  (`SequencePaxos.tla:2546`), but it needs `i \in Nodes`, counts against
  `MaxHalt`, and changes `stage1_ghost`. A message to a halted node stays
  in `chan` because `Receive` needs `i \in running` (1617-1619). The code
  consumes the same message. So each delivery to a retired node has no
  model action unless the trace spec maps it to `Lose(p, 1)`.
- The same holds for `rollback::one`: the simulation retires the rolled
  back node (`simulation.hpp:40`). `Rollback(R)` in the model restarts it
  with `Restart` (1765-1777).
- Effect: about half of the seeds with a crash cannot pass against
  SequencePaxos.tla as it stands. This is a gap of type R1 that is known
  before step 0. Q2 does not name ClusterStop.tla, which holds the
  retirement.
- Proposed fix: Add a table "simulation step -> model action" to the
  plan. Give the retirement its row: `Halt` plus `Lose(p, 1)` for each
  later delivery to that node, or a new model action. A new model action
  is a model change, which section 5 puts in its own plan. Name that
  plan as a dependency of the crash seeds. Until then, mark seeds with
  a retirement as "not validated" and count them. Add ClusterStop.tla to
  Q2.

### T2. Step 0 cannot be written by hand from the `detailed` output

- Place: section 3, step 0.
- What is wrong: The `detailed` text has one line per step (`note`). It
  has no state of a node after a step. `state_of` is called only in
  `failed()` (`simulation.hpp:966-971`), so the text holds a state only
  when a run fails. The F2 record needs the state after each step. The
  text cannot give it.
- The text also does not hold what a tick did. `tick()` writes only
  `"tick i"` (530-534). The election, the `handle_leader`, the resend and
  the messages it sent are not in the text. `detail_of` (290-320) writes
  no entries and no `last`, so an AcceptSync suffix or an AcceptDecide
  entry list cannot be rebuilt.
- Size: one seed is 250 random steps (`random_steps`, line 64), and then
  50 quiet rounds (`quiet_rounds`, line 65). Each quiet round delivers
  every message in flight and ticks three nodes. One seed is many
  hundreds of records. The etcd README says the time grows "non-linearly"
  above about 3000 events in a file (`refs/etcd/README.md:92`).
  A record set of that size, written by hand, has errors of its own. A
  refusal then says nothing about the code.
- Proposed fix: In step 0, write the records with a copy of
  `simulation.hpp` in the scratchpad that adds the F2/F3 writer, and
  touch nothing in the tree. That is still "one file outside the tree".
  Take a short run first (for example 30 random steps and 2 quiet
  rounds), and then the full seed 1.

### T3. Step 0 has no counter-check, so a pass proves nothing

- Place: step 0 and step 4.
- What is wrong: Step 0 measures time and finds the first refused
  record. It does not check that the trace spec refuses a wrong trace.
  `TraceMatched` depends on `TLCGet("queue")`, one worker and BFS
  (`refs/etcd/Traceetcdraft.tla:461-470`). A wrong config, a wrong
  `ASSUME`, or a trace spec that leaves a variable open accepts every
  trace. Step 0 decides Q2 with its result, so a pass that has no
  meaning gives a wrong decision.
- Proposed fix: In step 0, change one record by hand to a state that the
  model forbids (for example a promise that goes down). TLC must refuse
  it at that line. Write both results into the result file.

### T4. Step 0 against AcceptWithResult.tla needs a second record form, and the plan does not give it

- Place: step 0, F4, F7, Q2 (b).
- What is wrong: AcceptWithResult.tla differs from SequencePaxos.tla in
  each point that the trace format depends on:
  - The ballot owner is `Owner(b) == NodeSeq[((b - 1) % N) + 1]`
    (line 56). It is not `n * PidBase + pid`, so F4 does not apply.
  - `msgs` is a set (`Send(m) == msgs' = msgs \cup {m}`). There is no
    channel, no head, no loss and no duplicate action.
  - A commit and a sync are two actions (`Commit`, `SyncDone`, line 102).
    Messages wait in `queue` until the sync. The simulation runs with
    `sync_by_the_caller = false` (`simulation.hpp:141`), so the code
    sends at the commit.
  - The header says the ticks are left out (lines 9-11). Each resend of
    the code then has no action.
- Effect: step 0 needs a second mapping and a second trace spec. The
  plan does not say this, and step 0 cannot be the "one file" that the
  plan describes.
- Proposed fix: In step 0, check the trace against SequencePaxos.tla
  only. Answer Q2 (b) on paper: list the four points above as the known
  gaps of AcceptWithResult.tla.

### T5. The constants of SequencePaxos.tla are not mapped to the code

- Place: F7.
- What is wrong: F7 takes six bounds from the trace. SequencePaxos.tla
  has 57 constants (`SequencePaxos.tla:9-149`). About 20 of them select
  a form of the protocol: `Recovery`, `RuleR`, `DecidedIdxMonotone`,
  `VerifyCrc`, `ChainCheck`, `MetaRecord`, `VerifyPromise`,
  `MismatchAction`, `StateInStorage`, `ExecuteVerify`, `ClientSessions`,
  `ReadIndex`, `Witness`, `SingleNode`, `Mutation`, and others. Each one
  changes which messages the model sends. For example, `ExecuteVerify`
  bounds the promise by `verified_idx` (`PromiseLog`, `PromiseAcc`,
  648-649), and the code does the same (`bound_by_the_verified_idx`,
  `sequence_paxos.hpp:804`). `Recovery = FALSE` is the value that
  agrees with the clean start (see T1). A wrong value of one constant
  gives a refusal that looks like a fault of the code.
  `Values`, `MaxLoss` and `MaxDuplicate` are also not in the list of F7.
- Proposed fix: Add a table of all constants with the value for the
  simulation and the line of the code that decides it. Step 0 uses that
  table.

### T6. "Full state" is not full, and the plan does not name where TLC branches

- Place: F2 ("A full state leaves TLC nearly no search").
- What is wrong: The record holds the promise, `n_accepted`, the log,
  the decided index, the role, the phase and `n_leader`. The model also
  holds `accepted_idx`, `current_seq_num`, `cached_promise_message`,
  `buffered_proposals`, the rest of `leader_state` (`promises_meta`,
  `follower_seq_nums`, `accepted_indexes`, `max_promise_meta`,
  `max_promise_sync`), `verified_idx` and the execute-verify variables,
  and many ghost variables (`SequencePaxos.tla:150-293`).
- Most of these are functions of the earlier steps. TLC does not branch
  on them, so they do not cost search. Three things cost search:
  - `\E replace \in BOOLEAN` in `HandlePromise` (line 1107) and in
    `HandleLeader` (line 1658). Each election and each tie in a promise
    can double the states until a later AcceptSync tells them apart.
  - The model actions with no harness step: `Select`, `AppendVerdict`,
    `CheckVerdict`, `RunEntry`, `LeaderRunAgain` and others (from
    line 2790). The code does this work inside `end_the_step`
    (`node.hpp:446-475`, `serve_clients_in_the_step`). The trace spec
    must then search the order of these actions within one record.
  - R2: one tick can be an election, a `handle_leader`, a
    `resend_message_timeout` and a heartbeat resend (`node.hpp:291-323`).
- The paper warning is about a thin record. Here the hidden variables
  are deterministic. The real question is whether the messages are
  recorded field by field. F2 says "the messages the step sent", but
  it gives no mapping of the fields.
- Proposed fix: Call the record "the durable state and the role", not
  "full state". List the points where the model can branch (above), and
  measure distinct states per record in step 0. The plan already
  measures distinct states, so add the count per record.

### T7. The message fields have no mapping

- Place: F2, F3.
- What is wrong: The model message has 22 fields (`EmptyMessage`,
  `SequencePaxos.tla:384-389`). Some fields differ from the code:
  - `last` is a prefix of the log (`PrefixAt`, 396). The code sends a
    chain hash (`decided_chain_hash`, `accepted{..., m->last_chain_hash}`,
    `sequence_paxos.hpp:2102`). The harness can write the log prefix
    instead, but only when it knows the log of the sender at that time.
  - `seq_num` has `session` and `counter` in both, which is good.
  - `log_sync` is a sequence of one record in the model. In the code it
    is a struct (`x.log_sync.sync_idx`, `simulation.hpp:310`).
  - The C++ `message` is a variant of `paxos_message`, `ble_message` and
    `shutdown_message` (`message.hpp:300`). The model has no BLE and no
    shutdown messages.
- Proposed fix: Add a table of the message fields: code field, model
  field, conversion. The harness writes `last` as the log prefix of
  the sender. The trace spec compares each sent message with the model
  messages that the action adds to `chan`.

### T8. BLE messages and the index of a lost message are not said

- Place: F5, F6.
- What is wrong: The simulation keeps BLE messages in the same `network`
  (`simulation.hpp:481-482`, 1036). The `drop` and `duplicate` steps take
  any message (988-997), and the BLE messages are part of that choice.
  - A delivery, a drop or a duplicate of a BLE message must be a record
    with no model action. F5 names only the leader change.
  - `Lose(p, k)` takes the index `k` in the channel of Paxos messages
    alone. The harness must count `k` without the BLE messages.
  - FIFO holds: `first_of_channel` (518-528) gives the first message of
    the pair in the shared network. The first Paxos message of the pair
    is then the head of the model channel. A duplicate goes to the end
    of the network (996), as `Duplicate` appends at the end of the
    channel (`SequencePaxos.tla:2133-2138`). So (a) of the task is
    answered yes, once BLE messages are left out.
- Proposed fix: Write it in F6: a BLE step is a record with
  `UNCHANGED vars`; the harness computes `k` over Paxos messages only.

### T9. The first mode of F6 is not the mode of the test, and step 0 uses the other one

- Place: F6, step 0.
- What is wrong: F6 starts with `batches::single`. Step 0 takes "seed 1
  of `test_simulation` (fifo)". `run_seeds` calls `run` without the
  `sizes` argument (`simulation.hpp:1074`), and the default is
  `batches::random` (`simulation.hpp:951`). So the FIFO test case runs
  with random batches, which F6 puts second. Only the seed 57 case uses
  `batches::single` (`test/test_simulation.cpp:31-33`), and that case is
  a reorder run.
- Proposed fix: State in step 0 that the spike calls
  `run(1, delivery::fifo, ..., batches::single)`. State in step 2 that
  `run_seeds` gets a `batches` parameter, or accept random batches from
  the start and write the composition of the paper in step 0.

### T10. TLC cannot evaluate action composition, and the plan does not say how a record becomes several actions

- Place: F6 ("several Receive in sequence, the composition of the
  paper"), R2.
- What is wrong: The plan does not say how one record maps to a sequence
  of actions. TLC does not evaluate the `\cdot` operator of TLA+. I
  know this from the TLC documentation, but I did not check it in this
  session. The etcd spec stays on one line for several sub-steps with
  `pl` and `StepToNextTraceIfMessageIsProcessed`
  (`refs/etcd/Traceetcdraft.tla:113-124`).
- Proposed fix: Write the form in F7: a sub-step counter, or a record
  for each message of a batch with a flag that marks the commit at the
  end. Test this form in step 0.

### T11. `ndJsonDeserialize` needs the Community Modules, and the owner decision names only TLC, Apalache and TLAPS

- Place: F3, section 7, Q1.
- What is wrong: The owner allows TLC, Apalache and TLAPS from their
  origin, and only to check the model. `ndJsonDeserialize` comes from
  the TLA+ Community Modules, which is another jar from another origin
  (the plan says so in 1.1 and section 7). That jar is not in the
  decision. Q1 also says "TLC ... only to check the model". A trace check
  checks the code against the model, which is a wider use.
- Proposed fix: Let the harness write the trace as a TLA+ module, for
  example `TraceLog == << [kind |-> "deliver", ...], ... >>`, with
  `std::format`. Then TLC needs no Community Modules and no JSON. If
  NDJSON stays, add the Community Modules and the use "check the code
  against the model" to Q1 as questions for the owner.

### T12. The plan names etcd as a model, and the state file says that the owner rejects etcd in every part

- Place: 1.1, F7 ("the `TraceMatched` property of etcd"), R1.
- What is wrong: The state file says: "the owner rejects etcd in every
  part" (`Claude-state/mbedded-konsens.md:86-88`). The plan takes the
  `TraceView`, the `TraceMatched` property and the order of R1 from
  etcd.
- Evidence that the content is right: `TraceView == <<vars, l>>`,
  `ASSUME TLCGet("config").worker = 1` and `TraceMatched` stand in
  `refs/etcd/Traceetcdraft.tla:451-470`, as the plan says. The quote "a
  few minutes to validate 3000 traces" stands in README line 92.
- Proposed fix: Name the method by the paper of Cirstea et al. Name etcd
  only as one place where the method is used. Add a line about this to
  the owner message, so the owner can refuse the form before it is
  built.

### T13. CCF replay is not "with no search"

- Place: 1.1, the result paragraph ("a replay with no search").
- What is wrong: The README says "only top-level alternatives backtrack"
  and "A receive delivers any queued model envelope from its sender
  whose rendered packet has exactly the native packet's fields"
  (`refs/ccf/lean/consensus/replay/README.md:36-41`). That is a small,
  bounded search. The other CCF claims are correct: commit 3bab46e,
  2026-11-05, "Remove TLA+ specifications and associated tooling
  (#8663)"; "never repairs model state" (`lean/consensus/README.md:71`,
  `Reduction.lean:18`); the `cft_driver`. CCF also models a loss as a
  message that stays queued, with no loss action. That form fits T1.
- Proposed fix: Write "a replay with a bounded choice among equal
  messages".

### T14. A refused trace makes TLC write TTrace files into model/tla, and section 7 does not list them

- Place: section 7, step 3.
- What is wrong: The tree already holds 14 `*_TTrace_*.tla` and `.bin`
  files in `model/tla/` (for example `SequencePaxos_TTrace_1792571510.tla`).
  TLC writes these files next to the spec when a check fails. It also
  writes a `states/` directory. Section 7 lists neither. The plan expects
  refused traces, so these files appear.
- Proposed fix: In `validate.sh`, run TLC with `-metadir` on the
  compressing disk, and keep the TTrace output out of `model/tla/`. Add
  both to section 7.

### T15. The counter-check of step 4 does not say where the mutation lives

- Place: step 4.
- What is wrong: The mutation changes the library. Section 5 says that
  there is no trace code in the library. The plan does not say whether
  the mutation is a patch outside the tree, a compile definition, or a
  branch. "Guarded so that the safety checks of the run stay green"
  needs code in the library or in the harness.
- Proposed fix: The mutation is a patch file in the scratchpad. It is
  applied to a copy, built once, and deleted. The result file names the
  patch.

### T16. Reading `KONSENS_TRACE_DIR` is a read of a global

- Place: F8, step 2.
- What is wrong: The rule "Every function could run in a functional
  language" forbids a read of a global. The only exception is the run
  time limits. An environment variable that the harness reads deep in
  `world` breaks that rule.
- Proposed fix: The test case reads the variable once and passes the
  directory to `run` as a value. A pure function makes each record as a
  string. One place writes the file at the end of the run.

### T17. Smaller points

- 1.2 says that AcceptWithResult.tla gets commit, sync and crash "from
  OneEnvironment.tla". The module does not `EXTEND` or `INSTANCE`
  OneEnvironment. It holds its own copy (`AcceptWithResult.tla:13`,
  `Crash` at 390, `Restart` at 401). Write "in the form of".
- F4: the code ballot is `{config_id, n, priority, pid}` with the default
  `<=>` (`storage.hpp:22-29`). The order of the fields is the order of
  the comparison. The simulation keeps `configuration_id = 1` and
  `leader_priority = 0` (`simulation.hpp:141`), so F4 holds. But
  `ballot_above_promise` (`node.hpp:1383-1389`) can give `n = promise.n
  + 1`, and BLE has `recovery_round = 0`
  (`ballot_leader_election.hpp:26`). `HandleLeader(i, k)` takes `k \in
  1..MaxBallot` (`SequencePaxos.tla:2683`). The ASSUME of F4 must also
  check `n >= 1`.
- F5 is correct about the pid. `hb_timeout` returns only
  `current_ballot_` (`ballot_leader_election.hpp:59-76`), and that is
  always the ballot of the node itself. The `n.pid != pid_` branch of
  `handle_leader` (`sequence_paxos.hpp:792-795`) is not reached from
  `tick`. One more call site exists:
  `handle_leader(ble_.get_current_ballot())` after a majority refused an
  entry (`node.hpp:1971-1972`). It runs inside a delivery step, so the
  record of a delivery can also hold an election.
- Tick and `ResendMessageTimeout`: the code (`sequence_paxos.hpp:855-897`)
  and the model (`SequencePaxos.tla:2063-2116`) agree on the leader
  branch, the Prepare branch and the Recover branch. Two differences
  remain. The model resends the cached Promise (`cached_promise_message`),
  and the code builds a new one (`promise_for(*answered_prepare_)`,
  885-889). The model action needs `chan' # chan \/ phase' # phase`
  (2116), so a tick that sends nothing is a record with `UNCHANGED vars`.
  A tick resends only every `resend_ticks = 2` ticks (`simulation.hpp:63`).
- Sends at the commit: with `sync_by_the_caller = false`, the harness
  takes the messages after the commit, in the same step
  (`simulation.hpp:470-485`). `take_the_messages_before_the_sync`
  (`sequence_paxos.hpp:650-665`) keeps the order on each channel. So one
  record agrees with one model action. With random batches, the code
  sends after the last message of the batch, and the model sends after
  each `Receive`. The content and the order on each channel are the
  same.

### Verdict on plan 1: reject, and review a revised plan again

The method is sound, and the source claims are correct with the
exceptions in T12 and T13. The mapping is not ready. T1 (retirement),
T2 (step 0 cannot be written by hand), T3 (no counter-check in the
spike), T4 (second model), T5 (constants) and T11 (Community Modules)
change the steps and the questions to the owner. A revision needs one
mapping table (steps, constants, message fields) and a new step 0. A
second review of that revision can be short.

---

## Plan 2: error-branch coverage

### C1. The count 808 is correct

- Place: section 1.
- Evidence: `grep -ro "\[\[unlikely\]\]" src include program | wc -l`
  gives 808 in 23 files. The five counts per file are correct:
  persistent_storage.cpp 219, sequence_paxos.hpp 215, node.hpp 177,
  serve.cpp 46, config.cpp 44. `program/test/` has none.
- No change needed.

### C2. Template code that no test instantiates has no line in gcov

- Place: step 0, step 2.
- What is wrong: `node` is a class template (`node.hpp:132`,
  `template <storage, state_machine>`), and 392 of the 808 markers stand
  in `node.hpp` and `sequence_paxos.hpp`. A member function of a class
  template that no test calls is never instantiated. gcov then has no
  line record for it. Step 2 marks it "not decided", but it is "not
  reached".
- A header is also compiled in each test binary. Each object file has
  its own `.gcda`. The count of one marker line is the sum over all
  objects. Step 2 says "reads the gcov JSON of its line", as if there
  were one JSON per source file.
- Proposed fix: Add two forms to the spike of step 0: a member of a
  class template that is never called, and an inline function in a
  header that two translation units include. State in step 2: a marker
  with no line record is "not reached (not instantiated)", and the
  counts are summed over all objects. `gcovr` (a Debian package) merges
  the counts of a header over all objects and writes JSON. It is
  probably the known tool for that merge (not checked in this session).

### C3. A marker covers several error paths, and a marker counts as reached when one of them runs

- Place: section 1 ("the marker the list of the error branches").
- What is wrong: A marked block often holds more than one exit. In
  `error_code_or_throw` (`src/persistent_storage.cpp:182-193`) one marker
  covers `throw write_error` and `throw read_error`. In `execute`
  (1004-1012) one marker covers the BUSY return and the other return.
  If a test reaches the write error only, the read error shows as
  reached.
- Proposed fix: Count each `throw`, each `return std::unexpected` and
  each `return` of an error value inside a marked block as its own exit,
  by line. Report the exits with a count of zero, not only the markers.

### C4. Error exits with no marker are outside the measurement, and the plan does not count them

- Place: section 1, second point.
- What is wrong: The plan says that the result names the branches with
  no marker as outside the measurement. It does not list them. A rough
  count (a `throw`, `std::unexpected(` or `catch (` line with no
  `[[unlikely]]` in the same or the three lines above) finds 3 of 107
  throws, 17 of 416 `std::unexpected` lines and 3 of 3 `catch` blocks.
  Examples: `node.hpp:231` (`throw std::logic_error`), `node.hpp:401`
  (`throw sync_error`), `node.hpp:899`, `node.hpp:1573` and `1801`
  (`catch (...)`), `program/main.cpp:82`. This count is a heuristic. A
  nested branch inside a marked block counts as marked in it.
- A `catch` cannot carry `[[unlikely]]`, but it is an error path.
- Proposed fix: Let the script of step 2 also list these lines as
  "error exit with no marker". Each one is either a missing marker, which
  breaks the rule "Every error branch is marked unlikely", or a line to
  explain.

### C5. A process that ends by SIGKILL or by a fault writes no `.gcda`

- Place: section 1, list of tests; step 1.
- What is wrong: libgcov writes the counts when the process exits
  normally. `three_nodes.sh` sends SIGKILL to `konsensd`
  (`program/test/three_nodes.sh:6`, `77`, `297`, `423`). The counts of
  each killed process are lost. A test that expects the process to end
  on a fault of ours (a `logic_error` that nothing catches) also loses
  its counts. `-fprofile-update=atomic` does not change this. It only
  makes the counters correct across threads.
- Effect: the branches that these runs reach show as "not reached".
- Proposed fix: State the limit in the result file. Or call
  `__gcov_dump()` from a SIGTERM handler that only the coverage build
  has, and send SIGTERM before SIGKILL. That is a change of the tree,
  so it needs the owner's yes.

### C6. The power loss of counter_stress does not run in ctest

- Place: section 1, third point ("the power loss of counter_stress").
- What is wrong: ctest runs `counter_stress --clients 2 --increments 50`
  and the same with `--cbor` (`CMakeLists.txt:128`, `133`). `--crash`
  has the default 0 (`examples/counter_stress.cpp:96`). The crash path
  (`crash()` at 457-462, called at 910-911) then never runs in step 1.
  I found no other "power loss" in `test/` or `examples/`.
- `test_device_faults` is not a separate test. It is the FUSE test
  (`CMakeLists.txt:144-151`, `test/fuse/test_device_faults.cpp`), so the
  list names one test twice.
- Proposed fix: Correct the list. If the crash path of counter_stress
  is to be measured, step 1 runs it once more with `--crash`, and the
  result file says so.

### C7. gcov must be the gcov of g++ 16, and the plan does not name it

- Place: section 1 ("gcov is part of the gcc package"), step 0.
- What is wrong: A `.gcda` file of one gcc version is read by the gcov
  of the same version. On this machine, `/usr/bin/gcov` comes from the
  package `gcc` and points at `gcov-13` from `gcc-13-x86-64-linux-gnu`
  (`dpkg -S`). In an image with g++ 16, plain `gcov` can be another
  version.
- Also: `python3` in this session is `/usr/local/bin/python3`, which is
  not a package of the distribution. The plan names "python3 of the
  distribution". In the build image this can be different (not
  checked).
- Proposed fix: Name `gcov-16` in step 0 and step 2. Write its version
  into the result file. Use `/usr/bin/python3`.

### C8. The plan has no step that runs it, and no place outside the container for the data

- Place: section 1, last point; step 1.
- What is wrong: The plan says that this session has no podman and no
  g++ 16. It does not say who runs step 0 and step 1, or where. The FUSE
  test needs `/dev/fuse` and `fusermount3` in the container
  (`CMakeLists.txt:137-139`). The rule "What a run finds lies outside its
  container" asks that the `.gcda` files, the JSON and the result file
  lie in a mounted directory. Step 1 does not say this.
- Proposed fix: Add the place of the run (a Remote Control session on
  the owner's machine, or a CI job), the podman options for
  `/dev/fuse`, and the mounted directory for the build and the results.

### C9. The plan does not show that the run fits the five minutes

- Place: step 1.
- What is wrong: The rule "A run of build and tests takes five minutes"
  allows one exception, fuzzing. A debug build with `--coverage` and
  every option on, and ctest with the default seed counts, can take
  longer. The FUSE test already has a timeout of 60 s
  (`CMakeLists.txt:152`), and coverage makes it slower. The plan gives no
  time and no question to the owner.
- Proposed fix: Measure the time in step 0 on one test, or ask the owner
  for an exception for this one run.

### C10. The claim about Yuan et al. in Q1 has no source

- Place: section 4, Q1 ("Yuan et al. found most faults in the branches
  of the first kind").
- What is wrong: The sentence in the plan's own source line is "92 % ...
  came from the handling of non-fatal errors". It does not sort the
  errors by "failure of the machine" against "input from outside". I did
  not check the paper (no web access). Under the rule "Look before you
  speak", the claim needs a quote or must go.
- Proposed fix: Quote the section of the paper, or remove the sentence
  and give the reason for proposal (a) in other words.

### C11. Smaller points

- `--coverage` and `-fprofile-update=atomic` are the right GCC option
  names. `gcov --json-format` exists. `KONSENS_FUSE_TESTS` and
  `KONSENS_PROGRAM` exist with these names and default `OFF`
  (`CMakeLists.txt:135`, `177`). `KONSENS_BENCH` is `ON` by default, so
  `konsens_bench_each_case_once` also runs. That is correct, but the
  result file must list it as one of the tests.
- A debug build has `built_for_debugging = true` (`node.hpp:43-47`). The
  marker at `node.hpp:2215` is reachable only in a debug build. The
  result file must say that the build is a debug build.
- For a `&&` or `||` condition, GCC 14 and later have
  `-fcondition-coverage`. It reports each condition. The spike can
  check whether it answers more than the branch counts (not checked).

### Verdict on plan 2: approve with changes

The plan is small and its count is correct. Before the run, it needs:
C2 (template and header lines), C3 (exits inside a marked block),
C4 (exits with no marker), C5 (lost counts after SIGKILL), C6 (correct
list of tests), C7 (`gcov-16`) and C8 (where it runs). C9 and C10 are
questions for the owner message. None of them changes the goal of the
plan.
