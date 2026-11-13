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

## Seeds 2 to 10

The same spike binary (code 654c088) wrote the traces of seeds 2 to 10.
TLC checked each against the same trace spec, three runs at a time,
`-Xmx700m`.

| seed | records | end | first record not matched | time (s) |
|---|---|---|---|---|
| 2 | 807 | refused | 21: deliver promise 2 to 3 | 5 |
| 3 | 558 | refused | 26: crash of node 1, not clean | 5 |
| 4 | 616 | refused | 33: deliver promise 3 to 2 | 5 |
| 5 | 839 | refused | 160: deliver promise 1 to 2 | 7 |
| 6 | 796 | refused | 27: deliver promise 1 to 2 | 8 |
| 7 | 570 | refused | 56: crash of node 1, not clean | 5 |
| 8 | 594 | refused | 30: deliver promise 2 to 3 | 4 |
| 9 | 602 | refused | 27: deliver promise 1 to 3 | 5 |
| 10 | 872 | whole trace accepted | none | 10 |

Seeds 3 and 7 end at a crash that is not clean. The trace spec maps
only a clean crash. A crash that is not clean retires the node (plan
clean-stop 4.9), so F6 of the plan ends the trace there, and phase 1
needs no more.

Seeds 2, 4, 5, 6, 8 and 9 are refused for one cause. A node holds a
proposal in its buffer and then becomes leader. At the majority of
promises the code sends an AcceptSync with an empty suffix, and then,
at the end of the step, an AcceptDecide that carries the buffered
proposal (`end_step`, include/konsens/sequence_paxos.hpp:471-477, the
group commit). `HandleMajorityPromises` of SequencePaxos.tla appends
the buffer to the log before the sync, so the model sends one
AcceptSync that carries the proposal. In each of the six traces the
network holds `accept_sync` and then `accept_decide` from the new
leader at the refused record.

So this refusal is also a fault of the model: the model is older than
the group commit.

## Not done

- Batches, clients, crashes that retire a node, and rollback were not
  traced.
- The values in the messages are not compared, only the types.
