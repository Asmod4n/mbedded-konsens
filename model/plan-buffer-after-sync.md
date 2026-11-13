# Plan: the new leader sends its buffer after the AcceptSync

Source: the trace spike of 2026-11-13
(model/results/20261113T004300Z-trace-spike-seed1-net.md on branch
trace-validation). TLC refused seeds 2, 4, 5, 6, 8 and 9 for one cause.
A node holds proposals in its buffer and then becomes leader. The code
sends the AcceptSync and then an AcceptDecide with the buffer. The
model sends one AcceptSync that holds the buffer.

## 1. Facts

- Code, include/konsens/sequence_paxos.hpp: `append` puts a proposal
  into `buffered_proposals_` when the node is leader in phase Prepare
  or Accept (line 828). `end_step` (lines 471-477) appends the buffer
  through `accept_entries_leader` when the node is leader in phase
  Accept. So the buffer goes out at the end of the step in which the
  majority of promises arrived, as an AcceptDecide behind the
  AcceptSync. This is the group commit.
- In each of the six traces the network at the refused record holds
  `accept_sync` with an empty suffix and then `accept_decide` with the
  buffered value, from the new leader to one follower.
- Model, SequencePaxos.tla `HandleMajorityPromises`: `lg2 == lg1 \o bp
  \o bss`; the AcceptSync of each follower is built from `lg2`, so the
  suffix holds the buffer.
- The model is a translation of Omni-Paxos (leader.rs:257-285), which
  puts the buffer into the sync. The code changed this with the group
  commit; the model was not changed with it.

## 2. Change

In `HandleMajorityPromises`, when `bss = <<>>`:

- the AcceptSync of each follower is built from `lg1`;
- in the same action, each promised follower then gets an AcceptDecide
  with `entries = bp`, `decided_idx = dec`, `last = PrefixAt(lg1, dec)`
  and the next sequence number, as `AcceptEntriesLeader` builds it;
- the log, `accepted_idx` and the own accepted index of the leader are
  the same as before (`lg1 \o bp`).

When `bp = <<>>` no AcceptDecide is sent. When `bss # <<>>`, the old
form stays: no trace holds a buffered stop sign, and the code cuts the
buffer after a stop sign. The old form stays as
`Mutation = "BufferInSync"`, so no config needs a new constant.

The action stays one step, as the code does it in one step. A Propose
in phase Accept still appends at once in the model; the code buffers it
to the end of the step. This difference does not change the order of
the entries, because the model has no step that holds two proposals.

## 3. Check

- The trace spec of the spike, with this model, on seeds 1 to 10. Seeds
  2, 4, 5, 6, 8 and 9 must pass the record they were refused at. Seeds
  1 and 10 must stay accepted whole.
- Cover `BufferAfterSyncReached`: a channel holds an AcceptSync directly
  followed by an AcceptDecide. TLC must violate it on a config that
  proposes before the first leader, chosen by a search over the MC_a_*
  configs; the result file names the config.
- TLC, BFS, one worker, `-deadlock`, one hour each, on MC_a_F1_dup.cfg
  and MC_a_F1_loss.cfg with the invariants of each config.

## 4. What this plan does not do

- No change to the code.
- No change for a buffered stop sign.
- No change to `Propose` in phase Accept.

## 5. Data this plan leaves on the disk

- TLC state directories in the scratchpad: deleted after the result
  file is written.
- One result file in model/results/: recorded in the state file.
