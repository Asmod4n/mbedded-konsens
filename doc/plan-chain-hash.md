# Plan: the chain of the log becomes a cryptographic hash

Branch chain-hash, from main 03e9477. Owner 2026-10-29 17:25: "Crc
ketten hash durch was robusteres ersetzen, crc nur für einzelne
Einträge". Owner 17:00: after 1.0 no change of the wire protocol, so
this change comes before 1.0.

Marks: "provisional" marks a form or a name that waits on the owner.

## 1. Facts checked in this session

1. The chain is CRC32C. `compute_chain_crc` extends the CRC of the
   previous chain value and the index over the encoded entry
   (src/storage.cpp:166-190). CRC32C is linear, and a client chooses the
   payload bytes of an entry.
2. A record of the log is `[entry, entry CRC, chain CRC, state, state
   CRC]` (src/persistent_storage.cpp:56-62). The entry CRC covers the
   index and the encoded entry. No CRC covers the chain field
   (`check_record`, :267-291).
3. The meta record holds `last_chain_crc` and `applied_chain_crc`
   (include/konsens/storage.hpp:127-136). `last_entry` holds the chain
   of the last record (:147-153). The format version is 5
   (persistent_storage.cpp:37).
4. The wire carries the chain in `promise` (decided and last),
   `accept_sync`, `accept_decide`, `decide` (decided), `accepted`
   (accepted) and the messages of the build (include/konsens/message.hpp:102-258).
5. `hardware_fault` and `fault_proof` hold two values of 32 bits each
   (sequence_paxos.hpp:74-80, persistent_storage.hpp:165-173). The kind
   of the proof says whether they are chains or results of Execute-Verify.
6. libhydrogen is a submodule and builds with the tree
   (CMakeLists.txt:29-43). `hydro_hash_hash` gives 16 to 65535 bytes
   with a context of 8 bytes; `hydro_hash_BYTES` is 32
   (deps/libhydrogen/hydrogen.h:69-91). Its hash is built on the
   Gimli permutation (from memory, not read in this session).

## 2. What others do

A fast checksum guards a block against random faults. Where an equal
hash is taken as equal data, a cryptographic hash is used: OpenZFS asks
for sha256 or stronger when dedup is on; the consistency checker of
CockroachDB compares replicas with SHA-512; FoundationDB compares the
data itself (owner card 2026-10-29, sources in the thread). A checksum
covers every stored byte of its block (OpenZFS, PAR 3.3.4).

## 3. The change

1. The chain value is `hydro_hash_hash` with 32 bytes
   (`hydro_hash_BYTES`) over the previous chain value, the index in
   little-endian order and the encoded entry, with one fixed context of
   8 bytes and no key. The chain value of the empty log is 32 zero
   bytes. One function computes it; the fold over a range calls it.
2. The type of a chain value is `std::array<std::byte, hydro_hash_BYTES>`
   under the name `chain_hash` (provisional). No libhydrogen type leaves
   the library.
3. The record becomes `[entry, entry CRC, chain hash, state, state
   CRC]`. The entry CRC covers the index, the encoded entry and the
   chain hash. The state CRC stays as it is.
4. The meta record holds `last_chain_hash` and `applied_chain_hash`.
   `last_entry` holds the chain hash. The format version becomes 6. A
   store of version 5 is refused, as each version is now; the stores
   before 1.0 hold test data only.
5. Each message of fact 4 carries `chain_hash` in place of the CRC.
   The CBOR form is a byte string of 32 bytes. The decoder refuses any
   other length, through one check function.
6. `hardware_fault` and `fault_proof` of a chain difference hold both
   chain hashes. The proofs of Execute-Verify keep their CRCs of 32
   bits. The form of the two kinds in one record is decided in the build
   by the review; the row of node.db gets a format version if it has
   none.
7. Every name with `chain_crc` becomes `chain_hash`, through the
   language server in the build container.
8. The model does not change: its chain is abstract.

## 4. Measurement, before the build

The cost is measured first, in one file outside the tree, with the
rules of a measurement in this session:

1. Arms: CRC32C chain (as now) and `hydro_hash_hash` with 32 bytes, and
   with 16 bytes as a third arm for comparison. Each arm in its own
   binary, run alternately, medians compared, the noise floor (one arm
   built twice) first.
2. Operation: the chain value of one entry. Inputs: entries of 64 B,
   1 KiB and 64 KiB. Builds: g++ 16 and clang++ 22, Release.
3. Unit: ns per entry and MiB/s. The result file holds the context of
   the run as the rules say.
4. Then konsens_bench with the append of entries of 64 B and 1 KiB, both
   arms, after the build.

No number about the cost is written before these runs.

## 5. Tests

- A flipped byte in the chain field is a CRC mismatch at its index.
- The chain hash of a known input equals a value computed with the
  command line of libhydrogen or a second implementation; not the value
  of the code under test.
- A message with a chain hash of 31 or 33 bytes is refused by the
  decoder.
- A store of format version 5 is refused.
- The existing tests of the chain, the vote and the build pass with the
  hash.

## 6. What this plan does not do

- No change of the per-entry CRC32C beyond item 3.3.
- No checksum that depends on the file system (owner 2026-10-29 13:14);
  that is a plan of its own and changes no wire.
- No repair (doc/plan-repair.md on branch repair; it needs this plan).

## 7. Order

1. This plan is reviewed.
2. The measurement of section 4, when processors are free.
3. The owner sees the measurement. The build starts after that.

## 8. Data of this plan

- The bench file and its binaries in the scratchpad: deleted after the
  run.
- The result file under bench/results: recorded in the state file.
- Build directories under the scratchpad: made again by the build
  command.
