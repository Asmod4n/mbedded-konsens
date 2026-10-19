# Review: plan of a stress form of the counter

Reviewed: `doc/plan-counter-stress.md`, against `examples/counter.cpp`,
`include/konsens/node.hpp`, `include/konsens/client.hpp`,
`include/konsens/sequence_paxos.hpp`, `test/simulation.hpp`, `README.md`,
`doc/plan-library-v0.1.md` section 7, `CMakeLists.txt` and both rule files.
Nothing was built.

## Findings

### Client behaviour that can hang

1. Fault: "Each client registers once." A registration at a follower goes
   out as one `proposal_forward`, and nothing resends it
   (`forward_proposals`, `resend_message_timeout`). A restart also clears
   `registrations_`. Either way, the client waits for good.
   Fix: a registration has the same timeout and resend as an increment.
   The client keeps the first session that comes back and ignores the
   others.

2. Fault: section 3 does not say when and where a refused call goes again.
   A `recovering` node refuses every call until rounds run. A loop that
   resends in the same round never ends.
   Fix: a refused call goes again in the next round, to a node of the
   random choice.

3. Fault: `session_expired` is not handled. The plan expires no session,
   so this reply can only come from a fault, and the client would wait.
   Fix: `session_expired` on any call ends the run with exit code 1 and
   the seed.

4. Fault: `stale_request` "sends the same call again". With one request
   at a time, `stale_request` can come only to an old call id (a resend
   that was decided after the next request). On the current call it is a
   fault, and a resend hides it.
   Fix: a reply to an old call is ignored. `stale_request` on the current
   call ends the run with exit code 1.

5. Fault: "runs rounds until every client has its N replies" and "until
   every node has applied the same decided index" have no bound. A
   liveness fault hangs the program and prints nothing.
   Fix: one named round limit for each phase. When it is reached, print
   the seed and the phase, and end with exit code 1.

6. Fault: the final read (check 2) can get `retry`
   (`drop_reads_of_an_old_promise`, `resend_read_index_requests`), and the
   plan does not handle it.
   Fix: a read that gets `retry` goes again in the next round, under the
   round limit of finding 5.

### Checks that cannot show their property

7. Fault: check 1 takes "the replies of all increments". A resent request
   can be answered at two nodes. The plan does not say which reply
   counts. A program that keeps every reply fails check 1 without a
   fault. A program that keeps any reply can hide a double execution.
   Fix: keep the first reply for each pair of client and request number.
   Each later reply for the same pair must be equal to it, else exit 1.
   This check is E1 directly.

8. Fault: check 1 shows nothing when no request was decided twice. The
   plan does not count this case, so a green run can be a run with no
   resend that reached the log twice.
   Fix: count the requests that the decided log holds more than once,
   and print the count. The defaults must give a count above 0, else the
   run fails.

9. Fault: check 2 is named R1, but one read after all faults stop shows
   only the final value. R1 is about a read that runs while writes are
   decided. "A name claims nothing that does not happen."
   Fix: name it "final value". For R1, let clients read while the faults
   run: a read that starts after m confirmed increments returns at least
   m.

10. Fault: the counter-check runs seeds 1 to 20 only on the broken
    library. A failure there does not show that the guard caused it.
    Fix: the unchanged program also passes seeds 1 to 20, and both
    results go into the commit message.

### Fault modes of the library

11. Fault: "Loss and duplicate keep the order" does not say where the copy
    of a duplicate goes, and "pair of nodes" does not say the direction.
    Fix: one FIFO queue for each ordered pair (from, to). The copy stands
    directly after the original in the same queue.

12. Fault: the restart does not give a new nonce. The README requires a
    new nonce at each start, and recovery matches responses by the nonce
    (`handle_recovery_response`).
    Fix: one counter of nonces, raised before each `node::with`, as `crash`
    in `test/simulation.hpp` does.

13. Fault: the plan does not say whether a restarted node is down for
    rounds, nor what happens to the messages that are queued to it.
    Fix: the stop and the start happen in one step, as `crash` does.
    Queued messages go to the new node.

### Missing steps for the builder

14. Fault: the plan does not say what an error from `tick`, `handle` or
    `node::with` does.
    Fix: print the seed, the step and the error message, and end with
    exit code 1, as `fail` in `counter.cpp` does.

15. Fault: the Debug build with ASan and UBSan has no switch in
    `CMakeLists.txt`.
    Fix: name the exact configure line, for example
    `-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-sanitize-recover=all"`,
    in its own build directory.

16. Fault: no test runs the program. The examples are built, but ctest does
    not run them.
    Fix: add an `add_test` for `counter_stress` with small `--clients`
    and `--increments` values and some seeds, or state that the owner
    leaves it out.

17. Fault: the counter-check changes `node.hpp`, and section 7 says "no
    change to the library".
    Fix: make the change in the working tree only, never commit it,
    revert it, and check with `git diff` that the tree is clean.

18. Fault: the flags have no ranges and no parser.
    Fix: parse with `std::from_chars`. Refuse a permille above 1000,
    `--clients 0`, `--increments 0` and a product C*N that overflows.
    Exit code 2.

19. Fault: no branch, commits or state file are named.
    Fix: one branch for the task. Commit and push each green step. Write
    the result into `Claude-state/mbedded-konsens.md`.

### Rules

20. Fault: "increments per second of wall time" is a number about speed
    from one run. It has no noise floor, no second arm and no file of
    results ("Measure before, measure after"; "Each run of the bench is
    one file"). A sentence of disclaimer does not satisfy the rule.
    Fix: drop the figure. Print rounds and message counts only. A
    throughput number comes later, through a bench run under the rules.

21. Fault: "as agent S did" names a session agent and no exact change
    ("Nothing points at a tool"; a name with no source).
    Fix: state the change: in `execute_request`, the guard
    `e.request_number > s.request_number` becomes `true`.

22. Fault: "a fixed number of rounds", the defaults and the round limits
    have no names ("A number has a name").
    Fix: one named constant for each, as in `counter.cpp`.

23. Fault: a reply table sized to C*N reserves memory from a count that
    the input gives ("A count in the input reserves nothing").
    Fix: collect the replies as they arrive, then sort them and compare
    with 1 to C*N.

Verdict: approve with changes
