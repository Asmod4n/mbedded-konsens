# Recovery delta: runs of 2026-10-29

Plan: model/plan-recovery-delta.md, review model/review-plan-recovery-delta.md.
Model: model/tla/SequencePaxos.tla at 7570a47 (branch recovery-delta).
TLC 2.19 (rev 5a47802), 1 worker, -deadlock, -Xmx3g or -Xmx2g,
-XX:+UseParallelGC, metadir on a compressing btrfs file system.
Simulation: depth 60, seed 1, limit 15 min. Machine: Firecracker VM,
kernel 6.18.44, amd64, 4 processors. Beside two or three mapping runs;
load average 4.2 to 5.0.

| Config | Kind | Time | Result | States |
|---|---|---|---|---|
| MC_mut_PrimaryStartAtAccepted | simulation | 3 s | Assert "RecoveryKeptPrefix after the answer of a primary" fails, as wanted; trace of 20 states | 8,199 generated |
| MC_mut_PrimaryStartAtAccepted | BFS | 30 min | stopped at the limit, no failure found; depth 16 | 7,262,330 distinct |
| MC_delta_crash | BFS | 1 h 44 min | complete, no violation; depth 37 | 24,581,710 distinct |
| MC_F6b | simulation | 15 min | no violation | 13,441,247 checked |
| MC_c_crash | simulation | 15 min | no violation | 9,796,363 checked |
| MC_d2_crash | simulation | 15 min | no violation | 13,619,808 checked |
| MC_ev_loss.sim | simulation | 15 min | no violation | 5,788,303 checked |
| MC_s_powerloss.sim | simulation | 15 min | no violation | 18,408,392 checked |
| MC_s_powerloss_log2.sim | simulation | 15 min | no violation | 17,372,001 checked |
| MC_mut_PowerLossStartAtAccepted_s.sim | simulation | 4 min | Assert "RecoveryKeptPrefix after a power loss" fails, as wanted; trace of 33 states | 5,752,595 generated |

The fingerprint collision probability of MC_delta_crash, based on the
actual fingerprints: 1.3E-5.

The configs power-loss/MC_mut_PowerLossStartAtAccepted.sim and
power-loss/MC_p_rollback no longer load: they lack constants that the
model has now (VerifyPromise and later). MC_mut_PowerLossStartAtAccepted_s.sim
(MC_s_powerloss.sim with the mutation) replaces the first;
MC_s_powerloss_log2.sim ran in place of the second.

## Result

- With one crash at MaxBallot 1, MaxLog 1, the node that recovers
  through the answer of a primary keeps the entries before the start
  index of Omni-Paxos, and they equal those of the primary, over the
  whole state space (MC_delta_crash).
- The simulations of a rollback, a crash, a halt and resume, Execute-
  Verify with loss and power loss show no violation in 15 min each.
- Each mutation breaks the assertion of its path.
- Not covered: DecidedIdxMonotone FALSE with Recovery TRUE; a kept
  decided prefix with a different suffix in a BFS (MaxLog 1 there).
