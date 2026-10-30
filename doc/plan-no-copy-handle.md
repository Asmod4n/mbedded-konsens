# Plan: node::handle(const received&) copies no body

Source: the rule "Nothing is copied to pass it" (Claude-state RULES.md),
and the owner, 2026-10-20 07:37 to 07:39 (UTC+2): the decoded value
belongs to the user, and no second owner is made for it. Open since
44b72e8: `node::handle(const received&)` copies each body into a
`message`, because `sequence_paxos::handle` takes a `paxos_message` by
value. This plan gets one review before the build.

## 1. Facts checked in this session

- `node.hpp:121-133`: `handle(const received&)` builds
  `paxos_msg{std::in_place_type<T>, *body}` from `cbor::ref<T>`. This
  copies every member of the body, the vectors of entries too.
- `sequence_paxos.hpp:239-294`: `handle(paxos_message m)` moves
  `promise`, `accept_sync`, `accept_decide`, `recovery_response` and
  `proposal_forward` into their handlers.
- `storage.hpp:80-87`: `append_entries` and `append_on_prefix` own a
  `std::vector<entry>`. The ops live only during `write_atomically`.
- `memory_storage.hpp:43-49`: `apply` takes the op by `const&` and
  copies the entries into its log. `persistent_storage.cpp:346-351`
  encodes them into the store. Neither needs to own the vector of the op.
- `sequence_paxos.hpp:541`: `accept_entries_leader` copies `entries`
  into the op.
- `leader_state::set_promise` (`sequence_paxos.hpp:86-101`) keeps the
  `log_sync` of a promise only when that promise is the greatest.

## 2. Change

1. `append_entries` and `append_on_prefix` hold
   `std::span<const entry>`. `memory_storage` copies as it does today.
   `persistent_storage` does not change.
2. `sequence_paxos` gets `handle(node_id from, node_id to, const T& msg)`
   for each alternative `T` of `paxos_msg`, constrained to those
   alternatives. It makes the checks of `handle(paxos_message)` today
   (error, `to`, `from`, recovering) and calls the handler.
   `handle(const paxos_message&)` visits into it. Every handler takes
   `const T&`.
3. Data that konsens keeps after the call is copied at the place that
   keeps it, and only when it keeps it:
   - `set_promise` takes `const std::optional<log_sync>&` and copies
     only when the promise is the greatest.
   - `recovery_response`: the log is copied when it is stored.
   - `proposal_forward`: a leader appends through the span, with no
     copy; a follower that buffers copies.
4. `ballot_leader_election` gets `handle(node_id from, node_id to,
   const T& msg)` in the same form.
5. `node::handle(const received&)` visits `r.msg` and calls one of
   these with `*body`. `node::handle(message)` becomes
   `handle(const message&)`.

No public name is new. The overloads take the names that exist.

## 3. Tests

- All tests of today pass with g++ 15, g++ 16 and clang 22, Debug with
  ASan and UBSan, and Release.
- A new test in `test_node.cpp`: a storage that records the address of
  the first byte of each entry that `write_atomically` gets. An
  `accept_decide` goes through `encode`, `decode` and
  `node::handle(const received&)`. The test expects the address that
  `cbor::ref` holds for that entry. Before the change the test fails.
- Seeds 1 to 300 of `counter_stress` with and without `--cbor`, equal
  output.

## 4. Measurement

The change is made for the rule, not for speed. The measurement
compares the time of both arms. One file outside the tree: decode and
`node::handle(const received&)` of an `accept_decide` with 64 entries
of 1 KiB each, into a storage that does nothing. One binary per arm
(before, after), each built the same way with g++ 16 and clang 22,
Release. The noise floor is measured first. The arms run alternately,
and the medians are compared. One file of results per run, as the
rules say.

## 5. Not in this plan

The copies on the sending side (`accept_entries_leader` puts the
entries into each `accept_decide` for each follower), Packed CBOR, and
any change to the wire form.

## 6. Order

1. Storage ops as spans; tests green.
2. `sequence_paxos` and `ballot_leader_election` take `const T&`;
   `node` calls them; the new test; tests green.
3. Measurement.
4. README if a public signature changed; state file.

## 7. Changes after the review

The review (approve with changes, 12 findings) is applied as follows.

- R1: `put_entries` in `persistent_storage.cpp` takes
  `std::span<const entry>`. Step 1 changes `persistent_storage` too.
- R2: every test that builds an op from a braced list puts the entries
  in a named `std::vector` first. No op is built from a temporary.
- R3: `forward_proposals` keeps its `std::vector<entry>` parameter. The
  `const&` handler of `proposal_forward` copies once into it, whether
  the follower buffers or forwards.
- R4: the six places that build an op refer to named objects:
  `handle_majority_promises` (the locals `suffix` and `proposals`),
  `handle_recovery_response` (the stored copy, cleared after the
  write), `handle_acceptsync` and `handle_acceptdecide` (the body),
  `accept_entries_leader` and `append` (their local vectors).
- R5: one function in `node` keeps today's dispatch: `serve_clients()`
  after a Paxos message, and a BLE message dropped while
  `recovering()` holds.
- R6: step 4 is dropped. `ballot_leader_election::handle(const
  ble_message&)` stays. `node` builds the `ble_message` from the small
  body, as today.
- R7: the constraint is written inline in the form of `node.hpp:126`.
  No trait is made.
- R8: the test compares `span.data()` in `write_atomically` with
  `entries.data()` of the decoded `accept_decide` in `cbor::ref`. It
  brings the follower to (follower, accept) with the expected sequence
  number first. The recording storage wraps `memory_storage`.
- R9, R10: the bench encodes one message per iteration with a rising
  sequence number, outside the timed part. The storage sums the payload
  sizes. Two cases: 64 entries of 1 KiB, and one `decide`. Columns for
  g++ 15, g++ 16 and clang 22, Release. Each arm names its commit. The
  run length comes from the state file. One cpu stays free, the
  priority is raised and recorded, no root, no pinning. Section 4 states
  no expected result.
- R11: `counter_stress` output is compared per seed before and after
  the change, in each mode.
- R12: the README states that the span of an op is valid only during
  `write_atomically`.
