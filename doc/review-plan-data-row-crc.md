# Review of doc/plan-data-row-crc.md

Reviewer: a second agent, read only, on data-row-crc 19e762a (main
0ebb361). Verdict: approve with changes. Findings 1, 2 and 3 go into
the plan before the build starts. All findings are folded into the plan.

## Facts of section 1

F1 to F4 and F6 to F8 agree with the code. F5 had two errors: the
retirement after a start with no clean-stop mark comes from
`kind_of_the_start` (node.hpp:67-91); and the check threads catch every
exception (:1427, :1645) and the node thread throws it again (:1662),
konsensd `main` catches `std::exception` (program/main.cpp:82). The
process still ends.

## Findings

1. Blocking. A damaged data row is raised by `transaction_view::find`
   (state_machine.hpp:41-48) inside `apply` (node.hpp:2183) and `query`
   (:2556), not by `read_or_throw`. The callers are static or const and
   cannot write a proof; the check threads must not write node.db;
   `apply_at_the_start` runs in `node::with` (:246). Fix: a damage type
   of its own derived from `read_error`, caught on the node thread, and
   every catch site named.
2. Blocking. `find` gives an error code only, so the proof cannot hold
   the two CRCs. `proof_kind::damage` means a damage of the log with a
   log index. A key field changes `fault_proof_size` and the format of
   node.db. Fix: a new kind, 0 in the CRC fields, no key.
3. Blocking for Jepsen. `damage-a-value!` flips the last byte of
   `value` (db.clj:135-143); under arm A that is a CRC byte, and the
   check passes with no check in the code. `:damage-data` is not in
   `faults-that-retire` (nemesis.clj:10). Fix: flip a byte of the value
   part, add the fault to the set, expect no answer and a retired start.
4. Not blocking. Every reader of `data` and `session` goes through
   `find_value` or `find_session_in`. `copy()` copies a damaged row
   unchecked; only tests call it.
5. Not blocking. A damaged session row is in scope; add a node test
   for it and one with `check_threads` above 0.
6. Not blocking. Arm B: the upsert sets both columns; compare the CRC
   as `std::int64_t`; the span of column 0 stays valid; 5 to 7 bytes
   more per row; a format 8 store fails as `unsupported_format`. Add a
   test of a second put of one key.
7. Not blocking. Arm A: the buffer keeps the largest value; a blob of
   0 bytes gives a null pointer, so check the length first; reset the
   statement on a refusal.
8. Not blocking. The model hides with `MaxDamages = 1` that two
   misdirected writes act as a lost write; it has no lost erase, no lost
   first write and no flip of the CRC.
9. Not blocking. TLC reports a deadlock after a node ended; set
   `CHECK_DEADLOCK FALSE` or run with `-deadlock`.
10. Not blocking. The chance of 2^-32 holds for random faults only;
    CRC32C is linear and a client chooses keys.
11. Not blocking. Section 7 must test the changed code only; the plan
    needs a list of its data; the bench cases `find` and `put` are new;
    bench/raw_konsens.cpp keeps the old schema.
12. Not blocking. test_persistent_storage.cpp:566-590 stays valid;
    three_nodes.sh damages `log` and `meta` only; sqlite_damage.hpp
    needs a helper for `data`.

## Not checked by the reviewer

TLC, the build and the tests. The sources of section 2.
