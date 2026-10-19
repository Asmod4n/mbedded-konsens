# Review: plan-decide-on-resend.md

Reviewed on 2026-10-19 against `model/tla/SequencePaxos.tla`,
`include/konsens/sequence_paxos.hpp`, `include/konsens/message.hpp`,
`model/plan-split-20261019.md` (Q1a, Q1b, Q2), the Q2 result file
`model/results/20261019T064254Z-split-Q2-sim.md`, both rule files and
Omni-Paxos 532ff79 (`leader.rs` 240-255 and 371-403, `follower.rs`
128-138 and 229-238, `util.rs` 115-129 and 382-390).

## Checks done for this review

The planned action went into a copy of the model in the scratch
directory. Each run is a TLC 2.19 simulation, depth 60, seed 1, one
worker, at most 280 s. The bounds are those of Q2, with the changes
given below. The model in the tree is unchanged.

| Run | Arm | Bounds changed | Result |
|---|---|---|---|
| C | Decide with the old sequence number | MaxLog 2, MaxCounter 5 | S5 violated |
| D | Decide with `decided_idx[i] + 1` | MaxCounter 4 | no violation in 280 s |
| F3 | planned form, cover NoSeed4 | none | cover reached in 1 s |

TLC printed the trace of run C up to state 12 and then stopped with an
ArrayIndexOutOfBoundsException. State 12 shows the sequence. AcceptDecide
{1,2} with v2 to node 1 was lost. Node 1 took the resend Decide {1,2} as
Expected. Node 1 has n_accepted 13, log `<<>>` and seq {1,2}. Leader 3
has log `<<v2>>`. A ProposalForward v1 is in flight to leader 3. The next
AcceptDecide {1,3} puts v1 at index 1 on node 1 and at index 2 on node 3.
The state after state 12 was not printed, so that last step was not
checked.

## Safety cases with no fault found

- Decide from an old leader: `check_valid_ballot` answers NotAccepted or
  calls `reconnected`, as it does for every message today. S1 and S2
  stay the same. Two leaders (ballots 11 and 12) are possible in Q2
  with MaxBallot 1, so the Q2 runs reach this case.
- Decide during AcceptSync: the follower is in Prepare and ignores it.
  If the AcceptSync arrived first, the Decide carries counter 2 of the
  new session and is Expected.
- Decide to a follower in Recover or Prepare: `handle_decide` returns
  before the sequence check, so no DroppedPreceding and no change.
- Decided index (S2, S2b, S7): `update_decided` caps the index at the
  accepted index of the follower and never lowers it. With an Expected
  sequence number, the log of the follower is a prefix of the log of
  the leader under the same ballot.
- R1: the Decide changes no state of the read round. Not run.
- DroppedPreceding for a lost last message: lost counter c, follower at
  c-1, new Decide c+1 in the same session. `check_msg_status` returns
  DroppedPreceding (`message.hpp` 39-48, `util.rs` 382-390). It fires.

## Findings

1. Fault: the C++ code has no `send_decide`. The Decide is built inline in `handle_accepted` (line 742). Section 1 and section 3 name a function that does not exist.
   Fix: write `send_decide(node_id to, std::uint64_t decided_idx)` after `leader.rs` 240, and call it from `handle_accepted` and from `resend_message_timeout`.

2. Fault: the plan does not say that it departs from the origin. Omni-Paxos resends with `resend = true`, which uses `get_seq_num` and does not raise the counter.
   Fix: state the change and its reason, which is finding 3.

3. Fault: the first counter-check expects that the old sequence number "must not break safety". The old sequence number breaks S5 (run C). A resend Decide with the counter of a lost AcceptDecide is Expected, the entries of that AcceptDecide are missing, and the next AcceptDecide is appended at a wrong index. At MaxLog 1 there is no second AcceptDecide, so the check shows nothing.
   Fix: the old sequence number must break S5. Run it at MaxLog 2, MaxCounter 5, MaxLoss 1, MaxTimeout 1.

4. Fault: the second counter-check (`decided_idx[i] + 1`) cannot break S1, S2 or S2b with three nodes. The follower caps the index at its own accepted index, and each entry that it holds under the ballot of the leader is also held by the leader, which is a majority. With MaxBallot 1 and one round per pid, S2b cannot break. Run D found nothing in 280 s.
   Fix: remove this check. Do not use a mutation of the decided index as a counter-check in a model with three nodes.

5. Fault: Q1b has MaxTimeout 0 (Q1b result file, line 22), so the new branch never runs in Q1b.
   Fix: name Q1b as the check that nothing changes without a timeout, or remove it from the list.

6. Fault: with MaxCounter 3 (Q2), the case is reached only when the entry travels in the AcceptSync: AcceptSync {1,1}, Decide {1,2} lost, resend {1,3}. Run F3 reached this case. When the entry travels in an AcceptDecide, the resend Decide has counter 4. Bounds cuts it off.
   Fix: run Q2 with MaxCounter 4. Add a cover invariant, such as NoSeed4 (follower in Recover at seq {1,2}, leader decided 1), that must be violated.

7. Fault: the plan runs only Q1b and Q2. The property list leaves out R1, S4 and S6. MC_e (R1) has MaxCounter 2, so the raised counter is cut off there.
   Fix: also run Q7 with MaxCounter raised by one, and Q9 (MC_c, StopSign).

8. Fault: in the model, the leader branch already sends AcceptStopSign or a StopSign Decide with the old sequence number to the set `ss`. The plan does not say what a follower in `ss` gets in addition, or in what order.
   Fix: send the old-number message first and the raised Decide after it in the same sequence, or leave out the raised Decide for a follower in `ss`. State which one.

9. Fault: `ResendMessageTimeout` ends with `UNCHANGED <<durable, volatile>>`, and `volatile` holds `leader_state`. The plan does not name this edit.
   Fix: list every variable of `volatile` except `leader_state` in UNCHANGED.

10. Fault: TLC has no default value for a constant. `DecideOnResend` "TRUE by default" needs an edit to all 74 `.cfg` files. The two counter-check mutations also need their names in the `ASSUME Mutation \in {...}` list, line 2015.
    Fix: make the old form a value of `Mutation` (for example "NoDecideOnResend") and add `MC_mut_*` files. Otherwise list the 74 cfg edits as a step.

11. Fault: section 1 says that the follower takes max(own, decided index). `update_decided` takes min(decided index, accepted index) and replies Accepted when it caps the index.
    Fix: write the rule of `update_decided` (`follower.rs` 142-158).

12. Fault: the C++ test catches up "after one resend timeout". After the timeout, the follower goes to Recover. It catches up after PrepareReq, Prepare, Promise and AcceptSync. The test also does not cover a lost AcceptDecide that carries entries.
    Fix: deliver that exchange in the test. Add a case where the lost last message is an AcceptDecide with an entry, and check log and decided index against the leader.

13. Fault: section 5 counts one Decide for each follower. A lost last message now starts a full resync: PrepareReq, Prepare, Promise, and an AcceptSync with the log suffix.
    Fix: name the resync in section 5.

14. Fault: the a-F1-loss BFS took 1 h 54 min for 86 M states (plan-split). Q2 BFS with the new action and MaxCounter 4 has more states. The plan does not say what a run that hits the 2 h limit shows.
    Fix: say that a BFS cut by the limit is a partial result, and record its depth in the result file.

15. Fault: the names `DecideOnResend` and `--no-reconnected` have no source outside the tree (rule "Every name comes from a source outside this tree").
    Fix: give both names to the owner as a question, with a proposal.

16. Fault: section 4 has no result file for each run, no branch and no state file update (rules "Each run of the bench is one file", "A message is a task on its own branch", "A state file holds what is in force").
    Fix: add these steps to section 4.

## Note outside the plan

The C++ `resend_message_timeout` has no StopSign branch. `resend_messages_leader` in the origin has one. The model has one. This is a difference between code and model that already exists.

Verdict: approve with changes
