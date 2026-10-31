# TLC runs of AcceptWithResult.tla with four ballots (after review round 2)

Tool and machine as in 20261031T074500Z-accept-with-result.md. Change of the model: FinishSelection keeps the own prefix of the leader by Keep (plan 3.9). With four ballots the owner of ballot 1 (n1) leads again in ballot 4.

| config | constants | result | distinct states | time |
|---|---|---|---|---|
| MC_aw_mut_no_repair_bound_l1_b4_c0_r1 | NoRepairBound, MaxBallot 4, MaxLog 1, MaxCrash 0, Requests {r1} | ChosenIsTrue broken, depth 22 | 2 609 140 | 2 min 11 s |
| MC_aw_base_l1_b4_c0_r1 | MaxBallot 4, MaxLog 1, MaxCrash 0, Requests {r1} | not complete at 900 s, no violation | 30 794 241 | 15 min |
| MC_aw_base_l1_b4 | MaxBallot 4, MaxLog 1 | not complete at 900 s, no violation | 37 372 202 | 15 min |
| MC_aw_mut_no_repair_bound_l1_b4 | NoRepairBound, MaxBallot 4, MaxLog 1 | not complete at 900 s, no violation | 18 305 079 | 15 min |

The trace of the mutation: n1 gives a wrong result for r1. n1 leads ballot 1 and proposes r1 with its wrong result. n2 leads ballot 2 (no entry above d reached by a majority) and n1 leads ballot 4. The selection of ballot 4 picks the right result, but n1 keeps its own wrong entry in the kept prefix and decides it.

The complete runs with four ballots are longer than 15 min. They run at night.
