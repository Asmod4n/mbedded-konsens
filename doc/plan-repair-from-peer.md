# Plan: the repair from a peer (stage E of plan-storage-only-source.md)

This plan gives the code form of section 2.4 items 1 to 8 of
doc/plan-storage-only-source.md. The model of that section is
`RepairFromPeer`, `DamageFound`, `OperatorAllowsRepair` and
`StartDamageCounts` in model/tla/SequencePaxos.tla:1678-1760. In the
model, a repair gives the node the durable tuple of a source that votes
and does not recover, and then the recovery. This plan reaches the same
tuple: the log of the source, its promise, its accepted round and its
decided index.

Not in this plan: item 9 (the copy of the whole environment after
`MDB_BAD_CHECKSUM`), item 10 (the state environment, stage D) and the
hidden identity of item 8. After `MDB_BAD_CHECKSUM` the node stays out
of the vote, as now.

## 1. Which errors are a damage

A damage is one of these error codes: `paxos_errc::hardware_fault`,
`paxos_errc::inconsistent_storage`, `storage_errc::crc_mismatch`,
`storage_errc::malformed_record`, `storage_errc::missing_entry`,
`storage_errc::missing_meta_record`. One function decides it. Every
other error keeps the present form: the node stops voting and stays out.

## 2. sequence_paxos

1. `with` that finds a damage in its step check returns a node that
   stopped voting, with the error set, and no longer fails. A caller
   then sees the damage through `get_error`. Every other error of
   `with` fails as now.
2. A new public method `start_repair(nonce)`. It is a wrong use
   (logic_error) when the node has no error or its error is not a
   damage. The caller gives a new nonce, as for `with`; the probes, the
   fetch and the recovery after the repair use it, and the kept
   recovery answers are cleared. So no late answer of an earlier
   recovery is taken (model: `RecoveryMsg(nonce[k] + 1)`,
   SequencePaxos.tla:1508). It clears the error, keeps the hardware
   fault as the proof of the last damage, clears `peers_seen_`, sets the
   node in the phase `repair`, and sends the first probe to every peer. In this phase the
   node takes no part in the vote, refuses client work with
   `client_errc::recovering`, and handles only the repair messages.
3. The scan. Before the first probe, the node reads its own log in one
   read transaction from index 0 upward, without the step check. Each
   entry must decode, its entry CRC and state CRC must hold, and its
   stored chain CRC must equal the chain CRC computed from the chain CRC
   before it and the entry. The scan ends at the first index that fails
   or is absent. Its result is the verified length `v` and the chain CRC
   at each index below `v`, which the node keeps for the search. A read
   error ends the scan at that index.
4. The search. The chain CRC covers the whole prefix, so "the prefix of
   length `l` agrees" is true up to one length and false after it. The
   node keeps `a` (a length that agrees, start 0) and `b` (a length that
   does not agree, start `v + 1`). Each round asks the source for the
   chain CRC at up to `repair_probe_count` (64) lengths between `a` and
   `b`, evenly spaced. A length above the log length of the source does
   not agree. The search ends when `b = a + 1`; `a` is the first index
   that differs.
5. The first answer from a peer chooses the source. Later rounds go to
   the source only. Each round has its own number, which the request
   carries and the answer echoes; an answer with another number is
   ignored. A peer ignores a request with more than
   `repair_probe_count` lengths, through one check function, and the
   decoder reserves nothing from a count in the input. A resend timeout in the phase `repair` starts the
   search again from the scan result, to every peer, with a new
   choice of the source.
6. The fetch. The node asks the source for its entries from `a` to the
   end of its log. The source answers with these entries, its promise,
   its accepted round, its decided index and its last chain CRC, from
   one step after the step check.
7. The check. `from_idx` of the answer must equal `a`. The node folds
   the chain CRC from its own chain CRC at `a - 1` over the received
   entries, with the same function that `handle_new_state` uses (it
   moves out of `handle_new_state` into one function). The result must
   equal the last chain CRC of the answer. The new tuple must pass the
   meta part of the step check (accepted round not above the promise, a
   log that is not empty has an accepted round, decided index not above
   the log length); that part moves out of `read_checked_meta` into one
   function that both use. Else the node refuses the answer, and the next
   resend timeout starts the search again (test "a repair refuses an
   entry whose chain CRC does not follow").
8. The write. One write transaction, without the step check, through a
   new storage member `write_repaired_log(from_idx, entries, promise,
   accepted_round, decided_idx)`. It deletes every key from `from_idx`
   upward, appends the entries, and writes the state of every kept
   entry whose state changes: committed below the new decided index, in
   work from it. It reads the state of each kept entry from the entry
   and not from the meta record, so the entries between the old and the
   new decided index change in both directions. Then it writes a new
   meta record. The storage computes the log length and the last
   chain CRC. The commit failure throws, as for every step.
9. After the commit, the mark is set from the new meta record (not
   raised), and the node starts the recovery of VR Revisited, as at a
   start with a promise (`recovering_`, `send_recovery_to_all_peers`),
   with the nonce of item 2.
10. A peer answers a probe or a fetch only when it votes, does not
    recover and does not repair. It runs the step check in the step of
    its answer. A peer whose check fails stops voting and does not
    answer.

## 3. The messages

Four new messages, appended to `paxos_msg`. Each carries the nonce of
the repair.

- `repair_request{nonce, round, lengths}`: the probe of item 2.4.
- `repair_response{nonce, round, log_len, chain_crcs}`: one chain CRC
  for each asked length that is not above `log_len`, in the order of
  `lengths`.
- `get_repair_entries{nonce, from_idx}`: the fetch of item 2.6.
- `repair_entries{nonce, from_idx, entries, promise, accepted_round,
  decided_idx, last_chain_crc}`.

The wire codes follow the existing order. The names are provisional.
`get_state` and `new_state` are not reused: they belong to the
recovery, which a node in the phase `repair` does not run, and
`new_state` carries `n_accepted` where the repair needs the promise and
the accepted round of the source.

## 4. The count, in the node storage

1. A new class `node_storage` (provisional) opens its own LMDB
   environment in a directory of its own. It holds one record with a
   CRC: the count of automatic repairs and a mark that a repair is
   open. Members: `get_repair_record()`, `count_a_repair()` (adds one,
   sets the mark, commits), `close_the_repair()` (clears the mark,
   commits) and `allow_one_more_repair()` (sets the count to 0,
   commits). A damage found while the mark is set continues the open
   repair and does not count again, so a crash during a repair does not
   use up the count (2.4 item 8: a crash is no damage). The node clears
   the mark after the commit of the repaired log, and at a start that
   finds no damage. A failed commit
   throws `sync_error`, as for the log.
2. `node::with` takes a `node_storage` after the internal storage. The
   node owns it. While its `sequence_paxos` stopped or repairs,
   `node::with` neither reads the promise nor applies entries. The
   leader election starts with the default ballot and is given the
   promise after the repair, as at a start; entries are applied only
   after the recovery.
3. When a step of the node, or `with`, ends with a damage, the node
   reads the count. Below `auto_repair_count` (1) it calls
   `count_a_repair()` and then `start_repair()`. At the limit it stays
   out of the vote, keeps the proof (`get_hardware_fault`, `get_error`)
   and a new getter `waits_for_the_operator()` is true. The notice to
   the operator after each automatic repair is the repair count
   (`get_repair_count()` of the node) and the kept hardware fault.
4. `allow_one_more_repair()` of the node forwards to the node storage.
   It does not restart a node that waits; a restart does that (model:
   a node in `refused` stays there).

## 5. Tests

- "a repair takes only the entries that differ, and sets the mark":
  a follower with a flipped entry in the middle, damaged in a forked
  child (test/lmdb_damage.hpp). It repairs from the leader; the entries
  below the first differing index keep their bytes and the log equals
  the leader's.
- "a repair refuses an entry whose chain CRC does not follow": a
  `repair_entries` with a changed entry is refused; the node writes
  nothing and stays in the phase `repair`.
- "a damage that the start checks find counts toward the automatic
  repair": a node opened on a damaged log repairs once; the count is 1;
  a second damage leaves it waiting for the operator;
  `allow_one_more_repair` sets the count to 0.
- The search finds the first differing index for every position in a
  log of 200 entries (one probe round is not enough there).
- A peer that recovers, repairs or stopped does not answer.
- Wire round trip of the four messages.

## 6. The peer check after a repair

A repair can lower the promise and the accepted index of the node. Its
peers then see a step back of the same connection and refuse the
lower values, as 2.3 item 6 says; that is the report the operator
needs. The node itself clears `peers_seen_` at `start_repair`. One
model run combines `PeerCheck` with `RepairFromPeer` before the build.
