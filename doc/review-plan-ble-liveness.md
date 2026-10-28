# Review: plan-ble-liveness.md

Reviewed on 2026-10-24 against `doc/plan-ble-liveness.md` (head
495f1cb), `include/konsens/ballot_leader_election.hpp`,
`include/konsens/node.hpp` (124-131, 352-379),
`include/konsens/sequence_paxos.hpp` (372-393, 696-730),
`include/konsens/storage.hpp` (17-24), `model/tla/SequencePaxos.tla`
(232-233, 1216-1240, 2000, 2090), the state file
`mbedded-konsens.md` (lines 498-528, 504), both rule files and Omni-Paxos
532ff79 (`ballot_leader_election.rs` 53-63, 197-294, `util.rs`
428-462, `omni_paxos.rs` 403-442).

## Checks done for this review

No model was written and no TLC run was made. Each claim below comes
from a reading of the code and of the Rust source. The cases in
findings 1 and 2 are traces by hand. They are not checked by TLC.

## Facts of section 1 that hold

- Fact 1: `node::tick` calls `ble_.hb_timeout(seq_paxos_.get_promise())`
  after `election_tick_timeout` ticks (`node.hpp` 124-131). Correct.
- Fact 2: the order update_leader, update_happiness, check_takeover,
  new_hb_round, then the raise of `leader_` to the promise, is correct
  (`.hpp` 58-75, not 58-78; `.rs` 197-220).
- Fact 4: the takeover with `n = leader.n + 1` is correct (`.hpp`
  124-136, `.rs` 260-274).
- Fact 5: the round and configuration check is correct (`.hpp`
  143-150, `.rs` 290-294). `repliers_` is a C++ addition. With
  `Quorum::Majority`, `is_prepare_quorum` and `is_accept_quorum` use
  the same size (`util.rs` 445-461). `majority_` in C++ is
  `(peers + 1) / 2 + 1`, the same value as `num_nodes / 2 + 1`.
- The ballot order: Rust compares (n, priority, pid) and ignores
  `config_id` (`.rs` 53-57). C++ compares (config_id, n, priority, pid)
  by the default `<=>` (`storage.hpp` 17-24). With one `config_id`, the
  two orders are equal.

## Findings

1. Fault: `EventuallyStableLeader` fails on correct code in the
   chained case. `update_leader` takes the ballots of the repliers
   (`r.ballot`), not their leaders (`.hpp` 98-105, `.rs` 222-229). A
   node that has no link to L never sees the ballot of L. Trace by
   hand, 3 nodes, links A-B and B-C, ballots (1,A) < (1,B) < (1,C), no
   fault: A takes `leader = (1,B)` and is happy, because B is happy. A
   is quorum-connected ({A, B} is a majority). A never has `leader =`
   the ballot of C, so the property is violated. The same holds for
   the constrained election with 5 nodes.
   Fix: state the property for L and its neighbours only. For example:
   eventually and for good, one live, quorum-connected L has
   `leader = current_ballot` and is happy, every live node with a link
   to L in both directions has `leader = L.current_ballot`, and no live
   node changes `current_ballot` again. Add fact 6 to section 1: the
   leader of a node is the highest ballot of its repliers.

2. Fault: the properties have no precondition for quorum loss. With
   `MaxCrash` 1 and `MaxCut` 2 on 3 nodes, a run can end with no
   quorum-connected node. The liveness then fails without a fault in
   the code. Also, the conjunct "one live node L has `leader =
   current_ballot`" alone holds in each state: an old leader in a
   minority keeps `leader_ == current_ballot_` and still gets its
   ballot back from `hb_timeout` (`.hpp` 71-73).
   Fix: guard each liveness property with "after the last fault, some
   live node is quorum-connected", or bound the faults per
   configuration so that a quorum-connected node stays. Require L to
   be quorum-connected and happy.

3. Fault: the plan leaves out `node::ballot_above_promise`
   (`node.hpp` 372-379). The node calls `handle_leader` with that
   ballot, not with the ballot of BLE (`node.hpp` 128). Omni-Paxos
   calls `handle_leader(new_leader)` directly (`omni_paxos.rs`
   435-442); the C++ form follows `try_become_leader` (`omni_paxos.rs`
   418-423). The state file names this round as provisional, and the
   owner said on 2026-10-21: check it in the model first. The state
   file calls the stuck election in BLE "the open item of a model with
   BLE". The plan does not name it.
   Fix: add the function to section 1 and to `Prepare`. Section 5 must
   say whether the owner's item is closed by this model. With
   permanent crashes only, the elected ballot is never below the
   promise at the return of `hb_timeout`, so the function changes the
   ballot only when the ballot equals the promise and the node is not
   the Sequence Paxos leader. That case needs a restart (finding 4).
   Write this argument in the plan, or model the role.

4. Fault: `Crash(i)` stops a node for good. The case of the owner's
   item comes from a restart: `ballot_leader_election::with` sets
   `n = recovery_round` and `leader_ = ` the recovered promise
   (`.hpp` 38-41), and the node drops BLE messages while Sequence
   Paxos recovers (`node.hpp` 358-360). The model has no `Restart`.
   Fix: add `Restart(i)` with this initial state, or name the restart
   in section 5 as a case that this plan does not check.

5. Fault: `LeaderBallotsUnique` can not fail. A ballot holds the pid,
   so the ballots of two nodes always differ. A node raises its own
   `n` at each takeover, because `leader_ >= current_ballot_` holds
   before the takeover. The first counter-check names this property as
   one way to fail; that way does not exist.
   Fix: remove it, or replace it with a property that can fail, such
   as "the leader of a node never falls" (`update_leader`, the
   takeover and the promise raise only raise it). Give it a mutation
   that breaks it.

6. Fault: the bound `MaxN` is reported with a wrong condition. A
   state with `n = MaxN` and no stable leader is a normal state just
   after a takeover at `MaxN`, before the followers see it. The plan
   also does not say what `HbTimeout` does when a takeover would give
   `MaxN + 1`. If the action is disabled, the node stops, and the
   liveness result is wrong in both directions.
   Fix: when a takeover would give `n > MaxN`, set a variable
   `overflow` and disable every action. Check the invariant
   `~overflow`. TLC stops at an invariant violation, so a run with
   overflow has no liveness result. Write it in the result file as
   "bound reached, liveness not decided", and raise `MaxN`.

7. Fault: the round modulo 2 is not needed, and its argument does not
   cover all messages. A request of round r on a link that `Disconnect`
   removed can stay in the bag. The plan does not say whether
   `Disconnect` empties the bag, or whether `Links` is symmetric. If a
   message of round r stays, a reply of round r + 2 can be mistaken for
   it.
   Fix: drop the round. At `HbTimeout(i)`, remove from the bags each
   request from i and each reply to i. This is exact:
   `handle_request` changes no state (`.hpp` 138-141), and
   `handle_reply` drops each reply of an older round (`.hpp` 145).
   Each channel then holds at most one request and one reply, also
   with `Asynchronous`, and the constraint on the round goes away.

8. Fault: the synchronous guard holds from the first state. The model
   then never reaches a state that only an early timeout gives, for
   example a takeover while the leader is alive but slow. Liveness from
   such a state is not checked.
   Fix: with finding 7, allow the early `HbTimeout` before a step
   `Stabilize` (no fairness on faults, at most once), and the guard of
   the plan after it. State the liveness after `Stabilize`. This is
   the usual form of partial synchrony (Dwork, Lynch, Stockmeyer 1988;
   not checked here).

9. Fault: `Prepare(i)` is a separate action with weak fairness. In the
   code, `handle_leader` runs in the same `tick` as `hb_timeout`
   (`node.hpp` 127-129). As a separate action, the next `HbTimeout(i)`
   can disable it, so weak fairness does not force it. The atomic set
   of the promise on each reached node also hides the order of a
   Prepare and the heartbeat rounds of the receiver.
   Fix: put the local part of `handle_leader` in `HbTimeout(i)`: the
   guard `n > promise` (`sequence_paxos.hpp` 377), the own promise, and
   one Prepare message per link. Deliver it with `DeliverPrepare` and
   weak fairness, with the rule of `handle_prepare` (`promise_ < n`,
   `sequence_paxos.hpp` 706, 724).

10. Fault: the plan does not name the step where BLE and Sequence
    Paxos disagree in one `hb_timeout`. A node can get the Prepare of
    L and then lose L. At its next `hb_timeout`, the takeover uses
    `leader_.n + 1` and ignores the promise. The promise raise after
    it can then set `leader_` to the ballot of L and `happy_` to true
    (`.hpp` 62-70), so the takeover ballot stays as `current_ballot_`
    and is not returned. The election takes one round more. This is
    correct code, but the model shows it only if `HbTimeout` keeps the
    code order and reads the promise.
    Fix: write the order of the five steps in the definition of
    `HbTimeout`, with the promise as input.

11. Fault: the counter-checks have no argument for "must break". For
    the first mutation, a trace by hand of the chained case gives no
    livelock: the old leader stays happy through "a happy node with a
    higher leader" (`.hpp` 120-121). For the second mutation, `n`
    grows by one in each round, so the mutant can end at `MaxN` and
    not in a cycle. With finding 6, both give "bound reached", the
    same result as a too small `MaxN` in the original. The claim that
    the paper names the second case is not checked here.
    Fix: run each mutant on every configuration and record the result.
    Report a mutant that does not break. Add a counter-check that must
    break the liveness for sure: no fairness on `DeliverReply`, or
    `Asynchronous` after `Stabilize`. Add a cover invariant that must
    be violated: a trace where a stable leader crashes and a new
    stable leader appears. Use the form of `SequencePaxos.tla`:
    `ASSUME Mutation \in {...}` and `MC_mut_*.cfg`.

12. Fault: the sizes in section 3 have no measurement behind them. No
    state count is known. TLC checks liveness on the full state graph;
    a run cut at 2 h gives no liveness result. Symmetry is not
    possible, because the pid orders the ballots. Each `WF` conjunct
    (up to 40 for 5 nodes) adds to the liveness check.
    Fix: run 3 nodes first and record states, distinct states, depth
    and time. Decide the 5-node runs after that. Say that a run cut by
    the limit is "not decided". Name the number of TLC workers. Keep
    `replies` as a function from peer to a reply or none, as
    `repliers_` makes it exact for C++.

13. Fault: the state constraint. The plan uses it only with
    `Asynchronous`, where no liveness is checked. That is correct. A
    state outside the constraint gets no successor, so in a liveness
    check it can give a false lasso or hide a real one. The exact text
    of the TLC documentation on this is not checked here. With finding
    7 the constraint is not needed.
    Fix: no constraint in any liveness run. Say so in section 3.

14. Fault: the ballot in the model has a priority, and the plan does
    not say its value. `SequencePaxos.tla` encodes a ballot as
    `n * PidBase + p` (line 232), with no priority. Section 4 composes
    the two modules.
    Fix: one priority for all nodes, and the encoding of
    `SequencePaxos.tla`, so that section 4 needs no translation.

15. Fault: names with no source outside the tree (rule "Every name
    comes from a source outside this tree"). `BLE` is the prefix of
    Omni-Paxos (`BLEMessage`, `BLEConfig`), but the struct is
    `BallotLeaderElection`, and the other module is named after its
    struct. `Links`, `MaxCut` and `PromiseMonotone` have no source.
    `EventuallyStableLeader` names the property of the failure
    detector Omega (Chandra, Hadzilacos, Toueg 1996; not checked
    here).
    Fix: propose `BallotLeaderElection.tla`. Give each other name to
    the owner with a proposal.

16. Fault: section 3 has no step for the state file and no workers
    limit (rules "A state file holds what is in force", "The load
    stays within the processors"). The push of a result has no
    `[skip ci]` mark (rule "The CI runs only after the local run is
    green").
    Fix: add these steps.

## Not checked

- The claims of the paper (sections 4 and 6). The paper is not in the
  tree (`refs/papers.md` holds the reference only).
- Every trace by hand above, with TLC.
- The TLC text on constraints with liveness, and the cost of each
  `WF` conjunct.
- The resend of Prepare by `resend_message_timeout`
  (`sequence_paxos.hpp` 425-430) and its effect on finding 9.

## Note outside the plan

The channel is a bag per ordered pair. The owner decided on
2026-10-20 that the election and the handshake require TCP (state
file, line 504). For BLE alone with finding 7, the order does not
matter. For section 4, the Prepare channel must be FIFO.

Verdict: approve with changes. Findings 1, 2, 3, 5, 6 and 9 change the
properties and the actions; they enter the plan before the build.
