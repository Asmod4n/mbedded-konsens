# Plan: the leader sends its decided index at each resend timeout

Source: the owner chose "Beides" on 2026-10-19 19:41 (UTC+2). Seed 4 of
`counter_stress --loss 50 --duplicate 50 --restart 20` showed that a
follower stays one entry behind when the last Decide to it is lost and
no later message shows the gap. Part 1 (`reconnected`, 52cda0f) lets
the caller report a dropped connection. This plan is part 2: the
library finds the gap by itself. This plan gets one review before the
build.

## 1. Form

At each resend timeout, a leader in phase Accept sends one Decide to
each promised follower. The Decide carries the ballot of the leader,
the next sequence number of that follower and the decided index of the
leader. It is the message that `send_decide` already builds, so the
follower handles it with `handle_decide` as it is:

- The previous message arrived: the sequence number is `Expected`, and
  the follower takes max(own, decided index) (S7).
- A message before it was lost: the status is `DroppedPreceding`, and
  the follower calls `reconnected`, as today for any gap.
- A duplicate of an older message: `Outdated`, ignored.

No new message, no new field, no new state. Raft has the same form:
the heartbeat of the leader carries leaderCommit (Raft dissertation
3.5, not checked here).

## 2. Model

- `ResendMessageTimeout(i)`, leader branch, phase Accept: add a Decide
  with `ls.follower_seq_nums[k] + 1` and `decided_idx[i]` for each `k`
  in `PromisedFollowers(ls)`, and raise `follower_seq_nums[k]` in
  `leader_state'`, as the code raises it in `send_decide`.
- A switch `DecideOnResend` (TRUE by default), so the old form stays
  as one arm.
- Runs: Q1b (one loss) and Q2 (timeout and loss) of
  `model/plan-split-20261019.md`, each a simulation of 15 min, then
  breadth-first with the 2 h limit, with the switch on. The properties
  are S1, S2, S2b, S3, S5, S7.
- Counter-check: a mutation that sends the Decide with the old
  sequence number `ls.follower_seq_nums[k]` (no raise). It must not
  break safety; the mutation that sends `decided_idx[i] + 1` must
  break S2b or S1.
- The model has no liveness. That the follower catches up is checked
  by the code tests, not by the model.

## 3. Code

- `resend_message_timeout`: in (leader, Accept), call `send_decide`
  for each promised follower.
- Test: the test "a follower that lost the last Decide catches up
  after reconnected" gets a second case without the call to
  `reconnected`: the follower catches up after one resend timeout.
- `counter_stress`: a flag `--no-reconnected` leaves out the calls to
  `reconnected`. Seeds 1 to 300 with `--loss 50 --duplicate 50
  --restart 20` must pass with and without the flag.
- README: one sentence on the Decide at each resend timeout.

## 4. Order

1. Model change, the counter-checks, the runs of section 2.
2. The code of section 3, only after step 1 has no violation.
3. Builds: host g++ 13, sanitizers, Ubuntu 26.10 in podman (g++ 16,
   clang 22).

## 5. Cost

One Decide per follower at each resend timeout of the leader. No
figure is given before a measurement.
