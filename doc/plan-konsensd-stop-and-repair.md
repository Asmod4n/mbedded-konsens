# Plan: konsensd during a repair

Source: the owner, 2026-11-01 16:17, on doc/plan-jepsen.md section 8.
Base: main 624d1c0.

## 1. The faults

1. A SIGTERM during a repair ends the node as a crash. The loop of
   `serve` ends at the signal (`program/serve.cpp:937`), `stopped` calls
   `node::stop()` (`:861`), and `stop` throws `std::logic_error` in a
   repair (`include/konsens/node.hpp:267-270`). `main` catches it and
   ends with `exit_failed` (`program/main.cpp:80-85`). No clean-stop
   mark is written, so the next start retires the node. A stop that the
   operator asked for costs the node.
2. A call during a repair answers 500. `client_errc::repairing` has no
   entry in `problem_of` (`program/rest.cpp:118-132`), so it falls to
   `status_internal_server_error`. The node takes the call later, as it
   does after `building`, so the answer is 503 with Retry-After.
3. Found by the test of section 3: a repair in konsensd never ends. A
   peer that connects during the repair calls `reconnected`
   (`include/konsens/sequence_paxos.hpp:1229`). It opens a step, and a
   step in a repair is never closed. At the end of the repair,
   `vote_again` finds the open step and throws `std::logic_error`. The
   node ends without a clean-stop mark and retires at its next start.
   `reconnected` also sets the phase to recover and sends a PrepareReq,
   which a node in repair must not do.

Section 8.3 of the Jepsen plan also named `session_expired` and
`stale_request`. Both stay 500: konsensd numbers the requests of a
session itself and expires a session only after its connection closed
(`program/serve.cpp:377`, `:510`), so either reply is a fault of
konsensd, and 500 says that.

## 2. The change

1. The loop of `serve` runs while no stop is asked or while the node
   repairs: `while (!l.stop_requested || repairs())`. When a stop comes
   during a repair, konsensd writes once to stderr that the node stops
   cleanly after its repair. When the repair ends, the loop ends and
   `stopped` stops the node cleanly. A stop in a repair stays a wrong
   use of the library.
2. A repair that cannot end, because no peer answers, keeps the process
   alive after SIGTERM. SIGKILL ends it, and the node retires, as after
   every crash. The section on konsensd in README.md says this.
3. `problem_of` puts `client_errc::repairing` beside `building`: 503
   "the node cannot take the call now", with Retry-After.
4. `reconnected` does nothing in a repair, as it does in retirement. The
   resend timeout already sends the repair requests again.
5. konsensd writes a line when a repair starts and when it ends. A 503
   alone does not show a repair, because a retired node answers 503 too.
6. A repair that waits for the operator does not hold the stop: no peer
   can end it. konsensd writes that the node ends without a clean stop
   and exits with a failure.

One branch for both: the test of the one is the setup of the other.

## 3. Test (program/test/three_nodes.sh)

The block stands before the section that kills node 3, so all three
nodes stop cleanly. The test changes the first byte of the encoded entry
of the first log record of node 2 with sqlite3, through `hex` and
`unhex`, so the record stays a BLOB and its CRC fails. A changed byte in
the tail is repaired at the start and opens no repair. Node 2 starts alone,
finds the damage and starts its repair; no peer answers.

- The log of node 2 says that it repairs its store, and not that it
  started retired. A read at node 2 answers 503.
- SIGTERM to node 2: the process still runs one second later, and its
  log says that it stops after the repair.
- Nodes 1 and 3 start. Node 2 ends its repair, stops cleanly and exits
  with 0.
- Node 2 starts again and serves a write.

Every wait has a bound. A node that does not end within 30 s gets
SIGKILL, and the check fails; `end` does the same.

test/test_repair.cpp: a reconnect during a repair sends nothing, and the
repair still ends with a vote. Without change 4 the test throws
"a read of the storage or a take of messages while a step is open".

test_rest: `client_errc::repairing` gives 503.

## 4. Checks

Release g++ 16: program/test/three_nodes.sh (tcp and unix forms),
test_rest, test_repair and test_node. The CI runs every build.

## 5. Review

A second agent reviewed the plan. Its findings and what became of them:

1. A repair that waits for the operator never ends: change 6.
2. The changed byte must lie in the encoded entry: section 3.
3. The block must stand before the SIGKILL section: section 3.
4. The script can hang: the bounds of section 3.
5. A 503 does not show a repair: change 5.
6. test_rest has no check for `repairing`: section 3.
7. konsensd has no README: change 2 names the section in README.md.
8. The line "stops cleanly after its repair" is written at the condition
   of the loop, so a repair that starts after the signal writes it too.
9. The count of repairs never goes back, so every later damage of node 2
   waits for the operator. That is outside this plan.

## 6. Data

None beyond the build directory, which a build command makes again.
