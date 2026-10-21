# Plan: recovery after a restart of every node, in the code

Source: model/plan-power-loss.md (sections 2 and 7) and the model at
57044f2, checked in model/results/20261021T091016Z-power-loss-runs.md.
The owner settled the form on 2026-10-19 and 2026-10-21: with no leader
after a start, the nodes compare only the last entry; a node with less
gets the rest. This plan gets one review before the build.

## 1. Facts checked in this session

- `sequence_paxos.hpp:255`: a recovering node drops every message but
  `recovery_response`, `recovery` too. When every node recovers, no
  node answers.
- `sequence_paxos.hpp:350-352`: `resend_message_timeout` sends Recovery
  to every peer. `sequence_paxos.hpp:790`: `decided_idx` takes the
  maximum. The two fixes of the model are in the code already.
- `sequence_paxos.hpp:758-773` `handle_recovery`: only a primary sends
  its log, and a node that is no primary sends a default ballot, no log
  and `decided_idx` 0.
- `sequence_paxos.hpp:775-812` `handle_recovery_response`: the f + 1
  answers count every answer.
- `message.hpp:123-131` `recovery_response` has no field `recovering`,
  no `accepted_idx` and no CRC.
- `sequence_paxos.hpp:206` and `node.hpp:68`: the caller gives the
  nonce to `with`. The library makes no random number.
- The model copies the whole log of the source. The code must not: the
  owner says the whole log is too large to send.
- `follower.rs:13-51` (Omni-Paxos 532ff79), as the model states at
  `SequencePaxos.tla:678-687`: a node whose `n_accepted` is higher
  than that of the leader sends its log from the `decided_idx` of the
  leader; with the same `n_accepted` and a longer log, from the
  `accepted_idx` of the leader.

## 2. Change

1. `recovery_response` gets three fields: `recovering` (bool),
   `accepted_idx` and `last_crc` (the CRC32C of the entry at
   `accepted_idx`, 0 for an empty log). The log stays in the answer of
   a primary only. A node that is no primary sends its own
   `n_accepted`, `accepted_idx` and `decided_idx`; the model sends them
   only when PowerLoss holds, the code always (there is no switch in
   the code).
2. A recovering node answers `recovery` with `recovering = true` and
   `primary = false`. It changes no state when it answers.
3. The normal path counts only the answers with `recovering = false`,
   as the model does since 57044f2.
4. Power-loss path: an answer from every peer, none of them a primary.
   The source is the node with the highest (`n_accepted`,
   `accepted_idx`) among the answers and the node itself; the lowest
   node id breaks a tie.
   - Before it decides, the node compares the last entry: two answers
     (or an answer and the node itself) with the same `n_accepted` and
     `accepted_idx` and a different `last_crc` are a hardware fault.
     The node stops voting with a new error value that carries both
     node ids, the index and both CRCs (PAR: the node stays silent
     until it is repaired).
   - When the source is the node itself, it writes the promise (the
     highest of all answers and its own) and goes on as in the normal
     path.
   - Otherwise it sends `get_state` to the source with the nonce, the
     expected `n_accepted` and `accepted_idx` of the source, and the
     index to start from: its own `accepted_idx` when its
     `n_accepted` is equal to that of the source, else its own
     `decided_idx` (the rule of `follower.rs:13-51`).
   - The source answers with `new_state`: the nonce, its `n_accepted`,
     `accepted_idx`, `decided_idx` and the entries from the start
     index. A source whose `n_accepted` or `accepted_idx` changed since
     its answer answers with the changed values and no entries.
   - The node takes `new_state` only when the values match what it
     expected. Then it writes the entries with `append_on_prefix`, the
     promise, `n_accepted`, and `decided_idx` as the maximum of its own
     and that of the source, in one `write_atomically`, and goes on as
     in the normal path. Otherwise it drops all answers and sends
     Recovery to every peer again with the same nonce.
   - A source answers `get_state` whether it recovers or not, and
     changes no state.
5. The message names `get_state` and `new_state` come from VR Revisited
   (state transfer); the section is not checked. They are provisional.
6. The plan for the nonce (R10): the README says that the caller gives
   a fresh nonce for each start, after a power loss too. The binary
   takes it from the random source of the system.
7. Not in the code: the switch `PowerLoss` of the model. The code has
   the mode always.

## 3. Differences to the model

- The model sends the whole log in the answer; the code sends the last
  entry and gets the rest with `get_state`. The model takes the log in
  one step; the code takes it only when the source has not changed, so
  each step of the code is a step of the model or no step.
- The model has no CRC; the hardware fault is the case that S5 with
  AcceptedIdxIsLogLength excludes.

## 4. Tests

- Unit tests in `test_sequence_paxos.cpp`, one for each rule:
  a recovering node answers Recovery; its answer does not count in the
  normal path (the trace of results/20261021T081623Z-power-loss-
  rollback-S2.md, steps 7 to 14); every node restarts and the node
  with the highest (`n_accepted`, `accepted_idx`) is the source; the
  node is its own source; a different `last_crc` stops voting with the
  error value; a changed source gives a new Recovery round; `get_state`
  starts at `accepted_idx` or at `decided_idx` as section 2 says.
- `counter_stress` gets a fault "every node restarts" and runs seeds 1
  to 300 on memory and on LMDB; S1, S2, S2b, S3, S5 and S7 hold after
  each step.
- A test of the wire form: `recovery_response`, `get_state` and
  `new_state` through encode and decode.
- g++ 16, Debug with ASan and UBSan, and Release.

## 5. Not in this plan

The liveness of the mode, more than f rolled-back disks (card R1), the
hash chain for a large database, the CRC32C instruction path.

## 6. Order

1. Plan and review.
2. Message fields and wire test.
3. Answer of a recovering node and the normal path; unit tests.
4. Power-loss path with `get_state` and `new_state`; unit tests.
5. `counter_stress` fault; seeds.
6. README and state file.
