# Plan: a result is read as views

Owner 2026-10-31 19:17: mbedded-cbor has views and copies nothing,
unless the caller uses it wrong.

## 1. The facts

- `decode_result` (src/verdict.cpp:57) calls `decode_bytes`
  (src/verdict.cpp:39-48). That copies the encoded bytes into a new
  std::string, then decodes into `request_result`
  (include/konsens/verdict.hpp:16-22). Its members `reply`,
  `mutation::key` and `mutation::value` are std::vector<std::byte>.
- mbedded-cbor (9126904) reads a std::vector<std::byte> as an RFC 8746
  typed array: `out.resize(count)` and then one byte at a time
  (include/cbor/structs.hpp:812-836). It reads a
  std::span<std::byte const> as a view of a byte string, with no copy
  (structs.hpp:589-598).
- On a follower at 64 in flight, the CBOR decode is 16 % of the
  instructions, and generic_read of a byte vector 8.2 % self; malloc and
  free are 13 % (bench/results/2026-10-31-2045Z). Ir, not time.
- `decode_result` has three callers in node.hpp (1316, 1611, 1839) and
  two in results_agree (src/verdict.cpp:520-521). Each caller reads
  the result while the entry that holds its bytes is alive.

## 2. The change

Review 1 (doc/review-plan-result-views-1.md) approved after blocking
fixes; this text holds them.

1. New types in verdict.hpp: `mutation_view` {span key;
   optional<span> value} and `request_result_view` {uint64 error; span
   reply; vector<mutation_view> mutations}. The members keep the names
   of `request_result`, because the names are the CBOR keys. The name
   follows std::string_view. The vector of views still allocates.
2. `decode_result(std::shared_ptr<void const> owner,
   std::span<const std::byte> encoded)` returns
   `std::optional<request_result_view>`. It calls
   `cbor::structs<request_result_view>::decode(owner, chars_of(encoded))`.
   The owner keeps the bytes alive (structs.hpp:1337-1349). A caller in
   node.hpp passes the owner of the open log
   (`open_log_view::owner`, sequence_paxos.hpp:1162-1180) through the
   aliasing constructor of shared_ptr. `done_log` carries that owner.
   `results_agree` is a cold path: it keeps its own copy of the bytes as
   the owner (review F2).
3. `request_result` converts to `request_result_view`, as std::string
   converts to std::string_view. `encode_result` encodes the view, so each
   byte member is a byte string (RFC 8949 3.1, major type 2), not a typed
   array under tag 64. `write_the_result` takes the view; a caller with
   an owning result converts it (node.hpp:1826, 1934) (review F4).
4. `reply_to_the_client` copies the reply into the vector it returns.
   `session_record` stays owning. `own_result` holds no view into its own
   bytes (review F3).
5. A comparison of spans uses std::ranges::equal (review F5).
6. The bytes of every result change, and results stand in the log. The
   format version of the store goes from 6 to 7
   (src/persistent_storage.cpp:39), so an open refuses a store of the old
   form (review F7). No release holds the old form.
7. `decode_bytes` stays for `decode_chosen_results`.

Out of this plan:
- `key_value_write` of konsensd, because the client writes it.
- `entry::payload` and `entry::result`, which are typed arrays in
  accept_decide too (review F1). Their share of the 16 % is not known.
- doc/plan-execute-verify.md 2b (provisional) asks for a data item or
  tag 24 for a result; this plan takes a byte string and names the
  difference (review F8).

## 3. Tests

- test_verdict: a result encodes each byte member as a byte string
  (RFC 8949 3.1, major type 2); the expected bytes are written by hand
  from the RFC. It decodes back to the same view.
- test_verdict: a decoded view points into the span it was read from.
- test_verdict, test_execute_verify, test_persistent_storage, test_node and test_simulation_clients with g++ 16.

## 4. Measurement

Arms base (accept-carries-the-result) and this branch, each its own
binary, with A/A of the base, in g++ 16, g++ 15 and clang 22.
bench/arms/cap-of-the-step.sh with cluster3.sh at 64 in flight, 4
rounds. Writes/s, latency, cpu per write.

## 5. Data on the disk

Build directories under bench-logs/kbuild (made again by a build
command), the bed under /mnt/zrun/b, one result file.
