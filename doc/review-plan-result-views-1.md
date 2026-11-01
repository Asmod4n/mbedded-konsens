# Review: doc/plan-result-views.md (branch result-views, 597b6d9)

mbedded-cbor read at 9126904. Paths in the worktree are relative to
wt-rv. Paths in the library are relative to fc/mbedded-cbor.

## Task, as I understood it

Goal: find the faults in the plan before it is built. Place: the
worktree wt-rv and the mbedded-cbor checkout at 9126904. Assumption: the
plan text at 597b6d9 is the plan under review. I do not build, commit or
push. I change no file except this review.

## Findings

### 1. §1 is true, with one gap and one misleading attribution (not blocking)

True, checked:
- src/verdict.cpp:57-60 `decode_result` calls `decode_bytes` (39-48).
  Line 41 copies the bytes into a new `std::string`.
- include/konsens/verdict.hpp:16-22 `request_result`;
  include/konsens/state_machine.hpp:15-20 `mutation` with
  `std::vector<std::byte> key` and `std::optional<std::vector<std::byte>> value`.
- structs.hpp:830 `out.resize(count)`, 832-835 one element per loop step.
  `std::byte` is a typed-array element (structs.hpp:274-282), so a
  `std::vector<std::byte>` is tag 64 over a byte string.
- structs.hpp:589-598: a `std::span<std::byte const>` reads a byte
  string (major type 2) as a view, with no copy.
- Callers: node.hpp:1316, 1611, 1839 and verdict.cpp:520-521.
- The numbers match bench/results/2026-10-31-2045Z-konsensd-callgrind-at-64-in-flight.txt:19-24.

Gap: decode_bytes copies a second time. verdict.cpp:47 `return **decoded;`
copies the whole `request_result` out of the `cbor::ref`, because
`ref::operator*` gives `T const&` for a type that is not a view
(ref.hpp:112). So each byte member is copied twice today. The view form
keeps one copy: the `std::vector<mutation_view>`.

Misleading: the result file attributes the 16 % to "struct_read of
accept_decide and entry, decode_*" (line 19). It does not split
generic_read of `std::vector<std::byte>` (8.2 %, line 24) by caller.
`entry::payload` and `entry::result` (storage.hpp:41, 46) are also
`std::vector<std::byte>` and are read the same way. The plan changes
only the members of `request_result`. Its share of the 8.2 % is not
known.

Fix: add line 47 to §1. Write that the 16 % and the 8.2 % include the
decode of `entry` and `accept_decide`. Name the measurement that gives
the share of decode_result (cgcallers.py on the 2045Z callgrind files),
or measure only the arms of §4 and say nothing about the share.

### 2. The owner of the decode is not named, and the library refuses a null or empty one (blocking)

§2.2 says "decodes from the span of the caller with no copy". The plan
does not say which owner it passes.

- Every `structs<T>::decode` that reads views needs a
  `std::shared_ptr<void const> owner` (structs.hpp:1337-1349).
  decode_with_owner calls `throw_logic_error_if_empty` (structs.hpp:1420).
- validity.hpp:189-194 tests the owner with `owner_before` against an
  empty `shared_ptr`. A null owner throws `std::logic_error`. An aliasing
  `shared_ptr` that is built from an empty `shared_ptr` has no control
  block, so it also throws.
- No public decode of a struct gives T by value from a borrowed range.
  `read<true>` is private (structs.hpp:1437). `schema<T>::at_path` over a
  bare `std::string_view` requires `value_holds_no_view` (schema.hpp:3323).
  The library gives a view only together with an owner.
- The decode takes a `borrowed_range_of_chars` (ref.hpp:58-60). A
  `std::span<const std::byte>` is not one. The call passes
  `chars_of(encoded)` (verdict.cpp:101).

The real owner exists at the hot callers, with no allocation.
`read_the_open_log` gives `open_log_view{std::shared_ptr<const std::vector<entry>> owner, ...}`
(sequence_paxos.hpp:1162-1180). The owner is never null (make_shared at 1177).
node.hpp:1598 (apply) and 1208 (check) hold it as `log` and `tail`.

Proposal (owner decides, because it is a lifetime question):

```cpp
// verdict.hpp
[[nodiscard]] std::optional<request_result_view> decode_result(std::shared_ptr<void const> owner,
                                                               std::span<const std::byte> encoded);
// verdict.cpp
const auto decoded = cbor::structs<request_result_view>::decode(std::move(owner), chars_of(encoded));
if (!decoded) [[unlikely]] {
    return std::nullopt;
}
return **decoded;
// node.hpp:1611
decode_result(std::shared_ptr<void const>(log->owner, e.result.data()), e.result)
```

The parameter `owner` states the lifetime in the declaration, as §2.2
requires. No comment is needed for it. `done_log` (node.hpp:1125-1130)
gets the owner beside `done`, built at node.hpp:1224 from `tail->owner`,
for node.hpp:1316 and 1839.

`results_agree` takes two spans with no owner (verdict.cpp:514). It
runs only when the bytes differ and a float rule exists (verdict.cpp:517).
It is a cold path. There, both `cbor::ref` stay local in verdict.cpp,
each over its own `std::make_shared<const std::string>` copy. Nothing
leaves the function, and cbor types stay inside the module.

Options that the owner can choose instead:
- A. An owner with a no-op deleter: `std::shared_ptr<void const>(encoded.data(), [](const void*) {})`.
  One control block per call. It claims an ownership that does not
  exist, and it goes around the check of validity.hpp:189. I do not
  recommend it.
- B. The real owner, as above. No allocation, one atomic count. Recommended.
- C. A decode in mbedded-cbor that gives T by value from a borrowed
  range. That is against schema.hpp:3323. It is a change of the library.

### 3. Lifetime at each caller: no view dangles if three conditions hold (blocking for the plan text)

- node.hpp:1611-1640: `e` is in `log->entries`; `log` holds the
  `shared_ptr` until the function returns (node.hpp:1598, 1602). A replaced
  `open_log_` cache does not free it. `write_the_result` copies key and
  value (write_set.hpp:42-51). `insert_or_assign_session` takes a
  `session_record` with an owning `reply` (storage.hpp:141-145).
  `reply_to_the_client` builds the reply in `response()` at once and
  pushes it into `replies_before_the_sync_` or `client_replies_`
  (node.hpp:1679-1710). `log_records_` holds CRCs only (node.hpp:1631).
  No dangling.
- node.hpp:1316: the result is only tested and dropped. No dangling.
- node.hpp:1839: `b.done` is a subspan of `tail->entries` (1224);
  `write_the_result` copies. No dangling.
- verdict.cpp:520-528: views are read in the function only. No dangling.
- `own_` (node.hpp:1118-1123, 2182): holds the owning `request_result`.
  It stays owning (§2.5). No dangling.

The three conditions, which the plan must state:
1. `reply_to_the_client` copies the reply into the
   `std::vector<std::byte>` of the reply (node.hpp:1688
   `return r->reply;` does not compile from a span; it needs the copy).
   A reply queue that holds a span dangles after `log` dies.
2. `session_record{e.request_number, r.reply}` at node.hpp:1795 copies
   explicitly. `session_record` stays owning.
3. `own_result` does not hold a `request_result_view` into its own
   `bytes`. A copy of `own_result` would then point into the old
   `bytes`.

### 4. write_the_result gets an owning result at two places (blocking)

§2.4 says `write_the_result` takes the view. node.hpp:1826 and 1934 pass
`own_result::result`, which is a `request_result` (§2.5). The plan does
not say how an owning result becomes a view. `encode_result` (§2.3)
needs the same step.

Fix: name the conversion. The form that the standard library gives is
the conversion of `std::string` to `std::string_view`:
`request_result::operator request_result_view() const`, and the same for
`mutation`. It allocates the `std::vector<mutation_view>`. State that
cost in the plan.

### 5. std::span has no operator== (not blocking, compile faults)

- verdict.cpp:527 `x.key == y.key` and node.hpp:1657
  `later.key == m->key` do not compile with spans. Use `std::ranges::equal`.
- A defaulted `operator==` on `mutation_view` and `request_result_view`
  is defined as deleted. test_verdict.cpp:100
  `decode_result(encoded) == r` does not compile. The test of §3 must
  compare member by member with `std::ranges::equal`.

### 6. The keys of the new encoding (not blocking)

`request_result` and `mutation` have no `CBOR_DEFINE_TYPE`
(grep of include/ and src/), so the keys are the names of the members
(doc/structs.md §3). The view types must keep the names `error`,
`reply`, `mutations`, `key`, `value`. State it in §2.1.

After the change, the owning `request_result` does not decode from the
new bytes: a `std::vector<std::byte>` reads only a typed array
(structs.hpp:378-379, doc/structs.md:63-64). Only decode_result decodes
a result (grep of src/, include/, test/, program/, bench/), so nothing
breaks. `decode_bytes` stays for `decode_chosen_results`
(verdict.cpp:76). State both in §2.

### 7. A store of the old form loses writes silently (blocking: the plan decides it)

The encoded result is stored in the log (`entry::result`). With the new
decode, an old result gives `incorrect_type` (structs.hpp:593-594).
Then node.hpp:1839 skips the writes of that entry with no error, and
node.hpp:1611-1613 answers the client with `rejected`.
src/persistent_storage.cpp:39 has `format_version = 6`, and
check_or_write_format refuses another format (1079-1080).

Fix: raise `format_version` to 7 in the same change. Then an old store
fails at open with `unsupported_format`. test_persistent_storage.cpp:536
tests that path. No tag and no release exists (`git tag` is empty;
CMakeLists.txt:2 says 0.0.1).

### 8. The encoding change affects nothing else that I found (not blocking)

- Float rounding: `round_the_data_item` runs on the reply and each value
  before the encode (node.hpp:1758-1764). It reads the data item, not
  the frame around it. No change.
- `results_agree` with a float rule: `data_items_agree` reads the spans
  (verdict.cpp:504-512). The byte-equal path (517) is unchanged.
- CRCs in tests: test_persistent_storage.cpp:598 and 878 store the
  constants 0x01020304 and 0x41424344 as values. They are not computed
  from a result. test_verdict.cpp:105-110 computes the CRC by hand over
  any bytes. No fixed result bytes exist in test/ (grep for `0xd8, 0x40`).
- The tests that encode results (test_execute_verify.cpp:57, 658, 781,
  829, 953, 1075-1076, 1138, 1178; test_verdict.cpp:97-103, 272-279)
  use `encode_result` on both sides. They change with it.
- TLA+: model/tla/AcceptWithResult.tla holds no byte form. The
  .bin traces are TLC states.
- Snapshot or backup: no code for either in include/, src/, program/.
- konsensd: program/key_value.hpp:40 gives a `konsens::applied`. The
  client reply is `reply` itself, not the encoded result. No change.
- Bench: bench/konsens_bench.cpp:246, 1858-1938 use the owning
  `konsens::mutation` of the state machine. No change. bench/arms/*.patch
  apply to their own commits.
- doc/plan-execute-verify.md:96-99 (2b, provisional) says the result
  holds the reply and each value "as a data item, not as a byte string",
  with tag 24 as the known form. The code today and this plan hold them
  as bytes. The plan should name 2b and say that it is still not
  followed. Tag 24 over the byte string (RFC 8949 3.4.5.1) is the known
  form; it costs two bytes per member.

### 9. Rules (not blocking unless noted)

- "A hypothesis is tested in one file first": that `structs` decodes
  `{span, optional<span>, vector<struct of spans>}` from a borrowed
  span with the owner of finding 2 is not shown. One small file outside
  the tree answers it before the branch.
- "A test run checks the changed code only": §3 runs the suites of
  plan-promise-again §5 (test_sequence_paxos, test_simulation_clients,
  test_simulation, test_reconfiguration). The changed lines of node.hpp
  (apply, check, base, read back, float rule) are tested in
  test_execute_verify (1324-1370 for the float rule). Add
  test_execute_verify. Add test_persistent_storage if finding 7 is taken.
- "Checks live in tests / the expectation is written from the
  specification": the test of §3 writes the expected bytes by hand from
  RFC 8949 3.1 (refs/rfc8949.txt), for example `0x42 0x01 0x02` for the
  reply {1, 2}, not from encode_result.
- "Nothing is allocated on the path that succeeds": the
  `std::vector<mutation_view>` allocates in the decode and again in the
  copy out of the ref (finding 1). It is fewer allocations than today.
  The plan should say that it stays.
- "Every name comes from a source": `request_result_view` and
  `mutation_view` follow `std::string_view`. The conversion of finding 4
  follows `std::string::operator basic_string_view`.
- §4 and §5 follow the rules of measurement and data.

## Verdict

Approve after blocking fixes: findings 2, 3 (state the three
conditions), 4 and 7. Finding 2 goes to the owner as a question with
option B as the proposal.

## Not checked

- No build and no compile. That the forms of findings 2 and 4 compile
  is not checked.
- Whether GCC 16 honours `[[clang::lifetimebound]]` (ref.hpp:20-26 sets
  it for clang only).
- Whether `structs` reserves memory from the count of an array when it
  reads `std::vector<mutation_view>`.
- The callers of `generic_read<std::vector<std::byte>>` in the callgrind
  files.
- The full text of test_execute_verify.cpp and test_verdict.cpp beyond
  the lines named.
