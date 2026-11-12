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

TLC, BFS, one worker each, on MC_base, MC_base_monotone and MC_base_s7
(invariants AcceptedIdxIsLogLength S1 S2 S2b S3 S5, property
S2Grows). A run that does not end in one hour stops, and its state
count is written down as partial. A violation is a finding for the
owner, because the code has the same form.

## 4. What this plan does not do

- No change to `HandleAcceptSync`. The code refuses a stale sync and
  keeps the log on a sync of the same ballot; the model does neither.
  That waits for the next refusal of the trace spike.
- No change to the code.

## 5. Data this plan leaves on the disk

- TLC state directories in the scratchpad: deleted after the result
  file is written.
- One result file in model/results/: recorded in the state file.
