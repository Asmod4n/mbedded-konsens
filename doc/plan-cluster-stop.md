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
   `entry_kind::cluster_stop` (provisional, appended to the enum). On a
   follower the entry goes to the leader as every other entry does. The
   entry has no payload and changes no state when it is applied.
2. After the leader accepted a `cluster_stop` entry, it appends nothing
   else in its ballot. Each later call answers
   `client_errc::stopped`.
3. When the leader decides the entry under its own ballot, it sends the
   decide to every peer, and after it the message `cluster_stop`
   (provisional) with its ballot. TCP keeps the order (owner,
   2026-10-20). The node then says to its caller that it stops:
   `node::stops()` (provisional) is true.
4. A node takes `cluster_stop` from the peer that leads its promised
   ballot, and only when the ballot of the message is its promise. A
   node in a repair takes it when the ballot of the message is not below
   its stored promise. A retired node ignores it. After that,
   `stops()` is true.
5. The two rules of the owner:
   - The entry acts once. The entry itself does nothing when it is
     applied, so a start that applies it again does nothing. Only the
     message stops a node, and no start sends the message.
   - The entry acts only under the ballot that decided it. Only the
     leader that decided it sends the message, at that decide. A leader
     of a later ballot sends nothing for an old entry. A node that was
     down at the stop catches up and stays running.
6. `node::stop()` is allowed in a repair:
   - the mark carries the verified index bounded by the first damaged
     index;
   - the next clean start finds the damage again (fact 5) and repairs.
7. The count of repairs is written when a repair ends, not when it
   starts. A repair that a clean stop interrupts does not count, so the
   next start repairs by itself again. A repair that waits for the
   operator stays waiting after a clean stop and the next start.
8. konsensd: `POST /cluster/stop` (provisional) on the control socket
   calls `stop_the_cluster`. It answers 202 when the entry is appended,
   and the error of the call otherwise. The loop of `serve` ends when
   `stops()` is true and every send has completed. Then `stopped`
   calls `stop()`, also in a repair.
9. A SIGTERM keeps its behaviour (question 2 in section 7).

## 3. The model first

Before the code, the model checks the two parts that touch safety:

1. `CleanStopInRepair(i)`: a node that waits in a repair (it left
   `running` through `LeaveRunning` or waits for the operator) restarts
   later with its own durable state and no recovery, and the start
   finds the damage again. The invariants of `SequencePaxos.tla` hold.
   Mutation: the restart takes the verified index of the mark above the
   damage, so the damaged entry counts as verified. The mutation must
   find a violation.
2. `ClusterStop`: the leader decides the entry and every node with that
   promise stops cleanly and restarts with no recovery. A node that was
   down catches up and does not stop. Mutation: a later leader sends the
   message for the old entry; the check that the cluster serves again
   after the restart must fail.

BFS with small bounds (owner, 2026-10-25): three nodes, one damage, one
stop. Each result is one file in `model/results/`.

## 4. Tests

1. test/test_repair.cpp: a node in a repair stops cleanly; its next
   start repairs from a peer by itself; the count is still 0 before the
   end of the repair and 1 after it.
2. test/test_repair.cpp: a node that waits for the operator stops
   cleanly and waits for the operator again after its start.
3. test/test_cluster_stop.cpp (new): three nodes, a call on a follower;
   every node says `stops()`, every node stops cleanly, every node
   starts clean and votes at once; a write after the restart is
   decided. A node that was stopped before the call starts after the
   restart, catches up over the entry and stays running.
4. test/test_cluster_stop.cpp: a node in a repair takes the message and
   stops; a message with a ballot below its promise does not stop it.
5. test/test_wire.cpp: `cluster_stop` round trip, and the old tags keep
   their numbers.
6. program/test/three_nodes.sh: the block of the repair (plan
   konsensd-stop-and-repair section 3) ends with `POST /cluster/stop`
   on node 1 instead of the start of nodes 1 and 3 for the repair; all
   three processes end with 0, and all three start clean.
7. Each change fails its test without it.

Release g++ 16 in podman with the io_uring seccomp profile: the tests
above, test_node, test_rest. The CI runs every build.

## 5. Order of the work

1. Model (section 3), results pushed.
2. Library: 2.6 and 2.7 with tests 4.1 and 4.2.
3. Library: 2.1 to 2.5 with tests 4.3 to 4.5.
4. konsensd: 2.8 with test 4.6, README.

## 6. Review

(to be filled by the review)

## 7. Open questions to the owner

1. The names: `stop_the_cluster`, `stops`, `entry_kind::cluster_stop`,
   the message `cluster_stop`, the path `/cluster/stop`. No
   specification names them. Proposal: these.
2. A SIGTERM in a repair waits today until the repair ends. With 2.6, it
   can stop cleanly at once, and the repair runs again at the next
   start, uncounted. Proposal: stop at once, so a single node never
   waits for a peer that is gone.

## 8. Data

The build directories, which a build command makes again, and the TLC
state directories, which are deleted after each run.
