# Review: plan of library v0 (commit f3bc0b4)

Verdict: approve with changes.

The reviewer read the plan, the result files in `model/results/`, the
configs and `SequencePaxos.tla` in `model/tla/`, `CMakeLists.txt` and
`README.md` of mbedded-cbor at 491582c, and the apt cache of this
machine. The reviewer ran no TLC and no build.

## Findings

1. **Place:** section 2, the table and the sentence "v0 contains only
   parts that the model already checks".
   **Fault:** the sentence is false. Line 3 of `SequencePaxos.tla`
   says "no BLE". The model has `HandleLeader(i, k)` in place of BLE,
   with at most two calls (`MaxElection = 2`). CRC32C, LMDB and the
   reopen are not in the model either. `Crash(i)` only assumes that
   the durable state survives.
   **Change:** use the table below. Add the line: "Not checked by the
   model: BallotLeaderElection, CRC32C, LMDB and its reopen. The C++
   tests are the only check of these parts."

   | Part | Run (complete) | Bounds | Not covered |
   |---|---|---|---|
   | SequencePaxos, max for decided_idx | regression-base-monotone 20261018T080254Z, 13 489 555 distinct states | 3 nodes, ballot n ≤ 2, log ≤ 2, 2 elections, no fault | faults |
   | S7 counter-check | a-base-source 20261017T001247Z, S7 violated | DecidedIdxMonotone = FALSE | - |
   | One crash | a-F3 20261016T225703Z | ballot n ≤ 1, log ≤ 1, Recovery off | model of 16 Oct, no regression |
   | One loss, one duplicate | a-F1-loss 20261017T011745Z, a-F1-duplicate 20261017T004507Z | ballot n ≤ 1, log ≤ 1 | no regression |
   | Recovery | F6b 20261017T052813Z, 45 345 778 distinct states | one rollback, counter ≤ 2, ballot n ≤ 1, log ≤ 1 | crash and loss with Recovery on; no regression |
   | Client sessions | regression-f 20261018T083502Z | `MaxElection = 1`: one leader, no change of leader | leader change, faults |
   | Read index | regression-e 20261018T085320Z | one read, one value, `MaxCrash = 0` | crash (e-crash stopped at 1 h) |

2. **Place:** section 2, the row "Recovery … F6b".
   **Fault:** the earlier F6b run with counter 3 stopped. The complete
   run has counter 2. The F6b run covers rollback only. Each run with
   Recovery on and a crash or a loss (c-crash, c-loss, d2-crash)
   stopped at its time limit. So "recovery after each start with a
   crash" is not checked.
   **Change:** give the bounds in the row. Write "partly: rollback
   only".

3. **Place:** section 2, the row "Read index … e".
   **Fault:** `MC_e.cfg` now holds `MaxCrash = 1`. The run with these
   bounds (20261018T102206Z) stopped at 1 h. The complete run had
   `MaxCrash = 0`.
   **Change:** name the run 20261018T085320Z and write "no crash".

4. **Place:** section 2, "each mutation breaks its property", and
   "S1 to S7" in the simulation test.
   **Fault:** S4 and S6 are properties of StopSign and of the next
   configuration. They have no meaning in v0. The mutations of S1, S2b,
   S3, S5, E1 and R1 and the counter-check of S7 did give violations.
   **Change:** write "S1, S2, S2b, S3, S5, S7, E1, R1".

5. **Place:** section 3, scope, and section 6, order.
   **Fault:** the scope does not fit one day for two agents. The scope
   has eight parts and a simulation that checks eight properties.
   Three of the parts have their own risk (findings 7, 8, 9).
   **Change:** make v0 the smallest cut below. Move the rest to v0.1,
   in this order: recovery, read index, client sessions, CBOR.
   - SequencePaxos on MemoryStorage and PersistentStorage.
   - BallotLeaderElection with fixed priority. It is needed when the
     owner stops the leader.
   - CRC32C, software form only.
   - The example `three_nodes.cpp`: write, read the decided entries,
     stop a node, reopen its LMDB, see the data.
   - The simulation with 3 nodes, one crash with reopen, one loss and
     one duplicate. These are the faults that the complete runs check.
     It checks S1, S2, S2b, S3, S5 and S7.

6. **Place:** section 3, "Recovery after each start" in the cut.
   **Fault:** the complete crash run a-F3 has Recovery off. With a
   synchronous commit, a crash keeps the durable state, and that run
   found no violation. Recovery guards against a rollback, for example
   a lost fsync. The owner test does not cause a rollback.
   **Change:** v0 has no recovery. The README says that v0 does not
   protect against a rollback of the disk.

7. **Place:** section 5 and section 7, mbedded-cbor.
   **Fault:** the risk is a fact, not a possibility. The README of
   mbedded-cbor says "packed and structs: C++26 reflection, g++ 16".
   It also says "the read side needs only C++23. It works with g++ 16
   and with clang 23". This machine has g++ 13.3 and clang 18.1.
   Nothing shows that the library compiles with these compilers.
   Packed CBOR needs `std::meta`.
   **Change:** v0 has no CBOR. Messages are C++ types. CBOR comes when
   the build uses the compilers that mbedded-cbor names, in a podman
   container.

8. **Place:** section 5, the test framework.
   **Fault:** mbedded-cbor uses doctest through
   `find_package(doctest REQUIRED)`. `doctest-dev` is not installed.
   The apt candidate is 2.4.11-1. `liblmdb-dev` is not installed. The
   candidate is 0.9.31-1build1.
   **Change:** add step 0, "install `doctest-dev` and `liblmdb-dev`".
   The owner approves the install. Use `find_package` for doctest and
   `pkg_check_modules(lmdb)` for LMDB.

9. **Place:** section 3, CRC32C "the hardware instruction where it
   exists".
   **Fault:** "Use what a library already has" puts a hand-made form
   outside the tree until the owner has seen its measurement. A form
   that no test has run is not written. A second hardware form makes
   two forms to test on one day. The reviewer did not find a CRC32C
   package in the apt cache (`libcrc32c-dev` has no candidate).
   **Change:** v0 has the software form only. The hardware form comes
   after a measurement outside the tree.

10. **Place:** sections 3 and 6, the specifications.
    **Fault:** "A grammar is read, not remembered" requires the text of
    each specification in the tree, in the same commit as the first
    function that reads it. The tree has no CRC32C specification. It
    has no LMDB API reference. `refs/omnipaxos/ORIGIN.md` names commit
    532ff79, but the tree has no BLE source.
    **Change:** step 1 downloads the CRC32C polynomial and check
    vectors from RFC 3720 (section 12.1 and appendix B.4) into
    `refs/`. It downloads `ballot_leader_election.rs` at 532ff79 into
    `refs/omnipaxos/` in the same commit as the BLE code. Code outside
    the tests has no comments, so the file and line citations stand in
    the tests.

11. **Place:** section 3, PersistentStorage.
    **Fault:** the plan has no error path for LMDB, for example
    `MDB_MAP_FULL`, a failed commit or a corrupt entry (CRC mismatch).
    The owner decides an error path.
    **Change:** add an open question for the owner with a provisional
    form. A failed commit or a CRC mismatch is a value of
    `std::error_code`. The node then stops voting. The test checks
    that a node with a CRC mismatch does not vote.

12. **Place:** section 6, "Each green step is committed and pushed".
    **Fault:** the plan names no branch. "A message is a task on its
    own branch" holds. Only the state repository has `main` alone.
    **Change:** name the branch, for example `library-v0`. Each agent
    works in its own build directory. At most four builds run at a
    time.
