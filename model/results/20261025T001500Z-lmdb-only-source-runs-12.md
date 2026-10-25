# LMDB as the only source: simulation runs, round 12

New: `EntryRunsOnOnePrefix` (plan 2.6 item 5) and the cover `NoPipeline`.
The model already lets a leader propose before the decide of the
entries before. The invariant checks that each node that accepted in the
round of a decided entry, and holds the same value at that index, holds
the same entries before it. Its run of the entry then gives the result
that the decided entry carries. Configs `MC_pipe_*`: three nodes,
values v1 v2 v3, `MaxLog = 3`, `MaxBallot = 2`, `MaxElection = 2`,
every local check on, `MismatchAction = "damage"`. TLC simulation, depth
60, one worker, limit 15 min, at most three runs at a time.

| Config | Model | Faults | Seed | States | Result |
|---|---|---|---|---|---|
| MC_pipe_cover | 17013c8 | none | 7 | 178 | NoPipeline violated, as a cover must be |
| MC_pipe_crash | 17013c8 | 1 crash | 7 | 4494944 | no violation |
| MC_pipe_fault | 17013c8 | 2 storage faults (stale_meta, rollback) | 7 | 4637714 | no violation |
| MC_pipe_net | 17013c8 | 1 loss, 1 duplicate | 7 | 6802 | violated, form 1 |
| MC_pipe_net | a8441f5 | 1 loss, 1 duplicate | 7 | 2740017 | violated, form 2 |
| MC_pipe_net | current | 1 loss, 1 duplicate | 7 | 4466494 | no violation |
| MC_pipe_net | current | 1 loss, 1 duplicate | 11 | 4261038 | no violation |

- Form 1 asked that a decided value stands at one index in every log.
  A duplicated forward of v1 made the leader append v1 twice
  (<<v1, v1>>). This is a resend of the same work. The session and the
  request number of 2.6 item 7 make its second run a no-op.
- Form 2 compared every node. Node 2 had accepted <<v3, v1>> in the
  older round 12 while the leader decided <<v2, v1>> in round 23. The run
  of node 2 counts only in round 12, and the sync of round 23 cuts it.
  So the plan needs: a cut by a sync aborts the state transaction and
  runs the entries again (2.6 item 7).
- Form 3 (current) compares only nodes that accepted in the same round.
  It held in every run. The crash and fault runs held form 1, which is
  stronger.
