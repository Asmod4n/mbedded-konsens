# Plan: library v0.1

Source: the cut of v0 that the owner confirmed on 2026-10-19. v0.1 adds
the parts that v0 left out. This plan gets one review by a fresh agent
before the build.

## 1. Scope

| Part | Form | Model |
|---|---|---|
| Recovery after each start | The rule of VR Revisited as in the model: after a start a node does not vote. It sends Recovery with a fresh nonce to all peers and waits for f+1 answers, one of them from a primary. It then takes the durable state of the primary and votes again. | `ApplyRecover`, `HandleRecovery`, `HandleRecoveryResponse`, `RecoveryComplete` |
| Client sessions | The session id is the log index of the RegisterClient entry. Each request carries a counter. The state keeps the last counter and its reply per session, so a request is executed once. A session ends by an ExpireSession entry. | `ClientTable`, `Executed`, E1 |
| Read index | The leader takes its decided index, confirms its leadership by one round of heartbeats to a majority, and answers when its applied state reaches that index. A follower asks the leader for the read index and answers from its own state. | `ReadStart`, `StartReadRound`, `ReadReply`, R1 |

The node API gets: `register_client()`, `request(session, counter,
payload)`, `read()`, each with its answer as a value from the decided
log. The library applies decided entries through a function that the
user gives (the state machine). The state machine is a template
parameter, so no call is indirect.

## 2. Not in v0.1

The liveness case of v0 (a follower that stays behind after a lost
Promise) changes the protocol of Omni-Paxos. It goes to the owner as a
question, with the trace. CBOR, reconfiguration, stage 1 and stage 2,
the witness and compaction stay out.

## 3. Tests

- The simulation test gets the three parts and checks E1 and R1 in the
  form of the model, besides S1 to S7. A crash in the simulation is a
  start with recovery.
- A unit test per new handler.
- Each new guard gets a counter-check: the test with the guard removed
  must fail. The result of each counter-check is written into the
  commit message.

## 4. Build

The Ubuntu 26.10 container (g++ 16, clang 22) and the host (g++ 13).
Both must be green.

## 5. Order

1. Agent R: recovery after start, in sequence_paxos and node, with
   tests.
2. Agent S at the same time: the state machine parameter, client
   sessions and the read index, with tests.
3. The simulation with all parts. Then the README.

At most two agents. The load stays within 4 processors.
