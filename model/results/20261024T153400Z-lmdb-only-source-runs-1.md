# LMDB as the only source: simulation runs, round 1

Model 965e894: RunningStorageFault damages the durable state of a
running node, and the node takes its next step on the damaged state.
StepCheck refuses a running node whose state the checks find. Tool:
TLC v1.8.0 (nightly jar 2026.10.20), one worker,
`-XX:ParallelGCThreads=1`, `-simulate -deadlock -depth 60 -seed 1`,
15 min limit, three runs at a time on 4 processors. Runs from a copy of
model/tla outside the repository. Invariants S1 S2 S2b S3 S4 S5 S6.

| Config | Kinds | StepChecks | States | Result |
|---|---|---|---|---|
| MC_r_promise | promise | TRUE | 144929 | S2b violated after 30 s |
| MC_r_decided | decided | TRUE | 4904600 | no violation, stopped by the limit |
| MC_r_accepted | accepted | TRUE | 5416701 | no violation, stopped by the limit |
| MC_r | all four | TRUE | 13761 | S5 violated after 6 s |
| MC_r_entry | entry | TRUE | 682 | S5 violated after 3 s |
| MC_r_nocheck | all four | FALSE | 8027 | S2b violated after 5 s |

- S5 (MC_r_entry, MC_r): a leader holds v2 at index 1, and a follower
  has accepted it. The leader loses its last key while it runs; the
  stored log stays consistent and is one entry shorter, so no check
  finds it. The leader reads length 0 and accepts v1 at index 1. Two
  logs of the same accepted round are not compatible.
- S2b (MC_r_promise): a node loses its promise while it runs. The
  checks find no fault when its log is empty, and the node promises a
  lower ballot again.
- The code at 06a52bf finds the first case through the field
  `accepted_idx_`: the next append fails. The model of the plan of
  2026-10-24 14:44 has no such field.
- The owner, 2026-10-24 15:30 to 15:35: each work of a client is a
  transaction with nested transactions; its entry carries a state, in
  work and then committed; it is done only when a majority reports the
  same value read after the commit. That form is not in this model yet.
- A simulation without a violation is not a complete run.
