# Plan: LMDB is the only source of the durable values

Source: the owner, 2026-10-24 14:44 and 14:45: "unsere einzige quelle
für daten ist lmdb", "wir antworten nie aus dem speicher", "wir sind
ein distributed log, nur das log kennt das log". The owner decided at
14:46 that this change comes before the start checks
(doc/plan-start-checks.md on branch start-checks).

Order of the work: this plan, then the model of section 3 and its TLC
runs, then one review of plan and model by a second agent, then the
go of the owner, then the code of section 2.

The owner, 14:49: "wir nutzen die modi, in denen lmdb schnell und
sicher ist". So a step runs in one LMDB write transaction (section 2).

The form of a client work, from the owner, 15:27 to 15:39 and 15:55:

- A client sends work to any node. That node forwards it to the
  leader.
- The leader appends one entry with the state "in work". There is one
  entry, not two: its state changes to "committed" later.
- Each client work is one transaction with nested transactions.
- A follower reports the CRC and the result that it read back after
  its commit. The leader sets "committed" only when a majority reports
  the same values. The decided index goes through the same check.
- The disk can lie. The read-back finds a write that the disk did not
  keep.
- A step takes all the work that waits, so a batch costs what the
  check costs (group commit, no fixed wait).
- A node finds an entry that is lost after its report through a chain
  CRC at each step (card, 15:55).
- The README names the offsite node.

## 1. Facts checked in this session

Line numbers are at commit 06a52bf.

- `sequence_paxos` keeps four durable values as fields: `promise_`,
  `accepted_round_`, `decided_idx_`, `accepted_idx_`
  (`include/konsens/sequence_paxos.hpp:1184-1187`).
- `with` reads them once from the storage (`:245-265`). After that, no
  code reads them from the storage again.
- The node changes the fields itself after it queues or writes an op:
  - `promise_`: `:385`, `:724`, `:1072`.
  - `accepted_round_`: `:767`, `:812`, `:1073`.
  - `decided_idx_`: `:768`, `:813`, `:840`, `:857`, `:874`, `:1074`.
  - `accepted_idx_`: `:685`, `:769`, `:814`, `:839`, `:1075`.
- Messages carry the fields: `prepare` (`:635`), `promise` (`:727`),
  `accept_decide` (`:691`), `accept_sync` (`:794`), `accepted`
  (`:822`, `:842`, `:859`), `decide` (`:433`, `:876`),
  `recovery_response` (`:898-899`, `:936`), `new_state` (`:994`),
  `prepare_req` (`:544`, `:622`), `not_accepted` (`:649`), the read
  index (`:1116`), and `indexed_read` (`:1153`, `:1175`).
- Decisions use the fields: the ballot checks (`:377`, `:645-648`,
  `:706`, `:1133`), the sync choice (`:710`, `:716`, `:804`), the
  decided update (`:834`, `:853`, `:870`), the recovery
  (`:922`, `:940`, `:953-956`, `:986`, `:1046`, `:1067-1068`), and the
  leader of a forward (`:475`, `:541`, `:669`).
- The read ranges use `accepted_idx_` as their end (`:501`, `:524`,
  `:612`, `:891`, `:987`).
- `node` reads `get_promise()` between steps (`node.hpp:127`, `:221`,
  `:374`, `:466`).
- One step is the calls of `handle` for a batch and then `commit`
  (`:276-301`). `commit` writes the waiting ops in one transaction
  (`:295`). Messages leave through `take_outgoing_messages`, which
  throws while ops wait (`:459-463`). Some handlers write the waiting
  ops in the middle of a step before they read
  (`get_storage_after_writing_waiting_ops`, `:591-595`;
  `handle_majority_promises` `:766`; `complete_recovery` `:1071`).
- `persistent_storage` opens one read transaction for each getter
  (`src/persistent_storage.cpp:454`, `:499`, `:515`, `:538`). The four
  values and the last CRC are read in separate transactions.
- `get_log_len` is the count of entries (`read_log_len`, `:192-200`),
  not the last key plus one.
- The environment opens with no flags (`src/persistent_storage.cpp:403`),
  so it has no `MDB_WRITEMAP`.
- A write transaction of LMDB reads its own changes: `mdb_page_get`
  looks in the dirty list of the transaction and of its parents first
  (`mdb.c`, `mdb_page_get`, LMDB_1.0.2).
- LMDB_1.0.2 allows one nested write transaction under a write
  transaction, but not with `MDB_WRITEMAP` (`mdb_txn_begin`,
  `mdb.c:3497-3506`).
- A normal commit syncs the data first and the meta page after it
  (bench/results/2026-10-24-145849Z-lmdb-txn-nometasync.txt).
- The page cache, tested on this machine (kernel 6.18, ext4), in one
  file outside the tree:
  - `fdatasync` keeps the pages in the cache.
  - `posix_fadvise(POSIX_FADV_DONTNEED)` keeps the pages of a file
    that is mapped.
  - `madvise(MADV_PAGEOUT)` on the mapping takes the pages out of the
    cache: `mincore` shows 0 of 64 pages, also through a new mapping
    of the file. The kernel skips a page that another process maps.
  - `madvise(MADV_PAGEOUT)` on one page of a file written with one
    `pwrite` keeps that page: the page is part of a larger folio. Then
    `posix_fadvise(POSIX_FADV_DONTNEED)` on the same page takes it out
    of the cache. `MADV_DONTNEED` and then `POSIX_FADV_DONTNEED` keep
    it. In an LMDB bench with values of 1024 bytes, `MADV_PAGEOUT` alone
    left 360 of 527 value pages in the cache, and the pair left 0.
- A value that a read transaction gives points into the map of LMDB,
  because the environment has no `MDB_WRITEMAP` and LMDB reads through
  the map.
- The model already has no copy of `promise`, `n_accepted` and
  `decided_idx`: they are durable variables
  (`model/tla/SequencePaxos.tla:210`). `accepted_idx` is a volatile
  variable (`:211`), and the invariant `AcceptedIdxIsLogLength` says
  that it equals the log length.
- A key that is lost while the node runs is not found by the code at
  06a52bf: the node sends the value of its field
  (doc/review-plan-start-checks-4.md, finding 1).

## 2. Change of the code

### 2.1 LMDB is the only source

1. `sequence_paxos` has no field for promise, accepted round, decided
   index or log length, and no list of waiting ops. The four values
   come from LMDB.
2. A step runs in one LMDB write transaction. The first call of a step
   begins it. Every read and every write of the step goes through it,
   so a read sees the writes of the same step. `commit` commits it
   once, and the messages of the step leave only after that commit,
   as now (`take_outgoing_messages`, `:459-463`). The writes in the
   middle of a step (`:591-595`, `:766`, `:1071`) go: they exist only
   so that a later read sees the write.
3. Between two steps, a read (the public getters, `node.hpp:127`,
   `:221`, `:374`, `:466`) takes a read transaction from the pool.
4. In one transaction, the storage gives the log length as the last
   key plus one, and compares it with the count of entries through the
   continuity function of the start-checks plan (its item 2.1). When
   they differ, the read returns `missing_entry`.
5. The checks of `with` in the start-checks plan (its item 2.4) and the
   chain check of 2.3 run at the first read of each step, not only at
   the start. One function makes them. A damage that they find makes
   the node stop voting and start recovery.
6. `cached_promise_message_` (`:728`) is built again from LMDB on a
   resend (`:440`), not kept.
7. The storage concept (`storage.hpp:121-129`) changes from
   `write_atomically(ops)` and separate getters to a transaction that
   reads and writes. The name of the type comes from LMDB
   (transaction, commit, abort) and is provisional. No LMDB name
   leaves the library.
8. A write error of a step throws `write_error`, as now. A read error
   of LMDB throws a type derived from `std::system_error` (owner,
   2026-10-24 14:39).
9. Every new error branch is `[[unlikely]]`.

### 2.2 The state of an entry

1. An entry has a state field: `in_work` or `committed`. The names are
   provisional.
2. An entry is appended with `in_work`. The step that raises the
   decided index to `d` sets `committed` on every entry up to `d`, in
   the same transaction.
3. The step check of 2.1 item 5 also checks that every entry up to the
   decided index is `committed` and every entry above it is `in_work`.
4. The CRC of an entry covers its index, its kind, its payload and its
   state.

### 2.3 The chain CRC

1. Each entry also carries a chain CRC: the CRC over the chain CRC of
   the entry before it, the index, the kind and the payload. The state
   is not in the chain, because it changes.
2. One meta record holds the promise, the accepted round, the decided
   index, the last index and its chain CRC, with one CRC over all of
   them. Now these are three keys (`src/persistent_storage.cpp:31-33`).
   The node writes the meta record when it is created: the seed node at
   the first start, every other node at the end of its recovery. The
   write of an entry and the write of the meta record are in one
   transaction. A running node with no meta record, or with a meta
   record whose CRC fails, is damaged. Round 3 of the model showed why:
   a node with an empty log that loses its promise breaks S1, and no
   other check finds it. This form is provisional; the owner decides
   it.
3. The step check reads the last entry and the meta record in its
   transaction and compares the index and the chain CRC. A lost last
   entry and a lost entry in the middle both make them differ.
4. The chain CRC of entry `k` stands for the prefix up to `k`. The model
   compares the prefix itself (`PrefixAt`, `VerifyCrc`).
5. What the chain does not find: a rollback of the disk that takes the
   entry and the meta record back together. The peers find that one
   (start-checks plan, recovery after the start).

### 2.4 The read-back and the vote

1. After the commit of a step, the node reads back each entry that the
   step wrote and the meta record, from the disk and not from the
   cache:
   - a read transaction gives the address of each value in the map;
   - `madvise(MADV_PAGEOUT)` and then `posix_fadvise(POSIX_FADV_DONTNEED)`
     on the same range take the pages of these values out of the cache;
   - a new read of the same values reads them from the disk;
   - the node computes the CRC of each entry and the chain CRC.
2. A value that is not equal to what the step wrote is a lost write.
   The node does not send its `accepted`, stops voting and starts
   recovery. It reports the proof: node, index, expected and found CRC.
3. `accepted` carries the chain CRC of the accepted index, read back. A
   `decide`, an `accept_decide` and an `accept_sync` carry the chain
   CRC of the decided index of the leader.
4. The leader counts an `accepted` only when its chain CRC equals the
   chain CRC of its own log at that index. It decides only from such
   reports.
5. A follower raises its decided index to `d` only when its own chain
   CRC at `d` equals the one in the message. Otherwise it keeps its
   decided index.
6. The rounding of a value address to its page is an open point: the
   rules forbid pointer arithmetic by hand. A candidate is
   `std::align` on a page-sized buffer. The owner decides.
7. With the disk cache of the drive on, a read can come from that cache
   and not from the medium. The read-back does not find a loss there.
   The README says this, as it says for a volatile cache now.

### 2.5 Execute-Verify and nested transactions

1. Each client work runs in a nested write transaction under the write
   transaction of the step. A user error aborts only the nested
   transaction, and the entry records the error as its result.
2. The environment keeps no `MDB_WRITEMAP`, so nested write
   transactions stay possible.
3. The result and its CRC go into the `accepted` of the follower, as
   the read-back of 2.4 gives them.

### 2.6 Group commit

1. A step takes every request that waits when it begins. No step waits
   for more work.
2. While one step syncs, new requests wait for the next step. So the
   batch grows with the cost of the sync and the read-back.
3. The bounded queue and the refusal of the backpressure decision stay
   as they are.

### 2.7 The README

The README names the offsite node: a node at a second site, so that
one site that fails does not take a majority of the copies.

### 2.8 Not in this change

No change to the wire beyond the CRC fields of 2.4 item 3. The file
format changes through 2.2 and 2.3, so the format version goes up.

## 3. Model

### 3.1 Change

- `accepted_idx` leaves `volatile`. Every use reads `Len(log[i])`.
  `AcceptedIdxIsLogLength` goes, because it holds by definition. Not
  done yet: the runs keep the variable and cap it at the damaged log.
- `RunningStorageFault(i, kind)`: the durable state of a running node
  changes, with the kinds of the start-checks model (`missing_entry`,
  `missing_accepted`, `missing_promise`, `missing_decided`). A
  `missing_entry` at the last position is a lost last entry.
- `StepChecks` (Bool): the checks of 2.1 items 4 and 5 at each step. A
  damage they find makes the node leave `running`, as `stop_voting`
  does.
- `VerifyCrc` (Bool): the comparisons of 2.4 items 3 to 5, with the log
  prefix in the field `last` as a stand-in for the chain CRC.
- `ChainCheck` (Bool): the chain check of 2.3 item 3. With it, the step
  check finds a lost last entry.
- `MetaRecord` (Bool): the meta record of 2.3 item 2. With it, the step
  check finds a lost or changed promise, accepted round or decided index.
- The node takes its next step on the damaged state, because the model
  has no copy of it.
- Not modelled: the state field of 2.2. It is a function of the decided
  index, so the model has nothing to check in it. The read-back of 2.4
  item 1 is modelled as the fault that the step check finds before the
  report.

### 3.2 Questions for the model

- Does a lost promise of a running node with an empty log break S2b?
  Round 1: yes (model/results/20261024T153400Z-lmdb-only-source-runs-1.md).
- Does a lost last entry of a running node break S2? With `VerifyCrc`
  and no `ChainCheck`: yes, when the node loses the entry after its
  report and votes on.
- Does `ChainCheck` with `VerifyCrc` keep S1 to S4 for every kind?
- Is `VerifyCrc` still needed with `ChainCheck`?

### 3.3 Configurations and runs

Nodes {1, 2, 3}, Values {v1, v2}, MaxBallot 2, MaxLog 2, one fault.
With running faults, S5 and S2b do not hold by definition of the
fault, so these runs check S1 S2 S3 S4 and the properties `S2Grows`
`BuildOnlyFromRunning`. One config for each kind, with and without
`VerifyCrc` and `ChainCheck`. Each run is a simulation of 15 min, at
most three at a time. Results go to
`model/results/<time>-lmdb-only-source-runs-<n>.md` and are pushed
after each round.

### 3.4 What the model does not show

- That the code reads LMDB as the model says. The tests of section 4
  show it.
- That `MADV_PAGEOUT` makes the read go to the disk in every case. The
  test of section 4 shows it for this machine only.
- The cost of the reads. Section 5 measures it.

## 4. Tests

- "a key lost while the node runs is found at the next step";
- "a last entry lost while the node runs is found by the chain check";
- "a promise lost while the node runs is found at the next step" for a
  log that is not empty;
- "a read in a step sees the writes of the same step";
- "no message leaves before the commit of its step";
- "a resent promise carries the stored values";
- "the read-back reads from the disk": `mincore` shows the pages out
  of the cache before the second read;
- "an entry that differs after the commit stops the vote";
- "a user error aborts only its nested transaction";
- "a leader does not decide on an accepted with another chain CRC".

## 5. Checks of the build

- g++ 16, image localhost/cxx-matrix-fuse, Release, all tests.
- counter_stress seeds 1 to 300 with `--cbor --power-loss 50`.
- The cost, by the rules of a measurement, before and after, in every
  build: the commit of a step, the read-back of a step, and the
  throughput for batch sizes 1, 16 and 256. No number is written
  before the run.

## 6. Provisional names

`RunningStorageFault`, `StepChecks`, `VerifyCrc`, `ChainCheck`,
`MetaRecord`,
`in_work`, `committed`, the transaction type of 2.1 item 7. The owner
decides the names.
