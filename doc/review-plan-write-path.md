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
