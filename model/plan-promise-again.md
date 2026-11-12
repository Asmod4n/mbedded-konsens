# Plan: the model promises the same ballot again

Source: the trace spike of 2026-11-12
(model/results/20261112T222300Z-trace-spike-seed1.md on branch
trace-validation). TLC refused record 93 of seed 1. The code promised a
ballot that the follower had promised before. The model does that only
in phase Recover.

## 1. Facts

- Code, include/konsens/sequence_paxos.hpp `handle_prepare`: a prepare
  is taken when `promise < n`, or when `promise == n` and the role is
  follower. The phase does not matter.
- This form came with plan-promise-again and the owner card of
  2026-10-31. The model was not changed with it.
- Model, SequencePaxos.tla `HandlePrepare`: a prepare is taken when
  `promise[i] < m.n`, or when `promise[i] = m.n` and `phase[i] =
  "Recover"`.

## 2. Change

`HandlePrepare` takes the prepare when `promise[i] = m.n` and
`role[i] = "Follower"`, as the code does. The old condition stays as
`Mutation = "NoPromiseAgain"`, so that no config needs a new constant.

## 3. Check

After review round 1: MC_base, MC_base_monotone and MC_base_s7 have no
loss, no duplicate and no timeout. In them a follower gets a Prepare of
its promised ballot only in phase Recover, so they do not reach the new
path. Their runs stopped after 757 678 distinct states with no
violation; they stay out of this check.

- Invariant `RecoverIsFollower`: the phase Recover comes with the role
  Follower. Then the old condition is a part of the new one, and the two
  differ only for a follower in phase Prepare, Accept or None.
- TLC, BFS, one worker each, on MC_a_F1_dup.cfg (one duplicate) and
  MC_a_F1_loss.cfg (one loss, one timeout), with the invariants of each
  config and `RecoverIsFollower`.
- Cover `NoPromiseAgainReached` on MC_a_F1_dup.cfg: a run must violate
  it. Then a follower out of phase Recover gets a Prepare of the ballot
  it promised.
- `Mutation = "NoPromiseAgain"` is the old form. It is stricter and not
  wrong, so no violation is expected under it. It is not run as a check
  of the check.

## 4. What this plan does not do

- No change to `HandleAcceptSync`. The code refuses a stale sync and
  keeps the log on a sync of the same ballot; the model does neither.
  That waits for the next refusal of the trace spike.
- No change to the code.
- Two older differences between the THEN branch and the code stay
  open. The code keeps the prepare and computes the promise again at
  each resend; the model keeps the finished promise. The model sends
  the promise through `IfVotes`; the code pushes it without a check in
  `handle_prepare`.

## 5. Data this plan leaves on the disk

- TLC state directories in the scratchpad: deleted after the result
  file is written.
- One result file in model/results/: recorded in the state file.
