# Plan: LMDB is the only source of the durable values

Source: the owner, 2026-10-24 14:44 and 14:45: "unsere einzige quelle
für daten ist lmdb", "wir antworten nie aus dem speicher", "wir sind
ein distributed log, nur das log kennt das log". The owner decided at
14:46 that this change comes before the start checks
(doc/plan-start-checks.md on branch start-checks).

Order of the work: this plan, then the model of section 3 and its TLC
runs, then one review of plan and model by a second agent, then the
go of the owner, then the code of section 2.

The owner, 19:38: "nur während eine transaction wissen wir was im log
existiert. Nen node hat nur seine FSM die sagt was gerade los ist im
ram." So outside a transaction, a node knows only the state of its
state machine (role, phase); every fact about the log is read in a
transaction.

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
  (`:922`, `:940`, `:954-956`, `:986`, `:1046`, `:1067-1068`), and the
  leader of a forward (`:475`, `:541`, `:669`).
- The read ranges use `accepted_idx_` as their end (`:501`, `:524`,
  `:612`, `:891`, `:987`).
- `node` reads `get_promise()` between steps (`node.hpp:127`, `:221`,
  `:374`, `:466`).
- One step is the calls of `handle` for a batch and then `commit`
  (`sequence_paxos.hpp:276-301`). `commit` writes the waiting ops in one transaction
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
- A normal commit syncs the data first and writes the meta page after
  it (`mdb_txn_commit`, `mdb.c:4606-4616`: `mdb_env_sync0`, then
  `mdb_env_write_meta`).
- LMDB_1.0.2 has page checksums (`mdb_env_set_checksum`, `lmdb.h:1118`)
  and page encryption (`mdb_env_set_encrypt`, `lmdb.h:1108`). The plan
  does not use them: the owner decided on 2026-10-16 against
  `mdb_env_set_checksum` and `MDB_REMAP_CHUNKS`, because they break the
  views into the map. LMDB checks a
  checksum only on the path of `MDB_REMAP_CHUNKS` (`mdb_rpage_encsum`,
  `mdb.c:6860-6885`, called at `:7215-7233`), and only when a page first
  enters that cache. With a checksum alone, `MDB_REMAP_CHUNKS` stays off
  (`MDB_REMAPPING`, `mdb.c:2337`; only encryption sets it, `:12147`):
  LMDB writes the checksum (`:4296`), checks its size at open (`:5646`),
  and checks no page on a read. In one
  bench, the checksum added no cost above the noise floor of 17.1 % to a
  step or to a read after it, and the reads of a step called it zero
  times (bench/results/2026-10-24-165345Z-lmdb-page-checksum.txt).
- The page cache, tested on this machine (kernel 6.18, ext4), in small
  programs outside the tree
  (bench/results/2026-10-24-163112Z-page-cache-drop.txt):
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
- The read-back costs, against the same step with no read-back, in
  one bench on this machine (g++ 16 -O2, ext4 on a virtio disk, noise
  floor 16.2 %): +23 % to +24 % with 1 message per step, +29 % to
  +35 % with 16, +95 % to +128 % with 256
  (bench/results/2026-10-24-162320Z-lmdb-read-back.txt). The page-out
  left one page in the cache in 19 of 84 runs.
- A value that a read transaction gives points into the map of LMDB,
  because `MDB_REMAP_CHUNKS` is off (`MDB_REMAPPING`, `mdb.c:2337`).
  Encryption sets it (`:12147`); a checksum alone does not. With it, LMDB
  maps chunks and, with encryption, decrypts into its own buffer; the
  read-back of 2.4 then needs another form.
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
   `:221`, `:374`, `:466`) runs in a read transaction and calls the step
   check of item 5 first, so no value leaves the node without the
   check. The read transaction is kept and reused with `mdb_txn_reset`
   and `mdb_txn_renew`, as LMDB documents for that use.
4. In one transaction, the storage gives the log length as the last
   key plus one, and compares it with the count of entries through the
   continuity function of the start-checks plan (its item 2.1). When
   they differ, the read returns `missing_entry`.
5. The checks of `with` in the start-checks plan (its item 2.4), the
   state check of 2.2 and the chain check of 2.3 run at the first read
   of each step and again as its last read before the commit, not only
   at the start. One function makes them. A damage that they find makes
   the node stop voting. It then copies the whole durable state from a
   peer that votes and does not recover, and runs the recovery of VR
   Revisited, as at a start (owner, 2026-10-24 19:21). The model of this
   is round 8.
6. `cached_promise_message_` (`:728`) is built again from LMDB on a
   resend (`:440`), not kept.
7. The storage concept (`storage.hpp:121-129`) changes from
   `write_atomically(ops)` and separate getters to a transaction that
   reads and writes. Its members take their names from LMDB (commit,
   abort). The type is `storage::transaction` (owner, 2026-10-24 19:23);
   `konsens::transaction` (`node.hpp:70`) stays. No LMDB name leaves the
   library.
8. A write error of a step throws `write_error`, as now. A read error
   of LMDB throws a type derived from `std::system_error` (owner,
   2026-10-24 14:39).
9. Every new error branch is `[[unlikely]]`.

### 2.2 The state of an entry

1. An entry has a state field: `in_work` or `committed`. The names are
   provisional.
2. An entry is appended with `in_work`. Each step that raises the
   decided index from `o` to `d` sets `committed` on the entries
   `o + 1` to `d`, in the same transaction. These are all the writers of
   the decided index: the decide in `accept_decide`, `accept_sync` and
   `decide`, the majority of promises (`handle_majority_promises`), the
   end of recovery (`complete_recovery`), the build of a new node and
   the start in single-node mode. Each has a test.
3. The step check reads only the entries at the decided index and the
   one after it: the first is `committed`, the second is `in_work`.
4. The entry CRC covers the index, the kind, the payload and the
   result. The state has its own CRC, because it differs between nodes
   while the decided indexes differ.
5. Every comparison between nodes uses the chain CRC, never the entry
   CRC. Recovery compares the last CRC today (`sequence_paxos.hpp:899`,
   `:936`, `:994`); it uses the chain CRC.

### 2.3 The chain CRC

1. Each entry also carries a chain CRC: the CRC over the chain CRC of
   the entry before it, the index, the kind, the payload and the
   result. The state is not in the chain, because it changes.
2. One meta record holds the promise, the accepted round, the decided
   index, the last index and its chain CRC, with one CRC over all of
   them. Now these are three keys (`src/persistent_storage.cpp:31-33`).
   The node writes the meta record when it is created: the seed node at
   the first start, every other node at the end of its recovery. The
   write of an entry and the write of the meta record are in one
   transaction. A running node with no meta record, or with a meta
   record whose CRC fails, is damaged. Round 3 of the model showed why:
   a node with an empty log that loses its promise breaks S1, and no
   other check finds it. The owner chose this form, 2026-10-24 19:19.
3. The step check reads the last entry and the meta record in its
   transaction and compares the index and the chain CRC. This finds a
   lost last entry. A lost entry in the middle leaves both equal; the
   continuity check of 2.1 item 4 finds it.
4. The chain CRC of entry `k` stands for the prefix up to `k`. The model
   compares the prefix itself (`PrefixAt`, `VerifyCrc`).
5. What no local check finds: the disk gives back an older meta record
   with a valid CRC while the log stays (`stale_meta`), or older
   versions of the entries and the meta record together (`rollback`).
   LMDB writes a changed page to a new place and reuses freed pages, so
   an older version of a page can stand on the disk. Model round 5
   checks both kinds. At a start, the recovery of VR Revisited covers
   them; while the node runs, item 6 covers part of them.
6. Each node keeps, for each peer, the highest promise and the highest
   accepted index that the peer reported, in memory. A message of the
   peer with a lower value makes the node refuse it and report the
   proof. This finds only a step back that a peer saw. The README says
   this, and recommends a drive with power-loss protection or with its
   write cache off (owner, 2026-10-24 19:19). The model of this check is
   round 7.

### 2.4 The read-back and the vote

1. After the commit of a step, the node reads back each entry that the
   step wrote, the meta record, the pages on the path to them and the
   two LMDB meta pages, from the disk and not from the cache:
   - a read transaction gives the address of each value in the map;
   - `madvise(MADV_PAGEOUT)` and then `posix_fadvise(POSIX_FADV_DONTNEED)`
     on the same range take the pages out of the cache;
   - a page that stays in the cache stays; the node does nothing more
     about it and reports as usual (owner, 2026-10-24 19:28);
   - a new read transaction reads the values from the disk;
   - the node computes the CRC of each entry and the chain CRC.
   Measured (bench/results/2026-10-24-165704Z-lmdb-read-back-full.txt,
   noise floor 10.6 %): with the meta pages, +16 % to +120 % per
   message against no read-back; with the whole used file, +31 % to
   +156 %, and that cost grows with the file. In 34 of 84 runs with the
   meta pages, two to six pages stayed in the cache over the whole run
   of about 320 steps. In a separate test, the meta pages stayed after
   the first attempt in 1 of 3400 steps, and a second attempt dropped
   them (bench/results/2026-10-24-165906Z-lmdb-meta-page-drop.txt).
2. A value that is not equal to what the step wrote is a lost write.
   The node does not send its `accepted` and stops voting. The report of
   the proof (node, index, expected and found CRC) is provisional: it
   goes to the log sink of the library, as the logging decision of
   2026-10-17 says for records.
3. `accepted` carries the chain CRC of the accepted index, read back. A
   `decide`, an `accept_decide` and an `accept_sync` carry the chain
   CRC of the decided index of the leader. The recovery response and
   `new_state` carry the chain CRC of their last index, and the node
   compares it before it raises its decided index.
4. The leader counts an `accepted` only when its chain CRC equals the
   chain CRC of its own log at that index. It decides only from such
   reports.
5. A follower raises its decided index to `d` only when its own chain
   CRC at `d` equals the one in the message. Otherwise it keeps its
   decided index.
6. The page range: `mdb_env_info` gives the map address and size; the
   map is one `std::span`. The offset of a value is its address minus
   the map address, rounded down to a named page size, and
   `subspan` gives the range. The one pointer subtraction is open
   (section 7). `std::align` does not fit: it rounds up.
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
4. Open (section 7): a follower runs the work before the decide. When a
   later `accept_sync` cuts that entry (`sequence_paxos.hpp:804-810`),
   its effects must go too. The Jepsen decision of 2026-10-17 11:24 put
   the state in its own LMDB environment and its commit after the
   decide; the owner's form of 2026-10-24 15:35 reads the result back
   after that commit and before "committed". Both cannot hold for one
   commit.

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
format changes through 2.2 and 2.3, so the format version goes up. No
store of an older format exists outside the tests, so the library
refuses an older format and does not convert it. Item 10 of the
start-checks plan ("no change to the file format") changes with this.

## 3. Model

### 3.1 Change

- `accepted_idx` leaves `volatile`. Every use reads `Len(log[i])`.
  `AcceptedIdxIsLogLength` goes, because it holds by definition. Not
  done yet: the runs keep the variable and cap it at the damaged log.
  Round 8 removes it (owner rule of 19:38).
- `RunningStorageFault(i, kind)`: the durable state of a running node
  changes, with the kinds of the start-checks model (`missing_entry`,
  `missing_accepted`, `missing_promise`, `missing_decided`) and two
  more: `stale_meta` and `rollback` (2.3 item 5). A `missing_entry` at
  the last position is a lost last entry.
- `StepChecks` (Bool): the checks of 2.1 items 4 and 5 at each step. A
  damage they find makes the node leave `running`, as `stop_voting`
  does.
- `VerifyCrc` (Bool): the comparisons of 2.4 items 3 to 5, with the log
  prefix in the field `last` as a stand-in for the chain CRC.
- `ChainCheck` (Bool): the chain check of 2.3 item 3. With it, the step
  check finds a lost last entry.
- `MetaRecord` (Bool): the meta record of 2.3 item 2. With it, the step
  check finds a lost promise, accepted round or decided index, and a
  `stale_meta` whose log differs from the present log. It does not find
  a `stale_meta` with the same log, nor a `rollback`.
- The node takes its next step on the damaged state, because the model
  has no copy of it. A fault that the checks find is found in the same
  action: the model takes no step of the node between them. This holds
  for the code only with 2.1 items 3 and 5. A damage that appears in
  the middle of a step, after its first check, is caught by the last
  check of the step; the model does not show it.
- Not modelled: the state field of 2.2. The model treats it as a
  function of the decided index; the tests of 2.2 item 2 check the
  writers. The read-back of 2.4 item 1 is modelled as the fault that
  the step check finds before the report.
- Not modelled: the recovery of a node that stopped while it ran. The
  model keeps it out of `running` (section 7).

### 3.2 Questions for the model

- Does a lost promise of a running node with an empty log break S2b?
  Round 1: yes (model/results/20261024T153400Z-lmdb-only-source-runs-1.md).
- Does a lost last entry of a running node break S2? With `VerifyCrc`
  and no `ChainCheck`: yes, when the node loses the entry after its
  report and votes on.
- Does `ChainCheck` with `VerifyCrc` keep S1 to S4 for every kind?
- Is `VerifyCrc` still needed with `ChainCheck`? Round 4: no result
  changed; it stays for Execute-Verify.
- Do `stale_meta` and `rollback` of a running node break S1 to S4 with
  every check on? Round 5.
- Does a resend (`MaxTimeout = 1`) change round 4? Round 5.

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
- "the read-back takes the pages of the step out of the cache";
- "an entry that differs after the commit stops the vote";
- "a user error aborts only its nested transaction";
- "a leader does not decide on an accepted with another chain CRC".
- "each writer of the decided index sets the state of its entries", one
  test for each writer of 2.2 item 2;
- "a getter between two steps runs the step check";
- "recovery compares the chain CRC, not the entry CRC".

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

## 7. Open questions for the owner

1. Decided, 2026-10-24 19:19: one meta record with one CRC (2.3 item 2).
2. Decided, 2026-10-24 19:19: for a `stale_meta` or a `rollback` while
   the node runs (2.3 item 5), the README warns, and the peers check
   (2.3 item 6).
3. Decided, 2026-10-24 19:21: a node that stopped voting while it ran
   recovers from a peer (2.1 item 5).
4. Decided, 2026-10-24 19:28: a page that stays in the cache stays, and
   the node does nothing about it. No O_DIRECT: it would need a second
   reader of the LMDB pages.
5. The effects of a work whose entry is cut (2.5 item 4).
6. The page checksums of LMDB were decided against on 2026-10-16
   (section 1). Measured on 2026-10-24 at the owner's wish. The owner,
   19:29 and 19:30, ends the reason of 2026-10-16: a value
   from LMDB is used only inside its transaction and is never copied,
   so it does not matter how LMDB hands out a page. With a checksum
   alone, values still point into the map, so the read-back of 2.4 stays
   as it is; but LMDB then checks no page on a read (section 1).
7. The one pointer subtraction of 2.4 item 6.
8. Decided, 2026-10-24 19:23: `storage::transaction` (2.1 item 7).
   Measured with `MDB_REMAP_CHUNKS`
   (bench/results/2026-10-24-194139Z-lmdb-page-checksum-remap.txt,
   noise floor 11.0 %): the step costs no more; a read of the step after
   it costs +15 % (64 B) and +12 % (1024 B) with 256 messages per step,
   and nothing above the noise with 1 and 16. LMDB then checks pages on
   reads. A card asks the owner to decide.
