# Plan: the lease at the witness

Source: the decision of the owner of 2026-10-18 ("Lease, selten") and
the state file of the project. This version applies the review
`model/review-plan-witness-lease.md` (reject, 18 findings) and the
changes 1 to 9 of `model/review2-plan-witness-lease.md` (approve with
changes). Notes 10 to 16 of the second review are in sections 2, 7
and 8.

Base: `model/tla/SequencePaxos.tla` at commit 5ea2ff9 (the model after
plan-fixes-20261018 and its addendum). The regression counts come from
the result files named in section 6.

## 1. The fault that the lease removes

ForceSingle (stage 2) has the guard
`Cardinality(Nodes \ running) >= Majority(i)`. The guard trusts the
operator: the old majority is down. In a partition the old majority is
alive. It keeps its leader, decides new entries and answers reads. The
forced node decides too. Two histories grow at the same time.

The lease does not remove the loss of stage 2. An old leader can decide
until its lease ends, and the forced node does not know these entries.
The lease limits the loss to the decisions in the rest of one lease.
It removes the overlap: no old decision and no decision of the forced
node happen at the same time.

## 2. The form of the lease

- The witness holds one durable value: `epoch`. It changes the epoch
  only by compare-and-set. The witness keeps no expiry, and its clock
  is not part of the safety argument.
- The epoch of a node comes from its configuration: 0 for an old node,
  the raised epoch for the forced node. It never comes from a reply of
  the witness.
- A leader of the old cluster reads its own clock, then sends a request
  for a lease with its epoch. Each request carries an id and the boot
  id of the node. The witness grants it only if that epoch is its own
  epoch. The lease ends at the time of the request plus `LeaseLength`.
  A reply sets the lease from the time of its own request, found by
  the id. A reply of another boot is dropped. On a refusal the leader
  drops its lease at once.
- A leader decides an entry only while its lease is valid by its own
  clock. A leader starts a read round only while its lease is valid.
  A leader that gets a lease decides each index that is chosen and not
  yet decided.
- Provisional, card to the owner: a follower checks no lease. Only a
  leader decides. The model checks this claim with OldDecidedFrozen.
  The state file records the earlier form "each node checks an
  Accept". Note: a check at each node also stops a leader that was
  paused between its check and its effect (review 2, note 11).
- The clock is CLOCK_BOOTTIME on Linux, because CLOCK_MONOTONIC stops
  during suspend (clock_gettime(2)). Provisional.
- ForceSingle: the forced node stores its new epoch, then raises the
  epoch at the witness by compare-and-set. A retry of the raise accepts
  the epoch that it wanted. From then on the witness refuses every
  request of the old epoch.
- The forced node waits a full `LeaseLength` from the reply to the
  raise. During the wait it decides nothing and serves no read. After a
  boot during the wait it starts a full wait again. Provisional.
- The lease of a node is volatile. A restart drops it.
- The drift factor: the forced node waits `LeaseLength` times the
  factor. The factor is applied at the side that waits, in one place.
  It covers the drift of both clocks and the longest pause between a
  check and its effect. It is a field of the configuration, not
  measured.

## 3. The model

Constants: `Witness` (Bool) and `LeaseLength` (Int). Each cfg gets both,
also each MC_mut cfg; `check_constants.py` checks this. With `Witness`
FALSE every new action is disabled, every new variable keeps its
initial value, and each changed guard in an existing action is TRUE.

Time is kept as remaining durations, with no absolute time. `Tick`
requires `Witness` and one positive duration, and lowers each positive
duration by one.

Variables (each with its `@type` line, in `vars`, the view and every
explicit UNCHANGED list; the APA wrapper gets the new OVERRIDE names):

- `witness_epoch` (Int, durable at the witness).
- `node_epoch` (per node, durable).
- `lease_request` (per node, `-1` or `0..LeaseLength`: the time that
  remains, measured from the request).
- `lease_reply` (per node, `-1` none, `0` refused, `1` granted).
- `lease_remaining` (per node, volatile, `0..LeaseLength`).
- `wait_remaining` (`-1` before ForceSingle, then `0..LeaseLength`).
- Ghosts: `forced_decided_at_force`, `forced_has_decided` (Bool),
  `old_decided_max` (Int), `old_decided_after_forced` (Bool), and the
  field `forced_decided` in the read record.

Ghost updates: one definition `LeaseGhostUpdate`, conjoined to
GhostUpdate. The actions without GhostUpdate (ReadStart,
StartReadRound, ReadReply, SendDeactivated, Halt) list these ghosts in
UNCHANGED.

- `old_decided_max' = Max(old_decided_max, max of decided_idx'[k] for k \in Nodes)`:
  the highest value an old node ever held.
- `forced_has_decided' = forced_has_decided \/ decided_idx'[s] > forced_decided_at_force`.
- `old_decided_after_forced' = old_decided_after_forced \/ (forced_has_decided /\ old_decided_max' > old_decided_max)`.

Actions:

- `LeaseRequest(i)`: requires `Witness`, a leader `i \in Nodes` in
  Accept, `lease_request[i] = -1`. Sets `lease_request[i]` to
  `LeaseLength`.
- `LeaseGrant(i)`: the witness. Requires `lease_request[i] >= 0` and
  `lease_reply[i] = -1`. Sets `lease_reply[i]` to 1 if
  `node_epoch[i] = witness_epoch`, else 0.
- `LeaseReply(i)`: the node. Requires `lease_reply[i] >= 0`. On 1,
  `lease_remaining[i]` gets `lease_request[i]` (Tick lowered it in
  between). On 0, `lease_remaining[i]` becomes 0. Then
  `lease_request[i]` and `lease_reply[i]` become -1.
- `Tick`: lowers `lease_request`, `lease_remaining` and
  `wait_remaining` where they are positive.
- Changed: the IF condition of the decide branch of HandleAccepted
  (form b of review 2, finding 7) gets
  `i \notin Nodes \/ ~Witness \/ lease_remaining[i] > 0`. Without it
  the leader records `accepted_indexes` and decides nothing.
- New: `LeaderDecidesChosen(i)` for `i \in Nodes`, in the form of
  LeaderCountsOwnAccepted: a leader in Accept with a valid lease
  decides the highest chosen index above `decided_idx[i]`.
- Changed: LeaderCountsOwnPromise and LeaderCountsOwnAccepted get
  `wait_remaining <= 0`. ReadStart at a node of NextConfigNodes gets
  `wait_remaining <= 0`. StartReadRound for `i \in Nodes` gets
  `~Witness \/ lease_remaining[i] > 0`.
- Changed: ForceSingle with `Witness` TRUE has no majority guard,
  raises `witness_epoch`, writes `node_epoch[s]`, sets
  `wait_remaining` to `LeaseLength` and `forced_decided_at_force` to
  `decided_idx'[s]`. With `Witness` FALSE it is unchanged.
- Changed: Restart drops `lease_remaining`, `lease_request` and
  `lease_reply` of the node. A restart of node 6 with
  `wait_remaining > 0` sets it to `LeaseLength`.
- No action for a restart of the witness: its only state is durable.
  Mutation WitnessForgetsEpoch adds that action.

New mutation names enter the ASSUME of `Mutation`. A type invariant
holds the ranges of the new variables.

## 4. Properties

- LeaseHonoured (action): each step of HandleAccepted or
  LeaderDecidesChosen that raises `decided_idx[i]` at `i \in Nodes`
  starts with `lease_remaining[i] > 0`.
- WaitHonoured (action): each step that raises `decided_idx[s]` above
  `forced_decided_at_force` starts with `wait_remaining = 0`.
- GrantOnlyCurrentEpoch (action): a step that sets `lease_reply[i]` to
  1 starts with `node_epoch[i] = witness_epoch`.
- EpochFromConfiguration (invariant): `\A i \in Nodes : node_epoch[i] = 0`.
  Action part: `node_epoch[s]` for `s \in NextConfigNodes` changes only
  in ForceSingle.
- ForceRaisesEpoch == `[][force_single.node = 0 /\ force_single'.node # 0 => witness_epoch' > witness_epoch]_vars`.
- WitnessEpochGrows == `[][witness_epoch' >= witness_epoch]_vars`.
- LeaseWithinGrant (action): the step that sets `lease_remaining'[i]`
  from a reply gives `lease_remaining'[i] <= lease_request[i]`.
- ReadRoundHonoursLease (action): StartReadRound at `i \in Nodes` starts
  with `lease_remaining[i] > 0`.
- LeasesDisjoint (invariant):
  `force_single.node # 0 /\ wait_remaining = 0 => \A i \in Nodes : lease_remaining[i] = 0`.
- OldDecidedFrozen == `[][wait_remaining = 0 => old_decided_max' = old_decided_max]_vars`.
  It checks the claim that a follower needs no lease.
- NoConcurrentWriters (invariant): `~old_decided_after_forced`.
- StaleReadAfterForce (invariant): no read at an old node with
  `forced_decided` TRUE and `~stale` reaches "done".

NoTwoWriters, S1Force, S2Force and R1Force break in MC_w by the loss of
section 1. MC_w does not list them. MC_w_loss lists NoTwoWriters and
must find its violation, in the form of MC_d1_reach.

## 5. Mutations

Each mutation must break its direct property in a simulation of 15 min.
The far property is a second check.

| Mutation | Mutated place | Direct property | Far property |
|---|---|---|---|
| DecideWithoutLease | lease check in HandleAccepted | LeaseHonoured | NoConcurrentWriters |
| ForcedWithoutWait | wait in LeaderCountsOwnPromise/Accepted | WaitHonoured | NoConcurrentWriters |
| WitnessGrantsAnyEpoch | epoch check in LeaseGrant | GrantOnlyCurrentEpoch | NoConcurrentWriters |
| ForceWithoutRaise | the raise in ForceSingle | ForceRaisesEpoch | NoConcurrentWriters |
| WitnessForgetsEpoch | a witness restart sets the epoch to 0 | WitnessEpochGrows | NoConcurrentWriters |
| LeaseFromReply | LeaseReply sets `LeaseLength`, not `lease_request[i]` | LeaseWithinGrant | LeasesDisjoint |
| AdoptEpochFromRefusal | a refusal writes `witness_epoch` into `node_epoch[i]` | EpochFromConfiguration | NoConcurrentWriters |
| ReadWithoutLease | lease check in StartReadRound | ReadRoundHonoursLease | StaleReadAfterForce |

Trace for ForcedWithoutWait: LeaseRequest, LeaseGrant, LeaseReply
(lease 2), ForceSingle (wait 2), node 6 decides the undecided suffix of
`first_log`, the old leader decides index 1.

## 6. Runs

- Regression: MC_d1_reach (its violation and the length of its trace),
  and copies of MC_d1 and MC_d2 at MaxLog 1 that finish breadth-first.
  Each cfg runs on 5ea2ff9 and on the new model with `Witness = FALSE`,
  one after the other. The distinct states must be equal.
  base_monotone, e and f stay as they are. The Apalache type check of
  APA_F6c runs, because the type aliases change.
- MC_w: as MC_d2, with these changes: `Witness = TRUE`,
  `LeaseLength = 2`, MaxLog 1, MaxCrash 0, MaxWipe 0, MaxLoss 0,
  MaxRollback 0, MaxHalt 0, MaxElection 3, MaxReconfigure 0. Then a
  copy with MaxCrash 1, and a copy with MaxReconfigure 1.
  Breadth-first 1 h with `-deadlock -coverage 1`, then a simulation of
  15 min. Coverage must show the decide branch of HandleAccepted,
  LeaderDecidesChosen and the decisions of node 6 above zero. The
  growth of the state space is measured here.
- MC_w_reach: as MC_w, with ghost counters as invariants that must
  break: an old decision during the wait; node 6 decides v1 after the
  wait while node 2 decided v2 at index 1 (node 1 accepts v1 alone as
  leader, enters stage 1, node 2 decides v2 under its lease); a refused
  request after the force.
- MC_w_loss: as MC_w, with NoTwoWriters only. It must break.
- Each mutation: simulation, 15 min, on the cfg of MC_w.
- One run at a time with 3 workers.

## 7. What this plan does not do

- No drift and no pause in the model. The drift factor acts at the side
  that waits. A pause longer than the factor covers breaks the lease.
  This is the known limit of a lease without a fencing token (Burrows,
  "The Chubby lock service", OSDI 2006, sequencers).
- No limit of requests at the witness in the model.
- No view of each node at the witness. It is for the operator.
- No change to stage 1.
- One request per node in the model. The rule of the request id is in
  section 2.
- A witness that has lost its epoch is outside the fault model. The
  answers of the witness are authenticated, so that no other host can
  give a grant.
- After the wait, node 6 decides with no lease. That holds for one
  force.

## 8. Names for the owner

| Name | Source |
|---|---|
| lease | Gray and Cheriton, SOSP 1989 |
| epoch | Chubby, ZooKeeper |
| witness | Paris, 1986 |
| Tick, LeaseRequest, LeaseGrant, LeaseReply, LeaderDecidesChosen | no source, for the owner |
| LeaseHonoured, WaitHonoured, GrantOnlyCurrentEpoch, EpochFromConfiguration, ForceRaisesEpoch, WitnessEpochGrows, LeaseWithinGrant, ReadRoundHonoursLease, LeasesDisjoint, OldDecidedFrozen, NoConcurrentWriters, StaleReadAfterForce | no source, for the owner |
| the mutation names of section 5 | no source, for the owner |
