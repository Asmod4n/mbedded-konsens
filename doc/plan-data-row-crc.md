# Plan: each row of the table data carries a CRC32C

Branch data-row-crc, from main 0ebb361. Owner 2026-11-08 07:01: the
next gap of the storage is a CRC32C on each row of `data` (decision
card).

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
5. The node reads the state through `read_or_throw`
   (include/konsens/node.hpp:1066-1073). An error of a read raises
   `read_error`, nothing catches it, and the process ends. The next
   start finds no clean-stop mark, and the node retires
   (node.hpp:1112-1121). A retired node comes back only as a new node
   through a StopSign.
6. `write_the_proof_and_throw` (node.hpp:1221-1229) keeps a
   `fault_proof` in node.db before it raises `node_fault`. A read of the
   state does not call it today. The owner rule of 2026-10-17: each
   hardware fault that the node finds reports its evidence.
7. `error_is_a_damage` (include/konsens/sequence_paxos.hpp:86-96)
   counts `storage_errc::crc_mismatch` and `malformed_record` as a
   damage.
8. konsens.db has format 8 (persistent_storage.cpp:41). An open of
   another format is refused (:1143-1160).
9. The owner decided 2026-10-17 20:52: a scrub that reads every row
   belongs to the collection of hardware check programs, not to the
   binary.
10. The owner decided 2026-10-29 13:14: a checksum that the file system
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
4. `find_value` checks the row before it gives anything back. A row
   shorter than the CRC is `storage_errc::malformed_record`. A CRC that
   differs is `storage_errc::crc_mismatch`. On success, the span covers
   the value without the CRC.
5. The node at a damage of a row. A read of the state that gives a
   damage writes a proof and raises (fact 6), in place of a plain
   `read_error`. The proof has the kind `damage`, the index
   `applied_idx` of the meta record, the stored CRC and the computed
   CRC. The next start finds the proof and retires the node (fact 5).
   Provisional: a kind of its own for a damage of the state, and the key
   in the proof. Both need a new name and a new field in node.db.
6. konsens.db goes to format 9. A store of format 8 is refused at the
   open. There is no conversion: no release has shipped a store.
7. Not in this plan:
   - A check of every row at the start (fact 9).
   - A lost write: the disk gives back an older row with its correct
     CRC. A CRC on the row cannot find it. A follower finds it when
     Execute-Verify reads the row and its result differs. A local read
     and a read with the read index do not find it. The model shows the
     limit (section 4, MC_drc_limit_LostWrite). It stays open work.
   - Less CRC work on ZFS and btrfs (fact 10).

## 4. The model

model/tla/DataRowCrc.tla. A node writes, erases and reads rows. The
disk flips a value, puts the row of one key at the place of another
key, or (with `WithLostWrite`) gives back an older row. A read with
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

## 5. Tests

In test/test_persistent_storage.cpp, with `sqlite_damage`:

1. A put and a find give the value back, for a value of 0 bytes, 1 byte,
   and 64 KiB.
2. One byte of a stored value changed: `find` gives `crc_mismatch`, in a
   read-only transaction, in a write transaction and in a nested
   transaction.
3. The CRC bytes changed: `crc_mismatch`.
4. The row of key a copied under key b: `find(b)` gives `crc_mismatch`.
5. A row shorter than the CRC: `malformed_record` (arm A only).
6. The record of session 1 copied under session 2: `find_session(2)`
   gives `crc_mismatch`.
7. A store of format 8 is refused at the open.

In test/test_node.cpp: a node whose row is damaged gives no reply with
the damaged bytes to a local read, keeps a proof in node.db, and
retires at its next start.

## 6. Measurement

Noise floor first: one binary built twice, `sysbench cpu` ten runs.
Then the bench cases `find` and `put` of one row in a write transaction,
at a value of 64 B, 1 KiB and 64 KiB. Three arms, each in its own
binary: main 0ebb361, arm A, arm B. Built with g++ 16 and clang 22, each
at `-Os` and `-O2`, `-march=x86-64-v4`. Run alternately, medians of
five. One result file with a time stamp in bench/results/.

## 7. Checks

Release build with g++ 16 and clang 22, every test. ASan and UBSan once
before the merge.

## 8. Order

1. This plan and the model, checked with TLC.
2. A review of the plan by a second agent.
3. The owner's go.
4. The tests of section 5, then the code, then the measurement.
