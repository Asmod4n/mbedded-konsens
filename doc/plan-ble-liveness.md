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
  node is its own leader (`ballot_leader_election.hpp:58-78`;
  `ballot_leader_election.rs:197-220`). Only then does the node call
  `handle_leader` and send Prepare (`node.hpp:127-129`).
- A node is happy when it sees a happy reply of its leader in the round.
  A leader is happy when a majority takes it as leader, or when it sees
  a happy node with a higher leader (`:98-122`; `.rs:231-258`).
- A node that is not happy, sees no happy neighbour and has replies of
  a majority takes over with `n = leader.n + 1` (`:124-136`;
  `.rs:260-274`).
- A reply counts only in the round of its request and in the same
  configuration (`:143-150`; `.rs:290-294`). The C++ form also drops a
  second reply of the same peer in one round (`repliers_`); the Rust
  form keeps it. The C++ form has no `prev_replies` and no
  `seq_paxos_state` argument; with a majority quorum the accept quorum
  and the prepare quorum are the same size.
- The Omni-Paxos paper (EuroSys 2023, section 4) claims progress when
  one server is quorum-connected. Not checked here; the model checks
  it.

## 2. Model

A new module `model/tla/BLE.tla`. It does not extend
`SequencePaxos.tla` in the first step.

- Per node: `hb_round`, `replies` (a set of records: from, ballot,
  leader, happy), `current_ballot`, `leader`, `happy`, `promise`.
  A ballot is the tuple (n, priority, pid) in the order of the code;
  `config_id` is one constant.
- `promise` stands for the promise of Sequence Paxos. `Prepare(i)`: a
  node that `hb_timeout` returned its own ballot for sets the promise of
  each node it reaches to that ballot, if higher. This is the only part
  of Sequence Paxos in the model.
- Channels: one message bag per ordered pair; a request and a reply
  carry the round.
- `Links`: a constant set of pairs that can carry a message. It gives
  the cases of partial connectivity of the paper: quorum loss,
  constrained election, chained (paper section 6, not checked here).
- Faults: `Crash(i)` stops a node for good, at most `MaxCrash`.
  `Disconnect` removes a link, at most `MaxCut`. After the last fault,
  the network is stable: each message on a link of `Links` arrives.
- Actions: `Send` is part of `HbTimeout(i)`; `DeliverRequest`,
  `DeliverReply`, `HbTimeout(i)`, `Prepare(i)`, `Crash(i)`,
  `Disconnect(p)`.
- `HbTimeout(i)` is enabled after the delivery of the replies of the
  round on every live link, or with a flag `Asynchronous` at any time.
  The first form stands for a timeout longer than the round trip. Only
  that form can have liveness.
- The round number grows without bound. The model keeps the round
  modulo 2, because a reply of an older round is dropped and, with
  delivery before the timeout, no reply is older than the last round.
  With `Asynchronous`, the round is bounded by a constraint and no
  liveness is checked.
- The ballot number grows only at a takeover. It is bounded by
  `MaxN`; a state with `n = MaxN` and no stable leader is reported as
  a violation of the bound, not cut by a constraint.

Properties:
- Safety, `LeaderBallotsUnique`: no two nodes take over with the same
  ballot.
- `PromiseMonotone`: the promise of a node never falls.
- Liveness, `EventuallyStableLeader`: with weak fairness of
  `HbTimeout`, `DeliverRequest`, `DeliverReply` and `Prepare` of live
  nodes, and after the last fault, eventually one live node L has
  `leader = current_ballot`, every live node that is quorum-connected
  has `leader = L's ballot` and is happy, and this stays.
- `EventuallyPromised`: eventually a majority has promise = the ballot
  of L.

## 3. Runs

- Configurations, 3 nodes and 5 nodes:
  - all links, the first leader crashes;
  - quorum loss, constrained election and chained, from the paper;
  - a minority partition with the leader in it.
- Each with `MaxCrash` 1, `MaxCut` 1 or 2, `MaxN` 4.
- Breadth-first with liveness checking, 2 h limit at most, one run at a
  time beside other work (load stays at most at the number of
  processors). Simulation is not used for liveness.
- Counter-checks: a mutation that drops the condition "all neighbours
  unhappy" must break `LeaderBallotsUnique` or the liveness; a mutation
  that takes over with `n = current_ballot.n + 1` must break the
  liveness in the chained case (the paper names this case).
- Results go to `model/results/<time>-ble-liveness-runs-<n>.md`,
  pushed after each round.

## 4. Second step, after the first runs

Compose `BLE.tla` with `SequencePaxos.tla`: `HbTimeout` replaces the
free `HandleLeader(i, k)` of the Sequence Paxos model, and the
liveness becomes "a proposed value is decided". This step gets its
own plan after the results of section 3.

## 5. Not in this plan

- No change of the code. If a run finds a fault, the fault goes to the
  owner with the trace.
- No clock and no time in the model; a timeout is an order of events.
- The witness lease and the leader priority from the round-trip time
  (owner 2026-10-18) are not modelled.

## 6. Provisional names

`BLE.tla`, `Links`, `MaxCut`, `Asynchronous`, `LeaderBallotsUnique`,
`PromiseMonotone`, `EventuallyStableLeader`, `EventuallyPromised`. The
other names come from the code and from Omni-Paxos.
