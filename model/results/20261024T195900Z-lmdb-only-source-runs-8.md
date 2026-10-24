# LMDB as the only source: simulation runs, round 8

Model 0d78a0d: HighWater (plan 2.3 item 7) and RepairFromPeer (plan
2.1 item 5). With HighWater, each node keeps, in memory, the highest
promise, the highest pair of accepted round and log length, and the
highest decided index that it read or wrote; a restart forgets it. A
running storage fault below the mark is found. With RepairFromPeer, a
node that finds a damage takes the durable state of a node that votes
and does not recover, and restarts with the recovery. Every other check
on, PeerCheck off. Tool and flags as round 2, `-seed 7`, 15 min limit,
three runs at a time on 4 processors, then two. Invariants S1 S2 S3 S4
HighWaterHolds AcceptedIdxIsLogLen, properties S2Grows
BuildOnlyFromRunning.

| Config | Kind | RepairFromPeer | States | Result |
|---|---|---|---|---|
| MC_r_stale_hw | stale_meta | FALSE | 4295297 | no violation, stopped by the limit |
| MC_r_rollback_hw | rollback | FALSE | 4069325 | no violation, stopped by the limit |
| MC_r_meta_hw | the four of round 4 | FALSE | 3962980 | no violation, stopped by the limit |
| MC_r_stale_repair | stale_meta | TRUE | 3574903 | no violation, stopped by the limit |
| MC_r_rollback_repair | rollback | TRUE | 3663896 | no violation, stopped by the limit |

- The traces of round 5 (S1 with every local check on) do not occur.
  The disjunct `HighWater /\ BelowMark(Mark(d), high_water[i])` was
  evaluated 281926 times (stale_meta) and 342492 times (rollback).
- HighWaterHolds held: no step without a damage took a node below its
  mark, so the check gave no false report in these runs.
- AcceptedIdxIsLogLen held in every run: the copy accepted_idx of a
  running node equals its log length. The model can read the log
  length in place of the copy.
- The repair from a peer ran: the Restart in DamageFound was evaluated
  451470 times (stale_meta) and 473902 times (rollback), with no
  violation.
- A simulation without a violation is not a complete run.
