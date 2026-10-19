# Notes: the lease at the witness in the model

Source: `model/plan-witness-lease.md` at commit 8b7cafd, the reviews
`model/review-plan-witness-lease.md` and
`model/review2-plan-witness-lease.md` with its check of version 3.
Base: `model/tla` at commit 5ea2ff9. `SequencePaxos.base.tla` is that
base, not changed.

## What was built

`SequencePaxos.tla`:

- Constants `Witness` (Bool) and `LeaseLength` (Int), each with its
  `@type` line and an ASSUME.
- Variables `witness_epoch`, `node_epoch` (domain `Servers`),
  `lease_request`, `lease_reply`, `lease_remaining` (domain `Nodes`),
  `wait_remaining`. Ghosts `forced_decided_at_force`,
  `forced_has_decided`, `old_decided_max`, `old_decided_after_forced`,
  and the field `forced_decided` of the read record (type alias `read`).
  The tuples `leasevars` and `leaseghost` are in `vars` and in `View`.
- `LeaseValid(i) == i \notin Nodes \/ ~Witness \/ lease_remaining[i] > 0`.
- `LeaseGhostUpdate`, conjoined to `GhostUpdate`. With `Witness` FALSE
  it keeps each ghost unchanged.
- New actions: `Tick`, `LeaseRequest`, `LeaseGrant`, `LeaseReply`,
  `LeaderDecidesChosen`, `WitnessRestart` (only with the mutation
  WitnessForgetsEpoch). Each requires `Witness`.
- Changed: the IF condition of the decide branch of HandleAccepted
  (form b), the wait in LeaderCountsOwnPromise and
  LeaderCountsOwnAccepted, the wait in ReadStart at a node of
  NextConfigNodes, the lease in StartReadRound, ForceSingle (no majority
  guard with `Witness`, raise of the epoch, `node_epoch[s]`, the wait),
  Restart (drops the lease, starts the wait again at node 6).
- Each action that has no GhostUpdate lists `leaseghost` in UNCHANGED.
  Each action lists `leasevars` in UNCHANGED or sets each of its parts.
- The eight mutations of section 5 are in the ASSUME of `Mutation`.
- Properties: `LeaseTypeInvariant`, LeaseHonoured, WaitHonoured,
  GrantOnlyCurrentEpoch, EpochFromConfiguration,
  `EpochFromConfigurationAction`, ForceRaisesEpoch, WitnessEpochGrows,
  LeaseWithinGrant, ReadRoundHonoursLease, LeasesDisjoint,
  OldDecidedFrozen, NoConcurrentWriters, StaleReadAfterForce.
- Reach invariants for MC_w_reach: `OldDecidesNothingDuringWait`,
  `ForcedAgreesWithOld`, `NoRefusalAfterForce`.

Cfgs:

- Each existing `MC_*.cfg` has `Witness = FALSE` and `LeaseLength = 0`.
  `APA_F6c.tla` has `OVERRIDE_Witness` and `OVERRIDE_LeaseLength`.
  `APA_F6c.cfg` has no constants and did not change.
- `MC_w.cfg`: MC_d2 with the changes of section 6. `VIEW View`, no
  SYMMETRY (MC_d2 has none). Its own INVARIANTS and PROPERTIES.
- `MC_w_crash.cfg` (MaxCrash 1), `MC_w_reconf.cfg` (MaxReconfigure 1),
  `MC_w_reach.cfg`, `MC_w_loss.cfg` (NoTwoWriters only).
- `MC_mut_<Name>.cfg` for each of the eight mutations, on the constants
  of MC_w, with the direct property and the far property.
- Regression: `MC_d1_log1.cfg`, `MC_d2_log1.cfg` (MaxLog 1), and the arms
  for the base `MC_d1_log1.base.cfg`, `MC_d2_log1.base.cfg`,
  `MC_d1_reach.base.cfg`. The other lines of each pair are the same.

`check_constants.py`: a cfg with the suffix `.base.cfg` is checked
against the constants of `SequencePaxos.base.tla`. Each other cfg is
checked against `SequencePaxos.tla`, as before.

## How to run the base arm

SANY refuses `SequencePaxos.base.tla`: the file name does not match the
module name `SequencePaxos`. To run the base arm, copy it as
`SequencePaxos.tla` into its own directory, with the `.base.cfg` files.

## Provisional forms

Each one is marked "Provisional" in a TLA comment where it is code.

1. AdoptEpochFromRefusal acts in LeaseReply, at the node that takes the
   refusal. The trace of review 2, finding 6, names LeaseGrant. The node
   adopts the epoch, so the node-side step is the place. Both places
   break EpochFromConfiguration.
2. Wipe drops the volatile lease of the node and starts the wait again
   at node 6, as Restart does. The plan names only Restart. `node_epoch`
   stays after a wipe, because it comes from the configuration.
3. MC_w_reach reads the existing ghosts and state, with no new ghost
   counters. The three invariants are in one cfg. TLC stops at the first
   violation. So the run needs `-continue`, or one cfg per invariant.
4. LeaseHonoured finds the decide steps by state: a node of `Nodes` in
   (Leader, Accept) before and after the step, whose `decided_idx`
   rises. Only the decide branch of HandleAccepted and
   LeaderDecidesChosen do that.
5. ReadRoundHonoursLease finds StartReadRound by a change of
   `read_round[i].id`. Only StartReadRound changes it.
6. WaitHonoured requires a rise of `decided_idx[s]` and a value above
   `forced_decided_at_force`, as section 4 says "raises ... above".
7. LeaderDecidesChosen also requires `~recovering[i]`. A recovering node
   is a follower, so the guard removes no behaviour.
8. LeaseGrant is enabled for each node of `Nodes`, also a halted node:
   the witness acts alone. LeaseReply needs a running node. A reply to a
   halted node stays until Resume, and Restart drops it.
9. With ForceWithoutRaise, ForceSingle still writes
   `node_epoch[s] = witness_epoch + 1`. Only the raise at the witness is
   missing.
10. MC_w also leaves out EachReadSeesDecided and StaleReadIsMarked of
    MC_d2. Review 1, finding 5: an old decision during the wait extends
    `decided_history`, and a read at node 6 then breaks both with no
    mutation.
11. MC_mut_AdoptEpochFromRefusal also lists EpochFromConfigurationAction.
    The mutation does not break it. It does no harm.

## Smoke tests in this session

All with TLC 2.19, `-workers 1`, one run at a time, 180 s at most.

The logs are in `logs/`. These are smoke tests, not the runs of
section 6.

- SANY parses `SequencePaxos.tla` with no error.
- `python3 -I check_constants.py .`: exit 0, 32 constants, 66 cfgs.
- MC_w, simulation, `-deadlock -depth 40`, 180 s: 1,749,520 states
  checked. No violation, no type error, no TLC error.
- MC_w_reach, breadth-first, `-deadlock -continue`, 180 s: 588 reports
  of a violation of NoRefusalAfterForce. The first is at state 8:
  LeaseGrant refuses after ForceSingle. At the end: 819,492 distinct
  states, level 9. OldDecidesNothingDuringWait and ForcedAgreesWithOld
  did not break in this time. Their traces are deeper than level 9.
  This is not checked.
- MC_mut_ForcedWithoutWait, simulation, 180 s: WaitHonoured breaks at
  once. After it, TLC threw an ArrayIndexOutOfBoundsException when it
  printed the last state. A breadth-first run of a smaller copy
  (ReadIndex FALSE, MaxRead 0, MaxElection 2, WaitHonoured only) gave
  the full trace in 7 states: ForceSingle, HandleLeader at 6, Propose at
  6, LeaderCountsOwnPromise, LeaderCountsOwnAccepted with
  `wait_remaining = 2`. 46,664 distinct states.
- Regression, MC_d1_reach, breadth-first, both arms: SingleNodeDecidesNothing
  breaks in both, trace of 12 states, 1,037,502 states generated and
  372,826 distinct states in both arms.
- Regression, MC_d1_log1 and MC_d2_log1: neither finishes breadth-first
  in 180 s. A copy with the constraint `Bounds /\ TLCGet("level") < 9`
  did not finish in 180 s either. MC_d1_log1 showed 454,537 distinct
  states in both arms at the last progress line. That run did not
  finish, so it is no proof. MC_d2_log1 cannot be compared. Both need
  the full runs of section 6.

## Open questions for the owner

1. The wait in ReadStart at node 6 is a new guard with no mutation.
   Proposal: a mutation ReadDuringWait, direct property "a read at node 6
   starts with `wait_remaining <= 0`", far property R1New.
2. AdoptEpochFromRefusal: LeaseReply (built) or LeaseGrant (the trace of
   review 2)? Proposal: LeaseReply.
3. Wipe: drop the lease and start the wait again (built), or leave the
   lease to Restart only? Proposal: as built.
4. MC_w_reach: one cfg run with `-continue` (built), or one cfg for each
   of the three invariants? Proposal: three cfgs, because the refusal is
   at depth 8 and with `-continue` TLC writes each violation of it.
5. MC_w leaves out EachReadSeesDecided and StaleReadIsMarked (item 10
   above). Proposal: keep them out of MC_w and add them to MC_w_loss.
6. `APA_F6c.tla` holds `OVERRIDE_MaxHalt` and `OVERRIDE_StaleReadNode`
   twice. That is in the base. It was not changed.
7. Names with no source, for the owner: `leasevars`, `leaseghost`,
   `LeaseValid`, `LeaseGhostUpdate`, `LeaseTypeInvariant`,
   `EpochFromConfigurationAction`, `WitnessRestart`,
   `OldDecidesNothingDuringWait`, `ForcedAgreesWithOld`,
   `NoRefusalAfterForce`, and the names of section 8 of the plan.
