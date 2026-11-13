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
  or Accept (line 828). `end_step` (lines 471-478) appends the buffer
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

After review 1 (model/review-plan-buffer-after-sync.md).

In `HandleMajorityPromises`, when `bss = <<>>` and not `stop`:

- the AcceptSync of each promised follower is built from `lg1`, with
  `last = PrefixAt(lg1, dec)`, as before;
- in the same action, each promised follower then gets an AcceptDecide
  with `entries = bp`, `decided_idx = dec` and
  `last = PrefixAt(lg1 \o bp, dec)`, as `accept_entries_leader` reads
  the chain hash after the append;
- the new leader state is `NextSeqNums(WithAccSyncSeqs(ls2, fs), fs)`.
  The AcceptSync takes counter 1 of the new session (`send_accsync`),
  the AcceptDecide takes the next counter from that state;
- one `Send` carries both: `out[k] = <<AcceptSync, AcceptDecide>>`. A
  channel with room for one message loses the AcceptDecide, as a loss
  in the code;
- the log, `accepted_idx` and the own accepted index of the leader are
  the same as before (`lg1 \o bp`).

When `bp = <<>>` no AcceptDecide is sent. When `stop` holds, no
AcceptDecide is sent; the model keeps the buffer and the code clears it
(sequence_paxos.hpp:1725-1726), a difference that stays. When
`bss # <<>>`, the old form stays. The code has no separate stop-sign
buffer: its stop sign is an entry of `buffered_proposals_` and goes out
in the AcceptDecide; the model carries it in its own buffer and in its
own message AcceptStopSign. No trace of phase 1 holds a
reconfiguration, so this gap waits for the phase that traces one.

The old form stays as `Mutation = "BufferInSync"`, with a line in the
ASSUME list of mutations and in the comment above it.

Known gaps for the phase with `batches::random`: the code buffers a
proposal and a forwarded proposal to the end of the step, the model
appends each at once. With `batches::single` each step holds one
append or one delivery (test/simulation.hpp:490-516, 536-548), so the
order and the number of AcceptDecide messages agree. A promise that
arrives late in the same batch also differs there.

## 3. Check

- The trace spec of the spike, with this model, on seeds 1 to 10. Seeds
  2, 4, 5, 6, 8 and 9 must pass the record they were refused at. Seeds
  1 and 10 must stay accepted whole.
- Counter-checks on seed 2: the AcceptDecide of the new leader with
  counter 1, and with `decided_idx = 0`, each as a model mutation of
  one run; both must be refused. The three counter-checks of the spike
  run again.
- Action property `NoSyncAndDecideInOneStep`: no step appends an
  AcceptSync and an AcceptDecide to the same channel. TLC must violate
  it on MC_a_F1_dup.cfg, and it must hold under
  `Mutation = "BufferInSync"` on the same config with a time limit.
- TLC, BFS, one worker, `-deadlock`, one hour each, on MC_a_F1_dup.cfg
  and MC_a_F1_loss.cfg with the invariants of each config. The result
  file says whether TLC searched the whole state space.

## 4. What this plan does not do

- No change to the code.
- No change for a buffered stop sign.
- No change to `Propose` in phase Accept or to a forwarded proposal.

## 5. Data this plan leaves on the disk

- TLC state directories in the scratchpad: deleted after the result
  file is written.
- One result file in model/results/: recorded in the state file.
