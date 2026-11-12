# Spike: the trace of seed 1 against SequencePaxos.tla

Step 0 of doc/plan-trace-validation.md. The spike stands outside the
tree. Its sources are in the project files under
`actor-raft/trace-validation/spike-20261112/`: the patch of
`test/simulation.hpp` that writes the records, the test case, the trace
spec and its config.

## What was measured

- Code: mbedded-konsens 654c088 (clean), mbedded-cbor c9214cd.
- Run: `simulation::run<sqlite_cluster>(1, delivery::fifo, true,
  rollback::none, clients::none, batches::single)`.
- Records: one per step of the harness. A record holds the event (kind,
  nodes, message name, value) and the state of all three nodes after the
  step: promise, n_accepted, log, decided index, role, phase. A ballot is
  `n * 10 + pid`. The trace is a TLA+ module.
- Check: `TraceSimulation.tla` extends SequencePaxos.tla. Constants are
  those of `MC_base.cfg`, with bounds raised (MaxBallot 9, MaxLog 60,
  Cap 60, MaxCrash 1, MaxLoss 1, MaxDuplicate 1, MaxTimeout 2000,
  MaxElection 200) and Values = 60 records `[type |-> "Value", ...]`.
  A record moves the line counter `l` only when the model state equals
  the record. Up to 3 extra actions of the touched node are allowed
  (HandleLeader to the recorded ballot, LeaderCountsOwnPromise,
  LeaderCountsOwnAccepted, and ResendMessageTimeout on a tick). The end
  of the trace is the violation of the invariant `l <= Len(Trace)`.

## Context

- Build: g++ 16.2.0 (Ubuntu 16.2.0-3ubuntu1), `-g -O1 -std=c++23`,
  libstdc++ of g++ 16, glibc 2.44, SQLite 3.53.4, in podman 4.9.3, image
  ubuntu:26.10.
- TLC 2026.11.06.222357 (rev d8fb231, tla2tools.jar v1.8.0, sha256
  e1f11b75c4b5bc44...), OpenJDK 21.0.12.1, one worker.
- Machine: kernel 6.18.44-fc-v378 (Firecracker VM), 4 cpus, Intel Xeon
  2.30 GHz, run as root in the session container.

## Results

| measure | value |
|---|---|
| records in the trace of seed 1 | 887 |
| records accepted | 92 |
| first refused record | 93: deliver promise from node 1 to node 3 |
| TLC time to the refusal (wall clock, JVM start included) | 4 s |
| distinct states | 8205 |
| largest out-degree | 2 |

Counter-check: the same trace with the decided index of node 1 raised
by one at record 50 is refused at record 50 (TLC, 2 s).

## The first refusal

At record 93 the code delivers a Promise from node 1 to node 3. In each
model state at that line, the head of the channel from node 1 to node 3
is a ProposalForward. The code had delivered the ProposalForward of
value 8 at record 81, so in the code the Promise stood behind it.

Cause, found with a second run that also records the messages in
flight. In the code the channel holds, from record 69 on, a Promise for
each Prepare of ballot 13 that node 1 received: node 1 promises the same
ballot again (plan doc/plan-promise-again-on-the-same-ballot.md, owner
card 2026-10-31). `HandlePrepare` of SequencePaxos.tla (line 1039)
promises the same ballot again only in phase Recover. So the model has
fewer Promise messages in the channel, and the head differs.
AcceptWithResult.tla holds the new form (`NoPromiseAgain`, line 134);
SequencePaxos.tla does not.

So the first refusal is a fault of the model, not of the code: the
model is older than the code (risk R1 of the plan). The second
suspected cause, the release of messages before the sync, keeps the
order per peer and is not the cause here.

## Not done

- No other seed was traced.
- Batches, clients, crashes that retire a node, and rollback were not
  traced.
