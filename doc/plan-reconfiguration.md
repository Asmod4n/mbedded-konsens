# Plan: reconfiguration by StopSign

Source: the owner, 2026-10-27 13:15 ("Ja" to: plan and build the
reconfiguration first). Form B of the rejoin (doc/plan-sqlite-storage.md
section 14) needs it: a node that crashed joins again as a new node
through a StopSign. New nodes and stage 2 of the single-node mode need
it too.

Base: branch `sqlite-storage` at 57907b9. Branch: `reconfiguration`.

Marks: "provisional" marks a form that waits on the owner. Section 6
lists the findings of review 1 and what changed.

## 1. Facts checked in this session

1. The C++ code has the entry kind `stop_sign` (storage.hpp:31) and no
   reconfiguration. `sequence_paxos.hpp` does not name it. `node`
   treats it as a normal entry (node.hpp:1193, 1272). An append of a
   StopSign while the node holds a request in work gives
   `client_errc::retry` (node.hpp:260). The check reads only the work
   of the node that appends (node.hpp:759).
2. The configuration is not in the store. The caller passes
   `node_config` (configuration_id, pid, peers) at each start
   (node.hpp:39-46). Each ballot carries `config_id` (storage.hpp:19).
   A message carries no configuration id (message.hpp:254-286).
3. Omni-Paxos at 532ff79:
   - `reconfigure` makes a `StopSign` with the next `ClusterConfig` and
     optional metadata (sequence_paxos/mod.rs:310-327). A leader in
     the prepare phase buffers it; a leader in the accept phase
     appends it (`accept_stopsign_leader`, leader.rs:135-146); any
     other node forwards it to the leader (mod.rs:386-399).
   - After a StopSign is accepted, `append` returns
     `PendingReconfigEntry` and `reconfigure` returns
     `PendingReconfigConfig` (mod.rs:299-316). When the synced log
     holds a StopSign, the leader drops its other buffered proposals
     (leader.rs:264-274).
   - `is_reconfigured` returns the StopSign when it is decided
     (mod.rs:285-288).
   - The user starts the instance of the next configuration and gives a
     new node the log or the state up to the StopSign
     (docs/omnipaxos/reconfiguration.md).
4. The model `SequencePaxos.tla` has the StopSign as the last entry of
   the old log (`StopSignEntry`, `AcceptStopSignLeader`,
   `HandleAcceptStopSign`, `ForwardStopSign`) and `BuildForServer`: a
   server of the next configuration starts with the decided prefix of
   a node j of the old configuration, where j runs, does not recover,
   votes, and has the StopSign decided (`IsReconfigured`). The
   property `BuildOnlyFromRunning` checks it. The two configurations
   share no node (`ASSUME NextConfigNodes \cap Nodes = {}`,
   SequencePaxos.tla:3543). `StopSignAllowed` is checked only in
   `Reconfigure` (2388), not in `HandleForwardStopSign` (1247-1259).
   No run had Execute-Verify and a reconfiguration together.
5. `get_state` and `new_state` (message.hpp:142-161) carry entries
   from an index. `handle_get_state` (sequence_paxos.hpp:1656-1678)
   sends the whole accepted range in one `new_state`. A message from a
   node that is not in `peers_` is dropped (sequence_paxos.hpp:426).
6. `try_again` is a message of Execute-Verify (message.hpp:226).

## 2. The form

1. `node::reconfigure(const cluster_config& next)` appends a StopSign.
   The entry `stop_sign` carries `cluster_config{configuration_id,
   nodes}` in its payload, as packed CBOR. The next `configuration_id`
   is the current one plus one. `nodes` is not empty and holds no
   duplicate and no zero. It shares no pid with the current
   configuration (model 3543; owner 2026-10-27 13:43). Any other value is a
   wrong use (`std::logic_error`).
2. The StopSign is the last entry of a configuration. A leader in the
   accept phase appends it. A leader in the prepare phase holds one
   buffered StopSign apart from the buffered proposals and appends it
   with the accept sync. A follower forwards it to the leader.
3. The leader checks `StopSignAllowed` for every StopSign it appends:
   its own, a forwarded one, and a buffered one. The check of a
   buffered StopSign runs where the buffer is flushed, in the accept
   sync and in the accept phase, on the log plus the buffered proposals
   before the StopSign (model `lg1 \o bp`). Where the check fails, the
   StopSign is dropped. A local proposer gets `client_errc::retry`. A
   remote proposer gets no message, as for any forwarded proposal
   today (sequence_paxos.hpp:1949); its client asks again after its
   timeout. The model gets the same check, and a run with
   Execute-Verify, client sessions and a reconfiguration checks it
   before the build (section 4.1).
4. After a node holds a StopSign at the end of the log of its
   configuration, `append` and `reconfigure` give the error
   `client_errc::reconfiguring` (owner 2026-10-27 13:43). When the
   leader flushes its buffer, it cuts every proposal after a StopSign
   and every second StopSign. A local proposer of a cut entry gets the
   same error; a remote one gets no message (item 3).
5. A StopSign that is not decided can be removed by an accept sync of a
   new leader, as any entry that is not decided. Then `append` works
   again. Only a decided StopSign ends the configuration.
6. `node::reconfigured()` returns the next configuration once the
   StopSign is decided and applied. At the decide the node:
   - drops its pending reads and read index rounds and gives them the
     error of 2.4;
   - takes no client request and no read index after that;
   - keeps its store. The caller deletes the store of an old node after
     every node of the next configuration has started.
7. The build of a node of the next configuration (model
   `BuildForServer`; names: owner 2026-10-27 13:43):
   - The node starts with `node_config{next.configuration_id, pid,
     peers}` and an empty store in its own directory. A new field
     `build_from` lists the nodes of the old configuration.
   - It sends `build_request{nonce, from_idx}` to one node of
     `build_from`. An old node takes this message from a node that the
     decided StopSign lists, and from no other node.
   - The source answers with `build_response{nonce, from_idx, entries,
     decided_idx, decided_chain_crc}` only when it runs, does not
     recover, votes, and has the StopSign decided. It sends the decided
     prefix, never an entry above its decided index. One answer carries
     at most `max_build_entries` entries (named limit). Else it answers
     `build_refused{nonce}`, and the node asks the next node of
     `build_from`.
   - The node checks the CRC of each record and the chain CRC, and
     writes each part in one transaction. After the part that ends with
     the StopSign it writes, in one transaction, `decided_idx =
     accepted_idx = log length`, the promise and the accepted round as
     the default ballot of the next configuration, and the meta field
     `config_first_idx` = log length + 1. Then it syncs, and only then
     takes part in Sequence Paxos and in Ballot Leader Election.
   - A start with a store whose `config_first_idx` is not set and with
     `configuration_id > 1` continues the build from the end of its
     log. A start of configuration 1 writes `config_first_idx = 1`,
     also when it opens a store written before this change.
   - A start whose `configuration_id` differs from the `config_id` of
     the ballots in the store, or that is above 1 while the store holds
     no decided StopSign before `config_first_idx`, is a wrong use
     (`std::logic_error`).
8. `config_first_idx` defines the scope of 2.4: only a StopSign at an
   index at or above it ends the current configuration. The indexes of
   the next configuration continue the old log. The state that
   Execute-Verify applies is the state of the whole log.
9. A node of the old configuration that restarts after the decide of
   its StopSign serves as a source without the recovery, when its
   synced store holds the decided StopSign. It never votes again.
   Model run: results/20261027T134426Z-build-from-recovering.md.

## 3. What this plan does not do

- No retirement after a crash and no clean-stop mark (plan 14 of
  doc/plan-sqlite-storage.md).
- No compaction and no snapshot. A new node copies the whole log.
- No joint consensus and no change of one node at a time.
- No pid shared by two configurations (owner 2026-10-27 13:43).

## 4. Model and tests

1. Model, before the build: `HandleForwardStopSign` and the accept sync
   of a buffered StopSign check `StopSignAllowed`. Runs:
   `MC_ev_reconf.sim.cfg` (Execute-Verify, client sessions,
   NextConfigNodes {6, 7, 8}, MaxReconfigure 1) with the invariant
   `StopSignCoversRequests` (EVReconfigure.tla), first without the
   check (expected: violated), then with it (expected: no error); the
   cover `CoverReconfigured` (expected: violated). Done:
   results/20261027T132108Z-ev-reconfigure.md. The run with the check
   is a simulation of 15 min, not a full search.
2. Unit tests of `sequence_paxos`: a StopSign appended by a leader in
   the accept phase; buffered in the prepare phase; forwarded by a
   follower; refused when the leader holds a request in work, for its
   own, a forwarded and a buffered StopSign; an append after a StopSign
   gives the error of 2.4; entries after a StopSign in the buffer are
   cut; an undecided StopSign removed by an accept sync, after which
   `append` works; `reconfigured()` only after the decide; a pending
   read across the decide gets the error.
3. The wire: `cluster_config` in the payload, `build_request`,
   `build_response`, `build_refused`; a truncated input and a payload
   with a duplicate node each fail as a value.
4. A cluster test on the SQLite store: three nodes reconfigure to three
   new nodes; the next configuration decides entries; S1, S2, S2b, S3,
   S5 and S7 hold after each step across both configurations; a client
   session and its duplicate request across the configurations; a
   target of one node.
5. The source rules of 2.7: no answer from a node that recovers, from a
   node without a decided StopSign, from a node that does not vote, to
   a node that the StopSign does not list; never an entry above the
   decided index.
6. A seed node of configuration 1 and a store written before this
   change open with `config_first_idx = 1`; a start with a wrong
   `configuration_id` is a wrong use; a crash of the new node during
   the build, then a start that continues; a damaged record in a part (CRC) gives a request to the
   next source; a node of configuration 2 that restarts with the
   StopSign in the middle of its log.
7. A new user calls `reconfigure` with the same configuration id, with
   no node, with a pid of the current configuration: each is a wrong
   use.
8. The test run during the work covers the changed files; the full
   suite runs after that.

## 5. Order

This plan, review 1 (done), the model run of 4.1, the cards to the
owner, the code of 2.1 to 2.6 with the tests of 4.2 and 4.3, then 2.7
to 2.9 with 4.4 to 4.6.

## 6. Review 1 and changes

1. Shared nodes are not in the model: the configurations are disjoint
   now (2.1; owner 2026-10-27 13:43).
2. No run had Execute-Verify with a reconfiguration, and a forwarded
   StopSign is not checked: 2.3 and 4.1.
3. A new node's messages are dropped, and the recovery path does not
   fit: a separate build path (2.7).
4. `get_state` sends one message and the accepted log: the build sends
   bounded parts of the decided prefix (2.7).
5. `try_again` has a meaning: new names (owner 2026-10-27 13:43).
6. At least three nodes and the own pid: removed; any non-empty set
   (2.1).
7. The StopSign in the general buffer: a separate buffer and cut
   entries (2.2, 2.4).
8. The scope of "holds a StopSign": `config_first_idx` (2.8).
9. A partial build: transactions per part and a resumed build (2.7).
10. An old node restarts after the decide: it serves without the
    recovery (2.9).
11. Reads across the decide: dropped with an error (2.6).
12. The same pid in two configurations: excluded by 2.1.

Review 2 approved the plan with four fixes: the reply to a remote
proposer (2.3), the check at each flush (2.3), `config_first_idx` of
configuration 1 (2.7), and the wrong-use check at the start (2.7).

## 7. Data on the disk

Listed at the end of the build.
