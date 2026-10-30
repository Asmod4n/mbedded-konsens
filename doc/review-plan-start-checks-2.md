# Review 2 of the plan of the start checks

Review of plan and model at f02a0fc by a second agent, 2026-10-24.

## Findings

1. **Blocking (owner question).** A missing promise is not found when the log is empty, and that breaks safety with the checks on.
   - Code: `with` recovers only when `*p != ballot{}` (sequence_paxos.hpp:268-271). A store whose only meta value is the promise reads as new.
   - Model: DamagedStates restricts missing_promise to `Len(log[i]) > 0` (SequencePaxos.tla, DamagedStates).
   - Copy B trace: node 3 promises ballot 12 with an empty log. It loses the promise and then promises ballot 11. Leader 1 decides v1 at 11 while leader 2 is in Accept at 12 without v1. S2b breaks.
   - Matrix row 2.8 only requires the non-empty-log case, so the plan inherits the gap.
   - Proposal: put the case in plan 3.6 and in the model. Ask the owner whether the hidden identity of rule R ("identity present, promise absent") becomes a fourth check in `with`.

2. **Should fix.** The model of missing_promise does not restart the way the code does.
   - `Restart` always goes to phase "Recover" and recovers. Its own comment says "The promise is always set". The code instead starts in (follower, none) with no recovery (sequence_paxos.hpp:268, 1183; matrix 2.8 says the same).
   - So "With the recovery and without the checks, no simulation found a violation" (plan 3.5; runs-1, runs-3) is not about the code for missing_promise.
   - Proposal: restart a node whose read promise is `DefaultBallot` in phase "None", with no recovery (as in copy B). Rerun the nocheck missing_promise config and rewrite the claim.

3. **Should fix.** The missing_entry model is not what `read_entries` does.
   - `read_entries` works by key (persistent_storage.cpp:449-488), and `to` is the count.
   - Take a log of n keys that loses key k < n-1. A range read up to the count returns n-2 entries, and key n-1 is never read. A single-index read of position p ≠ k returns the true entry, not a shifted one.
   - Round 3's S2 trace (node reads `<<v1>>` after losing key 0) is not reproduced by the code. `get_entries(0,1)` returns empty there.
   - Plan section 1 ("the entries after it move down by one position") and the model comment ("as read_entries does now") are too strong. With the checks this does not matter, because every k < Len is found. The nocheck traces only show the need.
   - Proposal: correct the text. State that LogWithout is an approximation used only for the nocheck mutation.

4. **Should fix.** Nothing in the model shows that an undamaged state passes the checks.
   - Crash, Rollback, Resume and power loss restart with no ChecksFind. Of the three facts, only "decided ≤ log length" is a model invariant (AcceptedIdxIsLogLength). "Non-empty log ⇒ accepted round set" and "accepted ≤ promise" rest on code reading.
   - Copy A: `BuildForServer` and `ForceSingle` give a new node a log with `n_accepted = 0` and `promise = 0`. Example: MC_d2_crash state 17, node 6 has `<<v2>>`, decided 1, accepted 0. MC_c_crash hits the same through BuildForServer.
   - No code path does this yet. The code has no reconfiguration (git grep found none in src/include). The future build path would then be refused by check 2.4.1.
   - Proposal: add NoFalseRefusal to every config (it held in MC_s, MC_p and MC_p_rollback). Either set n_accepted in BuildForServer/ForceSingle, or record the constraint for the future build path in the plan.

5. **Should fix.** The paragraph "The damage of a running node between two reads is the same action" (3.2) is wrong.
   - The meta values are cached in memory (`with` :262-266). Damage to them at run time is read only at the next start.
   - The read check finds a missing key only when a read covers it.
   - A missing last key at run time is never found. `write_atomically` takes the count (`read_log_len`, :449ff), not the cached `accepted_idx_`. The next append succeeds at the old last index, and the stored log silently differs from the node's in-memory log.
   - Proposal: drop the claim, or model run-time damage on its own. Name the count/cache mismatch as an open item for the owner (for example, `write_atomically` compares the count with the caller's accepted index).

6. **Should fix.** Plan 3.5 claims more than the runs support, and some claims contradict the plan.
   - "With the checks, no violation" should read "no simulation found a violation (2 seeds × 15 min)".
   - "A missing last entry passes the checks" contradicts 3.2, the model (`decided_idx > Len(d.log)`) and the round 2 trace. It passes only when decided < length.
   - The StorageFault-steps column counts all kinds. `-coverage 14` was used, but no per-kind or per-branch (refused vs. restarted) counts were recorded. So nothing shows how often the undetected missing-last-entry branch ran in MC_s.
   - Proposal: report the coverage of each IF branch per kind. Or add a with-checks run restricted to missing_entry.

7. **Should fix.** The run set is thinner than the plan says.
   - All MC_s* configs have MaxCrash 0, MaxLoss 0, MaxTimeout 0, MaxDuplicate 0, NextConfigNodes {} and MaxHalt 0. In MC_c_crash, MaxCrash is 1 and reconfiguration is on. So S6 and BuildOnlyFromRunning are vacuous here, and no run combines the fault with a crash of another node.
   - No run combines the checks with PowerLoss = TRUE.
   - Proposal: state the real bounds in 3.4. Add one run with MaxCrash 1 and one with PowerLoss TRUE, each with the checks and NoFalseRefusal.

8. **Should fix (plan text).**
   - 3.3 still expects every kind except missing_decided to break S2/S2b. With Recovery TRUE, missing_accepted and missing_promise did not. So the model gives no evidence that check 2.4.1 is needed when recovery runs. Say that check 2.4.1 is defence in depth.
   - 3.2: "With missing_decided, and without StartChecks, the node restarts" should read "every damage that the checks do not find restarts the node through Restart".
   - 3.4 names MC_s_nocheck.cfg, which does not exist (there are per-kind configs).
   - 3.4 says "One TLC at a time", but runs-1/-2 say "Four runs at a time on 4 processors". With ParallelGC threads that can exceed the processors (rule "The load stays within the processors").

9. **Nit.** The Detected branch freezes the node with its volatile state: role, phase, leader_state and lease_remaining stay as they were (same as Halt). The plan says the node restarts.
   - Question 3 answered: a frozen node is no fault for S1/S2. Its durable state is a valid earlier state, decided_history only grows (S2Grows), and the node takes no further step. So it neither hides nor invents a violation.
   - S2b still quantifies over a frozen Leader/Accept node.
   - Proposal: reset the volatile state as a crash does (Follower, lease 0), or state the equivalence with Halt.

10. **Nit.** `StorageFault` requires `i \in Nodes`, so a node of the next configuration never gets a storage fault. Say so, or use Servers.

11. **Checked, no fault.**
   - UNCHANGED is complete in both branches of StorageFault: the Detected branch lists every vars component. The else branch matches Crash; Restart and GhostUpdate cover the rest.
   - Every other action that lists env members by name got `storage_faults, refused`. The others use `UNCHANGED env`.
   - With MaxStorageFault = 0, StorageFault is disabled and `refused = {}`, so no existing action changes its behaviour.
   - The Resume guard keeps a refused node out. No other action returns a node of Nodes to running: Crash, Wipe and Rollback need running, and BuildForServer/ForceSingle only target NextConfigNodes.
   - ChecksFind/Detected match plan section 2 exactly at the start: k < Len ⇔ the last key is not count−1 or the first key is not 0; a missing last key keeps continuity; decided > count finds it when decided = n.
   - "A missing meta value reads as 0" matches :327-329 and :523-525.
   - No forbidden words in the plan, the results or the model diff.

## Verdict
Approve with changes. Two things must be settled before the code is built:
- Finding 1: the owner decides how a missing promise on an empty log is found, or records it as not covered.
- Findings 2 and 4: correct the missing_promise restart, add NoFalseRefusal, then rerun.

The other findings are text and coverage fixes.

Scratch files (copies only, nothing in the repository):
- scratchpad/review-sc/A/out/d2.log
- scratchpad/review-sc/A/out/cc.log
- scratchpad/review-sc/B/out/chk.log
- scratchpad/review-sc/B/SequencePaxos.tla

The LMDB form of this review is in the git history before branch remove-lmdb.