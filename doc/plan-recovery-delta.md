# Plan: the code of the recovery delta

Stopped on 2026-10-29 after review (doc/review-plan-recovery-delta.md).
The node never starts a recovery: a start with no matching clean-stop
mark retires the node (include/konsens/node.hpp:55-64), a clean start
votes at once, and `start_recovery` has no caller outside
test/test_sequence_paxos.cpp:92. So the change has no use in the node.

Branch recovery-delta-code, from main 03e9477. The model of this
change is checked: model/plan-recovery-delta.md,
model/results/20261029T111410Z-recovery-delta.md (no violation;
each mutation breaks the assertion).

## 1. Facts checked in this session

- A node in recovery sends `recovery{nonce}`
  (include/konsens/message.hpp:150-153).
- A primary answers with its whole log: `handle_recovery` reads the
  range 0 to log_len (include/konsens/sequence_paxos.hpp:1686-1698).
- The node takes it with `complete_recovery(0, l.log, ...)`
  (sequence_paxos.hpp:1726). The copied log passes no chain check on
  this path. `complete_recovery` already takes a start index and the
  entries after it (:1876-1884).
- The power-loss path picks the start index (:1767), asks with
  `get_state` (:1768, :1789-1811) and checks its own prefix chain
  extended with the entries against the chain of the source
  (:1839-1849). A start index above the log of the source is a hardware
  fault there (:1839-1841).
- The wire checks the entries of a `recovery_response` (src/wire.cpp:91-94).
- Nine tests build a `recovery_response` by hand
  (test/test_sequence_paxos.cpp:639-803).

## 2. The form

The model has one step: the answer of the primary carries the entries
after the start index. The code keeps one step too, so that the model
covers it. A second round with `get_state` would let the primary append
between the two messages, which the model does not have.

1. `recovery` carries the state of the node that recovers:
   `n_accepted`, `accepted_idx` and `decided_idx`, read from its meta
   record when it sends.
2. A primary computes the start index with the rule of the model and of
   the power-loss path: `accepted_idx` of the node when its
   `n_accepted` equals that of the primary, else its `decided_idx`. The
   rule is one function, called by both paths (RULES: a check exists
   once).
3. `recovery_response` replaces `log` by `from_idx` and `entries`. A
   node that is no primary sends no entries, as now.
4. The node checks, before it writes:
   - `from_idx` equals the start index it computes itself;
   - `from_idx + entries.size()` equals `accepted_idx` of the answer;
   - its own prefix chain to `from_idx`, extended with the entries,
     equals `last_chain_crc` of the answer.
   A failed check stops the vote with a hardware fault that names the
   source, as on the power-loss path.
5. Then `complete_recovery(from_idx, entries, ...)`, as now.

## 3. Error paths

Both follow rules that are in force; neither is a new question.

- A start index above the log of the primary contradicts the model
  (the assertion holds). It is a hardware fault, as on the power-loss
  path. A start index equal to the length of the log gives no entries;
  that is valid, as in Omni-Paxos (follower.rs:29-36).
- A damaged prefix of the node that no check found: the whole copy
  overwrote it, the delta stops the vote. The scan at each start reads
  the whole log (`scan_the_whole_log`, sequence_paxos.hpp:888-898),
  so the damage is found before recovery in most cases. A damage of
  the own node is an exception (owner, 2026-10-25), and the node then
  recovers from a peer (owner, 2026-10-24).

## 4. Wire

`recovery` and `recovery_response` change their fields. The wire is
stable only from 1.0 on. The kinds keep their numbers.

## 5. Tests

- The nine tests of section 1 take the new fields.
- New, in test_sequence_paxos.cpp:
  - equal `n_accepted`: the answer carries only the entries after
    `accepted_idx` of the node, and the log after recovery equals the
    log of the primary;
  - other `n_accepted`: the answer starts at `decided_idx`;
  - a prefix of the node with a different entry before the start
    index: the vote stops with a hardware fault;
  - an answer whose `from_idx` is not the start index: the vote stops.
- A mutation: the start index taken as `accepted_idx` always. The
  second test must fail. Run once, not kept.
- The simulation tests, Release g++ 16, the full run once after the
  part is green.

## 6. Measure

The bytes of the answer of a primary for a node that is k entries
behind in a log of N entries, before and after, in one test binary per
arm. A count of bytes, not a time; it needs no machine to itself.

## 7. What this plan does not do

- No change of `build_request` (a new node has an empty log).
- No change of the chain CRC; the chain hash is a plan of its own.

## 8. Data of this plan

- Build directories under the scratchpad (main-build/*): made again by
  the build command.
- The measurement: one result file in bench/results.
