# Plan: the lease at the witness

Source: the decision of the owner of 2026-10-18 ("Lease, selten") and
the state file of the project. This version applies the review
`model/review-plan-witness-lease.md` (reject, 18 findings). It needs a
second review before anything is built.

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
  only by compare-and-set. The witness keeps no expiry and its clock
  is not part of the safety argument (review 13).
- The epoch of a node comes from its configuration: 0 for an old node,
  the raised epoch for the forced node, stored durably at ForceSingle.
  It never comes from a reply of the witness (review 16).
- A leader of the old cluster reads its own clock, then sends a request
  for a lease with its epoch. The witness grants it only if that epoch
  is its own epoch. The lease of the leader ends at the time of the
  request plus `LeaseLength`. On a refusal the leader drops its lease at
  once.
- A leader decides an entry only while its lease is valid by its own
  clock. A leader starts a read round only while its lease is valid.
- Provisional, card to the owner: a follower checks no lease. Only a
  leader decides. The model checks this claim with OldDecidedFrozen
  (review 1, 15). The state file records the earlier form "each node
  checks an Accept", and the owner decides between the two.
- ForceSingle raises the epoch at the witness. From then on the witness
  refuses every request of the old epoch.
- The forced node waits a full `LeaseLength` from the reply to the
  raise. During the wait it decides nothing and serves no read
  (review 3, provisional).
- The lease of a node is volatile. A restart drops it, because a
  monotonic clock starts again at boot.
- The drift of the clock rates: the forced node waits `LeaseLength`
  times the drift factor. The factor is applied at the side that waits,
  in one place. It is a field of the configuration, not measured
  (review 14).

## 3. The model

Constants: `Witness` (Bool) and `LeaseLength` (Int). With `Witness`
FALSE every new action is disabled and every new variable keeps its
initial value (review 18). Each cfg gets both constants, also each
MC_mut cfg; `check_constants.py` checks this.

Time is kept as remaining durations, with no absolute time (review 11).
`Tick` lowers each positive duration by one.

Variables (each with its `@type` line, in `vars`, the view and every
explicit UNCHANGED list; the APA wrapper gets the new OVERRIDE names):

- `witness_epoch` (Int, durable at the witness).
- `node_epoch` (per node, durable).
- `lease_request` (per node: `none`, or the remaining time measured
  from the request).
- `lease_remaining` (per node, volatile, `0..LeaseLength`).
- `wait_remaining` (`-1` before ForceSingle, then `0..LeaseLength`).
- Ghosts: `forced_decided_at_force` (decided_idx of the forced node at
  ForceSingle), `forced_has_decided` (Bool), `old_decided_after_wait`
  (Bool), `old_decided_max` (Int), and the field `forced_decided` in
  the read record.

Actions:

- `LeaseRequest(i)`: leader `i \in Nodes`, sets `lease_request[i]` to
  `LeaseLength`. Tick also lowers it.
- `LeaseGrant(i)`: the witness checks `node_epoch[i] = witness_epoch`.
  If it holds, `lease_remaining[i]` gets the value of
  `lease_request[i]`. If not, `lease_remaining[i]` becomes 0.
  `lease_request[i]` becomes `none`. One step for grant and reply is
  sound, because the time is taken from the request (review 6).
- `Tick`.
- Changed: the decide branch of HandleAccepted for `i \in Nodes`
  requires `lease_remaining[i] > 0` (review 2).
  LeaderCountsOwnPromise and LeaderCountsOwnAccepted for
  `i \in NextConfigNodes` require `wait_remaining = 0`. Each rise of
  `decided_idx[s]` above `forced_decided_at_force` counts as a decision
  of the forced node (review 3). StartReadRound for `i \in Nodes`
  requires `lease_remaining[i] > 0` (review 4). ReadStart at the forced
  node requires `wait_remaining = 0`. ForceSingle: with `Witness` TRUE
  it has no majority guard, raises `witness_epoch`, writes
  `node_epoch[s]` and sets `wait_remaining` to `LeaseLength`. Restart
  sets `lease_remaining` and `lease_request` of the node to their
  initial values.
- No action for a restart of the witness: its only state is durable and
  a restart changes nothing (review 13). Mutation WitnessForgetsEpoch
  adds that action.

New mutation names enter the ASSUME of `Mutation`.

## 4. Properties

Provisional names, from the sources in review 17 where one exists:

- LeaseHonoured (action): the decide branch of HandleAccepted at
  `i \in Nodes` runs with `lease_remaining[i] > 0`.
- WaitHonoured (action): each decision of the forced node runs with
  `wait_remaining = 0`.
- GrantOnlyCurrentEpoch (action): a grant gives a positive lease only
  if `node_epoch[i] = witness_epoch`.
- ForceRaisesEpoch == `[][force_single.node = 0 /\ force_single'.node # 0 => witness_epoch' > witness_epoch]_vars`.
- WitnessEpochGrows == `[][witness_epoch' >= witness_epoch]_vars`.
- LeaseWithinGrant (invariant): for each node,
  `lease_remaining[i] <= LeaseLength`, and after ForceSingle a node with
  `node_epoch[i] < witness_epoch` gets no new positive lease.
- ReadRoundHonoursLease (action): StartReadRound at `i \in Nodes` runs
  with a valid lease.
- LeasesDisjoint (invariant):
  `force_single.node # 0 /\ wait_remaining = 0 => \A i \in Nodes : lease_remaining[i] = 0`.
- OldDecidedFrozen ==
  `[][wait_remaining = 0 => old_decided_max' = old_decided_max]_vars`,
  where `old_decided_max` is the highest `decided_idx` of a node in
  `Nodes` (review 9). This checks the claim that a follower needs no
  lease.
- NoConcurrentWriters (invariant):
  `~(forced_has_decided /\ old_decided_after_wait)`.
- StaleReadAfterForce (invariant): no read at an old node with
  `forced_decided` TRUE and `~stale` reaches "done" (review 5).

NoTwoWriters, S1Force, S2Force and R1Force break in MC_w by the loss of
section 1 (review 8). MC_w does not list them. The cfg MC_w_loss lists
NoTwoWriters and must find its violation, in the form of MC_d1_reach.

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
| LeaseFromReply | LeaseGrant sets `LeaseLength`, not the request time | LeaseWithinGrant (strengthened: lease_remaining <= lease_request) | LeasesDisjoint |
| AdoptEpochFromRefusal | a refusal writes `witness_epoch` into `node_epoch[i]` | GrantOnlyCurrentEpoch with `node_epoch` of the configuration | NoConcurrentWriters |
| ReadWithoutLease | lease check in StartReadRound | ReadRoundHonoursLease | StaleReadAfterForce |

## 6. Runs

- Regression with `Witness = FALSE`: base_monotone (13,489,555
  distinct states), f (5,256,014), e (13,387,900), and d0, d1 as in
  `results/20261018T174440Z-d0.md` and `results/20261018T190030Z-d1.md`.
  The counts must be equal.
- MC_w: Nodes {1, 2, 3}, NextConfigNodes {6}, `Witness = TRUE`,
  `LeaseLength = 2`, SingleNode TRUE, MaxReadOnly 1, MaxForceSingle 1,
  MaxElection 3, MaxBallot 1, Values 2, MaxLog 1, ReadIndex TRUE,
  MaxRead 1, MaxHalt 0, Recovery TRUE, MaxCrash 0. Then a copy with
  MaxCrash 1. Breadth-first 1 h with `-deadlock -coverage 1`, then a
  simulation of 15 min. Coverage must show the decide branch of
  HandleAccepted and the decisions of node 6 above zero.
- MC_w_reach: ghost counters must reach an old decision during the
  wait, a decision of node 6 after the wait, and a refused request
  after the force.
- MC_w_loss: must violate NoTwoWriters.
- Each mutation: simulation, 15 min.
- One run at a time with 3 workers.

## 7. What this plan does not do

- No drift of the clock rates in the model. The drift factor acts at
  the side that waits (section 2).
- No limit of requests at the witness in the model.
- No view of each node at the witness. It is for the operator.
- No change to stage 1.
- The node-side check "reject a lower epoch" acts in the code on a
  witness that answers with a lower epoch than the node stored. The
  model has one force, so it has no step for this check.
