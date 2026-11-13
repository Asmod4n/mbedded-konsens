# Spike: the trace of seed 1 with the channels, against the new model

Second run of step 0 of doc/plan-trace-validation.md. The first run
(20261112T222300Z-trace-spike-seed1.md) was refused at record 93,
because SequencePaxos.tla did not promise the same ballot again. Branch
model-promise-again (d00ac39) changes the model as the code does.

The sources are in the project files under
`actor-raft/trace-validation/spike-20261113/`.

## What changed in the trace spec

- The model is SequencePaxos.tla of model-promise-again d00ac39.
- The trace is the same trace of seed 1, code 654c088. Each record now
  also holds the messages in flight (`net`): sender, receiver, name and
  values, in the order of the network of the harness.
- A record matches only when, for each pair of nodes, the types of the
  messages in the model channel equal the Paxos messages of `net` for
  that pair, in the same order.

Without the channel match, the run with the new model reached record
172 and then did not end: from depth 171 on, each tick had two
continuations, a resend and no resend, and both matched the state of
the nodes. The run was stopped after about 8 min; the last progress line gave
3 385 668 distinct states. The channel match keeps one of the two (F7 of the plan: "a
resend that sends nothing" is a step of its own).

## Context

- TLC 2026.11.06.222357 (rev d8fb231, sha256 e1f11b75c4b5bc44...),
  OpenJDK 21.0.12.1, BFS, one worker, 3040 MB heap.
- Machine: kernel 6.18.44 (Firecracker VM), 4 cpus, run as root in the
  session container.

## Results

| run | end | first record not matched | distinct states | time (s, wall clock, JVM start included) |
|---|---|---|---|---|
| trace of seed 1 | `NotDone` violated: whole trace accepted | none (887 of 887 matched) | 7513 | 7 |
| counter-check: decided index of node 1 plus 1 at record 50 | refused | 50 | - | 2 |
| counter-check: first `decide` from node 2 to node 1 taken out of `net` at record 605 | refused | 605 | - | 4 |
| counter-check: the old form of `HandlePrepare` (`phase[i] = "Recover"` in both branches) | refused | 55 | - | 3 |

The old form is refused at record 55 now, not at 93: the channel match
sees the second Promise when it is sent, not when it is delivered.

## Not done

- No other seed was traced.
- Batches, clients, crashes that retire a node, and rollback were not
  traced.
- The values in the messages are not compared, only the types.
