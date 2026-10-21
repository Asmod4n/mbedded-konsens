# Plan: one commit for all messages that wait

Source: the owner, 2026-10-21 17:08 (UTC+2): group commit, then the
CRC32C instruction path. 18:03: nothing that was confirmed may be lost.
18:04: `handle` has no source. This plan gets one review before the
build.

## 1. Facts checked in this session

- bench/results/2026-10-21-144527Z.txt: a write on LMDB costs 2.0 ms,
  64 writes sent at once cost 2.0 ms per write, one LMDB commit of one
  entry costs 349 us (1 KiB, g++ 16 Release).
- `sequence_paxos.hpp` commits once in each handler that writes:
  `accept_entries_leader` (595), `handle_acceptdecide` (757),
  `handle_decide` (778), `handle_accepted` (799), and the rare paths
  `handle_leader`, `handle_prepare`, `handle_majority_promises`,
  `handle_acceptsync`, `complete_recovery`. A write over three nodes
  makes 6 commits; 64 writes make 6 x 64.
- `storage.hpp:83-99`: `append_entries` and `append_on_prefix` hold a
  span of entries. The span is valid only while the message that owns
  the entries is alive (plan doc/plan-no-copy-handle.md).
- `persistent_storage.cpp:390-408`: `write_atomically` opens one LMDB
  write transaction, applies the ops in order, and commits. A failed
  commit ends the process.
- The hot path (`accept_entries_leader`, `handle_acceptdecide`,
  `handle_decide`, `handle_accepted`) reads no storage. Storage is read
  in `read_decided_suffix` (476, the apply of the state machine), in
  `handle_prepare` (527) and in the recovery paths (820-958).
- `node::handle_incoming` (73d1c67) calls `serve_clients` after each
  message, and `serve_clients` reads the decided suffix from storage.

## 2. The rule

No message, no Accepted and no client reply leaves a node before the
commit of every write it depends on. A crash before the commit is a
crash before the messages of the batch arrived.

## 3. Change

1. `node::handle_incoming(std::span<const received>)` handles each
   message in order and returns `[[nodiscard]] pending_commit`.
   `pending_commit` holds the storage ops of the batch and the spans of
   the received messages; it has no constructor that the user can call.
   The caller keeps the messages alive until `commit` returns.
2. `node::commit(pending_commit)` makes one `write_atomically` of all
   ops, then calls `serve_clients`, then releases the outgoing messages
   and the client replies of the batch to `take_outgoing_messages` and
   `take_client_replies`.
3. `tick`, `client_request`, `client_query`, `register_client` also
   return `pending_commit`. A leader that takes several client calls
   before `commit` appends their entries with one `append_entries`.
4. Inside the batch, `sequence_paxos` updates its volatile state as it
   does now, and collects the ops instead of writing them. Messages it
   makes go to a held list.
5. A handler that reads storage (section 1) while ops are pending:
   provisional form P1: the batch writes the pending ops with
   `write_atomically` before that handler runs, and keeps the held
   messages held. Form P2, for the review: one LMDB write transaction
   stays open over the batch, and the reads go through it.
6. A dropped `pending_commit` does not compile: `[[nodiscard]]` and
   every warning is an error. A second `handle_incoming` while a
   `pending_commit` exists is a wrong use and throws `std::logic_error`.
7. The old one-message `handle_incoming` stays as an arm of the bench
   until the measurement decides.

## 4. Names

`handle_incoming`: Omni-Paxos `omni_paxos.rs:353`. `commit`: ISO SQL
COMMIT, `mdb_txn_commit`. `pending_commit` has no source; provisional,
the owner decides.

## 5. Tests

- A batch of 64 client requests makes one commit per node per pass
  (a store that counts `write_atomically`).
- A crash between `handle_incoming` and `commit`: no Accepted and no
  reply left the node; after restart the client gets the reply of its
  retry once (session).
- AcceptDecide and Decide of the same entries in one batch: the
  entries are applied after the commit.
- A rare-path message in a batch with pending ops (P1).
- Seeds 1 to 300 of counter_stress with every fault, with and without
  `--cbor`; test_simulation on memory and LMDB, with batches of random
  size.
- g++ 16 Debug with ASan and UBSan, and Release.

## 6. Measurement

bench: write_lmdb and write_batch_lmdb, old arm and new arm, each in
its own binary, A/A first, 31 samples, 7 rounds. The number comes
after the run.

## 7. Model

A batch is a sequence of steps of the model whose messages leave
after the last step. The plan claims that the model needs no change;
the review checks the claim.

## 8. Order

1. Plan and review.
2. Storage ops collected, `pending_commit`, `commit`; unit tests.
3. Callers: tests, examples, bench, README.
4. Seeds and simulation.
5. Measurement.
