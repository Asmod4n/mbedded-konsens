# Review: plan to read the open log once in each round

Plan: doc/plan-one-read-per-round.md at b3d7804.
Rules: RULES.md and .claude/RULES.md of Claude-state.
State: mbedded-konsens.md, owner decisions of 2026-10-24 19:29 and
14:49.

Verdict: approved after fixes. Findings 1 to 5, 10 and 11 are
blocking.

## Order of a step

I read `end_the_step` (node.hpp:378-405). On the leader in phase
accept, one step runs in this order:

1. R1: `check_the_done_records` from `propose_before_the_step`
   (node.hpp:1344). It reads [min(verified, applied), log_len)
   (node.hpp:1200-1201).
2. A pass of a record calls `pass_the_checked_done_record`
   (node.hpp:1238). That calls `find_the_verified_idx` (read of
   [index, log_len), sequence_paxos.hpp:1059-1069) and
   `decide_if_chosen`, which can call `set_decided_index`.
3. `append_the_done_records` calls `accept_entries_leader`
   (node.hpp:1460). This is an INSERT into the log
   (sequence_paxos.hpp:1511). Then `extend_the_verified_idx` reads
   [verified, log_len) again (sequence_paxos.hpp:1513).
4. R2: `check_the_done_records` again (node.hpp:1466), on the new
   log_len.
5. `seq_paxos_.end_step` (sequence_paxos.hpp:413-438). Its accept
   gets an empty buffer, because `assign_the_tasks` took the buffer
   (node.hpp:1404).
6. R3: the apply (node.hpp:1486-1543). It reads [applied, decided)
   and sets applied to decided (node.hpp:1540).
7. R4: `execute_verify` (node.hpp:1879-1928). It reads
   [applied, log_len) with the new applied.

A follower runs 1, 2, 5, 6 and 7. A follower writes the log only in
`handle` (sequence_paxos.hpp:1604, :1677, :1723, :1953), before
`end_the_step`.

## Findings

1. Blocking. The plan says that a round reads once, "after the last
   write of the log in that round" (plan section 2). R1 comes before
   the INSERT of step 3 (node.hpp:1344 before node.hpp:1460). So a
   leader step that appends done records reads at least twice: R1,
   then R2 after the INSERT. A follower step reads once. The plan must
   state this. The measurement must count the leader and a follower
   apart.

2. Blocking. The plan names three readers. A fourth reader of the same
   kind is `find_the_verified_idx` (sequence_paxos.hpp:1059-1069),
   through `extend_the_verified_idx` (sequence_paxos.hpp:1092-1103).
   It runs in steps 2 and 3 above (sequence_paxos.hpp:1513, :1805)
   and in `handle` (:1613, :1679, :1724). Its range [verified,
   log_len) is inside the range of the plan. Two more readers of the
   open log are `holds_the_request` (node.hpp:1164-1182) and
   `log_or_buffered_proposals_contain` (sequence_paxos.hpp:980-997).
   The 31.4 rows for each write can come from any of these. The plan
   must list them and say which of them take the one read.

3. Blocking. `set_decided_index` writes the log. It runs
   `UPDATE log SET record` for each newly decided entry
   (persistent_storage.cpp:1559-1562, :762-795). It runs inside R1
   and R2, through `pass_the_checked_done_record` and
   `decide_if_chosen` (sequence_paxos.hpp:1803-1818). The plan does
   not say whether this write drops the read. If it drops the read,
   the apply reads again after each decide on the leader. If it does
   not, the plan must state the fact that makes this correct: the
   UPDATE changes the state byte only, and `entry` holds no state
   (storage.hpp:39-52). So the decoded entries stay equal. I propose
   the second form, with that fact as the reason.

4. Blocking. The plan does not say where the read lives. If it is a
   member that a write drops, a span that a caller holds points to
   freed memory. `check_the_done_records` holds `tail` and `at`
   across `pass_the_checked_done_record` (node.hpp:1201-1243).
   `execute_verify` holds `done_log b` to its end (node.hpp:1895-1927).
   The rule "Wrong use fails loudly and safely" asks that no case reads
   memory that is not alive. The plan must give a form in which a drop
   cannot happen while a span lives. The validity check must be one
   function in one place ("A check for safety exists once"), for
   example a count of log writes of the transaction that each use
   compares. A list of call sites that each drop the read is not one
   check.

5. Blocking. The plan uses "round" and does not define it. One
   transaction can hold several ended steps
   (sequence_paxos.hpp:472-475), and `handle` writes the log between
   those steps. So "the end of the round" must be the end of the step:
   `commit_phase_one` (sequence_paxos.hpp:449-488). The read must also
   drop in `roll_back_the_transaction`, in `stop_voting`
   (sequence_paxos.hpp:1254-1262), in `commit_the_open_transaction`
   (:509-523) and in `roll_back_the_step` (node.hpp:849-856). The plan
   must use the word "step" and list these points.

6. Not blocking. The answer to the question of the ranges is yes.
   Without a log write, applied and verified only grow. Verified goes
   down only at `append_on_prefix` (sequence_paxos.hpp:1613, :1679),
   which is a log write. The apply sets applied to decided
   (node.hpp:1540) before R4. So R4 reads [decided, log_len), a part
   of the read. Decided is at most log_len (persistent_storage.cpp:
   1555-1557). So R1 to R4 and `find_the_verified_idx` each take a
   part of one read of [min(verified, applied), log_len), when the
   start is taken at the time of the read. When the node is not in
   phase accept, R1 and R2 do not run (node.hpp:1187). Then the apply
   is the first user. The plan must say that the first user makes the
   read, with that start.

7. Not blocking. The answer to the owner rule is yes, with findings 4
   and 5. A span of the read is not a copy. The read ends with the
   step, so it ends inside its transaction.

8. Not blocking. Part B: the citations are right. `set_entry_states`
   stands at persistent_storage.cpp:762-795. The two entry-state
   checks stand at sequence_paxos.hpp:1175-1192. Other users of
   `entry_state` that part B touches:
   - storage.hpp:148 (the type) and :211 (the concept requires
     `get_entry_state`);
   - persistent_storage.hpp:41-42 and :96-97;
   - persistent_storage.cpp:60, :202, :267-313, :676-683, :704,
     :1322-1328, :1420-1428;
   - test/test_sequence_paxos.cpp:564-583, with checks at :757-807
     and :904;
   - test/test_persistent_storage.cpp:103-108, with checks at
     :674-722;
   - the forwarding wrappers test/node_cluster.hpp:75,
     test/test_node.cpp:355 and bench/konsens_bench.cpp:350.
   Section 6 of the plan lacks test_sequence_paxos. I did not check
   the claims about Raft, TigerBeetle and FoundationDB.

9. Not blocking. Part C: the citation node.hpp:1545-1558 is right. Its
   one caller stands at node.hpp:1529.

10. Blocking. The measurement uses g++ 16 Release only. The builds of
    the tree are g++ 15, g++ 16 and clang 22 (state, after the GCC 15
    floor, owner card 2026-10-28). The rule "Every speed question is
    asked of every build" asks one binary for each build and one table
    with a column for each. The state allows Release only (owner,
    2026-10-24). It does not allow one compiler only.

11. Blocking. The measurement section does not state these points of
    the rules:
    - the A/A noise floor is measured first, and a difference under it
      is not a difference;
    - the arms run alternately, and medians are compared;
    - each run writes one result file with a time stamp, with each
      repetition, the median, the spread and the full context of the
      run;
    - bench/sqlcount.cpp stands in the tree, so its change is
      committed before the run, and the result file names that commit;
    - arms.sh lies outside the tree
      (scratchpad/bench-logs/arms.sh), so the result file holds its
      text or a digest of it.
    The A/A arm and the separate binaries are right.

12. Not blocking. The result file 2026-10-30-203500Z says that the
    order of the pids to the nodes is not checked. The plan says "on
    the leader". The callgrind run and the new count must find the
    leader from the node, not from the most range reads.

13. Not blocking. The counted binary is from 35a6b5d. The plan names
    base f2f2fc5. `git diff --stat 35a6b5d f2f2fc5 -- include src
    program` is empty. The plan must say this.

14. Not blocking. The data list does not mark each item as recorded in
    the state file, needed by open work, or made again by a build
    command. The change to bench/sqlcount.cpp and the result files are
    not in the list.

15. Not blocking. callgrind gives instruction counts. The rule "A gain
    is measured, never estimated" says that these miss the time a
    processor waits. The plan must not give a gain in speed from them.
