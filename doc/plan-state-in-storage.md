# Plan: the state machine in LMDB (stage D of plan-storage-only-source.md)

This plan gives the code form of section 2.6 of
doc/plan-storage-only-source.md. The model is `StateInLmdb`, `StateRun`,
`StateApply`, `StateAbort`, `StateCurrent` and `StateFollows`
(model/tla/SequencePaxos.tla, rounds 12 to 14). The owner chose, on
2026-10-25 07:22 and 07:23: the state machine reads through a view
and gives back a write list, and an entry carries the write list of
the leader beside the result.

The work has three parts. Each part is one commit with its tests. D1
is the form that the model checks (`StateApply`, with no run before the
decide). The gate on `accepted`, the run of the leader before the
append and the follower that differs are not in the model (review of
this plan, findings 3 to 5). So D2 and D3 wait for a model round of
their own, its TLC runs and a review; D1 is built first.

- D1: the state environment, the concept, the apply after the decide,
  the reads from a read transaction.
- D2: Execute-Verify: the leader runs a request before it proposes it,
  the entry carries the result and the write list, a follower runs it
  again and compares.
- D3: the open state transaction over many steps, `max_in_flight`, the
  abort on a cut and the run again after a partial decide.

## 1. The state environment (D1)

1. A new class `state_storage` (provisional) opens its own LMDB
   environment in a directory of its own, with the page checksum and
   `MDB_REMAP_CHUNKS`, and without `MDB_WRITEMAP` (2.6 item 8). It has
   three databases: `state` (the keys and values of the machine),
   `sessions` (the client table: session id to the last request number
   and its reply) and `applied` (one record with a CRC: the applied
   index and the chain CRC of the log at the applied index, 2.4 item 9).
2. It gives a `read_only_transaction` and a `transaction`, as
   `persistent_storage` does. Both have `find(key)` and
   `find_session(session)`. The `transaction` also has
   `insert_or_assign(key, value)`, `erase(key)`,
   `insert_or_assign_session(session, record)`, `erase_session(session)`,
   `set_applied(index, chain_crc)` and `commit() &&`. The names come
   from `std::map` and from LMDB (commit). No LMDB name leaves the
   library: the types are named for the library.
3. `node::with` takes a `state_storage` after the `node_storage`. The
   node owns it. At a start the node reads the applied index from it
   and runs the decided entries after it.
4. A start check: the chain CRC at the applied index in the state
   storage must equal the chain CRC of the log at that index. A
   difference, or an applied index above the log length, is a damage
   (2.1 item 5). An applied index above the decided index is kept: its
   entries are decided (2.4 item 10, model `StateIsDecided`). Until
   item 9 of 2.4 stands, the node then stays out of the vote and reports
   both CRCs.
5. With item 9 of 2.4, a repair switches the state directory through a
   name in the node storage, and the node opens the state storage from
   that name. Until then, `node::with` takes the opened storage, as for
   the log (stage E left out item 9 too).

## 2. The concept (D1)

```
struct mutation {
    std::vector<std::byte> key;
    std::optional<std::vector<std::byte>> value;
};

struct applied {
    std::vector<std::byte> reply;
    std::vector<mutation> mutations;
};

template <typename M>
concept state_machine = std::movable<M> &&
    requires(const M& m, std::span<const std::byte> payload, const write_view& w, const read_view& r) {
        { m.apply(payload, w) } -> std::same_as<applied>;
        { m.query(payload, r) } -> std::same_as<std::vector<std::byte>>;
    };
```

1. `state_view` is a concept with `find(key) ->
   std::optional<std::span<const std::byte>>`. `transaction_view`
   (provisional) gives it over a transaction of the state storage:
   `write_view` over the open write transaction for `apply`, `read_view`
   over a read transaction for `query`. The span stays valid until the
   transaction ends; the machine writes nothing during `apply`, so LMDB
   keeps the page. The machine has no state of its own, so both members
   are const. A machine takes the view as `const state_view auto&`.
2. The name `mutation` comes from FoundationDB. A mutation with no
   value erases the key.
3. A user error is a reply, for example RFC 9290 in CBOR, with no
   mutations (owner, 2026-10-25 09:23). `apply` returns no error.
4. A `find` that fails is a fault of the own node: the view throws
   `read_error`, a type derived from `std::system_error` (owner,
   2026-10-25 09:26). No machine can turn it into a reply. A write to the
   state storage that fails throws `write_error`.

## 3. Apply after the decide (D1)

1. After the commit of each step, the node takes the decided entries
   after the applied index (`read_decided_suffix`), as now.
2. It opens one write transaction of the state. For each entry it runs
   the kind: `register_client` inserts a session, `expire_session`
   erases it, `client_request` checks the session and the request
   number as `execute_request` does now, runs `apply`, and writes the
   mutations and the new session record.
3. It writes the applied index and the chain CRC at it, and commits.
   A failed commit throws `sync_error`, as for the log.
4. The replies leave only after that commit.
5. `client_query` runs `query` on a read transaction of the state,
   after the read index is reached, as now.
6. `node` keeps no client table and no applied index in memory. It
   reads them from the state storage. `find_client_session(session)`
   and `get_applied_idx()` read a read transaction of the state, and
   throw `read_error` when the read fails. `expire_session` reads the
   session from a read transaction too.
7. `register_client` takes the log index of its entry as the session,
   as now; in D1 the index is known, because the entry runs after the
   decide.

## 4. Execute-Verify (D2)

1. An entry of kind `client_request` gets three fields: `reply`
   (bytes), `error` (an error value, 0 for none) and `writes` (the write
   list). The file format and the wire carry them; the format version
   goes up (plan 2.9). The three fields are part of the encoded entry,
   so the entry CRC and the chain CRC cover them, and `StateCurrent`
   sees a replaced entry with other writes as a cut. The chain CRC in
   `accepted` takes the place of "the result in `accepted`" of 2.9.
2. Every kind that changes the state runs, in log order, on the leader
   and on each follower: `register_client`, `client_request`,
   `expire_session` (review, finding 1).
3. The leader runs each client request before it appends it. The node
   takes the buffered proposals of `sequence_paxos` before its commit
   (`take_proposals_to_run()`), runs them in log order in the open state
   transaction, fills the three fields, and gives them back
   (`propose_ran(entries)`). A forwarded proposal goes to the buffer in
   the phase accept too, so it runs as well; it is accepted at the same
   commit, so no message leaves earlier.
4. A follower runs each new entry that changes the state from the
   log, in log order, in its open state transaction, before its
   `accepted` leaves. It compares its reply, error and write list with
   the fields of the entry, floats within 1 ULP (question 1). When they
   agree, it writes the write list of the entry, not its own, so the
   state stays equal on every node (owner, 2026-10-17 12:40). When they
   differ, the follower does not send `accepted` for that index or
   later ones, and it reports the entry, the leader and both CRCs as a
   new proof `result_mismatch` (provisional). It votes on; it waits for
   the leader to change (question 2).
5. `sequence_paxos` gets the bound of the verified index: an `accepted`
   leaves only up to `verified_idx` (provisional), which the node sets
   with `verified(index)`. `verified` queues an `accepted` when the
   bound rises above the last one sent in the ballot, and
   `resend_message_timeout` sends it again (review, finding 3). The
   leader counts itself accepted only up to its own verified index.
6. A new leader runs the suffix of its sync and the proposals of the
   phase prepare through the same run, and compares the suffix as a
   follower does (review, finding 2).
7. Entries at or below the decided index are applied in one step, in
   chunks, without the bound of D3; the bound counts only entries that
   are not decided (review, finding 4).
8. A follower whose result differs on an entry that is then decided by
   others has a damage of 2.6 item 6, with the repair of 2.4 (review,
   finding 5).
9. After the decide, the state commit compares nothing again: the run
   before the decide compared the result, and the writes are those of
   the entry. A run again after an abort (D3) compares again; a
   difference there is a damage of 2.1 item 5 (plan 2.6 item 6), with
   the repair of 2.4.

## 5. The open transaction and the pipeline (D3)

1. The state write transaction stays open over many steps. It holds
   the runs of the entries after the applied index, up to
   `max_in_flight` entries (provisional, a member of `node_config`,
   with no default: section 5 of the parent plan measures it first). The
   leader proposes no more client requests while the bound is full; the
   requests wait in the buffer.
2. At each step, after the log commit: when the decided index covers
   every entry that ran, the node writes the applied index and commits.
   When it covers only a part, the node aborts the transaction, opens a
   new one, runs the decided entries again (comparing them, item 4.5),
   commits, opens a new one and runs the rest again.
3. Before each run and each commit, the node compares the chain CRC of
   the last entry that ran in the open transaction with the chain CRC
   of the log at that index (2.6 item 7). A difference means that a
   write cut or replaced the log; the node aborts and runs the entries
   after the applied index again.
4. A crash, a recovery and a repair abort the open transaction (model
   `StateFollows`). A repair keeps or copies the state as 2.4 item 10
   says, and a wipe loses the state.
5. The two write transactions, of the log and of the state, stand in
   one thread. The test of plan section 4 runs many steps with commits,
   aborts and a reader in each environment.

## 6. Tests

- "a state change commits only after the decide";
- "a read of the state comes from a read transaction";
- "a write transaction of the log and an open write transaction of the
  state stand in one thread over many steps";
- "a result after the decide that differs from the entry stops the
  vote";
- "a follower whose result differs does not accept and reports";
- "a follower writes the write list of the leader";
- "a cut of the log aborts the open state transaction";
- "a partial decide runs the decided entries again and commits them";
- "a start whose applied chain CRC differs from the log stops the vote".

## 7. Questions for the owner

1. Decided, owner 2026-10-25 09:20 and 09:22: the library compares
   internally and invisibly to the state machine. The reply and each
   value of the write list are deterministic CBOR (RFC 8949 section
   4.2; owner 2026-10-17). The library walks both items; a float agrees
   within 1 ULP, every other item byte for byte. The bytes of the leader
   are stored. Contracts are a framework of their own.
2. The leader that differs steps down after three tries (2026-10-17
   05:08). When the leader's machine differs, both followers differ,
   heartbeats keep the leader, and nothing is decided. Proposal: a
   follower that differs stops answering the heartbeats of that
   leader, so a new leader is elected; the three tries come in a stage
   of their own. This goes into the model of D2.
3. Decided in the plan after the review: a run in which a `find` failed
   is a storage failure (2.4).
4. Decided, owner 2026-10-25 09:23: a user error comes back as a reply
   with no mutations, so `apply` returns `applied` and no error, and no
   nested transaction is needed.
5. Decided, owner 2026-10-25 09:26 to 09:30: a fault that a node finds in
   itself throws. A peer that delivers a wrong value is asked to try
   again; when the value is still wrong, the peer is told that it is
   broken. The majority shows which value is right. One repair runs
   automatically, and each further repair waits for the operator. This
   goes into the model of D2, together with a damage of the own log as an
   exception (card of 2026-10-25 09:29: the restart checks the whole log
   and repairs). It replaces the proposal of item 2.
