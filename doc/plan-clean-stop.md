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
plan replaces section 14 of doc/plan-sqlite-storage.md. That section was
a plan for the rejoin form of 7.3, which the model found unsafe with five
nodes; nothing of it was built.

Marks: "provisional" marks a form or a name that waits on the owner.
Section 7 lists the findings of review 1 and what changed.

## 1. Facts checked in this session

1. `sequence_paxos::with` queues the recovery when the stored promise is
   not `ballot{}` (sequence_paxos.hpp:384-387). A start with promise
   `ballot{}` votes at once.
2. The repair of a damaged log replaces the promise, the accepted round
   and the log of the node with those of a peer, then runs the recovery
   (sequence_paxos.hpp:2058-2078; persistent_storage.cpp:1554-1584). The
   node loses promises and Accepted messages that it sent before. This
   is a rollback.
3. `node_storage` (node.db) holds the repair record of the node
   (persistent_storage.hpp:185-205). `persistent_storage::copy` copies
   konsens.db only. `node_storage::open` creates node.db when it does not
   exist (persistent_storage.cpp:1970).
4. The model checks form B with a retirement after a rollback
   (RecoveryRollback.tla, Recovery "newid";
   results/20261027T113350Z-rejoin-new-identity.md). The code retires at
   every start that cannot show a clean stop, which is more often than
   the model. A retired node votes never.
5. After a recovery, the node is in `{follower, recover}` and sends
   `prepare_req` to all peers (sequence_paxos.hpp:1906-1915), the
   fail-recovery of Omni-Paxos.
6. `write_the_proof_and_throw` ends the process with a `node_fault`
   (node.hpp:690-697). No clean stop follows.
7. Starts of a node on a store that it used before, or on a prepared
   store with a promise: test_node.cpp:140, 503-545, 557-600, 643-659;
   test_client.cpp:196, 498, 537; test_execute_verify.cpp:611, 655, 851,
   904, 918, 931, 943; test_reconfiguration.cpp (`cluster::start` at
   120, used at 347, 372-373, 421-422, 458-459); test/simulation.hpp
   (crash at 535-547, rollback at 560-570); test/node_cluster.hpp:215
   and its users; examples/counter_stress.cpp:433-454 (`restart`,
   `lose_power`). simulation.hpp:542 and 566, node_cluster.hpp:215 and
   test_client.cpp:196 give a new node.db at a restart. The tests of
   `sequence_paxos` alone (test_sequence_paxos.cpp:320, 1272, 1308 and the
   recovery tests) start `sequence_paxos` without `node`.

## 2. The form

1. The mark is a row of node.db (`clean_stop`) that holds the pid, the
   configuration_id and the whole meta record of konsens.db at the stop,
   with a CRC32C over these bytes. Each write of node.db syncs before it
   returns. The store has no own identity: the meta record holds
   `last_chain_crc`, which names the log, and `applied_chain_crc`. A
   konsens.db from an older copy, or of another node, has another meta
   record (section 7, item 16).
2. `node::stop()` (provisional name) is the clean stop:
   - it is a wrong use (`std::logic_error`) after a write or a sync of
     the store failed (`throw_if_a_write_did_not_finish`), after
     `node_fault`, on a retired node, and while a step is open;
   - it gives each call that waits for its decide the error
     `client_errc::stopped` (provisional), drops the messages that are
     not released, and writes the mark from the committed meta record;
   - every call of the node after `stop()` is a wrong use.
3. Every start reads the mark and deletes it (synced) before the node
   releases any output, whatever the promise is. Then:
   - The caller states a first start (section 6, question 1). A first
     start on a store with promise `ballot{}` votes at once.
   - A mark that agrees with the store (pid, configuration_id and the
     whole meta record): the node starts
     without the recovery in `{follower, recover}` and sends
     `prepare_req` to all peers, as after a recovery (fact 5). A node
     whose promise names its own pid takes a ballot above the promise
     (`ballot_above_promise`), never the old ballot again.
   - A build in progress (plan-reconfiguration 2.7) continues as now,
     with or without the mark: its store holds no promise that it sent.
   - Every other start retires the node.
4. A retired node (name from the model: `retired`):
   - votes never, sends no Sequence Paxos and no Ballot Leader Election
     message, and answers none, with one exception: it answers
     `build_request` as plan-reconfiguration 2.9 says;
   - gives `client_errc::retired` (provisional) to each client call;
   - retires again at each later start, because a start without a mark
     that agrees retires;
   - `node::retired()` returns true. The caller deletes its stores after
     the next configuration has started without it.
5. `sequence_paxos::with` queues no recovery any more. The start is the
   decision of `node`; `sequence_paxos` gets it as a value
   (`start_kind`: first, clean, build, retired; provisional names). The
   tests of `sequence_paxos` alone that need the recovery start it
   through `start_recovery(nonce)` (provisional), the call that the
   repair path uses too.
6. After a damage of the own log (fact 2) and after a proof (fact 6):
   section 6, question 2.

## 3. What this plan does not do

- No automatic reconfiguration after a retirement. The caller calls
  `reconfigure` with new pids.
- No path for a crash of a majority at the same time; the operator
  decides (owner 13:12).
- No change to the model: fact 4 holds for the code.
- Two copies of one stopped directory that both start are the clone
  case (owner 2026-10-17, hidden identity); it is not built here.

## 4. Tests

1. Storage: node.db has no mark at first; the mark and its deletion each
   survive a close and an open; a copy of konsens.db holds no mark.
2. A node that stops cleanly and starts again votes: it sends
   `prepare_req`, it answers a prepare, and the cluster decides with it
   while one other node is down. A leader that stops cleanly and starts
   again leads only with a higher ballot.
3. A start without `stop()` retires: no message, `client_errc::retired`
   for a client call, and a second start retires too.
4. A start with the mark deletes it: a second start without `stop()`
   retires. A stop before the first promise, and a stop during a build,
   leave no mark that a later start after a crash can use.
5. A mark that does not agree retires: a konsens.db restored from an
   older copy, a node.db of another node, a mark of another pid.
6. Wrong uses: `stop()` after a write error, on a retired node, while a
   step is open; a call after `stop()`. A call that waits gets
   `client_errc::stopped`.
7. A retired old node answers `build_request` after its StopSign is
   decided (replaces test_reconfiguration.cpp:342).
8. A whole cluster that stops cleanly starts again and elects a leader
   without the operator (the question of the owner, 13:10).
9. The starts of fact 7 change: a restart that expects the old recovery
   calls `stop()` first and keeps its node.db, or expects the
   retirement. The simulations pick for each crash one of two kinds, a
   clean stop with a start, or a crash that retires; a rollback retires.
   The check that a majority decides counts only the nodes that are not
   retired, and a run with a retired majority checks safety only.
   counter_stress stops each node cleanly in `restart` and
   `lose_power`, and gets a crash that retires as a separate option.

## 5. Order

This plan, review 1 (done), the owner questions of section 6, the code
of 2.1 to 2.5 with the tests of 4.1 to 4.8, then the tests and the
example of 4.9, then the full suite.

## 6. Questions to the owner

1. A first start can follow a lost sync too: a node sends its first
   promise, loses that sync and starts on an empty store. Options:
   (a) the caller states a first start (a field of `node_config`); a
   start on an empty store without it retires; (b) an empty store always
   votes. Proposal: (a).
2. A damage of the own log, and a proof: options: (a) the node retires
   (the repair code goes); (b) the repair keeps the higher promise of
   its own and of the peer, and a model run checks it first. Proposal:
   (a).
3. Names: `node::stop()`, `client_errc::retired`, `client_errc::stopped`,
   `start_kind`, `start_recovery`, the row `clean_stop`. Proposal: as
   written.

## 7. Review 1 and changes

1. A stale mark after a stop with promise `ballot{}`: every start
   deletes the mark (2.3).
2. The mark was not tied to the store: it holds the pid, the
   configuration_id and the whole meta record (2.1, item 16).
3. A lost first sync: question 1.
4. `stop()` after a failed sync: a wrong use (2.2).
5. A retired node that stops: a wrong use (2.2).
6. The repair is a rollback: fact 2, question 2.
7. A start after a proof retires: fact 6, question 2.
8. Phase `none` after a clean start: `{follower, recover}` with
   `prepare_req` (2.3).
9. A bool with no safe default: `start_kind` and no recovery in `with`
   (2.5).
10. and 11. The list of restarts: fact 7.
12. A stop without a majority: waiting calls get an error (2.2).
13. A leader that restarts: `ballot_above_promise`, test 4.2.
14. "Takes no action": "votes never" (fact 4).
15. Wording of the replaced section 14: corrected.
16. During the build: the random store identity of 2.1 is dropped. The
    whole meta record with `last_chain_crc` already differs between two
    stores that hold another log, and a store with the same log and the
    same meta is the same state. The identity added a column and a
    migration with no case that the meta record misses.

## 8. Data on the disk

Listed at the end of the build.
