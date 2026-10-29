# Plan: the chain of the log becomes a cryptographic hash

Branch chain-hash, from main 03e9477. Owner 2026-10-29 17:25: "Crc
ketten hash durch was robusteres ersetzen, crc nur für einzelne
Einträge". Owner 17:00: after 1.0 no change of the wire protocol, so
this change comes before 1.0. Review: doc/review-plan-chain-hash.md;
its findings are folded in.

Marks: "provisional" marks a form or a name that waits on the owner.

## 1. Facts checked in this session

1. The chain is CRC32C over the previous chain, the index and the
   encoded entry (src/storage.cpp:166-190). CRC32C is linear, and a
   client chooses the payload bytes of an entry.
2. A record of the log is `[entry, entry CRC, chain CRC, state, state
   CRC]` (src/persistent_storage.cpp:56-62). The entry CRC covers the
   index and the encoded entry. No CRC covers the chain field
   (`check_record`, :267-291). Every read of a log row goes through
   `check_record` (:611, :641, :666).
3. `compute_entry_crc(index, e)` is also the CRC of the `work` message
   of Execute-Verify (include/konsens/node.hpp:1183, :1512;
   include/konsens/message.hpp:242).
4. The meta record holds `last_chain_crc` and `applied_chain_crc`
   (include/konsens/storage.hpp:127-137); `last_entry` holds the chain
   of the last record (:147-153). konsens.db has format version 5
   (persistent_storage.cpp:37). node.db has no format version
   (:47-48), and its clean-stop mark embeds the meta record
   (persistent_storage.hpp:175-182).
5. The wire carries the chain in `promise`, `accept_sync`,
   `accept_decide`, `accepted`, `decide`, `recovery_response`,
   `new_state` and the build messages (message.hpp:102-258). mbedded-cbor
   writes `std::array<std::byte, N>` as tag 64 over a byte string
   (RFC 8746) and refuses any other length
   (deps/mbedded-cbor/include/cbor/structs.hpp:866-868). The members are
   map keys on the wire.
6. `answer.base_crc` of Execute-Verify is a chain of CRC32C over the
   result CRCs, compared for equality (src/verdict.cpp:79-87,
   src/selection.cpp:38-45).
7. `hardware_fault` and `fault_proof` hold two values of 32 bits
   (sequence_paxos.hpp:74-80, persistent_storage.hpp:164-172).
8. libhydrogen is a submodule built with `-Os -march=x86-64` or
   `armv8-a` (CMakeLists.txt:28-43); `konsens` does not link it
   (:50-52). Its hash is Gimli (deps/libhydrogen/impl/hash.h).
   `hydro_hash_init` takes no key; the hash needs no `hydro_init`, and
   `hydro_init` reads /dev/random and can call `abort`.
9. Owner 2026-10-24: a cryptographic hash comes from libhydrogen, not
   SHA-256, and the consensus path computes no cryptographic hash
   (Claude-state mbedded-konsens.md:251-256). The owner message of
   2026-10-29 17:25 puts the hash on the consensus path, so the cost of
   section 4 goes to the owner before the build.

## 2. What others do

A fast checksum guards a block against random faults. Where an equal
hash is taken as equal data, a cryptographic hash is used: OpenZFS asks
for sha256 or stronger when dedup is on; the consistency checker of
CockroachDB compares replicas with SHA-512; FoundationDB compares the
data itself (sources in the thread, 2026-10-29). A checksum covers
every stored byte of its block (OpenZFS; PAR, FAST 2018, 3.3.4).
`base_crc` of fact 6 is the same case as the chain, so it becomes a
hash too.

## 3. The change

1. The chain value is the hash of libhydrogen with 32 bytes, over the
   previous chain value, the index in little-endian order and the
   encoded entry: `hydro_hash_init` with one fixed context of 8 bytes
   (provisional value) and no key, one `hydro_hash_update` for each
   part, `hydro_hash_final`. No copy into one buffer. The chain value of
   the empty log is 32 zero bytes. One function computes it; the fold
   over a range calls it. `hydro_init` is not called.
2. The public headers name a constant of konsens for the size, 32, and
   the type `std::array<std::byte, chain_hash_size>` under the name
   `chain_hash` (provisional). One `static_assert` in src/ states that
   the constant equals `hydro_hash_BYTES`. hydrogen.h is included in one
   .cpp only. `konsens` links libhydrogen PRIVATE.
3. The record becomes `[entry, entry CRC, chain hash, state, state
   CRC]`. The entry CRC of the record covers the index, the encoded
   entry and the chain hash, through a function of its own (name
   provisional: `compute_record_crc`). `compute_entry_crc` stays for
   the `work` message. `last_entry` holds the record CRC. The state CRC
   stays.
4. The meta record holds `last_chain_hash` and `applied_chain_hash`.
   konsens.db gets format version 6. node.db gets one format row, value
   1, that covers the clean-stop mark and the proof. A store of an older
   format is refused with the error of a wrong format version; the
   stores before 1.0 hold test data only.
5. Each message of fact 5 carries `chain_hash` in place of the CRC, as
   tag 64 over 32 bytes, which the library already checks. The members
   are renamed, so the map keys and the golden bytes of
   test/test_wire.cpp change.
6. `answer.base_crc` becomes a chain hash over the result CRCs, with
   the same function and another context (provisional value).
7. The proof (provisional form): `fault_proof` holds two `chain_hash`
   values for a chain difference and two CRCs for a result of
   Execute-Verify, each kind in its own form of the row.
8. Every name with `chain_crc` becomes `chain_hash`, through the
   language server in the build container. The text of an error that
   says "chain CRC" changes by hand.
9. The scan at start keeps only the count and the last chain value.
10. The model does not change: its chain is abstract.

## 4. Measurement, before the build

1. One file outside the tree. Arms: the CRC32C chain as now, and the
   hash of 3.1 with 32 bytes; a third arm with 16 bytes for comparison.
   libhydrogen and crc32c are built with the flags of the tree. Each
   arm in its own binary, run alternately, medians compared, the noise
   floor (one arm built twice) first. Run length: 31 samples per
   process, as the state gives for this machine.
2. Operation: the chain value of one entry; entries of 64 B, 1 KiB and
   64 KiB. Also the record CRC over the 32 extra bytes.
3. Builds: g++ 15, g++ 16, clang 22 and clang 23, Release, one column
   each.
4. After the build: konsens_bench, cases `commit`,
   `write_batch_size_lmdb`, `get_entries_lmdb`, `encode_decide`,
   `decode_decide` and `scan_the_whole_log`, old and new arm. The
   encoded size of each message with a chain, before and after.
5. The bench leaves one processor free, runs as the user of the bench,
   not as root, not pinned, with the priority recorded in each row.

No number about the cost is written before these runs.

## 5. Tests

- A flipped byte in the chain field is a CRC mismatch at its index.
- The chain hash of a known input equals the value of
  `hydro_hash_init/update/final` called directly in the test on bytes
  built by hand.
- A message with a chain hash of 31 or 33 bytes is refused by the
  decoder.
- A store of format version 5, and a node.db without a format row, are
  refused.
- The existing tests of the chain, the vote, the build and
  Execute-Verify pass with the hash.

## 6. What this plan does not do

- No checksum that depends on the file system (owner 2026-10-29 13:14).
  It changes no wire, so it is a plan of its own after this one.
- No repair (doc/plan-repair.md on branch repair; it needs this plan).

## 7. Order

1. The measurement of section 4.1 to 4.3, when processors are free.
2. The owner sees the measurement and the provisional forms of 3.1,
   3.2, 3.3 and 3.7. The build starts after that.

## 8. Data of this plan

- The bench file and its binaries in the scratchpad: deleted after the
  run.
- The result file under bench/results: recorded in the state file.
- Build directories under the scratchpad: made again by the build
  command.
