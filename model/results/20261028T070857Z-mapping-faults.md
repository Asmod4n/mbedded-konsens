# The mapping with a crash, a lost disk and a rollback

Commit of the model: 82b97c3 (branch model-proofs). Plan:
model/plan-proofs.md, version 3, section 3.2, second scope.
Module: model/tla/RefinementChecks.tla over SequencePaxos.tla and
MultiPaxosVoting.tla. TLC 2.19 (rev 5a47802), 3 workers, -deadlock,
-Xmx3g, one run at a time, 15 min each (day rule of the owner). Java:
OpenJDK 21.0.12.1 (Ubuntu). Machine: Firecracker VM, kernel 6.18.44,
amd64, 4 processors. Load average 3.1 to 3.4 during the runs.

## Change of the mapping

A step that restarts a node (crash, lost disk, rollback) adds no vote
to the history: the node loads a state that it held before. The end of
a recovery adds the copied log as a vote of the node at the copied
n_accepted. The action property `NoVoteBelowPromise` checks that each
new vote of a node is at a ballot at or above the highest promise that
the node ever gave.

## Bounds

The constants of MC_a_F3 (one crash), MC_a_F4 (one lost disk, rule R,
ballot 2), MC_F6b (one rollback, recovery) and MC_a_F4 with
RuleR = FALSE (the counter-check of run G).

## Runs

| Run | Result | Distinct states | Depth |
|---|---|---|---|
| MC_ref_F4_noR | NoVoteBelowPromise violated, as wanted (12 s) | 45,305 | 7 |
| MC_ref_F3 | no violation; did not end | 7,261,689 | 16 |
| MC_ref_F4 | no violation; did not end | 7,681,084 | 15 |
| MC_ref_F6b | no violation; did not end | 7,707,333 | 15 |

The counts are the last progress line before the limit.

## Result

- Without rule R, a node that lost its disk votes below a promise it
  gave before the loss. The mapping finds this in 7 steps.
- With rule R, with one crash, and with one rollback and recovery, no
  run found a violation of the mapping in 15 min. No run is complete.
- Not run yet: 5 nodes with a promise in flight (compare
  MC_rr_inflight_n5).
