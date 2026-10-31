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

1. New types in verdict.hpp: `mutation_view` {span key;
   optional<span> value} and `request_result_view` {uint64 error; span
   reply; vector<mutation_view> mutations}. The name follows
   std::string_view.
2. `decode_result` returns `std::optional<request_result_view>`. It
   decodes from the span of the caller with no copy of the bytes. The
   views point into that span. The declaration states it: the result
   lives as long as the span.
3. `encode_result(const request_result&)` encodes the view of the
   owning result. Each byte member is then a byte string, not a typed
   array under tag 64. This changes the bytes of every result, so the
   result CRC and the chain over the results change with it. No release
   holds the old form (the library is before 0.2).
4. `write_the_result`, `written_values_read_back`, `results_agree` and
   `reply_to_the_client` take the view.
5. The owning `request_result` and `mutation` stay for what
   `run_request` produces.

Out of this plan: `key_value_write` of konsensd, because the client
writes it; a change there changes the client protocol.

## 3. Tests

- test_verdict: a result encodes each byte member as a byte string
  (RFC 8949 3.1, major type 2), and decodes back to the same view.
- test_verdict: a decoded view points into the span it was read from.
- The suites of plan-promise-again §5 with g++ 16, plus test_verdict.

## 4. Measurement

Arms base (accept-carries-the-result) and this branch, each its own
binary, with A/A of the base, in g++ 16, g++ 15 and clang 22.
bench/arms/cap-of-the-step.sh with cluster3.sh at 64 in flight, 4
rounds. Writes/s, latency, cpu per write.

## 5. Data on the disk

Build directories under bench-logs/kbuild (made again by a build
command), the bed under /mnt/zrun/b, one result file.
