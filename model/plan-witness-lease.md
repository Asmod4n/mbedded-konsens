# Plan: the lease at the witness

Source: the decision of the owner of 2026-10-18 ("Lease, selten") and
the state file of the project. This plan changes the model. A fresh
agent reviews it before anything is built.

## 1. The fault that the lease removes

ForceSingle (stage 2) has the guard
`Cardinality(Nodes \ running) >= Majority(i)`. The guard trusts the
operator: the old majority is down. In a partition the old majority is
alive. It keeps its leader, decides new entries and answers reads. The
forced node decides too. Two histories grow at the same time.

The model cannot show this fault now, because the guard holds by
definition. The plan removes the guard and puts the lease in its place.

## 2. The form of the lease

- The witness holds two durable values: `epoch` and `expiry`.
- A leader of the old cluster asks the witness for a lease of length
  `LeaseLength` for its epoch. It reads its own clock before it sends
  the request. Its lease ends at that time plus `LeaseLength`.
- The witness grants the request only if the epoch of the request is
  its own epoch. It then sets `expiry` to its time plus `LeaseLength`.
- A leader decides an entry only while its lease is valid by its own
  clock. A leader answers a read only while its lease is valid.
- ForceSingle asks the witness to raise the epoch. The witness raises
  it and returns the time that remains until `expiry`. From then on it
  refuses every request of the old epoch.
- The forced node decides nothing before that remaining time has
  passed by its own clock.
- A restart of the witness keeps `epoch` and `expiry`.
- A follower needs no clock for safety, because only a leader decides.

## 3. The model

- New constants: `Witness` (Bool), `LeaseLength` and `MaxTime` (Int).
  With `Witness` FALSE the model is unchanged. Each existing cfg gets
  `Witness = FALSE` and its state count stays the same.
- New variables: `now` (global time), `witness` (`[epoch, expiry]`),
  `lease` (per node: the epoch and the end of the lease), and the ghost
  `force_time` (the time of ForceSingle) with `old_decide_time` (the
  last time an old node decided).
- Clocks: one global time `now`. Each node reads `now`. A drift of the
  clock rates is not modelled; it is an assumption (see 6).
- New actions: `Tick` (`now < MaxTime`), `RenewLease(i)`,
  `WitnessRestart` (changes nothing durable, and so only shows that no
  action reads a volatile field of the witness).
- Changed actions: HandleAccepted and LeaderCountsOwnAccepted (decide
  only with a valid lease), ReadReply (answer only with a valid lease),
  ForceSingle (raise the epoch, no majority guard when `Witness` is
  TRUE), and the decide steps of the forced node (only after the
  remaining time).
- The new fields enter `vars`, the view and every explicit UNCHANGED
  list.

## 4. Properties

- NoConcurrentWriters: an old node decides nothing at or after the
  first decision of the forced node. Ghost: `old_decide_time <
  first time a NextConfigNodes node decides`.
- LeaseHonoured (direct, action): an old leader that decides has a
  lease with end greater than `now`.
- WaitHonoured (direct, action): the first decision of the forced node
  happens at `now >= force_time + remaining time`.
- WitnessEpochGrows: `[][witness.epoch' >= witness.epoch]_vars`.
- NoTwoWriters stays. With the lease it is expected to break: an old
  decision between ForceSingle and the end of the lease is a loss that
  stage 2 accepts. The run shows if it breaks. The plan does not
  weaken NoTwoWriters; if it breaks, the owner decides.

## 5. Mutations

Each mutation must break its direct property and NoConcurrentWriters
in a simulation of 15 min.

| Mutation | Mutated place | Must break |
|---|---|---|
| DecideWithoutLease | lease check in HandleAccepted | LeaseHonoured, NoConcurrentWriters |
| ForcedWithoutWait | the wait of the forced node | WaitHonoured, NoConcurrentWriters |
| WitnessForgetsEpoch | WitnessRestart sets the epoch back | WitnessEpochGrows, NoConcurrentWriters |
| LeaseFromReply | the leader takes the time of the reply, not of the request | LeaseHonoured, NoConcurrentWriters |
| ReadWithoutLease | lease check in ReadReply | EachReadSeesDecided |

## 6. Runs

- Regression: base_monotone, f, e and the d cfgs with `Witness =
  FALSE`. The state counts must equal the last runs.
- New cfg MC_w: three old nodes, one new node, `Witness = TRUE`,
  `LeaseLength = 2`, `MaxTime = 6`, no majority guard, MaxCrash 0,
  then MaxCrash 1. Breadth-first 1 h with `-deadlock -coverage 1`, then
  a simulation of 15 min.
- Each mutation: simulation, 15 min.
- One run at a time with 3 workers.

## 7. What this plan does not do

- No drift of the clock rates. The code waits `LeaseLength` times a
  factor for the drift. The factor is a field of the configuration and
  is not measured.
- No limit of requests at the witness in the model. The rate is a
  field of the configuration.
- No view of each node at the witness. It is for the operator, not for
  safety.
- No change to stage 1.

## 8. Questions for the review

- Is the lease of the leader enough, or does a follower need a check,
  for example for a decide that a new leader takes over from promises?
- Is one global time a sound abstraction for the safety properties
  above?
- Does NoConcurrentWriters say what the owner asked for: no two writers
  at the same time?
