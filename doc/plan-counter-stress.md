# Plan: a stress form of the counter

Source: the owner chose "Ja, bauen" on 2026-10-19 19:16 (UTC+2). The
owner wants to load the counter on a machine of the owner tonight.
This plan gets one review before the build.

## 1. Goal

One program, `examples/counter_stress.cpp`, runs three nodes in one
process with the counter state machine of `examples/counter.cpp`. Many
clients increment the counter while messages are lost or arrive twice
and nodes restart. At the end the program checks that each confirmed
increment was executed exactly once, at every node.

## 2. Command line

`counter_stress [--clients C] [--increments N] [--seed S] [--loss P]
[--duplicate P] [--restart R]`

| Flag | Meaning | Default |
|---|---|---|
| `--clients` | number of clients | 4 |
| `--increments` | increments of each client | 1000 |
| `--seed` | seed of the random choices | 1 |
| `--loss` | chance in permille that a message is lost | 10 |
| `--duplicate` | chance in permille that a message arrives twice | 10 |
| `--restart` | chance in permille per round that one node restarts | 5 |

A wrong flag prints the usage and ends with exit code 2.

## 3. Behaviour

- The network is one FIFO queue per pair of nodes, as the README
  requires. Loss and duplicate keep the order of the messages that
  arrive.
- A restart stops one node and starts it again from a copy of its
  `memory_storage`, as `crash` in `test/simulation.hpp` does. A restart
  starts only while no node recovers (plan v0.1, section 7: more than
  f nodes in recovery wait).
- Each client registers once at a node of the random choice, then
  sends its increments with request numbers 1 to N, one at a time. A
  call that has no reply after a fixed number of rounds goes again,
  with the same request number, to a node of the random choice. A
  reply with the error `retry`, `recovering` or `stale_request` makes
  the client send the same call again.
- The program draws every choice from one `std::mt19937_64` with the
  seed, so a seed repeats its run.

## 4. Checks at the end

After every client has its N replies, the program stops all faults
and runs rounds until every node has applied the same decided index.
Then:

1. The replies of all increments are exactly the numbers 1 to C*N,
   each once. This shows that each confirmed increment was executed
   once (E1).
2. A read through the read index at each node gives C*N (R1).
3. The decided logs of the three nodes are equal (S1).

A failed check prints the seed and the check, and ends with exit code
1.

## 5. Output

One line per check, and at the end: the count of rounds, messages,
lost messages, duplicates, restarts and changes of leader, and the
increments per second of wall time. The line says that the number is
for one process without a network, Release build, and not a
measurement of the library over a network.

## 6. Tests and build

- The program runs at the defaults and with `--loss 0 --duplicate 0
  --restart 0` on the host (g++ 13) in a Release build and in a Debug
  build with ASan and UBSan, and exits with 0.
- A counter-check: with the session guard of node.hpp removed (as
  agent S did), the program must fail check 1 for some seed of 1 to
  20. The result goes into the commit message.
- The README gets a short section with the command and the flags.

## 7. Not in this plan

No network, no LMDB, no change to the library.

## 8. Changes after the review

The review `doc/review-plan-counter-stress.md` (approve with changes)
holds 23 findings. Each one is applied as its fix says, with these
choices:

- F16: a ctest entry runs the program with small values
  (`--clients 2 --increments 50`) and the defaults of the faults.
- F19: branch `claude/project-thread-4xjeai`; one commit for the
  program, its ctest entry and the README; the state file in
  Claude-state after the push.
- F20: the program prints no speed figure. A speed number needs a
  measurement after the rules, and that is not part of this plan.
- F9: check 2 is named "final read". Reads while the faults run check
  that a read started after m confirmed increments returns at least m.
