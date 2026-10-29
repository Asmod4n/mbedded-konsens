# Plan: a node repairs a damaged entry from a peer

Branch repair, from main 03e9477. Owner 2026-10-29 16:59: "Reparatur
muss rein". Owner 17:00: after 1.0 no change of the wire protocol.
Review: doc/review-plan-repair.md; its findings are folded in.

Marks: "provisional" marks a form or a name that waits on the owner.

## 1. Facts checked in this session

1. A damage of the own log ends the process. A step that finds a damage
   writes a proof into node.db and throws `node_fault`
   (include/konsens/node.hpp:780-802). A start with a proof, with a
   damage that the start checks find, or without a clean-stop mark,
   retires the node (node.hpp:49-65, :573-594, :759-768). A retired node
   comes back only as a new node, with a copy of the whole log.
2. Each record of the log is `[entry, entry CRC, chain CRC, state,
   state CRC]` (src/persistent_storage.cpp:56-62, :294-311). The entry
   CRC covers the index and the encoded entry. No CRC covers the chain
   CRC field (`check_record`, :267-291). The meta record holds the last
   chain CRC and is covered by its own CRC and by the high-water mark
   (include/konsens/sequence_paxos.hpp:1027-1036).
3. The scan of the whole log (sequence_paxos.hpp:888-898, :1913-1934)
   ends at the first record that fails, at a missing row and at a read
   error.
4. The repair that was removed (7a20c63) gave the node the log, the
   promise and the accepted round of a peer. That is a rollback, and it
   needed the recovery of VR Revisited after it.
5. The chain CRC is CRC32C (`crc32c::Extend`). CRC32C is linear, and a
   client chooses the payload bytes of an entry. So two different
   entries can have the same chain contribution.
6. A silent node still answers heartbeats (node.hpp:697-701,
   ballot_leader_election.hpp:140).
7. Owner decisions in force: a follower with a damaged entry stays
   silent until it is repaired (PAR, 2026-10-17). One repair runs
   automatically; each further one waits until the operator allows it
   (2026-10-24 20:57). A repair never overwrites correct data
   (2026-10-21).

## 2. What others do

Protocol-Aware Recovery (Alagappan et al., FAST 2018;
https://www.usenix.org/system/files/conference/fast18/fast18-alagappan.pdf,
read in this session):

- The identifier of an entry is stored apart from the entry (3.3.4). A
  damaged entry is replaced only by a copy with the same identifier.
- The leader sends a follower the correct copies (3.4.3, 4.2). A
  leader asks every follower (3.4.1-3.4.3).
- The node takes no part in normal work until the fault is fixed
  (4.2).
- The metadata is not taken from peers, because it is unique to the
  node. CLSTORE keeps two local copies (3.3.1).
- A majority of "dontHave" lets the leader discard an entry in its own
  term. Without a ballot, a message in flight can make that unsafe; it
  is not built here.

Where an equal hash is taken as equal data, a cryptographic hash is
used (ZFS dedup, CockroachDB; owner card 2026-10-29). The repair takes
a copy as equal to the lost bytes because the hash is equal. So the
repair needs the chain hash.

## 3. Order

1. The chain field is covered by a CRC (owner card, 2026-10-29).
2. The chain becomes a cryptographic hash (plan of the chain hash,
   with a measurement).
3. This repair. Its check uses the chain function, so it works with
   the hash of step 2 and never with CRC32C.
4. The branch remove-recovery does not conflict: this repair needs no
   recovery of VR Revisited. remove-recovery is merged before step 3.

## 4. The form for us

1. Who repairs. The scan decides, not the error code. The repair runs
   only when the meta record holds and the scan finds at least one
   damaged record below `meta.log_len`. A damaged record is a record
   that fails `check_record`, or a missing row. Every other damage
   (meta record, `storage_errc::corrupt`, hardware_fault,
   inconsistent_storage) takes the path of fact 1.
2. A local repair first. A record whose entry CRC holds keeps its
   entry; the node writes its chain field, its state and its state CRC
   again. The state comes from the own decided index of the meta
   record, never from a peer.
3. The check. The node folds the chain from index 0 to `meta.log_len`
   over its records that hold and the copies of the damaged records,
   in order of index, and compares the result with
   `meta.last_chain_crc`. One function does this fold; the build uses
   it too. No copy is written before the whole fold passes.
4. The request. The node sends `repair_request{nonce, from_idx,
   to_idx}` (provisional) for each range to every peer. A peer answers
   `repair_entries{nonce, from_idx, entries}` (provisional) with the
   entries it holds in that range, decided or not, from one read
   transaction. A peer that finds a damage in its own range answers
   with the entries before it and runs its own scan. A retired peer does
   not answer. The node refuses an answer whose `from_idx` or count
   differs from its own request. A range longer than
   `max_build_entries` (sequence_paxos.hpp:245) takes several
   requests.
5. The write. One write transaction through a new member of the storage
   concept (storage.hpp:218-232), without `read_checked_meta`. It
   replaces the damaged records and inserts the missing rows, with the
   state of item 2. The meta record does not change. Every storage of
   the tests implements it.
6. The silent node. A repair state apart from `error_`. In it the node
   handles only the repair messages, and sends no Sequence Paxos and no
   Ballot Leader Election message, heartbeat replies included. A leader
   that finds a damage stops being leader in the same step. The output
   of that step is dropped. Waiting client calls and new calls get
   `client_errc::repairing` (provisional).
7. Voting again. After the commit of the repair, one function runs the
   steps of a clean start of `node::with`: the leader election with the
   promise, the check of the applied state and the apply at the start.
   The volatile state of the leader and of the client rounds is reset.
   `verified_idx_` does not fall below its value before the pause. The
   node continues in `{follower, recover}` and sends `prepare_req` to
   all peers.
8. No proof for a repairable damage. A crash during the repair retires
   the node, as form B says, because the start finds no clean-stop mark.
   `stop()` is refused during a repair. So the count of automatic
   repairs is one row with a CRC in node.db without a mark of an open
   repair. At the limit the node stays silent and
   `waits_for_the_operator()` is true; `allow_one_more_repair()` sets
   the count to 0.
9. When no copy passes, the node sends the request again at each
   resend timeout. It does not retire by itself, because an uncommitted
   entry that only it holds is part of its votes. The management tool
   shows each range and the answer of each peer (same, different, none).
   The operator can retire the node.

## 5. The model

The repair changes no durable value. In the model it is a pause of the
node.

1. A new variable `damage_at` in `[Nodes -> 0..MaxLog]`.
   `RunningStorageFault` with the kind "missing_entry" sets
   `damage_at[i]` to the index and calls `DamageFound`.
2. `RepairExact(i)` (provisional): i is in `refused`, `damage_at[i] >
   0`, `repairs[i] < AutoRepairs`, and a peer that is not refused holds
   the same entry at that index. Effect: i leaves `refused`,
   `damage_at[i] = 0`, `Restart({i}, DurableOf(i), Q, FALSE)`, the
   count rises. `OperatorAllowsRepair` is enabled under it too.
3. The model does not follow form B (every restart runs
   `Restart(..., Recovery)`). These configs use Recovery FALSE,
   MaxCrash 0, SingleNode FALSE and no storage fault at a start, so they
   check only paths that the code has.
4. Runs: a BFS from MC_r_entry_chain.cfg and a simulation of 15 min
   from MC_ev_damage.sim.cfg, both with RepairFromPeer FALSE and a
   cover that a repair is reached. Mutation `RepairTakesOther`: the
   repair takes the entry of the peer without the equality; with
   MaxElection 3 and Values 2 it must break S2.

## 6. Tests

- A follower with one flipped entry repairs from the leader; every
  record equals its bytes before the damage.
- A flipped chain field, state byte or CRC field is repaired locally.
- A missing row is repaired. Two ranges are repaired.
- A copy with another entry at the index is refused, and the node stays
  silent.
- A damage found in a step does not throw. A leader that finds a
  damage stops being leader, and the others elect a new one.
- A silent node sends no heartbeat reply.
- After the repair the node votes, and `verified_idx` is not lower.
- The second damage waits for the operator; `allow_one_more_repair`
  allows one more.
- An answer with another `from_idx` or count is refused.
- Wire round trip of the two messages; the kinds in test/test_wire.cpp.

## 7. What this plan does not do

- No repair of the meta record. Two local copies, as CLSTORE keeps,
  change no wire and can come later.
- No repair of the rows of `data` and `session`.
- No discard of an uncommitted entry.
- No change of the build and of the clean stop.

## 8. Questions to the owner

None in this plan. The order of section 3 follows the consensus of
section 2.

## 9. Data of this plan

- TLC metadirs under /mnt/zrun/proofs/repair-*: deleted after each run.
- Result file model/results/<ts>-repair.md: recorded in the state file.
- Build directories under the scratchpad: made again by the build
  command.
