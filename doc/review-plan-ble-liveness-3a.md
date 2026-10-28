# Review: plan-ble-liveness.md, section 3a

Reviewed on 2026-10-28 against `doc/plan-ble-liveness.md` (head
ecdb8d4, lines 132-174), `model/tla/BallotLeaderElection.tla`,
`model/tla/MC_ble_*.cfg`, `include/konsens/ballot_leader_election.hpp`,
`include/konsens/node.hpp` (124-131, 352-379),
`include/konsens/sequence_paxos.hpp` (255-272, 370-393, 694-730,
1064-1085) and the state file `mbedded-konsens.md` (788-800).

Verdict: approve with changes.

## Checks done for this review

The model at ecdb8d4 has no `AtomicRounds`. For this review, a copy of
the module outside the tree got the form of items 1 and 2, with the
conditions of findings 1 and 2 below. TLC 1 worker, metadir under
`/mnt/zrun/review-3a`, 3 nodes, `MaxN` 3, `MaxEarlyTimeout` 1,
`MaxRestart` 1, `AtomicRounds = TRUE`, invariants only. The copy asserts
that `AbovePromiseOf` never changes a ballot that `HbTimeout` elects on
a node that does not recover. Result: no violation in 625,804 distinct
states at depth 13 after 60 s. The run was cut. It is not complete.
A first form of the assert also fired on ballots that are not elected;
that form was wrong and was corrected. The Rust source was not read
again. The other claims below come from a reading of the model and of
the code.

## Findings

1. Blocking. Place: `doc/plan-ble-liveness.md:156-159` (item 2).
   The atomic round fills a reply from each peer j that is alive, not
   recovering and linked. It does not test whether i itself recovers.
   The code drops every BLE message while Sequence Paxos recovers
   (`node.hpp:358-360`). The model does the same in `DeliverReply`
   (`BallotLeaderElection.tla:203`). With item 2 as written, a node
   that recovers gets replies after `Stabilize` and can take over. The
   code cannot do that. So the atomic form is not a subset of the
   asynchronous form, and the claim of item 5 is false.
   Fix: add "and i is not recovering" to item 2.

2. Not blocking. Place: `doc/plan-ble-liveness.md:157-158` (item 2).
   Item 2 sets `replies[i][j]` for each peer. A reply that arrived
   before `Stabilize` can already stand in `replies[i][j]`. The code
   keeps the first reply of a peer in a round (`repliers_`,
   `ballot_leader_election.hpp:145-148`), and `DeliverReply` writes only
   where `replies[i][j].none` (`BallotLeaderElection.tla:203`). An
   overwrite makes a behavior that the asynchronous form does not have.
   Fix: set `replies[i][j]` only where it is empty, and keep the other
   entries.

3. Blocking (text). Place: `doc/plan-ble-liveness.md:164-169` (item 5).
   With findings 1 and 2 applied, each atomic step is the asynchronous
   sequence "DeliverRequest(i, j) and DeliverReply(j, i) for each j,
   then HbTimeout(i)" with no other step between. So the subset claim
   holds. A liveness result on a subset does not hold for the full set.
   Item 5 names the excluded class only in general words. The class is
   concrete: the state of a peer j changes only at `HbTimeout(j)`. So
   the excluded replies are the replies that miss an `HbTimeout(j)`
   inside the round of i. That class holds the concurrent takeover: i
   and j are both unhappy, each sees the other unhappy, and both take
   over in the same round. In the atomic form this cannot happen after
   `Stabilize`. The second node to time out sees the first one happy
   and does not take over (`check_takeover`,
   `ballot_leader_election.hpp:124-136`). A trace by hand found no
   livelock from that case (equal `n`, the pid decides). It is not
   checked by TLC.
   Fix: name the concurrent takeover in item 5 as a case that the
   atomic form does not check. Option, to keep it: a step `Collect(i)`
   that takes the replies of all peers at once, at any point after the
   last `HbTimeout(i)`; `HbTimeout(i)` is enabled after `Collect(i)`.
   That keeps one interleaving point for each round and keeps the
   concurrent takeover. A second option is `Collect(i, j)` for each
   peer, which merges `DeliverRequest` and `DeliverReply`. The content
   of a reply is fixed at `DeliverRequest`, so this form gives the same
   replies as the asynchronous form. Measure the state counts of the
   three forms on `MC_ble_nofault` before the choice.

4. Blocking. Place: `doc/plan-ble-liveness.md:172-173` (item 7),
   `MC_ble_mut_no_above.cfg`, `BallotLeaderElection.tla:128-130,
   235-261`. The mutation `AbovePromise = FALSE` cannot break in this
   model, in the atomic form or in the asynchronous form.
   `AbovePromiseOf(i, b)` changes an elected ballot only when
   `promise[i] = c3`, the pid of the promise is i, and i is a follower.
   (Elected means `l3 = c3`, and `l3 >= promise[i]` after the raise.)
   A promise with the pid of i comes only from a lead of i, and then
   `promise = leader` with `AbovePromise = FALSE`. `DeliverPrepare`
   brings only ballots of other nodes. The leader of BLE never falls
   except at `Restart`, and `Restart` sets `leader = promise`. So after
   a restart `leader >= promise` while the pid of the promise is i. The
   raise to the promise then never happens, and a takeover gives
   `n = leader.n + 1 > promise.n`. The case is not reachable. The TLC
   run above found no state where it holds (cut, not complete).
   In the code, the case needs a promise with the pid of i above the
   BLE leader of i after the restart. `complete_recovery` sets
   `promise_ = pr`, the promise of the recovery source
   (`sequence_paxos.hpp:1072`). `EndRecovery` in the model keeps the
   promise (`BallotLeaderElection.tla:255-261`). Whether the 9 stuck
   seeds come from this path, and whether a power loss can lose the
   write of the promise, is not checked.
   Fix: before the run, either model `complete_recovery`: `EndRecovery`
   takes the promise of a recovery source, and `Restart` may start
   from a lower stored promise where the code can lose that write; or
   write in section 3a that this mutation cannot break in the model and
   that the model does not answer the question of the owner of
   2026-10-21. Do not spend a run on it in the current form.

5. Blocking. Place: `doc/plan-ble-liveness.md:123-124` and 172-173,
   `MC_ble_mut_no_quorum.cfg` (`MaxCut = 1`),
   `BallotLeaderElection.tla:125`. With 3 nodes and one cut link, each
   node keeps a link to a live peer, and the configuration has no
   restart. After `Stabilize`, each round of each node has at least one
   reply, so `Cardinality(Got) + 1 >= Majority` always holds. The
   mutation `NoQuorumCheck` then changes nothing after `Stabilize`, in
   the atomic form and in the asynchronous form (`RoundDone` waits for
   every reply). "Quorum loss" does not exist with 3 nodes and one cut.
   With two cuts, one node is alone. The mutation makes it take over in
   each round, `overflow` follows, and `NoOverflow` fails. A node alone
   has no link, so its ballot never reaches the majority, and
   `EventuallyStableLeader` holds. So with 3 nodes the mutation can
   only show itself as an overflow, which section 2 counts as "no
   liveness result".
   Fix: either state the expected break as "`NoOverflow` fails in a
   state with `stable = TRUE`, `MaxCut = 2`", or move this mutation to
   5 nodes with the quorum-loss case of the paper, where a node with a
   minority of links reaches the leader side.

6. Not blocking. Place: `doc/plan-ble-liveness.md:125-127`,
   `MC_ble_mut_no_timeout.cfg`. `NoTimeoutAfterStabilize` still breaks
   in the atomic form. `Stabilize` can be the first step. In `Init`
   every role is `Follower`, and without `HbTimeout` no node leads, so
   `StableLeaderAt` never holds. The shortest counterexample is this
   trivial one. It shows that the check can fail. It does not show that
   the check sees a leader that is lost after `Stabilize`.
   Fix: none needed. Option: a second form that disables `HbTimeout`
   only after a stable leader exists.

7. Not blocking. Place: `BallotLeaderElection.tla:297-303`, plan
   item 2. Fairness stays correct. `DeliverRequest` and `DeliverReply`
   are never enabled after `Stabilize`, so their weak fairness holds
   trivially, and every behavior reaches `Stabilize` by
   `WF(Stabilize)`. `HbTimeout(i)` is enabled in every state after
   `Stabilize` for a live i. At a fixpoint, an atomic `HbTimeout(i)`
   changes no variable, so it is a stuttering step, and weak fairness
   does not demand it. This is correct. The antecedent of
   `EventuallyStableLeader` holds in every behavior of the planned
   configurations, so the property does not hold vacuously.
   `DeliverPrepare` stays asynchronous with weak fairness only. A
   Prepare can wait many rounds. That adds behaviors and cannot hide a
   fault.
   Fix: none. State in item 2 that `HbTimeout` after `Stabilize` sends
   no request and leaves `reqs` and `reps` unchanged.

8. Not blocking. Place: `doc/plan-ble-liveness.md:153-155` (item 1).
   "A loss of messages, which the network allows": the model has no
   loss action. A message is lost only on a cut link or to a crashed
   node. The emptied messages are the requests and replies of the
   current round, and the atomic round replaces them with replies of a
   later state. So item 1 is a case of finding 3, not a new loss.
   Fix: say that the emptied replies are replaced by replies of a later
   state of the same peer.

9. Not blocking. Place: `doc/plan-ble-liveness.md:143-144`. "So the
   size comes from the order of heartbeat messages after `Stabilize`"
   is not shown. The runs without faults show that the faults are not
   the cause. They do not separate the states before `Stabilize` from
   the states after it. Before `Stabilize`, the asynchronous form with
   `MaxEarlyTimeout` 3 stays.
   Fix: count the distinct states with `stable = FALSE` (a run with
   `Stabilize` disabled) and write that number next to the claim.

10. Not blocking. Place: `BallotLeaderElection.tla:42, 295-303`.
    `was_stable` serves only the cover invariant
    `StableLeaderReplaced`. It is a history variable in `vars`, and it
    can multiply the states of each liveness run by up to 8 with 3
    nodes. Not measured.
    Fix: keep `was_stable` in the cover run only (a second `Next`
    without it for the liveness runs), and measure the state count
    with and without it.

11. Not blocking. Place: `doc/plan-ble-liveness.md:170-171` (item 6),
    all `MC_ble_*.cfg`. A cfg that does not name the new constant
    `AtomicRounds` does not parse.
    Fix: item 6 says that each cfg names `AtomicRounds`.

12. Not blocking. Place: `doc/plan-ble-liveness.md:134-142`,
    `model/results/`. The numbers of the first runs have no result
    file. `model/results/` has no file for these runs. Section 3 says
    that results go to `model/results/<time>-ble-liveness-runs-<n>.md`.
    Fix: write that file and cite it in section 3a.

13. Not blocking. Place: `doc/plan-ble-liveness.md:166-167`. Item 5
    cites "section 1, partial synchrony". Section 1 has no such fact.
    The timing stands in section 2, "Timing".
    Fix: cite section 2.
