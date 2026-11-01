# Review of plan-remove-recovery.md (commit 8acc3f3)

Reviewer: a second agent of the session, read only, 2026-10-29.
Result: not ready; 4 blocking findings, folded into the plan.

## Blocking

- B1. The test helper `make` (test_sequence_paxos.cpp:84-94) is the
  only caller of `start_recovery`; every test with a stored promise
  goes through the recovery. The helpers `new_state_of` (:205-218),
  `waiting_for_new_state` and `source_log` (:1094-1106),
  `cut::all_but_recovery` (:141-157), `running_node_in` (:1404-1421)
  and `check_stops_voting` (:1423-1428) use recovery messages.
- B2. test_wire.cpp: kind constants change (:437, :454, :550-551,
  :646); the tests at :482 and :492-501 use `recovery` only as a
  sample and must take another message; entries at :276-282, :385-388.
- B3. The case counts of section 2.6 were wrong; the decisions must
  stand in the plan, case by case (table in the plan).
- B4. The questions for the owner must be asked, not left open
  (`client_errc::recovering`; the flag `--power-loss`).

## Not blocking

Shared parts: `check_decided_chain_crc` stays (accept_sync, accept_decide,
decide); `complete_recovery` and `find_hardware_fault` have only recovery
callers. node.hpp:951, :1111, :1644 reduce to `!votes()`. The category
checks of `paxos_errc` live only in the case at :1057 (:1076-1077) and
must move. Kinds move down by four as in 7a20c63 ("Remove the repair of
a damaged log"); program/ fixes no kind. Docs list was incomplete
(README.md:117 with doc/img/start.svg, :257, :379; many rows of the
fault matrix). The rename goes through clangd. Checks miss program/,
counter_stress, the split of build jobs, the state file and the CI mark.

## Not checked by the reviewer

Nothing built or run; model files; refs/papers.md.
