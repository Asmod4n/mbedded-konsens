# Execute-Verify: runs, round 3

Model: e37b8ef with the uncommitted changes of this round. Plan:
doc/plan-execute-verify.md as of fc1c3dc (2.6.2, 2.6.5, 3.3). TLC 2.19
(rev 5a47802), OpenJDK 21.0.12.1, 4 processors.

Simulation command (in model/tla):

    timeout 900 java -XX:+UseParallelGC -Xmx3g -cp tla2tools.jar tlc2.TLC \
      -workers 1 -deadlock -simulate -depth 100 -seed 7 \
      -metadir <dir> -config <config>.sim.cfg SequencePaxos.tla

BFS command: the same without `-simulate -depth 100 -seed 7`. Exit 124 is
the end at the limit of 15 min. States is the last progress line. At most
3 TLC processes ran at the same time. The TLC state directory of each run
was deleted after the run.

## Changes

- 2.6.5: V takes `rejected(k)` where k is the first request entry above p
  and below the entry (FirstRequestAbove), in place of `k = p + 1`.
  AppendVerdict already named that k. Provisional: plan 2.3.1 still says
  `k = p + 1`.
- 2.6.2: Counted takes an answer only when its pointer equals the pointer
  of the leader (`an.ptr = a.p`, before `an.ptr <= a.p`).
- 3.3: an open mark covers a proof once. The new variable mark_covered
  (provisional name) is set by a covered throw and cleared by each new
  mark. A second covered proof under the same mark counts. Only
  ThrowStart sets mark_covered; the repair from a peer in StorageFault
  and DamageFound reads it and does not set it.
- Each proof carries the count n that it set, so that two proofs of the
  same index stay two members of proof[i]. Each entry of result_throws
  carries cov, mark and used (open_mark and mark_covered before the
  throw).
- New invariant CoveredProofCountsNothing (the first proof at or below an
  open mark counts nothing). New cover NoOperatorWait (a node whose
  throws all name one index, one covered, waits for the operator).
- CountProofTwice now targets CoveredProofCountsNothing. RepairsBounded
  no longer catches it: the proofs now carry n, and in round 2 the hit
  came from two equal proofs that were one member of the set.

## 2.6.5: the defect in the model before the change

Found by reading, not by a trace. Before the change, AppendVerdict named
the first request entry above p (q0), and V took `rejected(k)` only at
`k = p + 1`. Where p + 1 holds a verdict entry, `rejected(q0)` was stale
for V. p stayed, q0 stayed open, and the leader could append it again
after the decision, up to MaxLog. Probes on the model before the change
(module Probe, outside the tree):

| Probe | Config | Wall time | States | Result |
|---|---|---|---|---|
| RejectedOnRequest, StaleRejected | NoReject_wrong, MaxLog 4 | 15 min | 2960348 | holds |
| RejectedOnRequest, StaleRejected | NoReject_wrong, MaxLog 6 | 15 min | 2324457 | holds |
| RepeatRejected | NoReject_wrong, MaxLog 6 | 15 min | 2277342 | holds |
| cover NoRejectedAboveVerdict | NoReject_wrong, MaxLog 4 | 15 min | 2947964 | not reached |
| cover NoRejectedAboveVerdict | as above, Values {v1, v2}, MaxCounter 8 | 15 min | 2882044 | not reached |

The rejected entry never named a verdict entry. The case "verdict entry
at p + 1, then rejected" was not reached. On the changed model, the same
cover is not reached either (2941473 and 2850867 states, 15 min each).
RejectedOnRequest and RepeatRejected hold at MaxLog 6 (2258224 states,
15 min).

## 3.3: the same trace before and after

Probe NoRestartLoop: a running node with three throws of one index.
Config: wrong with Values {v1}, MaxLog 2, MaxCounter 8, MaxResultThrow 3.

| Model | Wall time | States | Trace |
|---|---|---|---|
| before | 39 s | 141945 | throw 1 counts (repairs 1), operator resets, throws 2 and 3 are both covered: no count, restart each time |
| after | 39 s | 142153 | the same prefix; throw 2 is covered, throw 3 counts (repairs 1) |

Before the change, only MaxResultThrow ended the loop.

BFS from state 41 of the "after" trace (throw 1 counted, repairs 1,
mark 1), level < 18, invariant NoOperatorWait: violated after 14 states,
278 distinct states, 1 s. In the end state, throw 2 is covered, throw 3
counts, repairs is 2, proof has 2 members, and node 1 is refused (waits
for the operator). RepairsBounded holds there.

## Covers and mutations

| Name | Config | Form | Wall time | States | Result |
|---|---|---|---|---|---|
| NoOperatorWait | wrong, Values {v1}, MaxLog 2, MaxResultThrow 3, RepairFromPeer TRUE | sim | 15 min | 3296541 | not reached |
| NoOperatorWait | as above, MaxLog 4, Values {v1, v2, v3} | sim | 15 min | 2680164 | not reached |
| NoOperatorWait | wrong, Values {v1}, MaxLog 2, MaxResultThrow 3, RepairFromPeer FALSE (in the tree) | sim | 2 min 59 s | 687981 | violated, 50 states |
| NoOperatorWait | as above, MaxLog 4, Values {v1, v2, v3} | sim | 15 min | 2664507 | not reached |
| NoReject | NoReject_wrong | sim | 3 s | 855 | violated, 44 states |
| NoResults | wrong | sim | 2 s | 1009 | violated, 64 states |
| NoResultsAfterLeaderChange | change | sim | 6 s | 10231 | violated, 59 states |
| RejectedUnchecked | flip, target RejectedHasNoMajority | sim | 3 s | 1096 | violated, 52 states |
| CountProofTwice | wrong, MaxLog 4, target RepairsBounded | sim | 15 min | 2635863 | not reached |
| CountProofTwice | wrong, Values {v1}, MaxLog 2, MaxResultThrow 3 (in the tree) | sim | 38 s | 137944 | CoveredProofCountsNothing violated, 42 states |

With RepairFromPeer TRUE, the operator step is enabled while repairs
equals AutoRepairs. The simulation takes it before the third throw.

## Section configs

Simulation, 15 min each, on the model with the three changes. No BFS was
tried: round 2 showed that no BFS ends in 15 min at MaxLog 2.

| Config | Touched by | States | Result |
|---|---|---|---|
| MC_ev_spread | rejected | 3316826 | no violation |
| MC_ev_flip | rejected | 2219133 | no violation |
| MC_ev_wrong | pointer | 2192455 | no violation |
| MC_ev_loss | pointer | 5466711 | no violation |
| MC_ev_change | pointer | 2017258 | no violation |
| MC_ev_damage | 3.3 | 3585024 | no violation |

spread and flip ran before the record fields of result_throws and the
invariant CoveredProofCountsNothing were added. That change adds ghost
fields only. wrong, loss, change and damage ran with the invariant.

Run later on 206e8e4, the three at the same time, with
CoveredProofCountsNothing:

| Config | Touched by | States | Result |
|---|---|---|---|
| MC_ev_flip_change | rejected, pointer | 2652997 | no violation |
| MC_ev_sessions | pointer | 1446706 | no violation |
| MC_ev_five | pointer | 1008370 | no violation |

## Not rerun

The covers and mutations that are not in the tables above.
