# TLC runs of AcceptWithResult.tla (plan-accept-carries-the-result.md, after review round 1)

Tool: TLC2 Version 2.19 of 08 August 2024 (rev: 5a47802), OpenJDK 21.0.12.1, -deadlock, -Xmx3g, breadth-first, metadir on a btrfs image with compress=zstd:1. Time limit 900 s per run. Kernel 6.18.44-fc-v185, 4 cores.

Common constants: Nodes = {n1, n2, n3}, MaxBallot = 2, MaxFaulty = 1. Unless the row says otherwise: Requests = {r1, r2}, MaxLog = 2, MaxCrash = 1, NoRecovery = TRUE, every mutation FALSE. Safety invariants: ChosenOnce, DecidedAgree, DecidedIsChosen, ChosenIsTrue, MajorityVotedOnce.

## Safety runs

| config | change | result | distinct states | time |
|---|---|---|---|---|
| MC_aw_base_l1 | MaxLog 1 | complete, no violation, depth 43 | 4 535 378 | 6 min 54 s (1 worker) |
| MC_aw_early_l1 | MaxLog 1, AcceptBeforeOwnSync | complete, no violation | 6 426 502 | 4 min 55 s |
| MC_aw_recovery_l1 | MaxLog 1, NoRecovery FALSE, RecoveryWaitsForLeader | complete, no violation, depth 45 | 14 863 391 | 7 min 28 s |
| MC_aw_base_c0_r1 | Requests {r1}, MaxCrash 0 | complete, no violation, depth 49 | 3 950 189 | 5 min 24 s |
| MC_aw_base_r1 | Requests {r1} | not complete at 900 s, no violation | 13 405 003 | 15 min |
| MC_aw_base_c0 | MaxCrash 0 | not complete at 900 s, no violation | 14 802 877 | 15 min |

MC_aw_base_c0_r1 is the only complete run in which a base is not empty: the second entry runs on the first.

## Mutations (each must break an invariant)

| config | mutation | broken invariant | distinct states | time |
|---|---|---|---|---|
| MC_aw_mut_no_check_accept | NoCheckAtAccept | ChosenIsTrue | 41 908 | 2 s |
| MC_aw_mut_no_check_sync_only | NoCheckAtSync, MaxLog 1 | ChosenIsTrue | 68 143 | 3 s |
| MC_aw_mut_no_selection | NoSelectionAtSync | ChosenIsTrue | 262 763 | 12 s |
| MC_aw_mut_no_repair_bound | NoRepairBound | DecidedAgree | 3 703 110 | 3 min 32 s |
| MC_aw_mut_unsynced_l1 | LeaderCountsUnsynced, MaxLog 1 | DecidedIsChosen | 2 198 027 | 2 min 10 s |

- NoSelectionAtSync: the new leader takes the log of the faulty old leader with its wrong entry and counts itself; the faulty old leader agrees with the sync. Two nodes are a majority.
- NoRepairBound: the faulty old leader keeps its wrong entry through the kept prefix of the sync, and decides it.
- MC_aw_mut_unsynced at MaxLog 2 did not break an invariant in 900 s (15 413 890 states, before review round 1).

## Covers (each must be reached)

| config | cover | reached | distinct states | time |
|---|---|---|---|---|
| MC_aw_cover_repaired | NoWrongProposalRepaired: an index first proposed with a wrong result is chosen with the result of a selection | yes | 4 965 040 | 2 min 38 s |
| MC_aw_cover_refusal | NoRefusal: a follower refuses | yes | 10 693 | 2 s |

## Not in the model

The rejected entry, entries that are not requests, the second run, the float rule (plan 3.6) and plan 3.8.
