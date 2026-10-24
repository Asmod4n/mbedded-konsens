# LMDB as the only source: simulation runs, round 10

Model b81ab05. New: `MismatchAction` (plan 2.5 item 3), the three forms
for a follower whose chain CRC at the new decided index differs from the
one in the message: "keep" keeps the old decided index, "damage" takes it
as a damage (repair from a peer), "stop" makes the node leave running.
`MismatchBlamesFollower` checks that the prefix in the message agrees
with the decided history, so that the follower is the node that differs.
`NoMismatch` is a cover. Every local check on (StepChecks, ChainCheck,
MetaRecord, HighWater, VerifyCrc, VerifyPromise, RepairFromPeer). Flags
as round 9.

| Config | Kind | MismatchAction | States | Result |
|---|---|---|---|---|
| MC_mm_cover_rollback | rollback | keep | 4564995 | NoMismatch held: no mismatch reached |
| MC_mm_cover_stale | stale_meta | keep | 4660455 | NoMismatch held: no mismatch reached |
| MC_mm_keep_rollback | rollback | keep | 3603784 | no violation |
| MC_mm_keep_stale | stale_meta | keep | 3298175 | no violation |
| MC_mm_damage_rollback | rollback | damage | 3109221 | no violation |
| MC_mm_damage_stale | stale_meta | damage | 3151519 | no violation |
| MC_mm_stop_rollback | rollback | stop | 3359733 | no violation |
| MC_mm_stop_stale | stale_meta | stop | 3139525 | no violation |

- With every local check on, no run reached a mismatch, so these runs do
  not tell the three forms apart. The local checks found each damage
  before a decide could show it. Round 11 runs the forms with
  `HighWater = FALSE`, where a rollback is not found locally.
- The two configs MC_hw_a_F1_dup_mono and MC_hw_a_F1_loss_mono did not
  run: their files had no value for `MismatchAction`. Round 11 runs them.
- All runs stopped at the limit of 15 min. A simulation without a
  violation is not a complete run.
