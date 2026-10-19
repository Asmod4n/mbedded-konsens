# Review: plan of library v0.1

The reviewer read `doc/plan-library-v0.1.md`, the headers in
`include/konsens/`, `src/persistent_storage.cpp` (entry encoding only),
`README.md`, `refs/papers.md`, the review of the v0 plan, and these parts
of `model/tla/SequencePaxos.tla`: `RecoveryResponseMsg`,
`RecoveryPrimaries`, `RecoveryComplete`, `HandleRecovery`,
`HandleRecoveryResponse`, `Dispatch`, `Restart`, `HandleLeader`,
`ResendMessageTimeout`, the client part to `ExpireSession`, `ReadStart`,
`StartReadRound`, the read handlers, `ReadReply`, `E1` and `R1`. The
reviewer ran no TLC and no build.

Each finding has a place, a fault and a change.

## Differences from the model

1. **Place:** section 1, read index, "takes its decided index".
   **Fault:** the model takes `accepted_idx` of the leader at the start
   of the round (`StartReadRound`, and the comment above `NoRead`). A new
   leader can have a `decided_idx` below an entry that the old leader
   decided and answered. A read with that index breaks R1.
   **Change:** write "takes its accepted index at the start of the
   round", as the model.

2. **Place:** section 1, read index, the answer step.
   **Fault:** `ReadReply` answers only when the node had no crash, no
   rollback and the same promise since it got the index. A
   `HeartbeatAck` counts only with the ballot and the round id of the
   current round, and only from a node whose promise is that ballot. The
   plan names none of these guards.
   **Change:** list the guards of `HandleHeartbeat`, `HandleHeartbeatAck`
   and `ReadReply`. A read whose node changed its promise is dropped.

3. **Place:** section 1, recovery, "one of them from a primary".
   **Fault:** `RecoveryPrimaries` takes only the primary of the highest
   promise among the answers, and its pid must be the pid of that
   ballot. The f+1 answers come from peers only. The node keeps the
   higher of its own promise and the promise of the primary. A node that
   does not vote, or that recovers itself, does not answer.
   **Change:** copy these four conditions into the plan.

4. **Place:** section 1, recovery, "and votes again".
   **Fault:** in the model the node then goes to (follower, Recover) and
   sends PrepareReq with the new promise. While it recovers, `Dispatch`
   drops every message except RecoveryResponse, and `HandleLeader`,
   `Propose`, `ClientSend`, `ReadStart` and `StartReadRound` do not
   occur. The plan does not say this.
   **Change:** add: "While a node recovers, it drops each message except
   RecoveryResponse. `tick()` does not call `handle_leader`. `append`,
   `request` and `read` return an error value." The error value goes to
   the owner (finding 13).

5. **Place:** section 1, recovery, "after a start".
   **Fault:** a node with the default promise starts the cluster. If it
   waits for a primary, no cluster ever starts. The model starts with
   `recovering = FALSE` and enters recovery only in `Restart`. The code
   of v0 makes the same difference (`*p != ballot{}`).
   **Change:** write "a start with a durable promise that is not the
   default ballot".

## Missing steps for the build agents

6. **Place:** section 1, recovery, "a fresh nonce".
   **Fault:** the plan does not say where the nonce comes from. In the
   model `nonce` grows over restarts and is not part of `DurableOf`. The
   `storage` concept has no field for it.
   **Change:** the caller gives the nonce to `node::with` as a value. The
   library reads no clock and no random source. The test checks that a
   response with an old nonce is ignored.

7. **Place:** section 1, recovery, "takes the durable state of the
   primary".
   **Fault:** the plan does not say how this state reaches the disk.
   **Change:** one `write_atomically` with `append_on_prefix{0, log}`,
   `set_accepted_round`, `set_decided_index` and `set_promise`. No new
   storage op is needed. No durable mark "recovering" is needed, because
   each start recovers again.

8. **Place:** section 1, recovery.
   **Fault:** when all nodes start at the same time, no node is a
   primary and no node answers Recovery. The cluster then never starts
   again. A power loss of the whole cluster causes this. The model
   excludes it (`Rollback`: "a full cluster from a backup is no fault",
   and one crash only).
   **Change:** this is a question for the owner. Provisional form: the
   library does as the model, and the README says that a restart of all
   nodes needs the operator.

9. **Place:** section 1, the node API.
   **Fault:** `register_client()`, `request()` and `read()` cannot give
   their answer as a return value. Each answer comes after a round of
   messages. The plan names no way back to the caller, and no way for a
   follower to give the read answer to its client.
   **Change:** each call takes an id from the caller and returns at once.
   The node gives finished answers through `take_client_replies()` and
   `take_read_replies()`, in the form of `take_outgoing_messages()`. A
   follower gives the read answer when its decided index reaches the
   read index of the ReadIndexReply. The caller can drop a pending read.

10. **Place:** section 1, client sessions, "the session id is the log
    index of the RegisterClient entry".
    **Fault:** two clients write equal RegisterClient entries. A
    follower forwards the entry and does not know its index. The node
    cannot find which decided entry belongs to which caller. The model
    has one client only.
    **Change:** the RegisterClient entry carries the id of the caller
    (finding 9) and the pid of the node. The test runs at least two
    clients.

11. **Place:** section 1, the state machine.
    **Fault:** the plan does not state the concept of the state
    machine, the type of a reply, or where the client table lives. The
    model keeps the client table in the replicated state and applies it
    from the decided prefix.
    **Change:** state a concept, as `konsens::storage` does: `apply` of a
    request payload returns a reply, and a query reads the state. The
    library keeps the client table itself and passes only the payload of
    a Request entry to the user. At each start and after each recovery,
    the node applies the decided log from index 0 again. The applied
    index never falls.

12. **Place:** section 1, the entries and the messages.
    **Fault:** `entry` has the kinds `normal` and `stop_sign` only, and
    `persistent_storage.cpp` refuses each other kind when it reads. The
    plan names no new kind, no field for session and request-number, and
    no new message type (Recovery, RecoveryResponse, ReadIndex,
    Heartbeat, HeartbeatAck, ReadIndexReply). Both agents edit
    `message.hpp`, `storage.hpp`, `sequence_paxos.hpp` and `node.hpp` at
    the same time.
    **Change:** step 0 adds the entry kinds, the encoding in LMDB, the
    message types and the empty API in one commit. Agent R and agent S
    start from that commit. The read heartbeat is a Paxos message with
    a name that differs from `heartbeat_request` of BLE. The plan says
    that LMDB files of v0 are not read.

13. **Place:** whole plan.
    **Fault:** the plan has no open question. The owner decides each
    error path: a request of an expired session (Raft 6.3
    SESSION_EXPIRED), a request-number below the last one, a read with
    no known leader or a changed promise, a call while the node
    recovers, and finding 8.
    **Change:** add a section with these questions, each with options and
    a provisional form. Send them in one message.

14. **Place:** section 1, client sessions, "A session ends by an
    ExpireSession entry".
    **Fault:** the plan does not say who appends that entry. In the
    model only the leader in Accept appends it, after a heartbeat
    timeout of the client.
    **Change:** add `expire_session(session)` on the leader. The caller
    decides the timeout, because the library reads no clock.

## Tests that cannot show the property

15. **Place:** section 3, "A crash in the simulation is a start with
    recovery", and the counter-check.
    **Fault:** with a synchronous commit, a crash keeps the durable state.
    The run a-F3 found no violation with recovery off. So the test of
    recovery with the guard removed does not fail. Only a rollback shows
    the need for recovery (run F6b).
    **Change:** the simulation adds a rollback: a node restarts from an
    earlier copy of its `memory_storage`. The counter-check of recovery
    uses that fault.

16. **Place:** section 3, the check of E1 and R1.
    **Fault:** E1 fails only when a request is decided twice, and R1
    fails only when a read spans a change of leader. A simulation with
    one client and no resend shows neither.
    **Change:** the simulated client sends each request again until it
    has the reply, with loss and duplicates on. Reads run while BLE
    changes the leader. The counter-check of finding 1 uses the decided
    index.

17. **Place:** section 3, "checks E1 and R1 in the form of the model,
    besides S1 to S7".
    **Fault:** S4 and S6 have no meaning without reconfiguration
    (finding 4 of the v0 review).
    **Change:** write "S1, S2, S2b, S3, S5, S7, E1, R1".

## Rules

18. **Place:** section 1, `request(session, counter, payload)`.
    **Fault:** "Every name comes from a source outside this tree". The
    model and VR Revisited 4.1 say request-number. Raft 6.3 names the
    calls RegisterClient, ClientRequest and ClientQuery.
    **Change:** use `request_number`. Take the names of the calls from
    Raft 6.3, or show the owner the choice.

19. **Place:** section 1 and section 5.
    **Fault:** "A grammar is read, not remembered". `refs/papers.md`
    holds references only, with "Section numbers: not checked" for VR
    Revisited and the Raft dissertation.
    **Change:** the first commit of each part downloads the text of
    VR Revisited (section 4.3) and of the dissertation (sections 6.3 and
    6.4) into `refs/`.

20. **Place:** section 5.
    **Fault:** the plan names no branch (finding 12 of the v0 review
    again), and the container is not named as podman.
    **Change:** name the branch, for example `library-v0.1`. Write "the
    Ubuntu 26.10 container in podman".

Verdict: approve with changes
