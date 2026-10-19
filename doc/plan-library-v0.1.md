# Plan: library v0.1

Source: the cut of v0 that the owner confirmed on 2026-10-19. v0.1 adds
the parts that v0 left out. This plan gets one review by a fresh agent
before the build. This version applies the review
`doc/review-plan-library-v0.1.md` (approve with changes). Section 6
holds the changes.

## 1. Scope

| Part | Form | Model |
|---|---|---|
| Recovery after each start | The rule of VR Revisited as in the model: after a start a node does not vote. It sends Recovery with a fresh nonce to all peers and waits for f+1 answers, one of them from a primary. It then takes the durable state of the primary and votes again. | `ApplyRecover`, `HandleRecovery`, `HandleRecoveryResponse`, `RecoveryComplete` |
| Client sessions | The session id is the log index of the RegisterClient entry. Each request carries a request number. The state keeps the last request number and its reply per session, so a request is executed once. A session ends by an ExpireSession entry. | `ClientTable`, `Executed`, E1 |
| Read index | The leader takes its decided index, confirms its leadership by one round of heartbeats to a majority, and answers when its applied state reaches that index. A follower asks the leader for the read index and answers from its own state. | `ReadStart`, `StartReadRound`, `ReadReply`, R1 |

The node API gets `register_client`, `client_request` and
`client_query` (the names of Raft dissertation 6.3, not checked). The
answers come out through `take_client_replies()` (section 6, F9). The library applies decided entries through a function that the
user gives (the state machine). The state machine is a template
parameter, so no call is indirect.

## 2. Not in v0.1

The liveness case of v0 (a follower that stays behind after a lost
Promise) changes the protocol of Omni-Paxos. It goes to the owner as a
question, with the trace. CBOR, reconfiguration, stage 1 and stage 2,
the witness and compaction stay out.

## 3. Tests

- The simulation test gets the three parts and checks S1, S2, S2b, S3,
  S5, S7, E1 and R1 in the form of the model. A crash in the simulation is a
  start with recovery.
- A unit test per new handler.
- Each new guard gets a counter-check: the test with the guard removed
  must fail. The result of each counter-check is written into the
  commit message.

## 4. Build

The Ubuntu 26.10 container in podman (g++ 16, clang 22) and the host
(g++ 13). Both must be green. Branch: `claude/project-thread-4xjeai`,
the branch of this session.

## 5. Order

0. Step 0, one commit: the new entry kinds, their LMDB encoding, the
   new messages and the empty API (F12).
1. Agent R after step 0: recovery after start, with tests.
2. Agent S after step 0, at the same time: the state machine
   parameter, client sessions and the read index, with tests.
3. The simulation with all parts, the rollback and two clients. Then
   the README.

At most two agents. The load stays within 4 processors.

## 6. Changes after the review

The ids are the findings of the review.

- F1, F2. The read index is the accepted_idx of the leader at the start
  of the round, as `StartReadRound` in the model. A HeartbeatAck counts
  only with the same ballot and round id, from a node whose promise is
  that ballot. A read is dropped when its node changed its promise,
  crashed or rolled back since the index came. The read message has a
  name of its own, not the name of the BLE heartbeat.
- F3. Recovery takes the conditions of the model: the answers come
  from f+1 peers; the primary holds the highest promise among the
  answers, and its pid is the pid of that ballot; the node keeps the
  higher promise; a node that is recovering or does not vote does not
  answer.
- F4. While it recovers, a node drops every message except
  RecoveryResponse, and refuses every call with the error value of
  section 7. When recovery completes, it goes to (follower, Recover)
  and sends PrepareReq.
- F5. A node recovers only when its durable promise is not the default
  ballot. A first start does not recover.
- F6. The caller passes the nonce to `node::with` as a value.
- F7. The recovered state reaches the disk in one `write_atomically`:
  `append_on_prefix{0, log}`, `set_accepted_round`,
  `set_decided_index`, `set_promise`. No new storage op and no durable
  mark.
- F9. Each call takes an id that the caller gives. The node gives the
  answers through `take_client_replies()`. A follower answers a query
  when its decided index reaches the index of the leader's answer. The
  caller can drop a pending call.
- F10. The RegisterClient entry carries the id of the caller and the
  pid of the node. The session id is the log index of that entry. The
  test runs two clients.
- F11. A concept `state_machine` with `apply(payload)` that returns a
  reply and `query(payload)` that reads the state. The library keeps
  the client table and passes only the payloads of requests. The node
  applies the decided log from index 0 at each start and after each
  recovery. The applied index never falls.
- F12. Step 0 (section 5). v0 LMDB files are not read by v0.1.
- F14. `expire_session(session)` at the leader appends ExpireSession.
  The caller decides the timeout.
- F15. The simulation gets a rollback: a restart from an earlier copy
  of memory_storage, as run F6b of the model. The counter-check of
  recovery runs with the rollback.
- F16. The simulation sends each request again after a loss, keeps loss
  and duplicates on, and runs reads across a change of leader.
- F17. Applied in section 3.
- F18. Applied: request number (VR Revisited 4.1), call names of Raft
  dissertation 6.3. Both sources are not checked here; refs/papers.md
  marks them.
- F19. Not applied. The owner decided that papers are not copied into
  refs/ (2026-10-17); refs/papers.md keeps the DOI and the section.
- F20. Applied in section 4.
- The stale AcceptSync guard keeps its state in memory only. This holds
  because a restart ends every channel, and recovery runs before the
  node takes an AcceptSync again.

## 7. Open questions for the owner (provisional forms)

Each provisional form is built and marked, so the work goes on.

| Case | Provisional form |
|---|---|
| A call during recovery | error value `recovering` |
| A request for an expired session | error value `session_expired` |
| A request number below the last one | error value `stale_request`; the same number gives the kept reply |
| A query with no known leader or a changed promise | error value `retry` |
| All nodes start at the same time, so no primary answers (F8) | as the model: recovery waits; the README says the operator acts |
