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
6. Tests:
   - Remove the 22 test cases that test only this recovery
     (test_sequence_paxos.cpp:626-1352, the list of section 1 without
     :363, :468, :609).
   - Adapt the cases that use it for one part: :363 (a retired node),
     :1057 (evidence of a different CRC32C), :1509, :1555, :1714. Each
     keeps what it tests with another path, or goes when that path is
     the recovery itself. The plan lists each decision in the commit.
   - The helper `start_recovery` and `answer_of` go.
   - test_wire.cpp: remove the cases of the four messages; the kind
     table follows.
   - test/simulation.hpp and examples/counter_stress.cpp: remove the
     conditions on `recovering()`.
7. Docs: README.md:119 and doc/storage-fault-matrix.md rows of the
   restart, the loss of a message and the rollback state form B: a
   start with no matching mark retires the node; it comes back as a
   new node.
8. The model keeps the recovery. A note in model/plan-recovery-delta.md
   says that the code has no recovery since this change.

## 3. Checks

- Release g++ 16: the changed test programs first, then the full run
  once (RULES: a run checks the work at hand first).
- g++ 15 and clang 22 Release, the full run.
- ASan and UBSan g++ 16, once before the merge into main.
- `grep` for each removed name over include, src, program, examples,
  bench and test gives nothing.

## 4. What this plan does not do

- No change of `phase::recover`, of the build or of the clean stop.
- No rename of `client_errc::recovering`.
- No change of the model.

## 5. Data of this plan

- Build directories under the scratchpad (main-build/*): made again by
  the build command.
