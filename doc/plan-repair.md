# Plan: a node repairs a damaged entry from a peer

Branch repair, from main 03e9477. Owner 2026-10-29 16:59: "Reparatur
muss rein". Owner 17:00: after 1.0 no change of the wire protocol.

Marks: "provisional" marks a form or a name that waits on the owner.

## 1. Facts checked in this session

1. A damage of the own log ends the process. A step that finds a damage
   writes a proof into node.db and throws `node_fault`
   (include/konsens/node.hpp:781-802). A start with a proof, or with a
   damage that the start checks find, retires the node
   (node.hpp:763-770, :573-594). A retired node comes back only as a new
   node, with a copy of the whole log (doc/plan-clean-stop.md, item 6).
2. Each record of the log is `[entry, entry CRC, chain CRC, state]`
   (src/persistent_storage.cpp:58-60, :262-309). The entry CRC covers
   the index and the encoded entry. The chain CRC of index i is computed
   from the chain CRC of i - 1, the index and the encoded entry. The
   meta record holds the last chain CRC (:74).
3. The scan of the whole log (`scan_the_whole_log`,
   include/konsens/sequence_paxos.hpp:888-898) ends at the first record
   that fails.
4. The repair that was removed (7a20c63; plan doc/plan-repair-from-peer.md)
   gave the node the log, the promise and the accepted round of a peer.
   That is a rollback: the node loses promises and Accepted messages
   that it sent (doc/plan-clean-stop.md, fact 2). It needed the recovery
   of VR Revisited after it.
5. The model checks that removed form: `RepairFromPeer`,
   `OperatorAllowsRepair`, `AutoRepairs` (model/tla/SequencePaxos.tla:686,
   :1912-1995).
6. Owner decisions in force: a follower with a damaged entry stays
   silent until it is repaired (PAR, owner 2026-10-17). One repair runs
   automatically; each further repair waits until the operator allows it
   (owner 2026-10-24 20:57). A repair never overwrites correct data
   (owner 2026-10-21).

## 2. What others do

Protocol-Aware Recovery (Alagappan et al., FAST 2018, CTRL;
https://www.usenix.org/system/files/conference/fast18/fast18-alagappan.pdf,
read in this session):

- The identifier of an entry is stored apart from the entry (3.3.4).
  A damaged entry is replaced only by a copy with the same identifier.
- A follower reports its damaged entries; the leader sends the correct
  copies (3.4.3, 4.2).
- A leader asks every follower for the entry. One "have" repairs it. A
  majority of "dontHave" shows that the entry is not committed, and the
  leader discards it and every later entry (3.4.1-3.4.3).
- The node takes no part in normal work until the fault is fixed or
  discarded (4.2).
- The metadata (term, vote) is not taken from peers, because it is
  unique to the node. CLSTORE keeps two local copies (3.3.1).

ZFS repairs a block from another copy only when the checksum of that
copy holds (from memory, not read in this session).

The consensus: a damaged entry is replaced only by a copy that the node
can identify as the entry that it wrote. The node keeps every vote that
it gave.

## 3. The form for us

The chain CRC is the identifier that is stored apart from the entry:
the record after the damage and the meta record hold it. So the node
can check a copy from any peer, and no copy that it did not write
passes.

1. The damaged range. The scan (fact 3) continues after a record that
   fails, to the end of the log. A damaged range is a run of records
   that fail, `[a, b)`. Its end is the first record at `b` that holds,
   or the meta record when `b` is the log length. The chain CRC of
   `a - 1` comes from the record before the range (0 when `a` is 0).
2. The check of a copy. The node folds the chain CRC from `a - 1` over
   the copies of `[a, b)`. The result must equal the chain CRC that the
   record at `b` holds as the chain before it: the node computes the
   chain of `b` from the result and the entry at `b`, and compares it
   with the stored chain CRC of `b`. When `b` is the log length, the
   result must equal the last chain CRC of the meta record. One function
   does this check, with the fold that the build uses.
3. The node cannot repair, and retires as now, when: the meta record
   fails; the chain CRC before `a` is not readable; no record holds
   after the range and the meta record fails; SQLite reports a damage of
   the file (no record is readable).
4. The request. The node sends `repair_request{nonce, from_idx,
   to_idx}` (provisional) to every peer, one request for each range.
   A peer answers with `repair_entries{nonce, from_idx, entries}`
   (provisional): the entries it holds in that range, decided or not,
   from one read transaction, after its own check of those records. A
   peer that holds fewer entries, or finds a damage in them, answers
   with the entries up to that point. The peer answers whether it votes
   or not, but not when it is retired. The count of entries in a
   request is limited by one named constant, through one check function;
   the decoder reserves nothing from a count in the input.
5. The node takes the first answer whose copies pass the check of item
   2. It writes the records of `[a, b)` in one write transaction, with
   the same entry CRC, chain CRC and state as the records that the
   chain CRC identifies. Nothing else in the store changes: not the
   meta record, not the promise, not any other record.
6. While a range is damaged, the node takes no part in the vote: it
   sends no Promise, no Accepted, no Ballot Leader Election message as a
   candidate, and refuses client work with `client_errc::repairing`
   (provisional). A leader that finds a damage stops being leader in the
   same step. After the last range is written, the node continues in
   `{follower, recover}` and sends `prepare_req` to all peers, as a
   clean start does (sequence_paxos.hpp:380-383). No recovery of VR
   Revisited is needed, because no vote of the node changed.
7. When no answer passes, the node sends the request again at each
   resend timeout. The operator sees the state in the management tool
   and can retire the node. The node does not retire by itself: an
   uncommitted entry that only this node holds is part of its votes.
   The discard of PAR (a majority of "dontHave") is not built.
8. A damage found by a step, not only by the start, runs the same path.
   The step does not throw `node_fault` for a damage of the log any
   more. The proof stays in node.db.
9. The count of automatic repairs (owner 2026-10-24 20:57) is one row
   with a CRC in node.db: the count and a mark that a repair is open. A
   damage found while the mark is set continues the open repair and does
   not count again. At the limit the node stays silent and
   `waits_for_the_operator()` is true. `allow_one_more_repair()` sets
   the count to 0. The names come from the removed code (7a20c63).

## 4. The model

The repair changes no durable value that the protocol reads. In the
model, it is a pause of the node: the node is silent from the damage
until the repair.

1. New action `RepairExact(i)` (provisional): enabled when node i is
   damaged and a peer holds the same entries at the damaged indices. It
   clears the damage. The log, the promise and the accepted round of i
   do not change.
2. The damage marks a node as silent; the node keeps its log in the
   model (the model holds the bytes that the chain CRC identifies).
3. Runs: a small BFS with one damage and one repair (MaxBallot 1,
   MaxLog 2, Cap 2), and simulations of 15 min on MC_c_crash and
   MC_ev_loss.sim with the new action. A mutation `RepairTakesOther`
   lets the repair take a different entry, and must break S2.
4. `RepairFromPeer` stays in the model as the checked form of the past,
   off in the new configs.

## 5. Tests

- A follower with one flipped record in the middle repairs from the
  leader. After the repair, every record equals its bytes before the
  damage (test with a damage in a forked child, as
  test/test_persistent_storage.cpp does).
- A flipped chain CRC field with an intact entry is repaired.
- Two damaged ranges are repaired.
- A damage of the last record is checked against the meta record.
- A copy from a peer with another entry at the index is refused, and
  the node stays silent.
- A damage found in a step does not throw, and the node repairs.
- A leader that finds a damage stops being leader.
- The second damage waits for the operator; `allow_one_more_repair`
  allows one more.
- A peer that is retired does not answer. A request above the limit is
  refused.
- Wire round trip of the two messages; the kinds after them in
  test/test_wire.cpp.

## 6. What this plan does not do

- No repair of the meta record. Two local copies, as CLSTORE keeps,
  change no wire and can come later.
- No repair of the rows of `data`; they carry no CRC (Claude-state
  mbedded-konsens.md, bit-flip gaps).
- No change of the chain CRC to a cryptographic hash. The check of
  item 3.2 uses the chain function, so the plan of the chain hash
  changes it in one place. Until then, a copy with another entry passes
  the check with a probability of 2^-32.
- No discard of an uncommitted entry (item 3.7).
- No change of the build and of the clean stop.

## 7. Order

The branch remove-recovery does not conflict: this repair needs no
recovery of VR Revisited. remove-recovery is built and merged first;
this branch then takes main.

## 8. Questions to the owner

None. The form follows the consensus of section 2. The provisional
names wait on the review.

## 9. Data of this plan

- TLC metadirs under /mnt/zrun/proofs/repair-*: deleted after each run.
- Result file model/results/<ts>-repair.md: recorded in the state file.
- Build directories under the scratchpad: made again by the build
  command.
