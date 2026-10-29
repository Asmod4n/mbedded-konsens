# Plan: remove the recovery that no start calls

Branch remove-recovery, from main 03e9477. Owner card 2026-10-29
16:31: "Entfernen".

## 1. Facts checked in this session

- `node` never starts a recovery. A start with no matching clean-stop
  mark is `start_kind::retired` (include/konsens/node.hpp:55-64). A
  clean start sends `prepare_req` at once
  (include/konsens/sequence_paxos.hpp:380-383).
- `start_recovery` (sequence_paxos.hpp:394-405) has one caller, the
  test helper (test/test_sequence_paxos.cpp:92).
- The recovery of VR Revisited 4.3 and the power-loss path are the
  members `recovering_`, `recovery_responses_`, `state_request_`
  (:2191-2196) and the functions `send_recovery_to_all_peers` (:1273),
  `handle_recovery` (:1676), `handle_recovery_response` (:1702),
  `keep_recovery_response`, `select_recovery_source`,
  `find_hardware_fault`, `handle_get_state`, `handle_new_state`,
  `find_recovery_primary` and `complete_recovery` (:1711-1910).
- The messages `recovery`, `recovery_response`, `get_state` and
  `new_state` (include/konsens/message.hpp:150-188) are the alternatives
  9 to 12 of `paxos_msg` (:267-271). The wire numbers the kinds by the
  index of the alternative.
- `phase::recover` is not this recovery. It is the phase Recover of
  Omni-Paxos after a clean start or a gap in the sequence numbers
  (:381, :701, :1299). It stays.
- `client_errc::recovering` is also the answer of a node that builds
  (`refuse_if_recovering`, :732-736). It stays, with the build as its
  only cause.
- `nonce_` is also the nonce of the build (`send_build_request`). It
  stays.
- Places outside sequence_paxos.hpp: node.hpp:681, :697, :951, :1111,
  :1644 (`recovering()`); src/wire.cpp:91-99 (`check_entries_of`);
  test/simulation.hpp:251-252 (names of the kinds), :573;
  examples/counter_stress.cpp:488, :570; test/test_wire.cpp (16
  lines); test/test_sequence_paxos.cpp (153 lines, 26 test cases).
- README.md:119 says that a node with a stored promise sends Recovery.
  doc/storage-fault-matrix.md:58-62, :70, :102-106 say the same.

## 2. The change

1. Remove `start_recovery`, the members and the functions of section
   1, and each branch on `recovering_`
   (:528-531, :590, :662-667, :915, :1295, :1686).
2. Remove `recovering()`. Each caller in node.hpp keeps its other
   conditions (`building()`, `retired()`, `votes()`).
3. The name `refuse_if_recovering` claims a cause that no longer
   exists. The name becomes `refuse_if_building`. The error code stays `client_errc::recovering` (an error code of the
   wire; a rename of it is a question for the owner, not part of this
   plan).
4. Remove the four messages from `paxos_msg` and their structs. The
   kinds after them move down by four. The wire is stable only from 1.0
   on. test/test_wire.cpp checks the kind numbers; they change with it.
5. Remove `check_entries_of` for the two removed messages.
6. Tests (review doc/review-plan-remove-recovery.md, B1 to B3):
   - The helper `make` (test_sequence_paxos.cpp:84-94) calls
     `start_recovery` for a stored promise. It becomes a clean start
     that drops its `prepare_req`. The helpers `answer_of` (:198),
     `new_state_of` (:205-218), `waiting_for_new_state` and
     `source_log` (:1094-1106) go; `cut::all_but_recovery` in
     `deliver_among` (:141-157) goes; `running_node_in` (:1404-1421)
     and `check_stops_voting` (:1423-1428) send `prepare_req` in place
     of `recovery`.
   - Decisions for each case:

     | Line | Case | Decision |
     |---|---|---|
     | :331 | clean start in recover | adapt: no check of `recovering()` and of a sent `recovery` |
     | :363 | retired node | adapt: `prepare_req` in place of `recovery{9}` |
     | :468 | gap in the sequence numbers | keep |
     | :609 | first start | adapt: no `recovering()`; `refuse_if_building` |
     | :626 to :1316 | recovery only (25 cases) | delete |
     | :1057 | CRC evidence at recovery | delete; its checks of the category (:1076-1077) move to a test of each `paxos_errc` value |
     | :1352 | rollback keeps S1 | delete; the rolled-back node retires (test_simulation_rollback) |
     | :1442, :1452, :1465 | step-check damage | adapt through the helpers |
     | :1509 | wrong CRC | adapt: a `prepare` that makes `promise_for` read the log from 0 in place of `get_state` |
     | :1555 | writers of the decided index | adapt: the SUBCASE "complete_recovery" (:1605-1628) goes; a SUBCASE for the build, which writes the decided index (sequence_paxos.hpp:2034) |
     | :1670 | decide with another chain CRC | keep |
     | :1714 | chain CRC against entry CRC at recovery | delete; :1670 covers the chain CRC in the vote |

   - test_wire.cpp: the entries of the four messages go (:276-282,
     :385-388); the kind constants change (:437, :454, :550-551,
     :646); the tests at :482 and :492-501 take `prepare_req` in place
     of `recovery`; the provisional names (:36-49) and the comment at
     :680 follow.
   - test/simulation.hpp: `recovers()` (:571-574), its callers (:992,
     :1002, :1012) and the comment at :42-47. examples/counter_stress.cpp:
     `recovers()` (:486-489) and its callers (:907, :911).
   - Comments that give a reason that ends: test_simulation_rollback.cpp:7-16,
     test_persistent_storage.cpp:479, :614.
7. Docs state form B: a start with no matching mark retires the node;
   it comes back as a new node. README.md:117 (doc/img/start.svg and
   its alt text), :119, :257, :379; doc/storage-fault-matrix.md:57,
   section 2.2 (:64-72), :102-106, :117, :124, :126, :139, :150, :220,
   :245. The text of `client_errc::recovering` (client.hpp:22-23) says
   that the node builds.
8. The flag `--power-loss` of counter_stress does a clean restart of
   every node (:449-450, :475-484), not a power loss. Its name claims
   what does not happen, so it becomes `--restart-all` (provisional,
   decided by the session).
9. `refuse_if_recovering` is renamed by clangd in the build container
   (rule on renames); callers: sequence_paxos.hpp:628, node.hpp:361,
   :386, :412, :440.
10. The model keeps the recovery. A note in model/plan-recovery-delta.md
   says that the code has no recovery since this change.

## 3. Checks

- Release g++ 16: the changed test programs first, then the full run
  once (RULES: a run checks the work at hand first).
- g++ 15 and clang 22 Release, the full run.
- ASan and UBSan g++ 16, once before the merge into main.
- program/ (KONSENS_PROGRAM) builds, and counter_stress runs once
  with a short seed range.
- Builds run one after another with -j4, or two at a time with -j2.
- Each push carries [skip ci] until the local run is green.
- The kinds after proposal_forward move down by four, as in 7a20c63;
  the commit says so.
- `grep` for each removed name over include, src, program, examples,
  bench and test gives nothing.

## 4. What this plan does not do

- No change of `phase::recover`, of the build or of the clean stop.
- No rename of `client_errc::recovering`.
- No change of the model.

## 5. Data of this plan

- Build directories under the scratchpad (main-build/*): made again by
  the build command.
- The state file mbedded-konsens.md records the change.

## 6. Question to the owner

- The error code `client_errc::recovering` has only the build as its
  cause after this change. Options: keep the name; rename it to
  `building`. Proposal: rename (the owner's word "build",
  plan-reconfiguration.md:99-100). Asked on a card.
