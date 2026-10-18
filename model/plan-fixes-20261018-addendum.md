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

| Mutation | Guard | Direct property |
|---|---|---|
| ForceWithoutStage1 | `read_only[i]` in ForceSingle | ForceOnlyFromStage1: a step that sets `force_single.node` to i starts with `read_only[i]`. |
| ForceAtTwoNodes | `force_single.node = 0` | ForceAtOneNode: `force_single.node` changes at most once from 0. |
| DeactivatedStillVotes | `~deactivated[i]` in the voting actions | DeactivatedNodeIsSilent, with MaxBallot 2 so that a ballot can start after stage 2. |
| BuildFromRecovering | `Votes(j) /\ ~recovering[j]` in BuildForServer | BuildOnlyFromVoter: a step that builds node s from j starts with `Votes(j) /\ ~recovering[j]`. |
| ReadBeforeServing | `SingleNodeServes` in ReadStart | ReadOnlyWhenServing: a step that starts a read at the single node starts with `SingleNodeServes(i)`. |
| StopSignLost | resend of a buffered StopSign | StopSignReachesNewConfig: liveness is too slow here. In its place, a ghost counter of StopSigns lost without a later resend, with an invariant that it is 0 when a new configuration decides. |

Each direct property needs a ghost field that records which action ran
and with which arguments, when the action cannot be read from the
changed variables.

## 3. Runs

- Each mutation: simulation, `-deadlock -depth 60`, 15 min.
- The unmutated model: the same simulation with all direct properties,
  then a coverage run that shows each guarded action fires.

## 4. What this addendum does not do

- No change to a guard or to the protocol.
