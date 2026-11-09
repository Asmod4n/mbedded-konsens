# Plan: the stop of the whole cluster

Source: the owner, 2026-11-09 13:41 to 14:04. A shutdown of the whole
cluster ends clean on every node. It is a method of its own: a call on
any node writes an entry into the log, and every node stops cleanly. A
node in a repair stops cleanly too, and the repair does not count.

Base: main 4199485. Branch: `cluster-stop`.

Marks: "provisional" marks a name that waits on the owner. Section 7
lists the open questions.

## 1. Facts checked in this session

1. The nightly Jepsen run on main f40582f (branch `jepsen-nightly`
   1df72b9, rw-register, node 2) started at 01:32:22, found a damage
   and started its repair. Nodes 1 and 3 were retired, and nodes 4 and
   5 were on the other side of a partition (`ss`: three connects in
   SYN-SENT). A retired node answers no `repair_request`
   (sequence_paxos.hpp:670). The SIGTERM at 01:32:37 waited for the end
   of the repair (serve.cpp:1082, `the_repair_holds_the_stop`), as
   doc/plan-konsensd-stop-and-repair.md 2.2 says. Jepsen sent SIGKILL
   after 60 s, and the node started retired.
2. The same wait happens when the operator stops all nodes: the peers
   stop first, and the node in the repair waits for a peer that is gone.
3. `node::stop()` is a wrong use in a repair (node.hpp:267).
4. `start_the_repair` writes the count of repairs before the repair
   runs (node.hpp:1174). A start that finds a damage repairs only on a
   clean start (node.hpp:232-235), and only while the count is below
   `automatic_repairs` (node.hpp:1168).
5. A clean start scans the whole log from index 0
   (`scan_the_whole_log`, `count_the_verified_entries`,
   sequence_paxos.hpp:1080, 2337), whatever verified index the mark
   holds.
6. `reconfigure` appends a StopSign through `append_an_entry`
   (node.hpp:525-576). A follower forwards an entry to the leader in
   `proposal_forward` (sequence_paxos.hpp:1704).
7. `paxos_msg` ends with `repair_entries` (message.hpp:246-249). The
   wire only grows at its end (owner, 2026-11-08).
8. The control socket of konsensd has one call,
   `POST /repair/allow-one-more` (rest.hpp:16).

## 2. The form

1. `node::stop_the_cluster()` (provisional) appends an entry of the kind
   `entry_kind::cluster_stop` (provisional, appended at the end of the
   enum). On a follower the entry goes to the leader as every other
   entry does. The entry has no payload, is not a request
   (`entry_is_a_request` is false for it), and changes no state when it
   is applied.
2. Rules for the entry, beside the StopSign:
   - it is refused after an accepted StopSign, and `reconfigure` is
     refused after an accepted `cluster_stop`;
   - after the leader accepted it, it appends nothing else in its
     ballot; entries in `buffered_proposals_` after it are cut, as
     `cut_the_buffer_after_a_stop_sign` does (sequence_paxos.hpp:1711),
     and their callers get `client_errc::stopped`.
3. When the leader decides the entry under its own ballot, it sends the
   decide, and after it the message `cluster_stop` (provisional) with
   its ballot and the decided index, to every peer of the configuration,
   not only to the promised followers (`decide_if_chosen` sends to the
   promised followers only, sequence_paxos.hpp:2119). TCP keeps the
   order (owner, 2026-10-20).
4. A peer acknowledges `cluster_stop` with `cluster_stop_ack`
   (provisional). The leader sends `cluster_stop` again at each resend
   timeout to every peer that has not acknowledged. No fixed wait stands
   in this path.
5. A node takes `cluster_stop`:
   - as a voting node, when the ballot of the message is its promise;
   - in a repair, when the ballot of the message is not below its
     stored promise. `handle` gets a branch for it in the block for
     `repair_` (sequence_paxos.hpp:676-688), which today takes only
     `repair_request` and `repair_entries`;
   - a retired node ignores it.
   It sends the acknowledgement, and `node::stops()` (provisional) is
   true.
6. The leader says `stops()` when every peer has acknowledged.
   `node::peers_that_did_not_stop()` (provisional) names the others, so
   that the operator stops them with SIGTERM.
7. The two rules of the owner:
   - The entry acts once. The entry does nothing when it is applied, so
     a start that applies it again does nothing. Only the message stops
     a node, and no start sends the message.
   - The entry acts only under the ballot that decided it. Only the
     leader that decided it sends the message. A leader of a later
     ballot sends nothing for an old entry. A node that was down at the
     stop catches up and stays running. When the leader changes before
     the decide, the call on the node answers that the stop did not
     happen, and the operator calls again.
8. `node::stop()` is allowed in a repair. The mark keeps
   `get_verified_idx()` unchanged, as `stop()` writes it today
   (node.hpp:275-277). `verified_idx_` is the Execute-Verify index
   (sequence_paxos.hpp:1299-1317) and not a CRC check; a lower value
   makes a promise report fewer accepted entries (wrong assumption 17
   of doc/plan-clean-stop.md). The next clean start scans the whole log
   (fact 5), finds the damage again and repairs.
9. The count of repairs is written when a repair ends, not when it
   starts. A repair ends at two places, and both write the count, read
   again at that moment, plus one:
   - the local repair in `start_the_repair` that reaches `vote_again`
     (node.hpp:1177-1189);
   - `write_the_passed_copies` reaching `vote_again`
     (sequence_paxos.hpp:2334), which the node sees at
     `repaired && votes()` (node.hpp:464-470).
   `allow_one_more_repair` writes 0 as before. A repair that a clean
   stop interrupts does not count, so the next start repairs by itself
   again. A repair that waits for the operator still waits after a
   clean stop and the next start.
10. konsensd: `POST /cluster/stop` (provisional) on the control socket
    calls `stop_the_cluster`. The call answers 200 when this node says
    `stops()`. It answers 503 when the leader changes before the
    decide, and the error of the call otherwise. The loop of `serve`
    ends when `stops()` is true and every send has completed; a socket
    closes with `shutdown(SHUT_WR)` before `close`. Then `stopped`
    calls `stop()`, also in a repair. A repair that failed to start
    after the operator allowed it (`the_repair_failed`, serve.cpp:654,
    704-709) also stops cleanly now: the next start finds the damage
    and waits for the operator again.
11. A SIGTERM keeps its behaviour (question 2 in section 7).

## 3. The model first

`SequencePaxos.tla` has no clean stop and no start without recovery. A
node that `LeaveRunning` takes is refused for ever, and `DamageFound`
copies the whole durable state of a source in one step, where the code
replaces ranges in place and keeps its own promise. So the model first
gets what the code has:

1. `CleanStop(i)` and the clean start: a restart from the node's own
   durable state, with no recovery, with the verified index of the
   mark. This is form B of doc/plan-clean-stop.md, which the model did
   not have.
2. A repair in place: a node with a damage waits in a repair, keeps its
   promise, takes the damaged entries from a source and votes again;
   the count of repairs moves to the end of the repair (`RepairsBounded`
   and `DamageFound` count at the start today).
3. `CleanStop` is enabled in a repair. Mutation: the mark carries the
   verified index bounded at the damage; it must find the violation of
   S2b.
4. `ClusterStop`, as a safety invariant: a node stops only on a
   `cluster_stop` whose ballot equals its promise when it takes it (or
   is not below it in a repair), and only for an entry decided under
   that ballot. Mutation: a leader of a later ballot sends the message
   for an old entry; the invariant must fail. A reachability check: a
   node that is down at the stop catches up over the entry and runs.

BFS with small bounds (owner, 2026-10-25): three nodes, one damage, one
stop, each run under five minutes or reported with its time. Each
result is one file in `model/results/`.

## 4. Tests

1. test/test_repair.cpp: a node in a repair stops cleanly; its next
   start repairs from a peer by itself; the count is 0 before the end
   of the repair and 1 after it. The same for a local repair.
2. test/test_repair.cpp: the verified index is above the damage; after
   the repair and a new leader, the entries between them are kept.
3. test/test_repair.cpp: a node that waits for the operator stops
   cleanly and waits for the operator again after its start.
4. test/test_cluster_stop.cpp (new): three nodes, a call on a follower;
   every node says `stops()`, every node stops cleanly, every node
   starts clean and votes at once; a write after the restart is
   decided. A node that was stopped before the call starts after the
   restart, catches up over the entry and stays running.
5. test/test_cluster_stop.cpp: a lost `cluster_stop` is sent again at
   the resend timeout; a peer that never answers stays in
   `peers_that_did_not_stop()`; a leader change before the decide
   stops nobody.
6. test/test_cluster_stop.cpp: a node in a repair takes the message and
   stops; a message with a ballot below its promise does not stop it.
7. test/test_wire.cpp: `cluster_stop` and `cluster_stop_ack` round trip,
   and the old tags keep their numbers.
8. program/test/three_nodes.sh: the block of the repair ends with
   `POST /cluster/stop` on node 1; all three processes end with 0, and
   all three start clean.
9. Each change fails its test without it.

Release g++ 16 in podman with the io_uring seccomp profile, only the
tests above. The CI runs every build.

## 5. Order of the work

1. Model (section 3), results pushed.
2. Library: 2.8 and 2.9 with tests 4.1 to 4.3.
3. Library: 2.1 to 2.7 with tests 4.4 to 4.7.
4. konsensd: 2.10 with test 4.8, README.

## 6. Review

One review (2026-11-09) rejected the first form. What changed:

1. The mark bounded the verified index at the damage. That is the S2b
   fault of wrong assumption 17: 2.8 keeps the index.
2. The model has no clean stop and no repair in place: section 3 adds
   both first, and the mutation is item 1.
3. "Serves again" is no BFS invariant: 3.4 states a safety invariant
   and a reachability check.
4. A lost message left a node running: 2.4 and 2.6 add the
   acknowledgement, the resend and the list of nodes that did not stop.
5. "202 when appended" was wrong on a follower: 2.10 answers at
   `stops()`.
6. A node in a repair drops every other message: 2.3 and 2.5.
7. The count has two end points: 2.9.
8. konsensd and a failed repair: 2.10.
9. Rules for the entry beside the StopSign: 2.1 and 2.2.
10. Only the tests of the change run: section 4.

Not checked: an older binary that reads the new `entry_kind` value. The
wire only grows at its end; a store with a `cluster_stop` entry is read
by this version and later ones.

## 7. Open questions to the owner

1. The names: `stop_the_cluster`, `stops`, `peers_that_did_not_stop`,
   `entry_kind::cluster_stop`, the messages `cluster_stop` and
   `cluster_stop_ack`, the path `/cluster/stop`. No
   specification names them. Proposal: these.
2. A SIGTERM in a repair waits today until the repair ends. With 2.8, it
   can stop cleanly at once, and the repair runs again at the next
   start, uncounted. Proposal: stop at once, so a single node never
   waits for a peer that is gone.

## 8. Data

The build directories, which a build command makes again, and the TLC
state directories, which are deleted after each run.
