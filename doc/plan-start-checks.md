# Plan: checks of the stored state at the start and at each read

Source: doc/storage-fault-matrix.md, rows 2.6 and 2.8, proposals 2 and
3. The owner has not decided these proposals yet. This form is
provisional. The owner decided on 2026-10-24 that a full map ends the
node and that rule R comes with the hidden identity; both are outside
this plan. This plan gets one review before the build.

## 1. Facts checked in this session

- `read_entries` returns the entries that LMDB has in [from, to) and
  skips a missing index (`src/persistent_storage.cpp:468-486`).
- `get_log_len` returns the count of entries, `ms_entries`
  (`:191-200`). It does not read the last index.
- A missing meta value reads as `ballot{}` or 0 (`:262`, `:327-329`,
  `:523-525`).
- `sequence_paxos::with` takes promise, accepted round, decided index
  and log length with no comparison
  (`include/konsens/sequence_paxos.hpp:241-275`).
- Each write path keeps these facts:
  - A node writes `set_promise` before or with each accepted entry
    (`:384`, `:723`, `:1069-1070`). So a non-empty log has a promise.
  - The accepted round is never above the promise (`:765`, `:811`,
    `:1061-1070`).
  - The decided index is never above the log length
    (`AcceptedIdxIsLogLength`, `SequencePaxos.tla:2132`).

## 2. Change

1. `persistent_storage::read_entries`: each index must equal
   `from + entries.size()`, when the first index is at most `from`.
   Else it returns `read_failure` with the new code
   `storage_errc::missing_entry` and the index that is missing.
2. `persistent_storage::open`: with a non-empty log, the last index
   must equal the count minus one, and the first index must be 0. Else
   `open` returns `missing_entry`. One cursor read of the first and
   the last key.
3. `sequence_paxos::with` returns the new code
   `paxos_errc::inconsistent_storage` when one of these fails:
   - the log is not empty and the promise is `ballot{}`;
   - the decided index is above the log length;
   - the accepted round is above the promise.
4. No change to the protocol, to the wire or to the file format.

## 3. Tests

The tests damage a closed LMDB directory with LMDB itself.
test_persistent_storage and test_sequence_paxos link `lmdb` for this,
as the bench does.

- test_persistent_storage:
  - "a missing entry in the middle of the log is missing_entry at its
    index at open";
  - "read_entries of a range with a missing entry gives missing_entry"
    (the store is opened before the damage, since LMDB sees the change
    of another environment of the same file at the next read
    transaction; not checked, the build shows it);
  - "a missing first entry is missing_entry at open".
- test_sequence_paxos:
  - "a log with no promise is inconsistent_storage";
  - "a decided index above the log is inconsistent_storage" (the test
    writes the meta value with its CRC32C);
  - "an accepted round above the promise is inconsistent_storage".
- Mutation, run once: each check removed. Each test fails.

## 4. Checks

- g++ 16, image localhost/cxx-matrix-fuse, Release, all tests,
  KONSENS_FUSE_TESTS=ON, KONSENS_TEST_TMPDIR=/dev/shm.
- counter_stress seeds 1 to 300 with `--cbor --power-loss 50`.
- ASan and UBSan once before the merge.
- The cost of the check at open: the bench case of an open, before
  and after, if one exists; else no number is written.

## 5. Provisional names

`missing_entry`, `inconsistent_storage`. No specification names these
faults. The owner decides the names.
