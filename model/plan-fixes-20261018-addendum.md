# Addendum to plan-fixes-20261018: one direct property per guard

Source: the counter-checks after the fixes (2026-10-18). Two mutations
did not break their property in a breadth-first search of 21 M states
and in a simulation of 15 min. The property of each checks a
consequence far from the guard, and the bounds rarely reach that
consequence.

## 1. Rule

Each guard has a direct action property that names the guard. Its
mutation must break it within a simulation of 15 min. A property that
checks a far consequence, such as NoTwoWriters, stays as a second check.

## 2. Direct properties

This version applies the review `model/review-plan-fixes-20261018-addendum.md`
(approve with changes). The mutations as built differ from the names of
the main plan in three places; the table follows the built mutations.

| Mutation | Mutated place | Direct property |
|---|---|---|
| ForceWithoutStage1 | `read_only[i]` in ForceSingle | ForceOnlyFromStage1 == `[][force_single.node = 0 /\ force_single'.node # 0 => read_only[force_single'.node]]_vars` |
| ForceAtTwoNodes | LeaveReadOnly after stage 2 | ReadOnlyEndsBeforeForce == `[][\A i \in Nodes : read_only[i] /\ ~read_only'[i] => force_single.node = 0]_vars`, with MaxReadOnly 2 |
| DeactivatedStillVotes | `~deactivated[i]` at Receive and voting | DeactivatedNodeIsSilent and DeactivatedKeepsDurable (durable state of a deactivated node changes only by wipe or rollback), with MaxCrash 1 and MaxBallot 1 |
| BuildFromRecovering | `j \in running /\ ~recovering[j]` in BuildForServer | BuildOnlyFromRunning, over a new ghost `build_source`, which only BuildForServer sets |
| ReadBeforeServing | the unchanged-check of ReadReply | ReplyOnlyIfUnchanged: a read that goes from indexed to done has the crash count, rollback count and ballot of its index step, with MaxCrash 1 |
| StopSignLost | `deactivated` cleared on wipe | DeactivatedStays (already in place). The item on a lost StopSign is dropped: one state cannot record a future resend. |

Only BuildOnlyFromRunning needs a ghost field. The ghost goes into
`reconfig`, `vars`, the view and every explicit UNCHANGED list.

The guard `SingleNodeServes` in ReadStart has no mutation yet. It gets
one: ReadStartBeforeServing, with SingleNode TRUE.

A direct property that restates its guard shows that the step without
the guard is reachable. It does not show that the guard is needed.
ForceWithoutStage1 also runs against every safety property of the d
cfgs. If none breaks, the owner decides if the guard stays.

Each new property also enters the unmutated d cfgs.

## 3. Runs

- Each mutation: `-simulate -deadlock -depth 60`, 15 min.
- The unmutated model: the same simulation with all direct properties,
  then a `-coverage 1` run that shows each guarded action fires.

## 4. What this addendum does not do

- No change to a guard or to the protocol.
