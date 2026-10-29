# Review of plan-recovery-delta.md (commit bb9e77b)

Reviewer: a second agent of the session, read only, 2026-10-29.
Result: not ready; 5 blocking findings. The plan is stopped by B1/B5.

## Blocking

- B1. Section 3 rests on a wrong fact. `scan_the_whole_log` runs only
  when the node votes (node.hpp:151-155), that is after a clean stop or
  a first start. A start with no matching mark is retired
  (node.hpp:55-64). No path of `node` starts a recovery:
  `start_recovery` has one caller, the test helper
  (test/test_sequence_paxos.cpp:92). A chain mismatch on the own prefix
  cannot tell an own fault from a fault of the source; the owner rule
  of 2026-10-25 makes an own fault an exception. The state file lists
  both error paths as questions for the owner.
- B2. The plan does not say which node raises the fault for a start
  index above the log of the primary. The power-loss source limits the
  index (sequence_paxos.hpp:1801) and only the receiver faults (:1840).
- B3. `recovery` is sent from `start_recovery` (no transaction,
  :394-405) and from the resend at the timeout before `begin_step`
  (:662-667); `read_meta` needs a transaction (:1070). The source of
  the values must be named (for example the high-water mark, :281-291).
- B4. 25 literals of `recovery_response` and 5 of `recovery` in
  test_sequence_paxos.cpp, the helper `answer_of` (:198), and
  test_wire.cpp (:276-279, :385-386, :482, :498, names :36-47) need
  changes; several new tests are missing.
- B5. The simulation tests and counter_stress run `node`, and `node`
  never recovers, so they do not cover this change.

## Not blocking

N1 the receiver checks are complete and in the right order; N2 a
wrong `from_idx` faults where `handle_new_state` drops; N3 the one-step
form matches the model, and the log and accepted_round of AcceptSync
are written in one step (:1529, :1540, commit :469); N4 the start rule
stands a third time in `promise_for` (:1243-1255); N5 `from_idx` of a
non-primary answer; N6 field names as in `prepare`; N7 the measurement
is not defined; N8 only bodies 9 and 10 of the wire change; N9 the
Omni-Paxos citation is not in the tree; N10 other line references are
correct.

## Not checked by the reviewer

Uniqueness of nonces; `complete_recovery` after :1910; LMDB commit
atomicity; Omni-Paxos sources; TLC traces.
