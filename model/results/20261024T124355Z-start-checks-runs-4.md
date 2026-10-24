# Start checks: simulation runs, rounds 4 to 6

Model 09e83c6 (review 2 applied). Tool: TLC v1.8.0, one worker,
`-XX:ParallelGCThreads=1`, `-simulate -deadlock -depth 60`, 15 min
limit, three runs at a time on 4 processors. Runs from a copy of
model/tla outside the repository. "Refused" and "restarted" are the
coverage counts of the two branches of StorageFault: the checks find
the damage, or the node restarts with it.

| Config | Seed | Kinds | Refused | Restarted | States | Result |
|---|---|---|---|---|---|---|
| MC_s | 1 | entry, accepted, decided | 250835 | 173464 | 4938257 | no violation |
| MC_s | 2 | entry, accepted, decided | 254905 | 176515 | 5051391 | no violation |
| MC_s_entry | 1 | entry | 82334 | 143277 | 4970346 | no violation |
| MC_s_crash | 1 | entry, accepted, decided | 225649 | 153401 | 4984580 | no violation |
| MC_s_powerloss | 1 | entry, accepted | 78812 | 64730 | 5261341 | no violation |
| MC_s_rollback | 1 | entry, accepted, decided | 159782 | 115544 | 5322184 | no violation |
| MC_s_promise | 1 | promise | 299 | 780 | 23654 | S2b violated |
| MC_s_nocheck_missing_promise | 1 | promise | 0 | 119 | 2852 | S2b violated |
| MC_s_norec | 1 | entry, accepted, decided | 908 | 624 | 18119 | S2b violated |

- NoFalseRefusal held in every run with the checks.
- MC_s_entry restarted 143277 times: these are the missing last keys,
  which the checks do not find. The recovery covered each of them in
  this simulation.
- MC_s_promise: a node with an empty log loses its promise and starts
  as a new node with no recovery, as the code at 4c57d44 does. S2b
  breaks with the checks on. This is finding 1 of the second review.
  The owner decided on 2026-10-24 12:04 that a node with an empty log
  gets a full copy and runs the recovery. Model 6276400 makes that the
  model; the old form is the mutation StartAsNewNode. Rounds 7 and 8
  run it.
- MC_s_norec: as in round 2, a missing last key with no recovery
  breaks S2b.
- A simulation without a violation is not a complete run.
