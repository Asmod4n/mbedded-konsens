# Plan: the clean-stop mark and the retirement of a node (form B)

Source: the owner, 2026-10-27. 11:28 chose form B of the rejoin: after a
start that may have lost its last sync, the identity of the node never
votes again, and the node joins as a new node through a StopSign. 13:12
chose which starts retire the identity: a clean stop writes and syncs a
mark; a start with the mark keeps the identity, deletes the mark and
votes at once; a start without the mark (a crash) retires the identity.
No backup carries the mark. A crash of a majority at the same time needs
the operator.

Base: branch `reconfiguration` at 0b1b966. Branch: `clean-stop`. This
plan replaces section 14 of doc/plan-sqlite-storage.md, which built the
rejoin form of 7.3 that the model found unsafe with five nodes.

Marks: "provisional" marks a form or a name that waits on the owner.

## 1. Facts checked in this session

1. `sequence_paxos::with` starts the recovery when the stored promise is
   not `ballot{}` (sequence_paxos.hpp:384-387). A first start (promise
   `ballot{}`) votes at once.
2. The repair of a damaged log ends in the recovery too
   (sequence_paxos.hpp:2077-2078). This path has no lost sync: the node
   rewrote its log from peers in one synced transaction.
3. `node_storage` (node.db) holds the repair record of the node
   (persistent_storage.hpp:185-205). `persistent_storage::copy` copies
   konsens.db only. A backup is a copy of konsens.db, so a backup holds
   nothing of node.db.
4. The model checks form B with a retirement after a rollback
   (RecoveryRollback.tla, Recovery "newid";
   results/20261027T113350Z-rejoin-new-identity.md). The model cannot see
   whether a start lost a sync; the code retires at every start without
   the mark, which retires more often than the model. That is safe:
   a retired node takes no action.
5. Tests that start a node again on its store and expect the recovery:
   test_node.cpp:140, test_reconfiguration.cpp:342, test/simulation.hpp
   (crash at 535-547 and rollback at 560-570, used by
   test_simulation.cpp, test_simulation_clients.cpp and
   test_simulation_rollback.cpp), test/node_cluster.hpp users in
   test_group_commit.cpp and test_device_faults. Tests of
   `sequence_paxos` alone (test_sequence_paxos.cpp:320, 1272, 1308 and
   the recovery tests) start `sequence_paxos` without `node`.
6. examples/counter.cpp, counter_stress.cpp and three_nodes.cpp start
   nodes with `node::with`; none starts a node again on a store that it
   used before in the same run (not checked for the restart flags of
   counter_stress).

## 2. The form

1. The mark lives in node.db as its own row (`clean_stop`). The
   functions of `node_storage` (provisional names):
   `write_the_clean_stop_mark()`, `has_a_clean_stop_mark()` and
   `delete_the_clean_stop_mark()`. Each write syncs before it returns.
2. `node::stop()` (provisional name) writes the mark. It is a wrong use
   (`std::logic_error`) while a call waits for its commit, and every call
   of the node after `stop()` is a wrong use. The node sends nothing and
   writes nothing after the mark.
3. `node::with` decides the start from two facts: the promise of the
   store and the mark.
   - Promise `ballot{}`: a first start. The node votes at once, as now.
   - Promise set and a mark: the node deletes the mark (synced), then
     starts without the recovery: phase `none`, Ballot Leader Election
     with the stored promise as the recovered leader, as now after a
     recovery.
   - Promise set and no mark: the node is retired.
4. A retired node (provisional name `retired`):
   - votes never again, sends no Sequence Paxos and no Ballot Leader
     Election message, and answers none, with one exception: it answers
     `build_request` as plan-reconfiguration 2.9 says, because a decided
     StopSign fixes its decided prefix and a lost sync only removes
     entries from the end;
   - gives `client_errc::retired` (provisional name) to each client call;
   - stays retired across each later start: `node::with` sees the
     promise and no mark again. The caller deletes its stores after the
     next configuration has started without it.
   - `node::retired()` returns true.
5. The repair path keeps its recovery (fact 2). `sequence_paxos::with`
   gets the start from `node` as a value of `sequence_paxos_config`
   (provisional `start_after_a_clean_stop`, a bool): true starts without
   the recovery; false keeps the recovery at a start with a promise, so
   that the tests of `sequence_paxos` alone keep their form.
6. The order at a start with the mark: delete the mark and sync, then
   send the first message. A crash between the two leaves no mark, and
   the next start retires. A crash after a first message and before the
   next clean stop leaves no mark either.

## 3. What this plan does not do

- No automatic reconfiguration after a retirement. The caller calls
  `reconfigure` with new pids.
- No path for a crash of a majority at the same time; the operator
  decides (owner 13:12).
- No change to the model: fact 4 holds for the code.

## 4. Tests

1. node_storage: a new node.db has no mark; the mark survives a close
   and an open; its deletion survives a close and an open; a copy of
   konsens.db leaves node.db alone.
2. A node that stops cleanly and starts again votes at once: no
   recovery message, it answers a prepare, and the cluster decides
   with it while one other node is down.
3. A node that starts again without `stop()` is retired: it sends no
   message, a client call gives `client_errc::retired`, and a second
   start is retired too.
4. A start with the mark deletes it: a second start without `stop()`
   retires.
5. `stop()` while a call waits for its commit, and any call after
   `stop()`, are wrong uses.
6. A retired old node answers `build_request` after its StopSign is
   decided (replaces test_reconfiguration.cpp:342).
7. A whole cluster that stops cleanly starts again and elects a leader
   without the operator (the question of the owner, 13:10).
8. The tests of fact 5 change: a restart that expects the old recovery
   calls `stop()` first, or expects the retirement. The simulations
   pick for each crash one of two kinds, a clean stop with a start, or
   a crash that retires; a rollback retires. The check that a majority
   decides counts only the nodes that are not retired, and a run with
   a retired majority checks safety only.

## 5. Order

This plan, the review, the cards of the provisional names, the code of
2.1 to 2.6 with the tests of 4.1 to 4.7, then the tests of 4.8, then the
full suite.

## 6. Data on the disk

Listed at the end of the build.
