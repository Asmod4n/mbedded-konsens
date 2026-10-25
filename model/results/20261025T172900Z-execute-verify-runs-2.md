# Execute-Verify: runs, round 2

Model: 04b79d1 (no uncommitted change). Plan: doc/plan-execute-verify.md
as of 10720ec (2.6.3 and 2.7.2). TLC 2.19 (rev 5a47802), OpenJDK
21.0.12.1, 4 processors.

Simulation command (in model/tla):

    timeout 900 java -XX:+UseParallelGC -Xmx3g -cp tla2tools.jar tlc2.TLC \
      -workers 1 -deadlock -simulate -depth 100 -seed 7 \
      -metadir <dir> -config <config>.sim.cfg SequencePaxos.tla

BFS command: the same without `-simulate -depth 100 -seed 7`.

Exit 124 is the end at the limit of 15 min; exit 12 is a violation. States
is the last progress line. Wall time is the TLC "Finished in" line, or
15 min at the limit.

## Changes since round 1

- 2.6.3: a selection holds only while its base equals the base that
  chosen and the other selections define (SelHolds). Select counts again
  an index whose selection does not hold, and AppendVerdict appends only
  selections that hold.
- 2.7.2: CheckVerdict runs only in the Accept phase of the promise.
- Flip may also turn the selection for p + 1 into rejected(p + 1), and,
  with two flips left, replace a result and move a node in one entry.
  The ghost flips holds <<node, index, ballot, count>>.
- New invariants PromiseIsVerified and RejectedHasNoMajority (4.11, 4.13).
  RejectedWithoutMajority and the cover NoStaleVerdictEntry are removed.
- New mutation VerdictWhileOpen (the guard of 2.6.5 off). StaleVerdictTaken
  also turns the guard off.
- New config MC_ev_damage (5.9): MC_ev_wrong with MaxStorageFault 1,
  RunningFaults, StorageFaultKind {"missing_entry", "stale_meta"}.
- InFlightOk, StopSignAllowed and the repair guard of StorageFault are
  written as IF in place of a disjunction (see the last section).

## Two violations before the fixes (round 2, model e65c2d1)

| Config | Invariant | Trace | Cause | Fix |
|---|---|---|---|---|
| MC_ev_flip_change | ThrowOnlyWrong | 55 states | a node in phase Prepare of ballot 23 refused the old flipped entry at index 4; the new leader counted it against its own entry at 4 | 2.7.2 |
| MC_ev_loss | ChosenOnOwnBase | 80 states | the leader kept a selection for index 2 on its own result for 1 after p passed 1 with another chosen result | 2.6.3 |

## BFS with small bounds

| Config | Bounds | Wall time | Distinct states | Result |
|---|---|---|---|---|
| MC_ev_flip | MaxLog 3, Values {v1, v2} | 3 min | 304006, queue 138025 | stopped: disk full |
| MC_ev_flip_change | MaxLog 3, Values {v1, v2}, MaxElection 2 | 3 min | 343218, queue 197101 | stopped: disk full |
| MC_ev_flip | MaxLog 2, Values {v1, v2} | 15 min | 1692604, queue 748373, depth 17 | did not end |
| MC_ev_loss | MaxLog 2, Values {v1, v2} | 15 min | 2937193, queue 2439581, depth 8 | did not end |

No BFS ended within 15 min, so the configs below use the 15-minute
simulation.

## Simulations after the fixes

All on the model of 04b79d1. Common: Values {v1, v2, v3}, MaxBallot 2,
Cap 3, MaxCounter 12, MaxSession 4, Recovery, StateInLmdb, RepairFromPeer,
StartDamageCounts, MaxInFlight 2, MaxResultThrow 2, ExecuteVerify.

| Config | Bounds and faults | Wall time | States | Result |
|---|---|---|---|---|
| MC_ev_wrong | MaxLog 4, MaxWrong 1, MaxElection 1 | 15 min | 2212004 | no violation |
| MC_ev_flip | MaxLog 4, Flip, MaxFlip 1 | 15 min | 2271494 | no violation |
| MC_ev_flip_change | as flip, MaxElection 3 | 15 min | 2713095 | no violation |
| MC_ev_change | MaxLog 5, MaxWrong 1, MaxElection 3 | 15 min | 1983611 | no violation |
| MC_ev_loss | MaxLog 4, MaxWrong 1, Lose 1, Duplicate 1, 1 crash, MaxElection 2, MaxTimeout 2 | 15 min | 5180963 | no violation |
| MC_ev_spread | MaxLog 3, Spread | 15 min | 3259206 | no violation |
| MC_ev_sessions | MaxLog 6, ClientSessions, MaxClientSend 4, MaxRequest 2, MaxWrong 1 | 15 min | 1521784 | no violation |
| MC_ev_five | 5 nodes, MaxLog 4, MaxWrong 2 | 15 min | 1025494 | no violation |
| MC_ev_damage | as wrong, 1 storage fault | 15 min | 3604180 | no violation |

Invariants of each: S1 S2 S3 S4 S5 HighWaterHolds AcceptedIdxIsLogLen
EntryRunsOnOnePrefix StateIsDecided ChosenByMajority ChosenIsRight
ChosenOnOwnBase StatesAgree ThrowOnlyWrong RepairsBounded
VerdictCoversRequest PromiseIsVerified RejectedHasNoMajority;
MC_ev_sessions also E1. Property: S2Grows.

The damage path runs in MC_ev_damage: a probe invariant
`storage_faults = 0` is violated after 3 states, and a probe invariant
"no proof of kind damage" after 3 states.

## Covers

| Cover | Config | Wall time | States | Result |
|---|---|---|---|---|
| NoResults | wrong | 2 s | 1054 | violated, 64 states |
| NoReject | spread | 15 min | 4126688 | not reached (no majority-free set exists, round 1) |
| NoReject | NoReject_wrong: MaxLog 4, Spread, MaxWrong 1 | 2 s | 821 | violated, 44 states |
| NoFollowerThrow | wrong | 10 s | 10949 | violated, 50 states |
| NoLeaderThrow | flip | 3 s | 692 | violated, 58 states |
| NoResultsAfterLeaderChange | change | 8 s | 11694 | violated, 45 states |
| NoRunAgain | wrong | 2 s | 886 | violated, 52 states |
| NoTryAgain | wrong | 2 s | 402 | violated, 47 states |
| NoIdleRead | wrong, ReadIndex, MaxRead 1 | 5 s | 2619 | violated, 58 states |

## Mutations

| Mutation | Config | Target | Wall time | States | Result |
|---|---|---|---|---|---|
| ApplyOwnResult | wrong | StatesAgree | 2 s | 2414 | violated, 52 states |
| CountOtherBase | wrong | ChosenOnOwnBase | 15 min | 2484538 | not reached |
| SelectWithoutMajority | wrong | ChosenByMajority | 22 s | 82627 | violated, 39 states |
| AcceptWithoutCheck | flip | ChosenByMajority | 2 s | 793 | violated, 56 states |
| ThrowWithoutRerun | flip | ThrowOnlyWrong | 1 s | 669 | violated, 48 states |
| CountProofTwice | wrong | RepairsBounded | 11 min 40 s | 2257327 | violated, 64 states |
| VerdictWithoutRequest | wrong | VerdictCoversRequest | 4 s | 7474 | violated, 60 states |
| ListedSkipsCheck | flip, MaxFlip 2 | ChosenByMajority | 15 min | 2703912 | not reached |
| OwnAcceptUnchecked | flip | PromiseIsVerified | 1 s | 1012 | violated, 48 states |
| StaleVerdictTaken | change | ChosenOnOwnBase | 15 min | 4264731 | not reached |
| RejectedUnchecked | flip | RejectedHasNoMajority | 2 s | 2518 | violated, 43 states |
| AppliedIsPointer | wrong, reads | NoIdleRead must hold | 15 min | 2656780 | holds, as 4.13 expects |

OwnAcceptUnchecked breaks the conjunct "the accept that a leader counts
for itself is at most its verified_idx": the leader appends a results
entry at 4 and counts 4 with verified_idx 3. No promise message is
involved.

Controls outside the tree, on the model of 04b79d1, 15 min each:

| Config | Mutation | Invariants | States | Result |
|---|---|---|---|---|
| flip, MaxFlip 2 | none | all of the section configs | 2232306 | no violation |
| change | VerdictWhileOpen | ChosenOnOwnBase | 4317311 | no violation |
| change, MaxLog 4, MaxElection 2 | StaleVerdictTaken | ChosenOnOwnBase | 4181249 | not reached |

## CountProofTwice in the form of 5.10

The simulation above reaches the violation itself. The probe form was
also run. Probe module (outside the tree, EXTENDS SequencePaxos):

    ListedThrows == {n \in 1..Len(result_throws) : result_throws[n].kind = "listed"}
    NoListedAfterRecovery ==
      ~ \E n \in ListedThrows : LET i == result_throws[n].node IN
          i \in running /\ ~recovering[i] /\ result_throws[n].idx \in ListedAt(i)

1. The simulation command above with MC_ev_mut_CountProofTwice.sim.cfg,
   INVARIANTS NoListedAfterRecovery and module Probe.tla: violated after
   74 states (488977 states).
2. A module Dbg2 that EXTENDS Probe, with `INIT LastState` (state 74 of
   step 1), `NEXT Next` and `CONSTRAINT Bounds /\ TLCGet("level") < 12`,
   INVARIANTS RepairsBounded, BFS: no violation, 14548 distinct states, 59 s.
   With round 1 the same form from its state 63 broke RepairsBounded in
   one step (8 distinct states).

## Generated states with ExecuteVerify = FALSE

Reduced MC_base (MaxLog 1, MaxElection 1, VIEW), BFS, -dump of both
state sets:

| Model | Generated | Distinct | Depth |
|---|---|---|---|
| ca23481 | 88120 | 30679 | 18 |
| round 1 | 103807 | 30679 | 18 |
| 04b79d1 | 88120 | 30679 | 18 |

The two dumped state sets are equal, after the four new message fields
with their default values (ptr 0, base <<>>, results <<>>, idx 0) are
removed. The difference of round 1 was only how often TLC generated a
state. The cause: InFlightOk and StopSignAllowed were disjunctions. TLC
splits a disjunction in an action into branches. With ExecuteVerify
FALSE, both disjuncts were true for a follower, so Propose produced each
successor twice (coverage of ForwardProposals: 14220 before, 28440
after). The chan rewrite was not the cause.

## Load

From 15:42 to 15:45 about 5 TLC processes ran at the same time. The
limit is 3. The cause: the queue of an xargs process started new runs
after its parent shell was stopped. The runs of that time are not in
the tables above. Every run in the tables ran with at most 3 TLC
processes.

## Not rerun after the fixes

Nothing. Every Execute-Verify config above ran on 04b79d1.
