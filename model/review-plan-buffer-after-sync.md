# Review: plan-buffer-after-sync.md

Plan: model/plan-buffer-after-sync.md, branch model-buffer-after-sync
(d00ac39). Code: include/konsens/sequence_paxos.hpp and
include/konsens/node.hpp of this worktree. Model:
model/tla/SequencePaxos.tla. Trace data: the spike result
model/results/20261113T004300Z-trace-spike-seed1-net.md on
origin/trace-validation.

## Verdict

Approve with changes. Section 1 is true. The change in section 2 is
the right change. The plan must state the sequence numbers and the
single send exactly, correct two reasons, and replace the cover with
a property that the old model cannot violate.

## Findings

1. Section 1 is true. Checked:
   - sequence_paxos.hpp:826-829: `append` pushes into
     `buffered_proposals_` when the node is leader in phase Prepare or
     Accept.
   - sequence_paxos.hpp:471-478: `end_step` cuts the buffer, calls
     `accept_entries_leader(buffered_proposals_)` and clears the buffer,
     when the node is leader in phase Accept. The block ends at line
     478, not 477.
   - sequence_paxos.hpp:1932-1946: `accept_the_taken_log` writes the
     taken log only, sets phase Accept and sends one AcceptSync to each
     promised follower. The buffer is not in the log at that time.
   - SequencePaxos.tla:1083 `lg2 == lg1 \o bp \o bss` and line 1099
     builds each AcceptSync from `lg2`.
   - refs/omnipaxos/omnipaxos/src/sequence_paxos/leader.rs:257-285
     appends the buffer and the stop sign before `send_accsync`.

2. Sequence numbers: the plan must give the exact form. In the code,
   `send_accsync` (sequence_paxos.hpp:1970-1974) increments the session
   and takes the next number. `accept_entries_leader`
   (sequence_paxos.hpp:1759-1765) then takes the next number of the same
   session. So the AcceptSync carries counter 1 and the AcceptDecide
   carries counter 2 of the new session. In the model,
   `WithAccSyncSeqs` (SequencePaxos.tla:526-529) sets counter 1 and
   `NextSeqNums` (SequencePaxos.tla:494-498) adds 1. The new leader
   state must be `NextSeqNums(WithAccSyncSeqs(ls2, fs), fs)`, and the
   AcceptDecide must read its `seq_num` from that state. Write this in
   the plan.

3. One send, not two: `chan'` can take one value in the action. The
   plan must use one `Send` (SequencePaxos.tla:434-436) with
   `out[k] = <<AccSyncMsg(ls2, k, lg1, dec), AcceptDecide>>`. Then the
   order in the channel is the order of the code. Where the channel has
   room for one message only, `Enqueue` loses the AcceptDecide. That is
   a loss, and the follower then sees counter 3 as DroppedPreceding.
   This matches the code. State it.

4. Decided index and `last`: `decided_idx = dec` is correct. The code
   reads the decided index after the append, and nothing in
   `accept_entries_leader` changes it (sequence_paxos.hpp:1750-1757).
   The code reads the chain hash on the log after the append, that is
   `lg1 \o bp`. `PrefixAt(lg1, dec)` equals `PrefixAt(lg1 \o bp, dec)`
   only when `dec <= Len(lg1)` (SequencePaxos.tla:396). With
   VerifyPromise, `MaxDecidedVerified` guarantees this
   (SequencePaxos.tla:485-490). Without VerifyPromise, `MaxDecided`
   gives no such bound in the model. Use `PrefixAt(lg1 \o bp, dec)`, as
   the code reads it. The `last` of the AcceptSync stays
   `PrefixAt(lg1, dec)`: `send_accsync` reads it before the append
   (sequence_paxos.hpp:1965).

5. Own accepted index, `log`, `accepted_idx`: correct. The code sets
   the own index to the length of `lg1` (sequence_paxos.hpp:1941) and
   then to the length of `lg1 \o bp` (sequence_paxos.hpp:1758). Only the
   last value is visible after the step. `AccBound(i, lg2, acc2)` and
   `acc2` stay as they are.

6. Which followers: correct. Both sends go to
   `get_promised_followers()` (sequence_paxos.hpp:1942 and 1759), and
   no promise arrives between them. The model uses one `fs` for both.

7. Followers that promise later: correct for phase 1 only. The code
   answers a later promise in a later step with `send_accsync` from the
   log that holds the buffer (sequence_paxos.hpp:1826-1835). The model
   answers from `log[i] = lg2` (SequencePaxos.tla:1117-1120). These are
   equal with `batches::single`, because a delivery holds one message
   (test/simulation.hpp:490-516). With `batches::random`, a late promise
   can be in the same batch as the majority. Then the code sends that
   follower an AcceptSync without the buffer and then the AcceptDecide.
   The model cannot do this. Record this as a known gap for the batch
   phase of the trace plan.

8. The reason for Propose in phase Accept is wrong in its words. The
   model can take two Propose steps in a row, so "the model has no
   step that holds two proposals" is not the point. The point is the
   harness: an append step makes one `append` and one `commit`
   (test/simulation.hpp:536-548), so `end_step` sends one AcceptDecide
   with one entry, with the next counter. The model `Propose`
   (SequencePaxos.tla:1700-1703) sends the same message. The same holds
   for a forwarded proposal: the code buffers it in phase Accept
   (sequence_paxos.hpp:2500-2506), and the model appends it at once
   (SequencePaxos.tla:1324-1327). The difference is harmless in phase 1.
   With `batches::random`, several forwards in one batch give one
   AcceptDecide in the code and several in the model. Replace the
   sentence with this fact and add the gap for the batch phase.

9. The reason for a buffered stop sign is wrong. The code has no
   separate buffered stop sign. `reconfigure` appends the stop sign as
   an entry (node.hpp:545). It goes into `buffered_proposals_`
   (sequence_paxos.hpp:828). `holds_a_stop_sign` refuses entries after
   it (sequence_paxos.hpp:822-824, 900-904). The cut
   (sequence_paxos.hpp:1723-1733) only removes entries behind the stop
   sign. So at the majority of promises the code sends `bp` and the stop
   sign together as one AcceptDecide behind the AcceptSync. The code also
   has no AcceptStopSign message (no match in include/konsens). The
   model sends a stop sign in phase Accept as AcceptStopSign
   (SequencePaxos.tla:964-975). Keeping the old form for `bss # <<>>`
   is acceptable for phase 1, because no trace holds a reconfiguration.
   The plan must give this reason: the model and the code carry a stop
   sign in different messages, and that is a separate difference.

10. The case `stop` (a stop sign in `lg1`) differs already. The model
    keeps `buffered_proposals[i]` (SequencePaxos.tla:1094-1095). The code
    clears the buffer (sequence_paxos.hpp:1725-1726, after 1939-1940
    set `accepted_stop_sign`). The plan keeps `bp = <<>>` there, so no
    AcceptDecide is sent, which is correct. The kept buffer is not in
    a trace record. Note it as a known difference, no change needed.

11. The cover does not show the new branch. A channel with an
    AcceptSync directly followed by an AcceptDecide is reachable in the
    old model too: `HandleMajorityPromises`, then `Propose` in phase
    Accept (SequencePaxos.tla:1700-1703), before the follower receives
    the AcceptSync. Use an action property that only the new branch
    violates: one step appends an AcceptSync and an AcceptDecide to the
    same channel. For example
    `[][~\E p \in Pairs : Len(chan'[p]) = Len(chan[p]) + 2 /\ chan'[p][Len(chan'[p]) - 1].type = "AcceptSync" /\ chan'[p][Len(chan'[p])].type = "AcceptDecide"]_vars`.
    Add the counter-check: with `Mutation = "BufferInSync"` the same
    property holds.

12. The cover can be reached, not checked by a run. MC_a_F1_dup.cfg
    and MC_a_F1_loss.cfg set `ClientSessions = FALSE`, `MaxLog = 1` and
    two values. `Propose` at a node whose promise names no other leader
    buffers the value (SequencePaxos.tla:943-950). A later
    `HandleLeader` and a majority of promises then take the new branch.
    The plan names the config only after a search. That is acceptable.

13. The trace check compares message types only (spike result, "Not
    done"). A wrong counter or a wrong decided index in the AcceptDecide
    shows only through the state of the follower after the delivery.
    Add two counter-checks on one of the six seeds, as the spike did:
    the AcceptDecide with the counter of the AcceptSync, and the
    AcceptDecide with `decided_idx = 0`. Both must be refused. Also run
    the three counter-checks of the spike again.

14. `Mutation = "BufferInSync"` needs a line in the ASSUME of the
    allowed values (SequencePaxos.tla:3505) and in the comment above it
    that names each mutation and the property it breaks. The plan does
    not name a property for it. Give one, or the cover of finding 11.

15. Section 3, the runs of one hour on MC_a_F1_dup.cfg and
    MC_a_F1_loss.cfg are adequate for the invariants. A run that ends
    at the hour proves nothing complete. The result file must say
    whether the state space was exhausted.

## Not checked

- The Omni-Paxos reference was read only at leader.rs:257-285.
- The trace spec and the six traces were not run. They are on another
  branch and in the project files.
- `first_changed_idx` in `send_accsync` (sequence_paxos.hpp:1960) has
  no form in `AccSyncMsg`. That is older than this plan.
