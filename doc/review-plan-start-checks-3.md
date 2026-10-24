# Review 3 of the plan of the start checks

Review of plan and model at 6690506 by a second agent, 2026-10-24.

## Findings

1. **Blocking: a key lost at run time at the end of a read range is found by no check.**
   - Item 2.2 (plan:94) checks only the keys that the cursor finds below `to`. The loop stops at the first key at or above `to`, and it stops at MDB_NOTFOUND, with no check (persistent_storage.cpp:466-475).
   - So a read of [from, to) whose key to−1 is missing returns one entry less, with no error. This holds when a later key exists, and also when the missing key is the last key of the log.
   - Every log read in sequence_paxos ends at `accepted_idx_`: `create_log_sync` (:612), `handle_recovery` (:891), `handle_get_state` (:983), `log_or_buffered_proposals_contain`. `read_decided_suffix` (:501) ends at the decided index.
   - Example that breaks S2 with one fault:
     - Leader L of 3 nodes decides entry k with {L, F1}. Then L loses its last key k while it runs.
     - F2 sends Prepare. `handle_prepare` writes no append, so item 2.5 does not run. `create_log_sync` sends a suffix without k, but the promise carries `accepted_idx_` = k+1.
     - F2 builds its log from the size of the suffix (:760), so its log has no k. It then overwrites k on F1 through AcceptSync.
   - The primary path of the recovery has the same fault: `complete_recovery(0, l->log, …)` (:922) does not compare `l->log.size()` with `l->accepted_idx`.
   - Proposal:
     - The continuity rule also covers the key at which the cursor stops: that key must equal `from + entries.size()`.
     - Every read up to the accepted index or the decided index in sequence_paxos compares the size of the result with `to − from`. When they differ, the node stops voting with `missing_entry`. This is the read form of item 2.5. A correct state always gives the full range, because the waiting ops are written first.
     - The recovery receiver checks that the size of `l->log` equals `l->accepted_idx`.
     - The owner decides this, because it is a question about safety.

2. **Blocking (design incomplete): item 2.5 says "the node then stops voting", but the code throws.**
   - `write_atomically` is called only from `write_waiting_ops_or_throw` (:577-589). That function throws `write_error` on any error and leaves `write_unfinished_` true.
   - It is reached from 8 places: :295, :523, :611, :766, :883, :928, :980, :1032, :1071. Most of them return void or a reference.
   - Proposal: the plan names the change. `missing_entry` from `write_atomically` goes to `stop_voting`, and every call site returns the error. Only other errors keep the throw. Add a test: "an append after a lost last key makes the node stop voting", at the level of sequence_paxos.

3. **Should fix: item 2.5 compares at the wrong point when one batch holds two appends.**
   - `write_atomically` reads `read_log_len` once (persistent_storage.cpp:436), and each `apply` returns the new length (:438-441).
   - `waiting_ops_` holds many ops between writes. Examples: `accept_entries_leader` (:684), and `handle_majority_promises` with `append_on_prefix` followed by `append_entries` (:764).
   - For the second op, the index that the caller holds is `min(sync_idx, accepted_idx_) + suffix.size()`, not the count at the start of the transaction. A literal reading of plan:113-118 ("the count that `read_log_len` gives") refuses that correct state.
   - Proposal:
     - Each op carries the length of the log before that op.
     - `write_atomically` compares it with the length that the previous op returned. It never compares it with `from_idx`.
     - With that rule, `append_on_prefix` with `from_idx` above the count stays valid, and the `std::min` at :282 stays. The existing test test_persistent_storage.cpp:128 (`append_on_prefix{5, two}` on a log of 1) then passes, with the index 1 in the op.
     - No caller in sequence_paxos has a stored count that differs from its index. I read :684, :764, :810, :836 and :1069.
     - Also say that `counting_storage` (bench:159-210) and the other storages ignore the field.

4. **Blocking (owner question): the seed field of item 2.6 is not safe on its own.**
   - A field in the config stays true after the first start. A seed node that later reads `ballot{}` would start with no recovery. Causes: a lost promise, a replaced disk, or a wipe.
   - That start is the mutation StartAsNewNode, and MC_s_promise_asnew breaks S2b with it (runs-7). So a node that wrongly believes it is a seed is unsafe.
   - Proposal: a seed starts with no recovery only when its store is new (no meta key at all, no hidden identity) and the config says seed. The plan also states that a seed whose disk is replaced must lose the seed field first. The owner decides.

5. **Should fix: "fewer than 2 seed nodes" (owner, 12:05) is not enough to start a cluster of more than 3 nodes.**
   - A recovering node finishes in one of two ways:
     - It finds a primary and a majority of answers from nodes that do not recover (:921, :1053).
     - Or it gets answers from all peers (:924).
   - With 2 seeds of 5 nodes, the seeds are not a majority. They cannot elect a leader, so no primary exists. The other nodes then wait for all peers.
   - This is liveness, not safety. Proposal: ask the owner to make the minimum a majority, f+1.
   - The model has all nodes as seeds (Init, SequencePaxos.tla:554-609). A start with some nodes that are not seeds is not modelled. Add it to 3.6.

6. **Should fix: plan 3.2 run-time argument (plan:170-182) misses these cases.**
   - Finding 1.
   - The read paths that come before the next append.
   - The primary log in the recovery.
   - Proposal: rewrite the list after findings 1 and 2. State that the model has no run-time damage, and that the code checks of items 2.2, 2.5 and the read form of 2.5 carry the argument alone.

7. **Should fix: the plan text is stale against model 6276400 and the decisions of the owner.**
   - plan:36 and plan:40 still say "Owner question 1/2". Both are decided (12:03, 12:04).
   - plan:166-168 says that a node with a `DefaultBallot` promise restarts "as a new node in phase None". Since 6276400 that happens only under the mutation StartAsNewNode (SequencePaxos.tla:1471-1473).
   - Review 2 finding 8 is still open: 3.3 (plan:187) does not say that checks 2.4.1 and 2.4.2 are defence in depth when the recovery runs.
   - Section 4 does not list the change to the harnesses for item 2.6. With 2.6, every harness start with an empty store now recovers unless the start is marked as seed: simulation.hpp, node_cluster.hpp, test_node.cpp and counter_stress.

8. **Should fix: section 3.5 claims more than the runs show.**
   - plan:232: "breaks S5 and S2 without the checks" rests on `LogWithout`, which review 2 found to be only an approximation of the code. Say "in the model".
   - plan:235, a missing promise without the checks and with the recovery: the cited runs (runs-1 and runs-3) are at da39406, where missing_promise required a non-empty log. No recorded run at 6276400 covers it.
     - I ran MC_s_nocheck_missing_promise on a copy of 6276400 (seed 1, 10 min, about 4.5 M states). It found no violation.
     - Record such a run in a results file.
   - plan:242: MC_s_powerloss has MaxLog 1. So `k < Len` never holds, and the continuity check never ran there. The line "All runs: … MaxLog 2 … MaxTimeout 0" (plan:196) is wrong for that config, which has MaxLog 1, MaxCrash 3 and MaxTimeout 1. In MC_s_crash, the crash can also hit the damaged node itself.

9. **Nit: item 2.6 versus the model.** I added a mutation "FullCopy" to a copy of the model. It sets the damaged node to `DurableOf(src)` of a running peer before the restart with the recovery.
   - The branch ran 36855 times in a 3-min coverage run.
   - A run of 10 min (seed 1, about 4.3 M states) found no violation.
   - So "full copy, then recovery" behaves as the model of 6276400 in this simulation. The model of 6276400 alone is faithful only if the copy cannot be chosen against the answers of the recovery. The tie-break prefers the node itself (:944).
   - Proposal: add this form to the model as the form of 2.6, or say why the copy cannot change the source that the recovery chooses.
   - A smaller point: a node with an empty log, an accepted round and no promise is refused by check 2.4.2. It does not get the full copy. Say so.

10. **Checked, no fault.**
    - Review 2 findings 1, 2, 3, 4, 7, 9 and 10 are fixed. The places are SequencePaxos.tla:1420-1422, 1441-1474 and 2241-2242, and plan 3.2, 3.4, 3.6 and 1.
    - Finding 5 is replaced by a new argument, which finding 6 above reopens.
    - Finding 6: the wording is fixed, and refused and restarted counts are given for each config.
    - NoFalseRefusal is in every sim config with the checks.
    - The forbidden words do not stand in the diff f02a0fc..6690506.

## Verdict
Approve with changes. The owner must decide findings 1, 2 and 4 before code is written. Findings 3 and 5 to 8 are design text and runs to add.

## Not checked
- Whether the `-Wextra` flags turn a missing initializer of the new op field into an error.
- The read paths of the client in counter_stress.cpp.

## Scratch files
- scratchpad/review-sc3/A/out.log
- scratchpad/review-sc3/B/out.log
- scratchpad/review-sc3/B/cov.log
- scratchpad/review-sc3/B/SequencePaxos.tla (the FullCopy variant)