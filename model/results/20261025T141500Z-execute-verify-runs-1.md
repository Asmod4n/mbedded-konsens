# Execute-Verify: simulation runs, round 1

Model: uncommitted changes on top of a9e5458 (the model part of the work
is in 7905e11; the changes after it add plan 2.8.1 as of a9e5458 and the
channel form below). Plan: doc/plan-execute-verify.md as of a9e5458.
TLC 2.19 (rev 5a47802), OpenJDK 21.0.12.1, 4 processors, at most 3 TLC
processes at the same time.

Command of each run (in model/tla):

    timeout 900 java -XX:+UseParallelGC -Xmx3g -cp tla2tools.jar tlc2.TLC \
      -workers 1 -deadlock -simulate -depth 100 -seed 7 \
      -metadir <dir> -config <config>.sim.cfg SequencePaxos.tla

Depth 100, seed 7, one worker, limit 15 min. Exit 124 is the end at the
limit; exit 12 is a violation. States is the last progress line.
Runs started 2026-10-25 12:11 UTC, the extra NoReject config at 14:05 UTC.

## Configurations of section 5

Common to all: Values {v1, v2, v3}, MaxBallot 2, Cap 3, MaxCounter 12,
MaxSession 4, Recovery, StateInLmdb, RepairFromPeer, StartDamageCounts,
MaxInFlight 2, MaxResultThrow 2, ExecuteVerify.

| Config | Faults | Limit | States | Result |
|---|---|---|---|---|
| MC_ev_wrong | 3 nodes, MaxLog 4, MaxWrong 1, MaxElection 1 | 15 min | 2700788 | no violation |
| MC_ev_flip | as wrong, MaxWrong 0, Flip, MaxFlip 1 | 15 min | 2654347 | no violation |
| MC_ev_flip_change | as flip, MaxElection 3 | 15 min | 3091267 | no violation |
| MC_ev_change | MaxLog 5, MaxWrong 1, MaxElection 3 | 15 min | 2373749 | no violation |
| MC_ev_loss | MaxWrong 1, Lose 1, Duplicate 1, 1 crash, MaxElection 2, MaxTimeout 2 | 15 min | 6024235 | no violation |
| MC_ev_spread | MaxLog 3, Spread, MaxWrong 0 | 15 min | 4105946 | no violation |
| MC_ev_sessions | MaxLog 6, ClientSessions, MaxClientSend 4, MaxRequest 2, MaxWrong 1 | 15 min | 1569778 | no violation |
| MC_ev_five | 5 nodes, MaxLog 4, MaxWrong 2 | 15 min | 1163691 | no violation |

Invariants of each: S1 S2 S3 S4 S5 HighWaterHolds AcceptedIdxIsLogLen
EntryRunsOnOnePrefix StateIsDecided ChosenByMajority ChosenIsRight
ChosenOnOwnBase StatesAgree ThrowOnlyWrong RepairsBounded
VerdictCoversRequest; MC_ev_sessions also E1. Property: S2Grows.

## Covers of 4.12

Each config checks only its cover.

| Cover | Config base | States | Result |
|---|---|---|---|
| NoResults | wrong | 967 | violated, trace of 52 states |
| NoReject | spread | 5085353 | not reached in 15 min |
| NoReject (MC_ev_cover_NoReject_wrong) | MaxLog 4, Spread, MaxWrong 1 | 1840 | violated, trace of 52 states |
| NoFollowerThrow | wrong | 1991 | violated, trace of 50 states |
| NoLeaderThrow | flip | 697 | violated, trace of 36 states |
| NoResultsAfterLeaderChange | change | 1658 | violated, trace of 48 states |
| NoStaleVerdictEntry | change | 2902762 | not reached in 15 min |
| NoRunAgain | wrong | 975 | violated, trace of 66 states |
| NoTryAgain | wrong | 860 | violated, trace of 72 states |
| NoIdleRead | wrong, ReadIndex, MaxRead 1 | 7917 | violated, trace of 78 states |

MC_ev_spread cannot reach NoReject. Its 3 nodes all work, and the
jitters are 0, 1 or 2. Of three jitters, either two are equal, or the
three are 0, 1 and 2, and 1 agrees with both others. So a majority
always agrees with one result, and the leader never appends rejected.

## Mutations of 4.13

Each config checks only the target invariant.

| Mutation | Config base | Target | States | Result |
|---|---|---|---|---|
| ApplyOwnResult | wrong | StatesAgree | 2276 | violated, 42 states |
| CountOtherBase | wrong | ChosenOnOwnBase | 42747 | violated, 48 states |
| SelectWithoutMajority | wrong | ChosenByMajority | 61647 | violated, 41 states |
| AcceptWithoutCheck | flip | ChosenByMajority | 719 | violated, 34 states |
| ThrowWithoutRerun | flip | ThrowOnlyWrong | 3421 | violated, 55 states |
| CountProofTwice | wrong | RepairsBounded | 3305478 | not reached in 15 min |
| VerdictWithoutRequest | wrong | VerdictCoversRequest | 5181 | violated, 53 states |
| ListedSkipsCheck | flip | ThrowOnlyWrong | 3290998 | not reached in 15 min |
| OwnAcceptUnchecked | flip_change | ThrowOnlyWrong | 3871128 | not reached in 15 min |
| StaleVerdictTaken | change | ChosenOnOwnBase | 2873537 | not reached in 15 min |
| RejectedUnchecked | spread | ChosenByMajority RejectedWithoutMajority | 5064450 | not reached in 15 min |
| AppliedIsPointer | wrong, reads | NoIdleRead (must hold) | 3093995 | NoIdleRead holds, as 4.13 expects |

ThrowWithoutRerun replaces ThrowWithoutList (plan 2.8.1 as of a9e5458).

## Runs outside the tree

These runs used configs and a probe module in the scratch directory,
with the same command.

- CountProofTwice with MaxLog 2: 9268850 states in 15 min, not reached.
  A probe cover on the CountProofTwice config was violated after 63
  states: the node that took a listed throw has recovered and is listed
  again. A BFS from that state (level below 7) violates RepairsBounded
  in one step: repairs 2, one proof. So the mutation breaks its target,
  but the 15-minute simulation does not reach it.
- RejectedUnchecked with SpreadAction "reject" and MaxLog 4: violates
  RejectedWithoutMajority after 62573 states. The same config with
  Mutation "none" also violates it after 62897 states. So the result
  does not show the mutation: with SpreadAction "reject", a rejected
  entry with a majority result is the design of that option.

## Configurations without Execute-Verify

ExecuteVerify = FALSE, BFS, -workers 1 -deadlock, before (ca23481) and
after (this model):

| Config | Before | After |
|---|---|---|
| MC_base, MaxLog 1, MaxElection 1, VIEW (scratch) | 30679 distinct, depth 18, no violation | 30679 distinct, depth 18, no violation |
| MC_mut_S3 | S3 violated, 98026 distinct, depth 9 | S3 violated, 98026 distinct, depth 9 |

The count of generated states differs (88120 before, 103807 after for
the first; 262243 and 320470 for the second). The distinct states and
the depth are equal. The cause of the difference is not known.

## TLC 2.19 and EXCEPT on the channel

The simulation of MC_ev_loss stopped with an ArrayIndexOutOfBounds
exception in tlc2.value.impl.ValueExcept.current, called from
FcnLambdaValue.apply, in the evaluation of chan[<<j, i>>] in Receive.
The model at ca23481 with the same faults stops in the same way. The
model now builds the channel as a function over Pairs in Receive,
ReceiveEV, Lose and Duplicate, in place of [chan EXCEPT ![p] = ...].
With that form the loss configs run to the limit. The BFS counts above
are equal with that form.
