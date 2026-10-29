# Review of plan-repair.md (commit 2663ea3)

Reviewer: a second agent of the session, read only, on 2026-10-29.
Folded into the plan at the next commit. One finding of the review
went to the owner only.

## Facts of section 1

- F2 is not complete. A record has five parts: entry, entry CRC, chain
  CRC, state, state CRC (persistent_storage.cpp:56-62, :294-311). No
  CRC covers the chain field (:267-291).
- F3: the scan also stops at a missing row and at a read error, and
  treats both as the end of the log (sequence_paxos.hpp:1919-1922).
- F5: AutoRepairs is at :688.
- F6: the decision of 2026-10-21 also asks for the known bugs of other
  systems and for faults of the network.

## Blocking

- B1. The proof in node.db retires the node at its next start
  (node.hpp:156, :177, :766-767). No repair survives a restart.
- B2. `error_is_a_damage` covers errors that are no damaged record.
  The scan decides, not the error code.
- B3. The chain CRC of b is not covered by a CRC, and after a lost
  write it can belong to an older history. Fold from 0 to log_len and
  compare with `meta.last_chain_crc`, which the meta CRC and the
  high-water mark cover.
- B4. CRC32C is linear. A client chooses payload bytes and can build two
  entries with the same chain contribution. The 2^-32 figure holds for
  random faults only.
- B5. A silent node still answers heartbeats (node.hpp:697-701,
  ballot_leader_election.hpp:140). A silent leader stays leader.
- B6. No code path: `handle` and `resend_message_timeout` return
  `error_` (:486-491, :656-658); `begin_step` and `commit` run
  `read_checked_meta` (:1090-1104, :455-460); a repair state apart from
  `error_`; a write path without `read_checked_meta`; a new member of
  the storage concept (storage.hpp:218-232).
- B7. Voting again needs the steps of a clean start of `node::with`
  (node.hpp:159-185), a reset of the volatile state, and a
  `verified_idx_` not below its value before the pause (:1239, :346,
  :382).
- B8. MC_c_crash and MC_ev_loss.sim have MaxStorageFault 0. The model
  does not follow form B: every restart runs `Restart(..., Recovery)`.

## Answers

- The state byte and the CRC fields can be repaired locally. The state
  comes from the own decided index, never from a peer.
- A missing row counts as a damaged record, up to `meta.log_len`.
- `storage_errc::corrupt` is not in `error_is_a_damage`; 3.3 "retires
  as now" is false for it.
- No self-retirement is safe. The discard of PAR is not safe without a
  ballot. The management tool shows each range and the answer of each
  peer.
- Model: a variable `damage_at`; `RepairExact(i)` from `refused` when a
  peer holds the same entry at k; `Restart({i}, DurableOf(i), Q, FALSE)`.
  Configs from MC_r_entry_chain.cfg and MC_ev_damage.sim.cfg with
  RepairFromPeer FALSE, Recovery FALSE, MaxCrash 0. The mutation needs
  MaxElection 3 and Values 2.
- Wire: the receiver checks `from_idx` and the count against its own
  request; `max_build_entries` exists (:245).

## Not checked by the reviewer

The PAR paper; the diff of remove-recovery; the storages of
test/node_cluster.hpp; CRCs of the session rows; that a decoded entry
encodes to the same bytes; run times; the `tick` path under a repair
state.
