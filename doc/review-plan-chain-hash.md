# Review of plan-chain-hash.md (commit 76b28ba)

Reviewer: a second agent of the session, read only, on 2026-10-29.
Folded into the plan at the next commit.

## Facts of section 1

- F3: the meta record is at storage.hpp:127-137.
- F4: `recovery_response` and `new_state` also carry the chain
  (message.hpp:164-165, :183, :186).
- F5: `fault_proof` is at persistent_storage.hpp:164-172. The kind
  `damage` holds chains only with `paxos_errc::hardware_fault`
  (node.hpp:785-789).
- F6: `konsens` does not link libhydrogen (CMakeLists.txt:50-52). The
  hash is Gimli (impl/hash.h).

## Blocking

- B1. The owner decision of 2026-10-24 says that the consensus path
  computes no cryptographic hash (Claude-state mbedded-konsens.md:251-256).
- B2. `compute_entry_crc(index, e)` is also the CRC of the `work`
  message (node.hpp:1183, :1512; message.hpp:242) and of a test
  (test_persistent_storage.cpp:541). The record CRC must be a separate
  function.
- B3. node.db has no format version, and the clean-stop mark embeds the
  meta record (persistent_storage.hpp:175-182).
- B4. The stored form of the proof is left to the build.
- B5. `hydro_hash_hash` takes one buffer; the input has three parts.
  Use `hydro_hash_init`, three `hydro_hash_update`, `hydro_hash_final`.
- B6. `hydro_hash_BYTES` in a public header shows the dependency. A
  named constant of konsens and a `static_assert` in src/.
- B7. The builds are g++ 15, g++ 16, clang 22 and clang 23.
- B8. konsens_bench has no case "append". The cases are commit,
  write_batch_size, get_entries, encode_decide, decode_decide,
  scan_the_whole_log.

## Answers

- Every read of a log row goes through `check_record`. `write_state`
  stays correct.
- `hydro_hash_init` with no key is supported. The hash needs no
  `hydro_init`; `hydro_init` reads /dev/random and can call `abort`.
- mbedded-cbor writes `std::array<std::byte, N>` as tag 64 over a byte
  string (RFC 8746) and refuses any other length
  (structs.hpp:866-868). No check function of our own.
- The member names are map keys on the wire; a rename changes the
  golden bytes of test_wire.cpp:427.
- `answer.base_crc` of Execute-Verify (verdict.cpp:79-87,
  selection.cpp:38-45) is a chain of CRCs compared for equality.
- Every place that passes a number for a chain stops compiling; the
  list is in the review answer of 2026-10-29.

## Not blocking

- `read_verified_chain_crcs` keeps one value per entry; only the count
  and the last value are used.
- libhydrogen has no command line and no keyless test vector.
- The error text at sequence_paxos.hpp:47 says "chain CRC".
- hydrogen.h changes a diagnostic without push and pop
  (hydrogen.h:18-21); include it in one .cpp only.

## Not checked by the reviewer

Warnings of hydrogen.h with -Werror on four compilers; the model;
cryptanalysis of Gimli; the aarch64 form; cbor::packed.
