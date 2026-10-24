# LMDB as the only source: simulation runs, round 11

Model b81ab05, configs of 90fbc9c. TLC simulation, seed 7, depth 60,
one worker, limit 15 min, three runs at a time. Started 2026-10-24
23:29 UTC.

The `_mono` configs run round 9's `a_F1` configs again with
`DecidedIdxMonotone = TRUE`. The `_nohw` configs run the three forms of
`MismatchAction` with `HighWater = FALSE`, so that a rollback is not
found locally. Every other local check is on, as in round 10.

| Config | MismatchAction | States | Result |
|---|---|---|---|
| MC_hw_a_F1_dup_mono | keep | 4481044 | no violation |
| MC_hw_a_F1_loss_mono | keep | 4818987 | no violation |
| MC_mm_cover_rollback_nohw | keep | 389 | NoMismatch violated, as a cover must be |
| MC_mm_keep_rollback_nohw | keep | 230155 | S1 violated |
| MC_mm_damage_rollback_nohw | damage | 230684 | S1 violated |
| MC_mm_stop_rollback_nohw | stop | 9973 | S1 violated |

- With `DecidedIdxMonotone = TRUE`, `HighWaterHolds` holds in both
  `a_F1` configs. The violation of round 9 came from the old form of
  Omni-Paxos, where an `AcceptSync` lowers the decided index.
- Without `HighWater`, a mismatch can occur (the cover).
- Without `HighWater`, S1 fails in each of the three forms. The traces
  of "keep" and "damage" are the same 25 states. `MismatchFound` is in
  none of the three traces. In the last state of the "keep" trace, the
  logs are <<v2>>, <<v1>>, <<v2>> and the decided indexes 0, 1, 1: node 2
  decided v1 and node 3 decided v2. No node compared a chain CRC that
  differs before the second decide.
- So the chain CRC in the decide does not keep agreement after a
  rollback that no local check finds. The high-water mark does. The form
  of `MismatchAction` does not change safety in any run of rounds 10 and
  11. The plan keeps "damage" (plan 2.5 item 3): it sends the node to
  the repair of 2.4, which the other damages use too.
- `MismatchBlamesFollower` held in every run.
