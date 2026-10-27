# ChangesetRepair: the repair of a state by a changeset

Model: `tla/ChangesetRepair.tla`. Keys {k1, k2, k3}, values {a, b},
3 decided writes, every sequence of writes and every start index of the
node. TLC 1 worker, `-deadlock`.

| config | MaxDamage | DigestCheck | exit | result |
|---|---|---|---|---|
| MC_cr_nodigest | 1 | FALSE | 12 | RepairIsWhole violated, 7,777 distinct states |
| MC_cr_digest | 1 | TRUE | 0 | complete, 10,716 distinct states, no violation |
| MC_cr_digest_d2 | 2 | TRUE | 0 | complete, 20,160 distinct states, no violation |
| MC_cr_cover | 0 | TRUE | 12 | CoverChangesetDone violated, as expected: the repair by changeset ends without a full copy |

The trace (`20261027T060500Z-changeset-nodigest-trace.out`): a damage
changes a value of a key that no later write changes. The changeset
holds only the keys that differ between the snapshot and the state of
the peer, so `sqlite3changeset_apply` sees no conflict, and the damage
stays.

So a repair by changeset is whole only when the node then compares the
digest of its whole state with the digest of the peer at the same
index. A difference asks for the full copy.
