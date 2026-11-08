# Plan: each row of the table data carries a CRC32C

Branch data-row-crc, from main 0ebb361. Owner 2026-11-08 07:01: the
next gap of the storage is a CRC32C on each row of `data` (decision
card). Review: doc/review-plan-data-row-crc.md; its findings are folded
in.

Marks: "provisional" marks a form or a name that waits on the owner.

## 1. Facts checked in this session

1. The schema of konsens.db is
   `data (key BLOB PRIMARY KEY, value BLOB NOT NULL) STRICT, WITHOUT
   ROWID` (src/persistent_storage.cpp:56). No column and no byte of a
   row holds a checksum.
2. `find_value` (persistent_storage.cpp:791-799) gives the bytes of the
   column `value` back as they are. `put_value` (:801-813) binds the
   value as it is. A bit that flips in a value reaches the state machine,
   a local read and a read with the read index with no error.
3. A record of `session` holds `[request number, reply, CRC32C]`. The
   CRC covers the request number and the reply, not the session id
   (`find_session_in` :825-856, `put_session` :858-877). The bytes of
   one session under the id of another session pass the check.
4. A record of `log` holds an entry CRC over the index, the entry and
   the chain hash (`check_record` :269-283). The log is not part of this
   plan.
5. A read of a data row in the state machine goes through
   `transaction_view::find` (include/konsens/state_machine.hpp:41-48).
   An error raises `read_error` there, inside `machine_.apply`
   (node.hpp:2183) and `machine_.query` (:2556). Other reads of the
   state go through the static `read_or_throw` (node.hpp:1066-1073).
   The check threads catch every exception (:1427, :1645) and the node
   thread throws it again (:1662). konsensd `main` catches
   `std::exception`, writes it and ends (program/main.cpp:82). The next
   start finds no clean-stop mark, and `kind_of_the_start` retires the
   node (node.hpp:67-91). A retired node comes back only as a new node
   through a StopSign.
6. `write_the_proof_and_throw` (node.hpp:1221-1229) keeps a
   `fault_proof` in node.db before it raises `node_fault`. A read of the
   state does not call it today. Every public step of the node runs
   inside `throw_on_a_damage` (node.hpp:1132-1153; callers :290 to
   :809). `node::with` runs `apply_at_the_start` outside it (:248). The owner rule of 2026-10-17: each
   hardware fault that the node finds reports its evidence.
7. `error_is_a_damage` (include/konsens/sequence_paxos.hpp:86-96)
   counts `storage_errc::crc_mismatch` and `malformed_record` as a
   damage.
8. konsens.db has format 8 (persistent_storage.cpp:41). An open of
   another format is refused (:1143-1160).
9. The owner decided 2026-10-17 20:52: a scrub that reads every row
   belongs to the collection of hardware check programs, not to the
   binary.
10. Every reader of `data` and `session` goes through `find_value` or
    `find_session_in` (persistent_storage.cpp:1385, :1392, :1641,
    :1652, :1825, :1835; write_set.hpp:31, :39). The other SQL on `data`
    is `count(*)` in the format check (:1156) and `VACUUM INTO` in
    `copy()` (:2044-2060), which only tests call. No repair, build,
    changeset or digest reads `data`.
11. The Jepsen fault `damage-a-value!` flips the last byte of the column
    `value` (jepsen/src/jepsen/konsensd/db.clj:135-143). `:damage-data`
    is not in `faults-that-retire` (jepsen/src/jepsen/konsensd/nemesis.clj:10).
12. The owner decided 2026-10-29 13:14: a checksum that the file system
    already gives is not computed twice. Which file system the node runs
    on is not read yet; that is a plan of its own.

## 2. What others do

- Protocol-Aware Recovery (FAST 2018, 3.3.4): the identifier of an
  entry is stored with the entry and covered with it, so a misdirected
  write is found. Read in an earlier session of this project (plan of
  the repair, section 2).
- ZFS: each block carries a checksum in its parent block pointer. That
  finds a flip, a misdirected write and a lost write. Not read in this
  session.
- CockroachDB: a stored value carries a checksum that covers the key and
  the value (`roachpb.Value`). Not read in this session.
- RocksDB: a checksum on each block of a file, and an option of a
  checksum on each key and value in memory. Not read in this session.

The sources agree: a checksum covers each stored block, and it covers
the identifier of the block too. For a row, the identifier is the key.
So this plan takes the CRC over the key and the value. It is shown, not
asked.

## 3. The form for us

1. A row of `data` holds the value and a CRC32C. The CRC covers the
   length of the key as 8 bytes little endian, the key, and the value.
   The length makes the line between key and value part of the CRC.
   `crc32c::Extend` chains the three parts, so nothing is copied.
   The chance of 2^-32 to miss a damage holds for a random fault. CRC32C
   is linear, so a client can choose two keys of one length that give
   the same CRC. A misdirected write is a random fault, so this does not
   matter here.
2. A record of `session` gets the session id into its CRC, as 8 bytes
   little endian in front of the body (fact 3). Nothing else changes in
   it.
3. Where the CRC stands. Two arms, measured (section 6):
   - Arm A: 4 bytes behind the value in the column `value`. The write
     copies the value into a buffer of the environment that is used
     again for each row, as `put_session` does today. No allocation
     after the buffer reached its size.
   - Arm B: a third column `crc INTEGER NOT NULL`. The write binds the
     value as it is and the CRC as an integer. No copy.
   The arm that is faster across the table is taken.
   Arm A: the length check comes before the pointer is read, because a
   blob of 0 bytes gives a null pointer. A refusal resets the statement,
   as `find_session_in` does. The buffer keeps the size of the largest
   value for the life of the store, as the session buffer does.
   Arm B: the upsert sets both columns. The read compares the CRC as
   `std::int64_t`, because STRICT checks only the write and a damaged
   row can hold any integer. Reading column 1 does not move the pointer
   of column 0. A CRC of 2^31 or more takes 6 bytes in the row.
4. `find_value` checks the row before it gives anything back. A row
   shorter than the CRC is `storage_errc::malformed_record`. A CRC that
   differs is `storage_errc::crc_mismatch`. On success, the span covers
   the value without the CRC.
5. The node at a damage of a row.
   - A damage of a row raises a type of its own, derived from
     `read_error`, with the error code `crc_mismatch` or
     `malformed_record`. Provisional name: `state_damage`.
     `transaction_view::find` and `read_or_throw` raise it when
     `error_is_a_damage` holds; every other read error stays a
     `read_error`.
   - `throw_on_a_damage` catches this exact type on the node thread,
     the rethrow from the check threads included, and calls
     `write_the_proof_and_throw`. `node::with` does the same around
     `apply_at_the_start`. These are the two places that catch it.
   - The proof has a kind of its own, the index `applied_idx` of the
     meta record, and 0 in both CRC fields: `find` gives an error code
     only, and the `state_read` concept stays as it is. Provisional
     name of the kind: `state_damage`. The proof holds no key, so
     node.db keeps its size of record and its format 1. An older binary
     reads the new kind as a damaged record; no release has shipped.
   - The next start finds the proof and retires the node (fact 6).
   - `find_client_session` (node.hpp:762) is a public const read
     outside a step. It cannot write a proof. A damage there raises
     `state_damage` to the caller; konsensd ends, and the next start
     retires the node because no clean-stop mark is there. The node
     keeps no proof of that damage.
6. konsens.db goes to format 9. A store of format 8 is refused at the
   open. There is no conversion: no release has shipped a store.
7. Not in this plan:
   - A check of every row at the start (fact 9).
   - A lost write: the disk gives back an older row with its correct
     CRC, a row that an erase removed, or no row where a first write
     stood. Two misdirected writes can act as one lost write. A CRC on
     the row cannot find any of these. A follower finds it when
     Execute-Verify reads the row and its result differs. A local read
     and a read with the read index do not find it. The model shows the
     limit (section 4, MC_drc_limit_LostWrite). It stays open work.
   - Less CRC work on ZFS and btrfs (fact 12).
   - `copy()` copies a damaged row with no check (fact 10). Only tests
     call it.
8. Jepsen (fact 11): `damage-a-value!` flips a byte inside the value,
   not inside the CRC; `:damage-data` goes into `faults-that-retire`;
   the check expects no answer for a damaged value and a retired start
   after it.

## 4. The model

model/tla/DataRowCrc.tla. A node writes, erases and reads rows. The
disk flips a value, flips the CRC, puts the row of one key at the place
of another key, or (with `WithLostWrite`) gives back an older row, a
removed row or no row. A read with
`CheckOnRead` ends the process at a CRC that differs. The invariant
`ReadGivesTheLastWrite` says that no read gives back a value that the
node did not write last.

| config | expected |
|---|---|
| MC_drc | no violation |
| MC_drc_cover_ends | `ProcessNeverEnds` is violated: a read finds a damage |
| MC_drc_mut_NoCheck | violated: the flip reaches the reply |
| MC_drc_mut_CrcWithoutKey | violated: the misdirected row reaches the reply |
| MC_drc_limit_LostWrite | violated: the limit of section 3.7 |
| MC_drc_limit_TwoMisdirects | violated: two misdirected writes act as a lost write |

Every config has `MaxDamages = 1` except the last, which has 2, and
`CHECK_DEADLOCK FALSE`: a node that ended takes no step. Results:
model/results/20261108T070830Z-data-row-crc.md.

## 5. Tests

In test/test_persistent_storage.cpp, with `sqlite_damage` and a new
helper there that changes a row of `data`:

1. A put and a find give the value back, for a value of 0 bytes, 1 byte,
   and 64 KiB. A put, a second put of the same key, and a find give the
   second value.
2. One byte of a stored value changed: `find` gives `crc_mismatch`, in a
   read-only transaction, in a write transaction and in a nested
   transaction.
3. The CRC bytes changed: `crc_mismatch`.
4. The row of key a copied under key b: `find(b)` gives `crc_mismatch`.
5. A row shorter than the CRC: `malformed_record` (arm A only).
6. The record of session 1 copied under session 2: `find_session(2)`
   gives `crc_mismatch`.
7. A store of format 8 is refused at the open.

In test/test_node.cpp: a node whose row of `data` is damaged gives no
reply with the damaged bytes to a local read, keeps a proof of the kind
`state_damage` in node.db, and retires at its next start. The same with
a damaged session record. The same with `check_threads` above 0, so
that the rethrow from a check thread writes the proof.

## 6. Measurement

Noise floor first: one binary built twice, `sysbench cpu` ten runs.
Then two new bench cases in bench/konsens_bench.cpp, `find` and `put`
of one row in a write transaction, at a value of 64 B, 1 KiB and 64
KiB. The cases stand in every arm. bench/raw_konsens.cpp keeps the old
schema, because it measures raw SQLite. Three arms, each in its own
binary: main 0ebb361, arm A, arm B. Built with g++ 16 and clang 22, each
at `-Os` and `-O2`, `-march=x86-64-v4`. Run alternately, medians of
five. One result file with a time stamp in bench/results/.

## 7. Checks

The changed code is tested on this machine: test_persistent_storage,
test_node, test_storage_interface, in Release with g++ 16 and clang 22.
ASan and UBSan once before the merge. The CI runs every build.
konsensd and Jepsen are not built here: a fresh clone of main cannot
fetch mruby-webmachine 8c76f51, which is on no branch of its origin.

## 8. Order

1. This plan and the model, checked with TLC.
2. A review of the plan by a second agent.
3. The owner's go.
4. The tests of section 5, then the code, then the measurement.

## 9. Data of this plan

- Build directories and ccache in the scratchpad: made again by a build
  command.
- The podman image localhost/konsens-dev: made again from its
  Containerfile in the scratchpad.
- TLC state directories in the scratchpad: deleted after the results
  file is pushed.
- The results file of the measurement: recorded in bench/results/.
