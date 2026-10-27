# Plan: reconfiguration by StopSign

Source: the owner, 2026-10-27 13:15 ("Ja" to: plan and build the
reconfiguration first). Form B of the rejoin (doc/plan-sqlite-storage.md
section 14) needs it: a node that crashed joins again as a new node
through a StopSign. New nodes and stage 2 of the single-node mode need
it too.

Base: branch `sqlite-storage` at 57907b9. Branch: `reconfiguration`.

Marks: "provisional" marks a form that waits on the owner.

## 1. Facts checked in this session

1. The C++ code has the entry kind `stop_sign` (storage.hpp:31) and no
   reconfiguration. `sequence_paxos.hpp` does not name it. `node`
   treats it as a normal entry (node.hpp:1193, 1272). An append of a
   StopSign while a request is in work gives `client_errc::retry`
   (node.hpp:260, plan 2.3.6 of the model, `StopSignAllowed`).
2. The configuration is not in the store. The caller passes
   `node_config` (configuration_id, pid, peers) at each start
   (node.hpp:39-46). Each ballot carries `config_id` (storage.hpp:19).
3. Omni-Paxos at 532ff79:
   - `reconfigure` makes a `StopSign` with the next `ClusterConfig` and
     optional metadata (sequence_paxos/mod.rs:310-327). A leader in
     the prepare phase buffers it; a leader in the accept phase
     appends it (`accept_stopsign_leader`, leader.rs:135-146); any
     other node forwards it to the leader (mod.rs:386-399).
   - After a StopSign is accepted, `append` returns
     `PendingReconfigEntry` and `reconfigure` returns
     `PendingReconfigConfig` (mod.rs:299-316).
   - `is_reconfigured` returns the StopSign when it is decided
     (mod.rs:285-288).
   - The user starts the instance of the next configuration. A node
     that was not in the old configuration does not see the StopSign;
     the user starts it and gives it the log or the state up to the
     StopSign (docs/omnipaxos/reconfiguration.md). The example keeps
     the pid of a node across configurations (nodes 1, 2, 4).
4. The model `SequencePaxos.tla` has the StopSign as the last entry of
   the old log (`StopSignEntry`, `AcceptStopSignLeader`,
   `HandleAcceptStopSign`, `ForwardStopSign`), and `BuildForServer`:
   a server of the next configuration starts with the decided prefix
   of a node j of the old configuration, where j runs, does not
   recover, votes, and has the StopSign decided (`IsReconfigured`).
   The property `BuildOnlyFromRunning` checks it. Runs c, c-wipe and
   c-wipe-n5 hold (results 20261018T213345Z-c-wipe.md,
   20261027T113350Z-rejoin-new-identity.md).
5. `get_state` and `new_state` (message.hpp:142-161) send entries from
   an index. The recovery uses them (sequence_paxos.hpp:1635, 1682).

## 2. The form

1. `node::reconfigure(const cluster_config& next)` appends a StopSign.
   The entry `stop_sign` carries the next configuration in its payload:
   `cluster_config{configuration_id, nodes}`, encoded as packed CBOR as
   every other structure on the wire. The next `configuration_id` is
   the current one plus one; any other value is a wrong use
   (`std::logic_error`). `nodes` holds at least three nodes, has no
   duplicate and no zero (`validate_cluster`, message.hpp:17).
2. The StopSign is the last entry of a configuration. The path follows
   Omni-Paxos and the model: a leader in the accept phase appends it, a
   leader in the prepare phase holds one buffered StopSign and appends
   it with the accept sync, a follower forwards it to the leader.
3. After a node holds a StopSign in its log, `append` and `reconfigure`
   give the error `client_errc::reconfiguring` (provisional name and
   provisional error path). A client request gets the same error. The
   error tells the client to send the request to the next
   configuration.
4. A StopSign that is not decided can be replaced by a new leader, as
   any entry that is not decided. Only a decided StopSign ends the
   configuration.
5. `node::reconfigured()` returns the next configuration once the
   StopSign is decided and applied. Then the node of the old
   configuration:
   - takes no client request and no read index (error of item 3);
   - answers `get_state` of a node of the next configuration with its
     decided log up to and with the StopSign, in parts, as the recovery
     does now;
   - keeps its store. The caller stops the old node and deletes its
     store after every node of the next configuration has started. The
     library does not delete a store.
6. A node of the next configuration starts with
   `node_config{next.configuration_id, pid, peers}` and a new store in
   its own directory. A node that is in both configurations keeps its
   pid (Omni-Paxos example) and gets a new store. Before it votes, it
   builds its log:
   - It sends `get_state` with `from_idx = 1` to a node of the old
     configuration. The caller gives the list of the old nodes in a new
     field of `node_config`, `build_from` (provisional name).
   - The source answers only when it runs, does not recover, votes,
     and has the StopSign decided (model `BuildForServer`). Else it
     answers `try_again`.
   - The new node checks the CRC of each record and the chain CRC,
     writes the log, sets `decided_idx = accepted_idx = log length`,
     syncs, and only then takes part in Sequence Paxos and in Ballot
     Leader Election of the next configuration.
   - A node of the old configuration builds from its own store when it
     holds the decided StopSign. The copy is `VACUUM INTO` the new
     store (plan-sqlite-storage.md 7.5).
7. A start with `configuration_id > 1` and a store that holds no log is
   the build of item 6. A start with a store whose log has no decided
   StopSign of the previous configuration at its end, and a
   `configuration_id` that differs from the ballots in the store, is a
   wrong use (`std::logic_error`).
8. The log of the next configuration continues the indexes of the old
   log. The StopSign stays at its index. The state that Execute-Verify
   applies is the state of the whole log.

## 3. What this plan does not do

- No retirement after a crash and no clean-stop mark. That is plan 14
  of doc/plan-sqlite-storage.md, which builds on this plan.
- No compaction and no snapshot. A new node copies the whole log.
- No change to the model. The model has the form of section 2.
- No joint consensus and no change of one node at a time.

## 4. Tests

1. Unit tests of `sequence_paxos`: a StopSign appended by a leader in
   the accept phase; buffered in the prepare phase; forwarded by a
   follower; an append after a StopSign gives the error of 2.3; a
   StopSign that is not decided and is replaced after a leader change;
   `reconfigured()` only after the decide.
2. The wire: `cluster_config` in the payload, a truncated payload, a
   payload with a duplicate node, each fails as a value.
3. A cluster test on the SQLite store: three nodes reconfigure to two
   old nodes and one new node; the next configuration decides entries;
   S1, S2, S2b, S3, S5 and S7 hold after each step across both
   configurations.
4. The source rules of item 2.6: no answer from a node that recovers,
   from a node without a decided StopSign, from a node that does not
   vote.
5. A crash of the new node during the build; a damaged record in the
   copy (CRC) gives the error of the copy and a new request to another
   source.
6. A new user calls `reconfigure` with the same configuration id, with
   two nodes, with its own pid missing: each is a wrong use.
7. The test run during the work covers the changed files; the full
   suite runs after that.

## 5. Order

This plan, review, the questions to the owner (items 2.3 and 2.6), the
code of 2.1 to 2.5 with the tests of 4.1 and 4.2, then 2.6 to 2.8 with
4.3 to 4.6.

## 6. Data on the disk

Listed at the end of the build.
