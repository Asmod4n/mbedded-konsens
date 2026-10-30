# Review: plan-write-path.md (commit fb17134)

## Task

Review doc/plan-write-path.md before the build. Look for safety faults
of arm A and of option 2 of arm B, wrong facts, missing measurements,
arms that cannot reach the goal, tests that do not show what they
claim, and breaks of the rules. This review changes no other file. It
builds nothing, runs nothing and pushes nothing.

Read: the plan; bench/results/2026-10-30-055400Z-write-path-beds.txt;
doc/plan-group-commit.md sections 2 and 9; doc/plan-execute-verify.md
sections 2.1 to 2.9 and 4; include/konsens/node.hpp 243-311, 830-836,
1113-1138, 1649-1700; include/konsens/sequence_paxos.hpp 402-460,
1230-1283, 1308-1340, 1512-1630, 205-222; include/konsens/message.hpp
65-74; program/serve.cpp 408-453, 496-516, 563-576, 690-779;
refs/omnipaxos/omnipaxos/src/sequence_paxos/leader.rs 115-133;
refs/papers.md; model/tla (file list, SequencePaxos.tla 174, 434, 873);
/mnt/project-files/actor-raft/model/tla (file list).

Not checked: the Raft dissertation, the SQLite documentation, the
Eve paper, any trace on the machine.

## Verdict

Approve with changes. The measurements M1 to M4 can start. Arm A is
not built before findings 1, 2, 3 and 6 are in the plan. The owner
question B is not sent before findings 4 and 5 are in its text.

## Findings

### 1. Blocking. Arm A rests on a wrong fact about the own count.

Plan line 87-89: "the leader counts its own accepted index only after
its commit". The code counts it before the commit.
`accept_entries_leader` calls `leader_state_.set_accepted_idx(pid_, ...)`
at sequence_paxos.hpp:1330, inside the step. The storage commit comes
later, at sequence_paxos.hpp:455-458. The same holds at 1432 and at
1615-1616. doc/plan-group-commit.md section 9, R8, says this in words:
"the leader counts its own entry before the commit; this is safe only
because every message is held". Arm A removes that premise for the
AcceptDecide.

Arm A is still safe in konsensd today, for a different reason. The
loop of serve.cpp is one thread (690-779). An Accepted for an entry of
step k arrives through a completion of the ring. The loop reads
completions only after `commit_the_step` of step k returns. So no
Accepted for that entry is counted before the leader's write is
durable. That is a property of the caller, not of `node`.

Fix: write the true fact into arm A. State the invariant: no message
is handled between the early release and the storage commit of the
same step. Make `node` enforce it: a `handle_incoming`, `tick` or
client call between the release and `commit` throws
`std::logic_error`. The model mutation of section 7 becomes "an
Accepted is handled between the release and the write". Do not move the
own count after the commit unless a measurement shows it is needed: a
count after the commit needs one more step, and a decide in that step
writes the decided index again.

### 2. Blocking. An early AcceptDecide overtakes held messages to the same follower.

Each leader message to a follower carries a sequence number from
`next_seq_num(f)`: Decide (sequence_paxos.hpp:1246-1247), AcceptSync
(1460-1463), AcceptDecide (1332-1336). A follower takes a message only
when its number is the next one (message.hpp:65-74). A higher number
is `dropped_preceding` and calls `reconnected` (sequence_paxos.hpp:
1274-1283): the follower goes to recover and sends PrepareReq.

In one step the leader can make a Decide (in `handle_accepted`, while
it handles the batch) and then an AcceptDecide (in `end_step`, 418-424)
to the same follower. Arm A sends the AcceptDecide early and holds the
Decide. The follower sees number n + 1 first. It drops the
AcceptDecide and starts a recovery. Then the Decide arrives as
`outdated`. At 64 in flight a step often holds both, so this happens
often. The same holds for an AcceptSync and an AcceptDecide in the step
where a leader takes the promises and has buffered proposals: the
follower is not in the accept phase and drops the AcceptDecide
(1518-1521).

This is a fault of liveness and of speed, and a large one. It also
makes the arm look slower than it is.

Fix: the early set is, for each follower, the prefix of the outgoing
messages to that follower in the order the step made them. The prefix
ends at the first message that is neither an AcceptDecide nor a
Decide. A Decide depends on no write of the leader that a later node
needs: the decided index is a fact of a majority that accepted
durably. The model checks that claim too. Tests: one step with a
Decide and an AcceptDecide to one follower; one step with an AcceptSync
and buffered proposals. Each test checks that no PrepareReq leaves and
that the follower stays in the accept phase. The model has a sequence
for each pair (SequencePaxos.tla:873), so it can show the fault if the
split keeps the order of the code.

### 3. Blocking. The plan does not say where the release happens, and the send waits for the commit.

The AcceptDecide is made inside `commit`: node.hpp:269 calls
`seq_paxos_.end_step()`, which calls `accept_entries_leader`
(sequence_paxos.hpp:418-424). serve.cpp queues messages in
`release_the_output` (408-428) after `l.node.commit` (446), and
`arm_the_sends` (563-576) only prepares the send. The kernel gets it at
the next `io_uring_submit_and_wait_timeout` (749). So "node gives the
AcceptDecide before commit" (plan line 90-92) sends nothing earlier.
The arm as written cannot gain.

The release point also decides safety. Between `end_step` and the
storage commit, node.hpp:275-279 runs `serve_clients_in_the_step`. On
an error it resets the transaction, which holds the leader's own
append, and drops the held output. If the AcceptDecide already left,
the followers hold entries that the leader's log does not hold, and
the leader's own count (finding 1) includes them. That is safe only if
every such error stops the vote of this identity for good.
`execute_verify` returns the error of `get_promise` at node.hpp:
1655-1658, which is not `stop_voting`. Not checked: whether that error
leaves the node voting.

Fix: name the release point as the last thing before the storage
commit: after `serve_clients_in_the_step` and after
`read_checked_meta` (sequence_paxos.hpp:445-449), just before
`txn.commit()` (455). After that point the only failure is the throw
of `write_error` (457), which ends the process, and the start after it
retires the identity. Name the change of serve.cpp: the loop queues the
early messages, prepares the sends and calls `io_uring_submit` before
the commit. Add a test for each error return between `end_step` and
the storage commit: no AcceptDecide leaves on that path.

### 4. Blocking for the owner question. Option 2 gives up section 2.7.7 of Execute-Verify.

doc/plan-execute-verify.md 2.7.7: "A bit flip in the leader's count,
compare or copy cannot reach the state." This holds because the
followers check the `results` entry before they accept it. Option 2
(plan line 114-118) replies from the leader's selection before any
follower checked it. A flip in the leader's count, compare or copy then
reaches the client. The followers refuse later, and the leader throws
(2.7.6), but the client already holds the wrong bytes.

The model has `Flip` (plan-execute-verify.md 4.3). With `Flip`, the
check of plan line 116-118 cannot be green for option 2. The plan does
not say whether the check runs with `Flip`.

Fix: the owner question names this loss as a consequence of option 2.
The model check of option 2 runs with `Flip` and has the invariant
"each reply is the chosen result". Expect a violation, and show the
trace to the owner. A form that keeps 2.7.7 replies only after a
majority accepted the `results` entry. That is the decide of option 1,
so the gain of option 2 comes from exactly the check that 2.7 needs.

### 5. Blocking for the owner question and the test. With floats, a later leader can choose other bytes.

Two results agree within `float_digits` (plan-execute-verify.md 2.2.2,
2.2a). They need not be equal byte for byte. Example with `Spread`
(4.2), three nodes L, f1, f2. L counts L = jitter 0 and f1 = jitter 1.
They agree, so L replies the bytes of jitter 0. L crashes before
`results` is decided. f2 leads, and the nodes answer again (2.5.3).
f1 now gives jitter 2, f2 gives jitter 1. They agree with each other,
and the tie goes to the lowest node id. If that is f1, `chosen` holds
jitter 2, which does not agree with the reply of jitter 0. If f1 gives
jitter 1 again, `chosen` holds jitter 1 while the client got jitter 0:
other bytes and another CRC. 2.9.1 says the client gets "the reply in
`chosen`, with its CRC". Option 2 breaks that.

The test of plan line 127-129 ("the new leader selects the same
result") fails for a result with floats. It passes only for exact
results.

Fix: the owner question says that under option 2 the reply can differ
from the applied state, inside the float tolerance and, with a second
run, outside it. The model runs option 2 with `Spread` and the
invariant "each reply agrees with the chosen result". The test has a
case with float results and two nodes at the edge of the tolerance.

### 6. Blocking. Section 7 names the wrong model.

Plan line 137: "/mnt/project-files/actor-raft/model/". That directory
holds 41 files in tla/ and no Execute-Verify configuration. The model
of this repository is model/tla/SequencePaxos.tla, with `Flip`
(SequencePaxos.tla:142-144), the Execute-Verify configurations
`MC_ev_*`, and 385 files in all.

Fix: name model/tla/SequencePaxos.tla and the configurations that the
arms extend: the group-commit configurations for arm A, the `MC_ev_*`
configurations with `Flip` and `Spread` for arm B.

### 7. Not blocking. The goal at 64 in flight needs a cut of CPU per write that no arm names.

Result file lines 31 and 39: the three nodes use 102 + 120 = 222 us and
107 + 114 = 221 us of CPU per write. The goal is 38 243 to 39 701
writes/s (plan line 24). 38 243 writes/s x 221 us = 8.5 processor
seconds per second. The bench has 3 processors (podman --cpus=3,
result file line 22), and the rule leaves one processor to the
machine. So the goal needs at most 3 / 38 243 = 78 us of CPU per write
for the three nodes, a cut by a factor of about 2.8, before the load
client is counted. Plan line 38 ("the CPU is not the limit at that load
now") is true at 3 400 writes/s and false at the goal.

This is a bound from measured numbers, not an estimate of a gain.

Fix: write the bound into section 1. Give it to the owner with
question B, because the goal may not be reachable on this machine.

### 8. Not blocking. The goal at 1 in flight needs a count of the commits on the path.

The goal is 465 to 612 writes/s at 1 in flight, so 1.63 to 2.15 ms for
each write. One raw SQLite commit takes 0.82 to 1.08 ms on the own bed
(1 / 1 224 to 1 / 929 writes/s). So the path holds at most two commits
in sequence, with the network and the CPU of the step on top. The CPU
of the three nodes per write is 3.6 to 4.4 ms (plan line 36-37).

Reading the code, a write makes four writing steps on each node: the
request entry, its decided index, the `results` entry, its decided
index. Fact 1.4 reports 2.1 fdatasync per node per write, and fact 1.5
two rounds. Not checked: which writes share one commit.

Arm A takes one leader commit off the path. Option 1 of arm B keeps
two follower commits on it. No arm takes off the commits that the
Decide, the answer of a follower and the reply wait for.

Fix: M2 maps each fdatasync to the writes it holds and to the messages
that wait for it. The plan then states, for each arm, which commits
stay on the path. It also states the CPU of the step on the path. An
arm that leaves more than two commits on the path cannot reach the
goal at 1 in flight, and the owner sees that before its build.

### 9. Not blocking. A missing measurement of the bed, and a wrong fact about it.

Both beds put the three nodes on one host disk (result file lines
9-10). Plan line 50-51: the shared bed "shows the cost of three nodes
on one disk". It shows the cost of one file system for three nodes.

Raw SQLite runs as one process (plan line 49). The cluster writes on
three file systems of one host disk at once. Nothing shows that the
host disk gives three times 79 401 writes/s.

Fix: correct the sentence. Add one cell: three raw SQLite processes at
once, one on each file system of the own bed, at 1 and 64 writes per
commit. That is the bound of the bed for three nodes.

### 10. Not blocking. Fact 1.6 gives a wrong share.

Plan line 37-38: "0.7 of one processor". From the result file:
3 389 x 222 us = 0.75 and 3 451 x 221 us = 0.76.

Fix: "0.75 to 0.76 of one processor".

### 11. Not blocking. Fact 1.1 has no floor.

Plan line 13-17 states that the bed changes the result. The result file
has one sample per cell and no A/A floor (lines 13-14). The rule says a
difference under the floor is not a difference.

Fix: mark fact 1.1 as provisional until the A/A floor of step 2 is
measured, and mark the goal numbers of item 1.3 the same way.

### 12. Not blocking. The table of builds does not match the builds of the tree.

Section 6 (line 131-132) tests Release with g++ 15, g++ 16 and clang
22. Section 2.2 (line 52-53) benches g++ 16, g++ 16 -march=x86-64-v3
and clang 22. The rule: one column for each build. "g16aa" is a name
with no definition in the plan.

Fix: add a g++ 15 column, or state that g++ 15 is not a build of
konsensd. Define the name, or write "g++ 16 -march=x86-64-v3".

### 13. Not blocking. M3 does not measure what it asks.

`sqlite3_wal_hook` gives the page count of the WAL after a commit. From
memory, not checked: the count is the whole WAL since the last
checkpoint, and a call of `sqlite3_wal_hook` replaces the hook of the
automatic checkpoint, so the WAL grows and the run changes. The hook
also needs a change of code; section 3 says "no change of code". The
dbstat table gives the pages of each table, not the frames of each
commit. Fact 1.4 counts calls of pwrite64; from memory, not checked,
SQLite writes a frame header and its page in two calls, so 20 calls
are not 20 pages (line 75-76).

Fix: check both claims in the SQLite text and put the text in refs/.
On a copy taken before a checkpoint, read the page number of each
frame from the WAL file. Map each page to its table with dbstat.

### 14. Not blocking. strace -c changes the batches that M1 counts.

Plan line 68-69. strace slows each system call, so more messages wait
for each step and the batch grows.

Fix: report the writes/s with and without strace in the same row. Take
the batch size from the counter of the Debug build only, and mark that
a Debug build has its own timing.

### 15. Not blocking. The test of arm A does not show what it claims.

Plan line 123-126. After a crash of the leader, the followers do not
decide by themselves. A new leader takes the entry from the promises.
The second sentence ("A decide on the leader never counts its own
accepted index before its commit") is false for the current code
(finding 1), and in konsensd no test can make an Accepted arrive
between the release and the commit.

Fix: test 1: the leader crashes after the release; a new leader is
elected, decides the entry, and the retry of the client gets one reply
from the session. Test 2: a call to `node` between the release and the
commit throws `std::logic_error` (finding 1). Tests of findings 2 and
3.

### 16. Not blocking. Breaks of the rules.

- Line 133: "(memory build-und-tests)". The words are not English, and
  the text points at the memory of a tool (.claude/RULES.md "Nothing
  points at a tool"). Fix: name the build and the tests.
- Section 5: the question B shows no search for a consensus
  (.claude/RULES.md "A known consensus is shown, not asked"). Systems
  of the Execute-Verify kind and systems that reply at the commit or at
  the apply are the sources to read. Not checked here.
- Section 9 keeps cpu.sh and disks.sh outside the tree over several
  runs. RULES.md "Each run of the bench is one file": the code of a
  bench is deleted after the run, or it stands in the tree and is
  committed before the run. Fix: commit the bed scripts under bench/,
  and name that commit in each result file.
- Section 2.3 measures the run length again. RULES.md "A run is as
  short as the noise allows": the length is measured once for each
  machine and kept in the state of the project. Fix: read it from the
  state file, or say that the new bed is a new machine for this rule.

### 17. Not blocking. Fact 1.7 hides a known obstacle.

refs/papers.md lists the Ongaro dissertation as CC BY 3.0, with the
download blocked by a missing root certificate (2026-10-29). Plan line
43-44 says "if its download works". Arm A does not need the
dissertation for its safety; findings 1 to 3 give the argument from
this code.

Fix: name the known obstacle. Keep the claim of the dissertation out of
the safety argument.

# Round 2

## Task

Review sections 10 to 12 and arm F of doc/plan-write-path.md at commit
b00f4a7 against the code. Check whether sections 10 to 12 contradict
sections 1 to 9. This review changes no other file.

Read: plan sections 10 to 12; bench/results/2026-10-30-061200Z-write-path-steps.txt;
bench/results/2026-10-30-060200Z-write-path-callgrind.txt (lines 44,
102, 114, 152); include/konsens/node.hpp 207, 243-311, 645-690, 875-935,
1140-1310, 1341-1372, 1436-1460, 1715-1780; include/konsens/sequence_paxos.hpp
430-460, 1608-1630; program/serve.cpp 408-453, 690-779;
program/main.cpp 82; include/konsens/persistent_storage.hpp 175-199.

Not checked: the SQLite documentation, the Raft dissertation,
CockroachDB, any run.

## Verdict

Approve with changes. Arm F is safe in the form the plan gives, under
the same invariant as arm A. As written it does not take the apply sync
off the path (finding 2.1). The plan must also become one text in force
(finding 2.3) before the build.

## Correction of round 1

- Findings 4 and 5 of round 1 are void. They rest on the two-round form
  of plan-execute-verify.md 2.1 and 2.6.5. The code makes one round:
  `work`, the answers, then one done record (node.hpp:1158-1235).
- Finding 8 of round 1 counted four writing steps per node per write.
  That count came from plan-execute-verify.md, not from the code. The
  code gives two per node: the append and the apply. This agrees with
  fact 1.4 (2.1 fdatasync per node per write) and with section 12.
- The protection of plan-execute-verify.md 2.7.7 still holds with arm F.
  A follower accepts a done record only up to its `verified_idx`
  (sequence_paxos.hpp:1617-1628). So a decide needs a majority that
  checked the record, and arm F replies only after the decide.

## Findings

### 2.1. Blocking. Arm F leaves the apply sync on the path at 1 in flight.

Section 12, last sentence: "With arms A and F ... step 4 has no sync on
the path." That is false for a client that sends its next write after
the reply. konsensd is one thread (serve.cpp:690-779). The apply commit
blocks that thread in `txn.commit()` (sequence_paxos.hpp:455-458). The
reply leaves before the sync, and the next request reaches the leader
during the sync. The leader handles that request, and sends the next
`work`, only after the sync returns. So arm F saves at most the time the
client and the network take to send the next request. It does not save
the 1.39 ms of the sync (result file, step 4).

The follower has the same fault. Item 5 of section 12 says that the
apply sync of the follower is not on the path. Each follower syncs its
apply when the Decide arrives, at about the time the leader replies.
Once the leader's own sync is off the path, the next `work` reaches the
followers during their apply sync (1.34 ms). The leader needs the answer
of one follower, and both followers sync at the same time.

Fix: arm F has two parts, measured as two arms.
- F1: the reply leaves before the apply commit, as written.
- F2: a commit whose writes are only the decided index and the apply
  does not sync, on the leader and on the followers. The next commit
  that syncs makes it durable. From memory, not checked: SQLite in WAL
  mode with `PRAGMA synchronous = NORMAL` commits without a sync, and a
  later sync of the WAL covers every earlier frame. Check this in the
  SQLite text before the plan uses it, and put the text in refs/.
  Safety of F2: a crash retires the identity, so no node reads a lost
  apply under that identity. A clean stop keeps the identity. So the
  clean stop syncs the log database before it writes the mark, because
  the mark is in another database (persistent_storage.hpp:184-199,
  node.hpp:207). The read path reads the committed state through a read
  transaction (node.hpp:1751-1758). In the same process that state holds
  a commit without a sync, so a read after a reply sees the write.
  F2 is a new design, so the owner decides it.

### 2.2. Blocking. Arms A and F rest on one invariant, and the plan must say so.

Arm F says the reply depends on a done record that is durable on a
majority after step 3. With arm A that is true only under the invariant
of R1: no Accepted is handled between the early release and the
leader's commit. The leader counts its own index before its write is
durable (sequence_paxos.hpp:1330). Without the invariant, the leader
could decide on its own index, which is not durable, plus one follower,
and reply. If the leader and that follower then crash, both identities
retire. The entry can then be absent from every promise of a new
leader, and the client holds a reply for a lost entry.

Fix: section 12 states that the reply of arm F rests on the invariant of
R1 when arm A is in force. The replies of arm F leave at the release
point of R3, inside the same interval. The model mutation of R1 also
releases a reply, and the invariant "each replied entry is decided on a
majority of durable logs" must fail with it.

### 2.3. Blocking. Sections 1 to 9 contradict sections 10 to 12.

- Arm A (line 90-92): "every client reply leaves after the commit, as
  now". Arm F changes that.
- Fact 1.5 still says that Execute-Verify adds a second round with the
  `results` entry. Section 11 says that round does not exist.
- Arm B (section 4), section 5, the arm B test of section 6, the arm B
  model of section 7 and step 3 of section 8 are still in the text.
  Section 11 withdraws question B.
- R4 and R5 of section 10 say what is done ("The question names both
  losses. The model runs with Flip and Spread"). Section 11 says R4 and
  R5 are void.
- R1 says "fact 1.5 and arm A are wrong about the count". The wrong
  count stands in arm A (line 87-89), not in fact 1.5.
- Sections 6, 7, 8 and 9 have no entry for arm F.

An agent that builds from this plan reads sections 1 to 9 as in force.

Fix: rewrite sections 1 to 9 so that they hold what is in force. Keep
the wrong assumption whole in section 11. Delete or mark as void the
text of arm B. Add arm F to sections 6 to 9.

### 2.4. Not blocking. Arm F must name the replies that leave early.

`client_replies_` is one list (node.hpp:661). It holds the replies of
the apply (`reply_to_the_client`, 1341-1366), and also the retry
replies of an old promise (1738), the read replies (1756), the retry
replies of reads (1727, 1773) and the refusals during a
reconfiguration (921). Most of these are made after the commit, in
`finish_the_step`. Some are made before it.

Fix: arm F releases early only the replies that `reply_to_the_client`
makes in `apply_the_done_records` of this step, rejected entries
included. Every other reply leaves after the commit, as now.

### 2.5. Not blocking. The release point inside `commit` needs an API, and the API is an owner question.

R3 puts the release just before `txn.commit()`, inside
`sequence_paxos::commit` (sequence_paxos.hpp:455). The replies and
`work` belong to `node` (node.hpp:649-663), and konsensd must call
`io_uring_submit` in that interval. So `node` needs a new form. One
option splits `commit` into a call that gives the early output and a
call that writes. Another option makes `commit` take a function that
sends. That is a new design.

The open item of R3 ("the error path of `serve_clients_in_the_step`
after the release is checked ... before the build") is closed by R3
itself. `serve_clients_in_the_step` (node.hpp:275) runs before
`seq_paxos_.commit()` (283), so its error path comes before the
release.

Fix: put the two options of the API in the next owner message, with a
proposal. Delete the open item of R3.

### 2.6. Not blocking. The crash between the reply and the apply commit is safe. The test is missing.

The reply bytes come from the decided done record (node.hpp:1271,
`decode_result(e.result)`), after its CRC check (1276) and after the
read-back (1292). The session record holds `{request_number, reply}`
(node.hpp:1453), and it is written by every node that applies the
decided record. A failed `txn.commit()` throws `write_error`
(sequence_paxos.hpp:457). main catches it and ends (main.cpp:82). The
next start retires the identity. A retry of the client at the new
leader gets the same reply bytes from the session record.

Fix: a test. The leader releases the reply, then crashes before the
apply commit. A new leader is elected. The client retries with the same
request number and gets the same bytes. The write is applied once on
every node.

### 2.7. Not blocking. Numbers of section 12 do not match the result file.

- "1.4 to 1.9 ms of a step with a sync is not the sync". The result file
  gives 3 233 - 2 010 = 1 223 us (leader, append), 2 814 - 1 390 =
  1 424 us (leader, apply), 2 828 - 1 340 = 1 488 us (follower, apply).
  That is 1.2 to 1.5 ms.
- The result file gives one sample for each step, from 40 writes. The
  rule asks for each repetition, the median and the spread.
- The times come from a run under strace, which slows each node (63
  writes/s against 99 to 115). Section 12 does not say that next to the
  numbers.
- The raw lines give the kinds of the messages as numbers with no
  legend. Some lines of n1 show "in=4, out=5,5": one message in, two
  of one kind out. That is the pattern of a leader that sends an
  AcceptDecide to each follower, but the summary names n3 as the leader.
  Not checked.
- The context lacks items that the rule names: the dirty mark (a patch
  outside the tree was applied), the C library, the scheduler, the
  priority, the number of threads, the digest of the processor flags.
- Line 152 of the callgrind file says "start included". Section 10
  quotes 42.9 and 31.2 calls of `sqlite3_step` per write without that
  note.

Fix: give the medians and the spread over the 40 writes, with the
legend of the kinds and the full context. Mark each time as measured
under strace.

### 2.8. Not blocking. The known practice is from memory.

Section 12 quotes the Raft dissertation, chapter 5, and CockroachDB,
both from memory. The argument of arm F needs neither. It rests on two
facts of this code: the reply comes from a decided record, and a crash
retires the identity (finding 2.6). Remove the quotes, or read the
sources and give the sections.

## Arm S (section 15), round 1

Verdict: approve with changes. Ten findings; the changes R1 to R10 in
section 15 of the plan answer them. Findings 1 to 3 (a stale synced
length after append_on_prefix, a decide of one index only, the scope of
item 5) changed the form before the build.

## Arm S2 (section 17), round 1

Verdict: approve with changes. Ten findings; Q1 to Q9 in section 17 of
the plan answer them. Findings 1 to 4 (the sync number of held output,
a partial rollback, the meaning of an open step, reads of committed
state) changed the form before the build.

## Review of arm P (section 19)

## Verdict: CHANGES

The idea is safe in my reading. A value in memory is the value that the
node committed after a check. No vote happens without a check at the
begin or at the end of a step. Eight changes are requested before the
build (P1 to P8). The main ones: one divergence after a failed commit,
one earlier decision that the arm reverses, one client read path that
loses its check, tests that do not hold or do not build, and a simpler
form.

## Answers

### 1. Can the value in memory differ from a checked read?

Yes, in one path: a failed commit.

- `commit_phase_two` (sequence_paxos.hpp:494-499) throws `write_error`
  when the commit fails. `persistent_storage::transaction::commit`
  (persistent_storage.cpp:1700-1707) runs `commit_without_a_sync` and
  then `sync_the_wal`. When only the sync fails, the commit is visible
  to the next reader. Item 1 sets the kept record "once
  `commit_phase_two` committed it". So the kept record stays at the
  old step, and a checked read gives the new step. "An error drops
  it" does not name this place. The getters do not call
  `throw_if_a_write_did_not_finish` (sequence_paxos.hpp:923-938), so a
  caller still reaches them through `node::get_sequence_paxos()`
  (node.hpp:629).

No difference in the other paths:

- Start: `with` (sequence_paxos.hpp:337) runs before any kept record.
  It takes the checked read. node.hpp:174 also runs at the start.
- Recovery (`start_kind::clean`) and build: every write goes through
  `txn_` (for example sequence_paxos.hpp:1938-1955) and ends in
  `commit_phase_one`, which checks (sequence_paxos.hpp:464).
- Stop: `commit_the_open_transaction` checks (sequence_paxos.hpp:516)
  and goes through `commit_phase_two`. `sync_the_last_commit`
  (sequence_paxos.hpp:578-597) writes the decided index with the value
  that it read, so the four values do not change.
- Retire: `retired_` is set in memory only (node.hpp:1003-1012).
- S2 with the transaction open over ended steps: `txn_` is set, so
  item 2 takes the meta of the transaction. `commit_phase_one` checked
  it at the end of the last step (sequence_paxos.hpp:464-475), and the
  mark does not rise on that path.
- The window between `commit_phase_one` and `commit_phase_two`: `txn_`
  is empty and `pending_commit_` holds the step. The checked read
  gives the last committed step. The kept record is the same step.
- Rollback: a rollback leaves the last committed state, which equals
  the kept record. The callers in sequence_paxos also set `error_` or
  `write_unfinished_` (sequence_paxos.hpp:419-420, 456-457, 572-573,
  1232-1235, 1240-1242). One caller does not:
  `node::roll_back_the_step` (node.hpp:848-855) rolls back after a
  failed `serve_clients_in_the_step` or a failed apply, with no error,
  when no ended steps are held. The committed state does not change
  there either.
- sync_by_the_caller: `commit_without_a_sync` makes the commit visible
  at once, so the kept record and the read agree.
  `the_sync_failed` (sequence_paxos.hpp:568-574) rolls back, so item 1
  drops the record there.
- Read-only path: `read_decided_suffix`, `get_prefix_chain_crc` and
  `check_applied_chain_crc` (sequence_paxos.hpp:909-972) keep their own
  read-only transaction and their own check. Arm P does not touch them.

### 2. Does a path write the storage outside a transaction of sequence_paxos?

No path writes the four values outside `sequence_paxos`.

- `own_storage_` is `node_storage`. It opens its own file
  (`node_file_name` = "node.db", persistent_storage.cpp:32, 2002). It holds the
  proof and the clean-stop mark only.
- The node writes the store only through `seq_paxos_.txn_`
  (node.hpp:961-966, 972-978, 984-990).
- A nested transaction is a savepoint in `txn_`
  (persistent_storage.cpp:1670-1683). Its methods do not write the
  meta row (persistent_storage.cpp:1760-1818). `get_meta` throws while
  a nested transaction is open (persistent_storage.cpp:1386), so the
  D1 cache cannot hold a value of a rolled back savepoint.
  `roll_back_the_nested` does not reset `writer_meta`
  (persistent_storage.cpp:1239-1245). That is correct only while a
  nested transaction does not write the meta.
- `sync_the_last_commit` is a write transaction of `sequence_paxos`
  that does not use `txn_` (sequence_paxos.hpp:578-597). It keeps the
  values. Item 1 does not name it.
- A second connection writes the store in the tests
  (sqlite_damage.hpp:28-37). Not checked: whether a tool of the tree
  writes the store while a node runs.

### 3. Does the plan miss a damage that the checked reads find now?

Three differences.

a. The client read path loses its check. `finish_the_step` reads the
   promise (node.hpp:429) and then calls `serve_reads`
   (node.hpp:436). `answer_indexed_reads` (node.hpp, the function after
   `serve_reads`) opens a read-only transaction and reads `applied_idx`
   with `get_meta` only, with a CRC check and no step check. Today the
   getter runs the step check through the reader connection just
   before. With arm P, a reply to a client read can leave from a store
   that the step check calls damaged. This holds above all for a step
   with no write transaction: `commit_phase_one` then returns before
   its check (sequence_paxos.hpp:461-463). The last check can then
   lie many calls back.

b. Arm P reverses an earlier decision. plan-lmdb-only-source.md:150-153
   (2.1 item 3) says that a getter between two steps runs the step
   check, "so no value leaves the node without the check". Review
   finding 5 (review-plan-lmdb-only-source-1.md:17-21) asked for it.
   The test at test_sequence_paxos.cpp:668-689 checks it: after a
   damage, `get_promise` and `get_decided_idx` must return
   `inconsistent_storage`. With arm P both return the committed value,
   so this test fails. Section 19 names neither the decision nor the
   test.

c. The time of detection moves. Without S2, a damage between two steps
   is found by the next `begin_step`. With S2 and an open transaction,
   `begin_step` does no check (sequence_paxos.hpp:1213-1216). The
   damage is found at the end of the next step
   (sequence_paxos.hpp:464). The output of that step leaves only after
   that check (node.hpp:398-405), so no vote leaves on the damage.

I found no path where a vote or a Paxos message leaves without a check
of its step. `tick` (node.hpp:232-236) passes the promise to
`handle_leader`, which runs `begin_step` first
(sequence_paxos.hpp:719).

### 4. Are the tests enough, and do they build?

- "A storage that counts its reads": no such helper exists.
  sqlite_storage.hpp and sqlite_damage.hpp have none. Two wrappers
  exist that the test can extend: `recording_storage`
  (test_node.cpp:342-412, in an unnamed namespace) and `disk_storage`
  (node_cluster.hpp:46-120). A simpler form needs no wrapper: after a
  commit, put an older meta value with `change_with_sqlite`, as
  test_sequence_paxos.cpp:668-689 does. `get_promise` then returns the
  committed promise. That shows that it read nothing.
- "After a rollback, the next get_promise makes the checked read": it
  does not name which rollback. The one rollback with no error is
  `node::roll_back_the_step` (node.hpp:848-855). The other rollbacks
  come with `error_` or `write_unfinished_`. The test must name each
  path: `roll_back_the_step`, a damage that stops the vote,
  `the_sync_failed`, and a failed commit.
- "A damage between two steps is found by the next begin_step": this
  is wrong with S2 and an open transaction (answer 3c). Also,
  `change_with_sqlite` runs `BEGIN IMMEDIATE` and requires
  `SQLITE_OK` (sqlite_damage.hpp:32). It fails while S2 holds the
  write transaction. So the test runs only with no open transaction.
- Missing: a test of the failed commit (answer 1). `disk_storage` can
  take a flag that makes `commit` run `commit_without_a_sync` and then
  return an error.
- Missing: a test of the client read path (answer 3a).
- Missing: the S2 run of the new tests (section 17 Q7 runs R7 with S2;
  arm P needs the same).

### 5. Other faults and a simpler form

- The simpler form: `mark_` (sequence_paxos.hpp:2102) already holds the
  four values of the last committed step. It rises only at
  sequence_paxos.hpp:349, 480 and 502, each time with a checked record.
  A checked record is never below the mark (sequence_paxos.hpp:1172).
  So `raise_mark` (sequence_paxos.hpp:309-314) gives exactly
  `mark_of` of that record. A rollback, also the one of
  `roll_back_the_step`, does not change the committed state and does
  not change `mark_`. Outside `txn_`, with no `error_` and no
  `write_unfinished_`, `mark_` equals the committed state. No new
  member and no drop point is needed. The condition covers the failed
  commit too, because `commit_phase_two` sets `write_unfinished_`
  before the commit (sequence_paxos.hpp:495).
- `execute_verify` reads the promise (node.hpp:1888) and then reads
  `m = seq_paxos_.read_meta()` (node.hpp:1891) in the same
  transaction. `m->promise` is the same value. This removes one of the
  two calls per step, with no new state.
- Item 1 keeps a whole `meta_record`. The getters give four values. A
  kept `applied_idx` would be wrong after the next `set_applied`.
- The name `read_meta_between_steps` claims a read. With arm P it
  reads in the cold path only.
- Section 19 says that the checks run four times per step. With S2,
  a step that continues an open transaction runs three: `begin_step`
  does not check (sequence_paxos.hpp:1213-1216).
- Item 2 calls `txn_->get_meta()`. It is cheap only with the D1 cache
  (persistent_storage.cpp:1203-1213). The plan does not say that the
  base holds D1.

## Requested changes

P1. The failed commit. Use the form of P3. Or, if the kept record
    stays: `commit_phase_two` drops the kept record before it calls
    `commit`, and sets it after the commit succeeded. Test: a
    `disk_storage` whose `commit` runs `commit_without_a_sync` and
    returns an error. After the `write_error`, `get_promise` returns
    the promise of the failed step or an error, never the promise of
    the step before.

P2. Name the reversed decision. Section 19 cites
    plan-lmdb-only-source.md:150-153 and
    review-plan-lmdb-only-source-1.md:17-21, and says why the new form
    keeps their goal: the value in memory is the last checked value,
    and every vote runs after a check of its step. The test at
    test_sequence_paxos.cpp:668-689 changes: after the damage,
    `get_promise` returns `ballot_of(2, 2)`, and the next `handle`
    returns `inconsistent_storage` with `!votes()`. The owner decides
    this before the build, because it is a question about safety.

P3. Simpler form. The four getters return `mark_` when `!txn_ &&
    !error_ && !write_unfinished_`. Inside `txn_` they return
    `txn_->get_meta()`. Else they make the checked read as now. Item 1
    is then not needed. If the owner prefers a separate member, it has
    the type `high_water_mark` and not `meta_record`.

P4. `execute_verify` takes the promise from `m->promise` of
    `read_meta()` (node.hpp:1888-1893). Measure P4 alone as its own
    arm, beside P3, against the same base. The plan then shows which
    of the two calls carries the cost.

P5. The client read path keeps a check. `answer_indexed_reads` runs
    `seq_paxos_.read_checked_meta(t)` on its own read-only transaction
    `t` (node is a friend, sequence_paxos.hpp:621-622). A damage there
    stops the vote, and no reply leaves. This runs only when `reads_`
    is not empty. Test: a client read with a read index, a damage of
    the meta row through `change_with_sqlite`, then a step with no
    write transaction. No reply leaves, and the node stops voting.

P6. Fix the tests of section 19:
    - Replace "a storage that counts its reads" with the damage form of
      answer 4. It needs no new helper.
    - Replace the rollback test with one test for each path: a failed
      `serve_clients_in_the_step` that runs `roll_back_the_step`
      (`get_promise` gives the last committed promise), a damage that
      stops the vote, `the_sync_failed`, and the failed commit of
      P1.
    - The damage test states its form: without S2 the next
      `begin_step` finds it. With S2 it runs only when no transaction
      is open. Each damage test ends with `check_stops_voting`
      (test_sequence_paxos.cpp:610-615), so it also shows that no
      message leaves.
    - Run every new test with S2 too.

P7. Correct the facts of section 19. With S2, a continued step runs
    three checks, not four. The base of the measurement holds D1.

P8. The name `read_meta_between_steps` no longer says what happens.
    Proposal: `meta_between_steps`. The owner decides the name.

## Review of arms U and M (section 20)

## Verdict: CHANGES

The form is sound. The plan leaves out the bookkeeping that multishot changes, and the Unix form conflicts with its own test.

## Answers

### 1. A completion after a close, and c.pending

- An id is never reused. `added_connection` takes `l.next_id++` (serve.cpp:183) in a 56-bit space (serve.cpp:47-48).
- A completion for an id that is gone is dropped (serve.cpp:676-679).
- The connection stays until `c.pending == 0` (serve.cpp:225-227). A late completion therefore finds its connection, but only while pending is right.
- Today every completion decrements pending (serve.cpp:681). With multishot, one SQE gives many CQEs. That code would reach zero after the first CQE, erase the connection while the recv is still armed, and drop the rest at serve.cpp:677. With provided buffers, each dropped CQE also loses a buffer for good.
- The plan does not say how pending counts a multishot. See M1.
- The end of a multishot after a close: `begin_the_close` calls `shutdown(SHUT_RDWR)` (serve.cpp:223). The recv then ends with res 0 and no F_MORE. Queued bytes can still arrive first, with F_MORE. The slipstream engine has the same behaviour (deps/slipstreamIO/src/engine_posix.c:280-305). For the kernel I did not check this in this session. See M12.

### 2. A bound on provided buffers

- Memory per connection does not change. `http_in` closes above `most_client_bytes` (serve.cpp:591-593). A frame above `max_frame_bytes` closes the connection (serve.cpp:541-545, frame.cpp:48).
- Within one turn, one connection can take every buffer of its ring. The kernel fills buffers as long as bytes and buffers exist.
- Every buffer of a CQE in `l.ready` goes back while that CQE is handled. So at the end of the loop over `l.ready` (serve.cpp:834-836), every buffer of the turn is back.
- A recv on another connection that finds no buffer ends with -ENOBUFS and no F_MORE (engine_posix.c:283-288; the comment there names it the kernel's contract). That connection then waits for a re-arm.
- Today `handle_a_receive` closes on every `res <= 0` (serve.cpp:582). Without a change, ENOBUFS closes a healthy connection. See M4.
- With one ring for all connections, a client that sends large bodies can take the buffers that the peers need for their messages. The peer messages carry the heartbeats of leader election. See M5.
- The CQ holds 2 x `ring_entries` = 512 entries (serve.cpp:43, 808). More buffers than free CQ entries can overflow the CQ. From memory, the kernel then ends the multishot without F_MORE. I did not check this in the kernel source in this session. See M5.

### 3. The provided buffer ring on the slipstream engine

- The engine serves `IORING_REGISTER_PBUF_RING` with flags 0 and a caller ring (deps/slipstreamIO/src/slipstream_engine.c:623-645). `io_uring_setup_buf_ring` with flags 0 uses that form (liburing 2.13).
- The engine serves multishot recv with buffer select (engine_posix.c:274-306) and multishot accept (engine_posix.c:250-271).
- test/parity.c:575-606 and :969-971 compare both with the kernel on an AF_UNIX socketpair.
- So the source says it works. No konsensd run on the engine shows it yet. M refuses to start on the engine only if the one-file test of M12 shows a difference.
- The bench containers now allow io_uring through a seccomp profile (state file mbedded-konsens.md:196). The bench therefore measures the kernel. A run under the default seccomp profile of podman gives the engine.

### 4. The Unix socket form

- `split_host_and_port("unix:/p")` gives host "unix" and port "/p", and the port fails (address.cpp:57-74). So no address that works today changes meaning, except `unix:<digits>`. With the prefix, `unix:7151` names a socket file "7151". The prefix check must come before the split.
- `sockaddr_un` (110 bytes) fits in `sockaddr_storage` (address.hpp:20-23). The length is `offsetof(sockaddr_un, sun_path) + path size + 1`, as unix(7) gives it.
- The bound of a path: `sun_path` holds 108 bytes. A path of 107 bytes and its NUL fit. "Longer than sun_path" is off by one. The bound is 107 bytes.
- An abstract name: no. An abstract socket has no file permissions, and any process in the network namespace connects. A NUL anywhere in the path is malformed. TOML lets a string hold `\u0000`, so the check is needed.
- A stale socket file: the plan says the bind fails. three_nodes.sh restarts nodes after a clean stop (three_nodes.sh:135-136, 185-190, 195-196) and after SIGKILL (three_nodes.sh:202-207). Under the plan, every restart on `unix:` fails to bind. The unix run of three_nodes cannot pass. See M9.
- TCP_NODELAY: `start_a_connect` sets it on every socket (serve.cpp:520). On AF_UNIX it fails with EOPNOTSUPP, and the result is ignored. The plan sets it on TCP only. That is right.
- SO_REUSEADDR: `listener_on` sets it on every socket (serve.cpp:716). On AF_UNIX it has no effect. Set it on TCP only too, so that no call claims an effect that does not happen.
- Permissions: bind creates the file with mode 0777 & ~umask. A connect needs write permission on the file (unix(7)). The plan says nothing on this. See M8.

### 5. The tests

- No test_address file exists. The tests of `split_host_and_port` and `resolve` are in program/test/test_frame.cpp:74-85.
- ptest.sh is outside the tree (bench-logs/ptest.sh:4). The tree runs three_nodes through CMakeLists.txt:179. three_nodes.sh writes TCP addresses only (three_nodes.sh:21-25) and dials TCP (three_nodes.sh:14, 58).
- The address set in the plan has three members. The finite set has more: IPv4 literal, host name, [v6], `unix:` path, and the malformed `unix:` forms. See M11.
- The flag combinations are not covered. The plan has no function that takes (res, flags), so no test can reach each combination without a kernel. See M2.
- `listener_on` on a Unix path has its own finite set of states of the path. See M11.

### 6. Other points and a simpler form

- The bench keeps its connections open (driver/main.go:62, IdleConnTimeout one hour). A run does about 64 accepts. The bench cannot measure multishot accept. It stays in M, since the owner asked for it, but the result says that it is not measured.
- M changes the TCP path too. The plan measures M on Unix sockets only. If M enters the tree, TCP users get it unmeasured. See M13.
- The buffer memory must stay alive until the ring is gone. See M6.
- Multishot recv needs buffer select. So no simpler form of M without a buffer ring exists.
- An accept that fails at once, for example on EMFILE, re-arms at once (serve.cpp:673). That loops today too. M keeps it. It is not new and not part of this review.
- The plan ends with no list of its data. See M14.

## Requested changes

M1. Pending with multishot. An armed multishot recv adds one to `c.pending`. Only a CQE without IORING_CQE_F_MORE subtracts one. The plan states this in the M bullet.

M2. A pure function decides. One function takes `res` and `flags` of a receive CQE and returns a value of an enum. The members are: bytes and still armed (res > 0, F_BUFFER, F_MORE); bytes and ended (res > 0, F_BUFFER, no F_MORE); end of stream (res 0, no F_MORE); no buffer (-ENOBUFS, no F_MORE); an error (other res < 0, no F_MORE). A CQE with res > 0 and no F_BUFFER, or with F_MORE and res <= 0, is a fault of ours and throws std::logic_error. The function reads F_BUFFER and F_MORE only, so IORING_CQE_F_SOCK_NONEMPTY changes nothing. A second function does the same for an accept CQE: a new descriptor and still armed; a new descriptor and ended; an error and ended. `handle_a_receive` and the accept branch (serve.cpp:667-675) act on the result. A test checks every member, and one case sets F_SOCK_NONEMPTY.

M3. One place returns a buffer. Every CQE with F_BUFFER gives its buffer back to the ring after the bytes are copied. This holds on every path: a closing connection (serve.cpp:582), an outgoing peer (serve.cpp:582), and an id that is gone (serve.cpp:677). The return is one function, called from `handle_a_completion` once for each CQE. The plan names `io_uring_buf_ring_add` and `io_uring_buf_ring_advance` for it.

M4. ENOBUFS is not a close. A connection with ENOBUFS goes into a list. That list is re-armed after the loop over `l.ready` (serve.cpp:834-836). At that point every buffer of the turn is back. The words "after the buffers are back" change to this form.

M5. Two buffer rings and a static bound. One ring serves the peer connections. One ring serves the client connections. A client then cannot take the buffers of the peers. The count of the two rings together is not greater than the CQ entries, 2 x `ring_entries`. A `static_assert` checks it. The plan names the provisional count and size of each ring and the total memory. The measurement then decides the values.

M6. The lifetime of the buffer memory. `io_uring_free_buf_ring` runs before `io_uring_queue_exit` (serve.cpp:855). The memory of the buffers is freed after `io_uring_queue_exit`. The kernel or the engine can write into it until then. A failure of `io_uring_setup_buf_ring` at start ends `serve` with `exit_failed` and a message, as `io_uring_queue_init` does (serve.cpp:808-811).

M7. A receive CQE with -EINVAL in M is a fault of ours. It is not a client error. The process ends with a message that names the operation. A kernel without multishot recv then fails at once and does not close each connection in a loop.

M8. The Unix form, in full:
- `resolve` checks the prefix `unix:` before `split_host_and_port`.
- The path has 1 to 107 bytes. The bound is one named constant, `sizeof(sockaddr_un::sun_path) - 1`.
- A NUL anywhere in the path is malformed. No abstract name exists.
- The length is `offsetof(sockaddr_un, sun_path) + size + 1`.
- A relative path is allowed. It is relative to the working directory, as bind resolves it.
- TCP_NODELAY (serve.cpp:520) and SO_REUSEADDR (serve.cpp:716) are set on AF_INET and AF_INET6 only.
- The socket file takes 0777 & ~umask. konsensd does no chmod. The plan states this.

M9. The stale socket file. This is an error path, so the owner decides. Proposal: at start, if the path is a socket (lstat, S_ISSOCK) and a connect to it gives ECONNREFUSED, konsensd removes it and binds. A socket that accepts, or a file that is not a socket, makes the start fail, as now. A clean stop removes the file. Sources, from memory and not checked in this session: gRPC and systemd remove the path before bind; Go removes it at the close of its listener; Redis removes it before bind. Without a change of this kind, the unix run of three_nodes.sh fails at lines 136, 189, 196 and 207.

M10. The test names in the plan are the real ones. The `resolve` tests are in program/test/test_frame.cpp. ptest.sh is a bench script outside the tree. In the tree, three_nodes.sh takes a third argument for the address form. CMakeLists.txt:179 gets a second entry, `three_nodes_unix`. The unix run uses `curl --unix-socket` and `http://localhost` as the URL. Both runs together stay within five minutes. The plan names that time after it is measured.

M11. Every member of each finite set has a test:
- `resolve`: an IPv4 literal; a host name; [v6]; `unix:` with an absolute path; `unix:` with a relative path; `unix:7151` as a path; `unix:` with an empty path (malformed); a path of 107 bytes (accepted); a path of 108 bytes (malformed); a path with a leading NUL (malformed); a path with a NUL inside (malformed).
- `listener_on` on a Unix path: no file; a stale socket; a socket that accepts; a regular file; a directory that does not exist.
- The two functions of M2: every member, as M2 lists.

M12. One file first. Before the branch, one small file outside the tree shows, on the kernel and on the engine (podman with its default seccomp profile):
- `shutdown(SHUT_RDWR)` ends a multishot recv with res 0 and no F_MORE, on AF_UNIX and on TCP, also with queued bytes.
- An empty ring ends a multishot recv with -ENOBUFS and no F_MORE, and the buffer count stays right.
- Multishot accept works on an AF_UNIX listener.
M refuses to start on the engine only if this file shows a difference.

M13. The bench table. Arms R1, U and M, each on TCP and on Unix sockets. R1 on Unix sockets is a cell that says "does not parse". The driver dials through `http.Transport.DialContext` with `net.Dialer.DialContext(ctx, "unix", path)`. The URL host is a fixed name. cluster.sh prints the target as `unix:<path>`. The socket path under arms.sh:10 (`/z/b/arms/<arm>-c64/...`) stays under 107 bytes; the plan says so. The result says that multishot accept is not measured.

M14. The plan ends with a list of its data on the disk, as the rules ask. Each item is marked: in the state file, needed by open work, or made again by a build command.

## Review of section 22 (multishot as the default, arm C as cbor::packed)

Round 1: not approved, 13 findings. The wrong assumption about the
receive paths (finding 1), the engine (2), the one definition rule (3),
the wire header in an unnamed namespace (4), the place of the
description (5), the tests (6), the builds (8), the base (9), the order
(10), the tag registry (11) and the verified decode (12) are taken into
section 22.

Round 2: not approved, 3 findings. A: the descriptions stand in the
header that defines each struct, so the one definition rule holds for
indexed_result. B: the result of the place of the description is a
file under bench/results/. C: the verified read of the form inside
compares the description; step 4 tests a description that differs.
These are small corrections and are taken without a third round.

## Review of section 23 (arm W, the base as a write set)

Round 1: not approved, 12 findings. In short: the cause was not split by
caller (now in 2026-10-30-152000Z-konsensd-storage-calls-per-write.txt:
at 64 in flight run_the_tasks writes 2.92 results and
write_the_done_results 1.72 per write on the leader); one write set
cannot give the views below index that run_again, check_the_done_records
and apply_the_done_records take, so each write needs its log index and a
read takes an upper index; a removal by key must keep a later write of
the same key; the rebuild needs each event that replaces a record; the
write set must copy write_the_result for each kind, an erased session
included; the view type of the state machine changes; the SAVEPOINT
has no purpose on these paths; the model holds the base per index
(model/tla/SequencePaxos.tla:629 BaseWith, :2821 NodeBase) and needs no
change; the memory bound is the bound of own_ and of the done records
above applied_idx; the test needs an oracle that replays the records in
log order; the form is new and goes to the owner first.
