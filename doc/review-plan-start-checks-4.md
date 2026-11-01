# Review 4 of the plan for the start checks

Plan and model at 7e513b2, code at 4c57d44. Reviewer: a second agent, 2026-10-24.

## Task restated
- Goal: review the plan doc/plan-start-checks.md and the model SequencePaxos.tla at 7e513b2 before any code is written. I check the plan against review 3, against the owner decisions of 2026-10-24 (14:01, 14:03, 14:06) and against the code at 4c57d44.
- Place: /home/claude/mbedded-konsens. I read every file through `git show` and `git diff`.
- Assumptions: the line numbers in the plan refer to 4c57d44. The decided points stay closed.
- What I do not do: I change no file in the repository, I build nothing, and I reopen no decision. My TLC runs used copies under a scratch directory outside the tree.

## Findings

1. **Blocking: item 2.7 does not name `get_last_entry_crc`.**
   - The node calls it at sequence_paxos.hpp:884, :929 and :981. It reads the last key and does not compare it with `accepted_idx_ - 1` (persistent_storage.cpp:535-566).
   - Case: the last key is lost at run time. The node then sends `accepted_idx_` = n with the CRC of entry n-2 (:897-899, :993-995). A recovery answer from a node that is not the primary reads no log (:890), so no item-2.7 read happens before the send. Plan 3.2 (plan:216-217) says that "a recovery answer reads before it sends". That is true only for the primary.
   - Effect: a healthy recovering node compares this CRC in `find_hardware_fault` (:962-976) with a healthy peer at the same (round, index). It then stops voting with `hardware_fault`, which names the wrong node. Safety holds, but the error does not go to whoever can repair it.
   - Proposal: add it to item 2.7. Either the storage concept (storage.hpp:128) returns the index with the CRC, and the node compares that index with `accepted_idx_ - 1` (no entry when `accepted_idx_` is 0). Or the node reads `get_entries(accepted_idx_-1, accepted_idx_)` and computes the CRC. The owner picks one, because the first form changes the concept and every storage.

2. **Should fix: item 2.7 does not name the read at :1033, and :1037 accepts a short result.**
   - The read is `get_entries(ns.accepted_idx-1, ns.accepted_idx)`. Its `to` is `accepted_idx_` or `decided_idx_` (:956, :1025). So in a correct state it always returns one entry.
   - When the result is empty, `crc` stays 0 and the node stops with a `hardware_fault` that names itself (:1037-1042).
   - The plan lists :1033 as a caller at plan:100 but not in item 2.7.
   - Proposal: name :1033 in item 2.7, and remove the empty-result branch.

3. **Should fix: item 2.7 says "the node stops voting", but `read_decided_suffix` is `const`.**
   - `read_decided_suffix` (:498-503) cannot call `stop_voting`. Its error goes to `node::apply_decided_entries` (node.hpp:394-397) and to the public `node::read_decided_suffix` (node.hpp:255-258). Neither of them stops the vote.
   - `log_or_buffered_proposals_contain` (:518-529) returns the error to node.hpp:238 and does not stop the vote either.
   - Proposal: name the place that stops the vote for each of these reads.

4. **Should fix: the rule for the key where the cursor stops (plan:155-156) rejects reads that are complete.**
   - The plan says "that key must equal `from + entries.size()`". Case: a read [from, to) is complete, but the next key after it is missing, so the cursor stops at a key above `to`. The read then fails, and the index it reports lies outside the range.
   - Reads that end below the count meet this case: `read_decided_suffix`, :1033, and the bench case get_entries.
   - Proposal: "When the cursor stops at a key at or above `to`, `from + entries.size()` must equal `to`."

5. **Should fix: item 2.8 does not say what happens to the short answer.**
   - `find_recovery_primary` (:1050-1062) finds the same primary answer on every later answer. While a primary answer is kept, the path through all peers is closed (:924-926). So the node can wait without end.
   - "Its accepted index" is ambiguous: it is the accepted index of the primary (`l->accepted_idx`).
   - Proposal: keep the answer with primary = false and no log, as :1011-1019 does for a changed state. Write "the accepted index of the primary". Extend the test at plan:353-354 so that the node later completes the recovery.

6. **Should fix: item 2.5 does not say what the next start does (plan:132-133).**
   - Case: a lost last key, and the decided index is at the old length. The next start finds decided > length, and check 2.4.3 refuses the node. The recovery does not run.
   - `handle_decide` (:853-856) can write `set_decided_index{n}` after the loss, because no append op is in that batch. That makes this case likely.
   - Proposal: write "the start checks refuse the store, or the recovery repairs it when the decided index is below the log length". Ask the owner whether the length check of item 2.5 also covers `set_decided_index` against the running length.

7. **Should fix: section 7 is stale against the decision of 14:06.**
   - Section 7 says that `missing_entry` is a value. Item 2.5 now throws it as `write_error`, which derives from `std::system_error` (storage.hpp:110). That agrees with the rule "An error goes to whoever can repair it".
   - Item 2.7 stops the vote at a read. That agrees with the rule too, because sequence_paxos finds that fault, not the file system.
   - Proposal: section 7 states both forms and this reason.
   - Open question for the owner, which does not reopen 14:06: do the read errors that the storage reports (`stop_voting` at :713, :719, :886, :893 and others) stay values?

8. **Should fix: the code that this plan builds is not the model at HEAD.**
   - Item 2.6 says "until the full copy exists, the recovery copies the log as it does now" (plan:144-146). That is model 6276400 (runs-7). Model 80d8829 has only the full copy, and no constant runs the form without the copy.
   - plan:310-311 ("The model takes the copy from the recovery") is now wrong.
   - Proposal: add a constant or mutation for the form without the copy, or cite 6276400 and runs-7 as the model of the code from this plan. Fix 3.6.

9. **Should fix: the copy branch of the model (question 3).**
   - Source rule: `running \ {i}`, `Votes(src)`, `~recovering[src]` (SequencePaxos.tla:1476-1478). A refused node is not in `running`, so it is never a source.
   - The comment at :1444-1445 says "the source rule of BuildForServer". BuildForServer (:1831-1841) also requires `~read_only`, `~deactivated` and `IsReconfigured`, and it copies only the decided prefix. The copy branch takes the whole durable state. Proposal: correct the comment.
   - The copy branch exists only when such a source exists. Otherwise the fault is not enabled, so "no promise and no source" is never explored. Proposal: state this in 3.6.
   - **Can a damaged source spread its damage? In the model, no:**
     - The model has no damage at run time (plan:207, :222).
     - A node with a fault at the start is recovering, so it is no source until it has recovered.
     - I also ran MC_s with MaxStorageFault = 2, seed 3, for 10 min: about 4.5 M states, no violation.
   - **In the code, yes:**
     - A source with a lost last key keeps voting until its next read or append.
     - `persistent_storage::copy` (persistent_storage.cpp:569-575) would copy a store that has n-1 entries at round r.
     - The receiver passes the start checks when its decided index is below n-1. Then only its recovery protects it.
   - Proposal: the design of the full copy reads the log of the source through a read that item 2.7 covers. The receiver checks the length as item 2.8 does. 3.6 states that a damaged source is two faults and is not in the model.
   - Not checked: whether a copy of the store would also copy the hidden identity. The hidden identity does not exist at 4c57d44 (a grep in include and src found nothing).

10. **Should fix: item 2.6 depends on the hidden identity, which no code has yet.**
    - Proposal: state the order of the work (rule R first), or a provisional form "no meta key" until the hidden identity exists.

11. **Should fix: plan 3.2 (plan:214-219) is not accurate.**
    - `handle_prepare` reads only when it sends a sync (:710-722). A promise without a sync carries `accepted_idx_` = n from memory, and the store is not read.
    - Safety still holds: the next AcceptSync or AcceptDecide carries the length n, and item 2.5 fails that write. A recovery answer that is not the primary is finding 1.
    - Proposal: rewrite these lines with these two cases.

12. **Should fix: section 4 has no test of a correct batch with two appends.**
    - Review 3 finding 3 (`handle_majority_promises`: `append_on_prefix` then `append_entries`, :764) has no positive test.
    - Proposal: add "a batch of append_on_prefix and append_entries on a whole log commits". Also add tests for the reads of findings 1 to 3.

13. **Nit: the results file of rounds 9 and 10.**
    - The "Copy" column counts the successor states of the copy branch, one for each source, not the faults taken. In my 200-s coverage run of MC_s, the `\E src` line had 35050, `Restart(DurableOf(src))` had 70100 and the plain `Restart` (:1480) had 7112.
    - The file has no column for the plain restart, so the coverage of the missing last entry in rounds 9 and 10 is not visible.
    - Proposal: add a "Restarted" column, and say what the count counts.

14. **Nit: text.**
    - plan:5-6 still says "The owner has not decided these proposals yet".
    - plan:13 and section 8 list only review 1.
    - The bullet at plan:297 says "a crash of another node", but in MC_s_crash the crash can also hit the damaged node (plan:270).
    - Not checked: whether NoFalseRefusal was in the configs of rounds 1 to 3 (da39406).

## Review 3 findings, status
- 1: in part. Items 2.7 and 2.8 exist, but the list of reads is incomplete (findings 1 to 3 above).
- 2: replaced by the owner decision of 14:06. The test is at plan:355.
- 3: fixed, plan:113-128 (the min is at :289).
- 4: fixed, plan:138-141 (owner, 14:01).
- 5: fixed, plan:141-143 and plan:271-272.
- 6: rewritten at plan:207-227, with the errors of finding 11.
- 7: fixed, plan:36, :40, :199-204, :233-236 and :328-331.
- 8: fixed, plan:287 and :265-270, and runs-9 has MC_s_nocheck_missing_promise and MC_s_powerloss_log2.
- 9: fixed in model 80d8829 and plan:204-205, with the limits of findings 8 and 9.

## Question 4: do the results support 3.5?
Yes, for the model:
- Runs 2 and 3 show the violations without the checks or without the recovery.
- Runs 4, 7 and 9 show no violation with the checks and the recovery.
- The kinds other than a lost promise were last run at 09e83c6 (runs-4). Model 80d8829 changes only the branch for a lost promise, so those runs still hold.

## Forbidden words
None in `git diff 6690506 7e513b2`.

## My runs
All in a scratch directory outside the tree:
- A/out.log: coverage of MC_s_promise, 200 s.
- C/out.log: coverage of MC_s, 200 s.
- B/out.log: MaxStorageFault 2, seed 3, 10 min, no violation.

## Verdict
Approve with changes. Finding 1 needs a decision of the owner before code is written. Findings 2 to 12 are plan text, tests and model notes to add.
