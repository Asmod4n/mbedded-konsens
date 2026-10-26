# One environment: runs, round 1

Model: model/tla/OneEnvironment.tla at 922b49f (the configs with MaxLog
1 at 0ee0024 and 45992d5; the module did not change after 922b49f).
Plan: doc/plan-one-environment.md, section 13. TLC 2.19 (rev 5a47802),
OpenJDK 21.0.12.1, 4 processors.

BFS command (in model/tla):

    timeout <limit> java -XX:+UseParallelGC -XX:ParallelGCThreads=1 -Xmx2g \
      -cp tla2tools.jar tlc2.TLC -workers 1 -deadlock \
      -metadir <dir> -config <config>.cfg OneEnvironment.tla

Simulation: the same with `-simulate -depth 100 -seed 7`. Exit 12 is a
violated invariant, exit 124 the end at the limit, exit 0 a complete run.
At most three TLC processes ran at the same time, one worker each. The
state directory of each run was deleted after the run.

Bounds of every config: Nodes {n1, n2, n3}, Requests {r1, r2},
MaxBallot 2, MaxLog 2, MaxCrash 1, except where the name ends in `_l1`
(MaxLog 1) and `cover_NoRetryChosen` (MaxLog 3).

A first form of the module (22d6a85) synced each write in the same
action. Review 1 found that its crash lost only the volatile values.
Its runs are not reported here; 922b49f splits each commit from its
sync.

## Safe forms

Invariants: ChosenOnce, DecidedAgree, DecidedIsChosen, StateIsFold,
AppliedIsDecided; p0 also NoOldWorkRecords.

| Config | Mode | Limit | Exit | States | Depth | Result |
|---|---|---|---|---|---|---|
| base_l1 | BFS | 2 h | 0 | 19925386 distinct | 49 | complete, no violation (28 min 03 s) |
| parallel_l1 | BFS | 2 h | 0 | 24153694 distinct | 49 | complete, no violation (33 min 41 s) |
| p0_l1 | BFS | 2 h | 0 | 26499784 distinct | 50 | complete, no violation (37 min 48 s) |
| base | BFS | 15 min | 124 | 14907672 distinct | 26 | no violation, not complete |
| parallel | BFS | 15 min | 124 | 15113455 distinct | 26 | no violation, not complete |
| p0 | BFS | 15 min | 124 | 15878346 distinct | 24 | no violation, not complete |
| NoRecovery | BFS | 15 min | 124 | 13943515 distinct | 27 | no violation, not complete |
| base | simulation | 15 min | 124 | 53653874 checked | | no violation |
| parallel | simulation | 15 min | 124 | 55748584 checked | | no violation |
| p0 | simulation | 15 min | 124 | 53923025 checked | | no violation |
| NoRecovery | simulation | 15 min | 124 | 48600099 checked | | no violation |

parallel is the form of question 4 of the plan: the leader sends "done"
while its own sync runs, and counts itself only after the sync.

NoRecovery holds as the plan says (13.2): every promise and every
accept leave only after their sync, so the recovery protects nothing in
this model.

## Mutations

| Config | Target | Exit | States | Trace | Result |
|---|---|---|---|---|---|
| mut_ApplyAtDone | StateIsFold | 12 | 3144697 distinct | 23 | violated |
| mut_ReplyAtLeaderSync | ChosenOnce | 12 | 534513 distinct | 19 | violated |
| mut_DoneWithoutSync | ChosenOnce | 124 | | | not reached in 15 min |
| mut_DoneWithoutSync_l1 | ChosenOnce | 12 | 8522904 distinct | 30 | violated |
| mut_LeaderCountsUnsynced | ChosenOnce | 124 | | | not reached in 15 min |
| mut_LeaderCountsUnsynced_l1 | ChosenOnce | 12 | 11270459 distinct | 30 | violated |

- ApplyAtDone: n1 leads ballot 1, commits the done record of r1 and
  applies it (state <<r1>>, applied 1). Ballot 2 chooses r2 at index 1.
  The state of n1 holds r1, which is not chosen. No crash is needed: a
  leader change alone gives it.
- ReplyAtLeaderSync: two leaders each reply after their own sync, with
  r1 and with r2 at index 1.
- DoneWithoutSync_l1: n2 sends "synced" for r1 before its sync; n1
  decides r1 and replies. n2 crashes and loses the done record. Ballot 2
  (n2 and n3) chooses r2 at index 1.
- LeaderCountsUnsynced_l1: the same loss at the leader, with
  DoneBeforeOwnSync.

## Covers

| Config | Exit | States | Trace | Result |
|---|---|---|---|---|
| cover_NoForgottenDone | 12 | 4977461 distinct | 24 | reached |
| cover_NoForgottenDoneAfterCrash | 12 | 7577237 distinct | 25 | reached |
| cover_NoRetryChosen | 12 | 1086902 distinct | 19 | reached |
| cover_NoDecidedLost | 12 | 124738 distinct | 16 | reached |
| cover_NoWorkRecordSurvives | 12 | 1231 distinct | 9 | reached |

- NoForgottenDoneAfterCrash is the case of the owner (2026-10-26 07:19):
  n1 holds the synced done record of r1 at index 1 and is down. Ballot 2
  chose r2 at index 1 and replied. The state of every node is empty, so
  no node holds the effect of r1.
- NoWorkRecordSurvives (P0): a work record, a bare sync, a crash: the
  work record survives on the disk, and the start deletes it.

## Open

- The safe forms at MaxLog 2 have no complete BFS; the simulations of
  15 minutes found no violation.
- MaxBallot 3 and two crashes are not run.
