# Plan: a model of the leader election and its liveness

Source: the owner, 2026-10-24 20:33 (UTC): "dann müssen wir das noch
modellieren und prüfen, dass es dann noch weitergeht". The question
before it (20:30): how does a node find out that the leader is gone.
The answer was the heartbeats of the ballot leader election
(`include/konsens/ballot_leader_election.hpp`, after
`ballot_leader_election.rs` of Omni-Paxos at 532ff79). The model of
Sequence Paxos (`model/tla/SequencePaxos.tla`) has no ballot leader
election and checks no liveness. This plan adds both. It changes no
code. It gets one review before the model is built.

## 1. Facts

- The node has no clock. The caller calls `node::tick()`; after
  `election_tick_timeout` ticks, `ble_.hb_timeout(promise)` ends one
  heartbeat round (`node.hpp:124-131`).
- `hb_timeout` runs `update_leader`, `update_happiness`,
  `check_takeover`, `new_hb_round`, then raises `leader_` to the promise
  of Sequence Paxos if that is higher. It returns a ballot only when the
  node is its own leader (`ballot_leader_election.hpp:58-75`;
  `ballot_leader_election.rs:197-220`). Only then does the node call
  `handle_leader` and send Prepare (`node.hpp:127-129`).
- A node is happy when it sees a happy reply of its leader in the round.
  A leader is happy when the replies whose leader is not above its own
  ballot, with itself, make a majority, or when it sees a happy node
  with a higher leader (`:98-122`; `.rs:231-258`).
- A node that is not happy, sees no happy neighbour and has replies of
  a majority takes over with `n = leader.n + 1` (`:124-136`;
  `.rs:260-274`).
- A reply counts only in the round of its request and in the same
  configuration (`:143-150`; `.rs:290-294`). The C++ form also drops a
  second reply of the same peer in one round (`repliers_`); the Rust
  form keeps it. The C++ form has no `prev_replies` and no
  `seq_paxos_state` argument; with a majority quorum the accept quorum
  and the prepare quorum are the same size.
- `update_leader` takes the highest ballot of the repliers (`r.ballot`),
  not their leaders (`:98-105`; `.rs:222-229`). So a node takes as
  leader only a node that it reaches itself.
- `node::ballot_above_promise` (`node.hpp:372-379`) raises the ballot
  that BLE elects to `promise.n + 1` when the node is not leader and the
  promise is not below it. Omni-Paxos calls `handle_leader` with the BLE
  ballot as it is (`omni_paxos.rs:435-442`). The state file holds this
  form as provisional: 9 of 4000 simulation seeds made no progress
  without it. The owner, 2026-10-21 17:51 (UTC+2): check it in the model
  first.
- While Sequence Paxos recovers, the node drops every BLE message
  (`node.hpp:358-360`). A restarted node starts BLE with
  `recovery_round` and the recovered leader (`:28-45`).
- The Omni-Paxos paper (EuroSys 2023, section 4) claims progress when
  one server is quorum-connected. Not checked here; the model checks
  it.

## 2. Model

A new module `model/tla/BallotLeaderElection.tla`, the name of the
source file. It does not extend `SequencePaxos.tla` in the first step.

- Per node: `hb_round`, `replies` (a function from peer to the reply of
  this round, or none), `current_ballot`, `leader`, `happy`, `promise`,
  `role`, `recovering`. A ballot is the tuple (n, priority, pid) in the
  order of the code; `config_id` is one constant. The priority of each
  node is a constant `Priority`; the runs use 0 for every node.
- `promise` and `role` stand for Sequence Paxos. `HbTimeout(i)` runs the
  five steps of `hb_timeout` in the order of the code. When it returns
  a ballot, the same action calls `handle_leader` with
  `ballot_above_promise(b)` (switch `AbovePromise`, default TRUE; FALSE
  is the form of Omni-Paxos), sets the promise of `i` and puts a
  Prepare on each link. `DeliverPrepare(j)` follows `handle_prepare`:
  `j` takes a higher ballot as promise and becomes follower. This is the
  only part of Sequence Paxos in the model.
- Channels: a request and a reply carry the round. At `HbTimeout(i)`,
  each request from `i` and each reply to `i` leaves the channels, as
  the code drops a reply of an older round. So no round number grows
  with the run in the channels; `hb_round` itself is kept modulo 2.
  The Prepare channel per pair is FIFO (owner, 2026-10-20: the election
  requires TCP).
- `Links`: a constant set of pairs that can carry a message. Each
  configuration names its cases: all links, quorum loss, constrained
  election and chained (paper section 6, not checked here).
- Faults: `Crash(i)` stops a node for good, at most `MaxCrash`.
  `Restart(i)` restarts a node with `recovery_round`, the recovered
  leader and `recovering`, at most `MaxRestart`. `EndRecovery(i)` ends
  the recovery. `Disconnect(p)` removes a link, at most `MaxCut`.
- Timing: before the step `Stabilize`, `HbTimeout` may happen at any
  time. `Stabilize` happens once, after which no fault happens and
  `HbTimeout(i)` is enabled only after every reply of the round on a
  live link has arrived. That stands for a timeout longer than the
  round trip.
- Bound: a takeover or a raise above `MaxN` sets `overflow` and
  disables every action. The invariant `~overflow` reports it; a run
  with `overflow` gives no liveness result.

Properties:
- `PromiseMonotone`: the promise of a node never falls (Paxos, a node
  never takes a lower ballot).
- `NoOverflow`: `~overflow`.
- Liveness, `EventuallyStableLeader`, with weak fairness of
  `HbTimeout`, each delivery and `EndRecovery` of live nodes: if after
  `Stabilize` some live node reaches a majority of nodes over live
  links, then eventually some live node L that reaches a majority is
  happy, has `leader = current_ballot`, has a majority of nodes with
  `promise = current_ballot(L)`, and this stays.
- Cover: `StableLeaderReplaced`, an invariant that must fail: a stable
  leader crashes and a new stable leader appears.

## 3. Runs

- Configurations of 3 nodes first: all links with the first leader
  crashed; all links with a restart of a follower; chained; quorum
  loss; constrained election. Each with `MaxCrash` 1, `MaxRestart` 1,
  `MaxCut` at most 1, `MaxN` 4. 5 nodes only after the counts of 3
  nodes are recorded.
- Breadth-first with liveness checking, one worker, at most 2 h for one
  run, at most three runs at a time. A run cut by the limit gives no
  liveness result and says so. Simulation is not used for liveness.
  No symmetry: the pid orders the ballots.
- Counter-checks, each a `Mutation` with its own `MC_mut_*.cfg`:
  - `AbovePromise = FALSE` (the form of Omni-Paxos): expected to break
    `EventuallyStableLeader` with a restart; this is the check that the
    owner asked for on 2026-10-21. If it does not break, the result
    says that the model does not show the stuck election.
  - `NoQuorumCheck`: takeover without a majority of replies; expected to
    break `EventuallyStableLeader` in the case of quorum loss.
  - `NoTimeoutAfterStabilize`: `HbTimeout` stays disabled after
    `Stabilize`; must break `EventuallyStableLeader`. This shows that
    the liveness check can fail.
- Results go to `model/results/<time>-ble-liveness-runs-<n>.md`,
  pushed after each round with `[skip ci]`. The state file of
  mbedded-konsens in Claude-state gets the result after each round.

## 4. Second step, after the first runs

Compose the module with `SequencePaxos.tla`: `HbTimeout` replaces the
free `HandleLeader(i, k)`, and the liveness becomes "a proposed value
is decided". The ballot of `SequencePaxos.tla` is `n * PidBase + p`
(line 232); the composition maps the tuple to it. This step gets its
own plan after the results of section 3.

## 5. Not in this plan

- No change of the code. If a run finds a fault, the fault goes to the
  owner with the trace.
- No clock and no time in the model; a timeout is an order of events.
- The witness lease and the leader priority from the round-trip time
  (owner 2026-10-18) are not modelled.

## 6. Provisional names

`Links`, `MaxCut`, `MaxRestart`, `Stabilize`, `overflow`,
`AbovePromise`, `NoQuorumCheck`, `NoTimeoutAfterStabilize`,
`EventuallyStableLeader`, `StableLeaderReplaced`. The other names come
from the code and from Omni-Paxos. `EventuallyStableLeader` is the
property of the failure detector Omega (Chandra, Hadzilacos, Toueg
1996; not checked here).

## 7. Review

doc/review-plan-ble-liveness.md: approve with changes. Findings 1 to 3
and 5 to 12, 14 to 16 are in sections 1 to 6. Finding 4: restarts are
in. Finding 13: the plan uses no constraint in a liveness check.
