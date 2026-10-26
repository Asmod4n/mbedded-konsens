# Plan: one environment, one sync for each task and node

Source: the owner, 2026-10-26.

- 07:14 (card "Eine Umgebung"): the log and the state (the state
  machine and the client table) stand in one LMDB environment with
  named databases, so that the apply and the verdict share one commit.
  This replaces the two environments of plan-lmdb-only-source.md 2.6
  item 1 (owner 2026-10-24 20:47).
- 07:16: the form of a task, given by the owner. It replaces the open
  card on `max_in_flight`; that card is withdrawn.
  1. One environment, two named databases: one for metadata, one for
     data.
  2. A request that reaches any node goes to the leader. The node that
     receives it stores nothing.
  3. The leader tells the nodes "here is work". Each node records the
     work as in progress with no sync, runs it, and sends "I am done"
     with its result to the leader, in log order.
  4. When a majority has the same result, the leader writes "this work
     is done" and syncs. It then sends it to all other nodes. Each of
     them writes it and syncs. The goal: one sync for each task on each
     node.
- 07:18: the client gets its reply after a majority reported "synced
  done" to the leader. That step needs no further sync, only one more
  round trip.
- 07:30: the binary syncs through io_uring: `fdatasync` and the send
  of the messages that wait on it, linked (`IOSQE_IO_LINK`). LMDB
  commits with no sync of its own; the binary takes the file descriptor
  (`mdb_env_get_fd`). A "synced" ack leaves only after the sync of its
  node. The library does no IO: a step tells its caller "sync, then
  send these messages". The leader may send "done" while its own sync
  runs (Raft dissertation 10.2.1); this is a proposal, not decided.
- 07:19: a node may forget work. The client keeps its session and
  request number, waits for its timeout and asks again. The cluster
  answers "already done" with the stored reply, or runs the work again
  when the majority forgot it. No node keeps an effect that the
  majority forgot.

Order of the work: this plan, the model of section 13 and its TLC runs,
one review by a second agent, the go of the owner, then the code.

Why (measured at fa14f44, bench/results/2026-10-26-021654Z.txt, g++ 16
Release, A/A 4.4 %): a write of 1 KiB through three nodes
(`write_lmdb`) takes 4.70 ms. A plain LMDB put with its commit
(`lmdb_put`, 1 KiB) takes 0.33 ms. The write makes 27 commits of the
log storage over the three nodes (`commits_per_op`); the commits of the
state storage come on top and the bench does not count them. A review
of the code counted about 9 syncs in series for each request. The
apply commits the state environment as a second sync on each node
(`include/konsens/node.hpp:1085`).

## 1. Facts checked in this session

Line numbers of the code are at fa14f44. Line numbers of LMDB are at
LMDB_1.0.2 (c279e04, the submodule `deps/lmdb`), read in a checkout of
that commit outside the tree.

- The log environment has the named databases `log` and `meta`
  (`src/persistent_storage.cpp:27`, `:31-32`). The state environment
  has `state`, `sessions` and `applied` (`:37-41`). The node storage
  has `node` (`:47-48`). All three open with `MDB_NOTLS |
  MDB_REMAP_CHUNKS` (`:30`, `:1052`, `:1168`, `:1563`).
- `node::with` takes the log storage, a `node_storage` and a
  `state_storage` (`node.hpp:104-106`). `node_config` has
  `max_in_flight` (`node.hpp:46`), and `with` refuses 0 (`:108-111`).
- `node::commit` commits the step of `sequence_paxos` first
  (`node.hpp:200`), then serves the clients (`:216`). The apply of the
  chosen results opens a write transaction of the state (`:1041`) and
  commits it (`:1085`). The open state transaction `open_` stays open
  over steps (`:1474`, `open_the_base` `:1247-1263`).
- The applied record holds the applied index, the chain CRC at it and
  the pointer `p` of the verdict function (`:1084`,
  `src/persistent_storage.cpp:42-44`).
- LMDB: "only one write transaction may be active at a time"
  (`lmdb.h:25`). A parent transaction issues no operation while it has
  an active child transaction (`lmdb.h:1132-1137`). So a write
  transaction of the state cannot stay open in the same environment
  while a step writes the log.
- LMDB: `mdb_txn_begin` takes `MDB_NOSYNC` and `MDB_NOMETASYNC` for one
  transaction (`lmdb.h:1144-1147`). `mdb_txn_commit` skips the data
  sync for such a transaction (`mdb.c:4607-4609`) and writes the meta
  page after it (`:4616`).
- LMDB: with `MDB_NOSYNC`, "a system crash can corrupt the database or
  lose the last transactions", unless the file system keeps the order
  of writes (`lmdb.h:648-661`). With `MDB_NOMETASYNC`, LMDB keeps the
  integrity, and a system crash may undo the last transaction
  (`lmdb.h:640-647`). The reuse of free pages takes the oldest reader
  as its bound (`mdb_find_oldest`, `mdb.c:2642-2656`, used at
  `:2804-2826`). The bound does not look at the last sync, so a commit
  with no sync can reuse a page that the last durable meta page still
  names.
- LMDB: `mdb_env_sync` flushes every commit before it (`lmdb.h:872-890`,
  `mdb_env_sync0` `mdb.c:3113`). So one sync makes every earlier
  commit of the environment durable.
- ext4 with `data=ordered` (the mount of the bench, `/lmdb`) does not
  keep the order of the writes of one file. Not checked in this
  session: the text of the kernel documentation.
- LMDB: a commit syncs the data pages first and writes the meta page
  after it (`mdb.c:4601-4616`). Without `MDB_NOSYNC` and
  `MDB_NOMETASYNC`, the meta page goes through `me_mfd`, a descriptor
  opened with `O_DSYNC` (`mdb.c:4945-4949`, `:5340`, `:6387`). So a
  normal commit makes two flushes: `fdatasync` of the data, then a
  write with `O_DSYNC` of the meta page.
- LMDB_1.0.2 has `mdb_txn_prepare` (`lmdb.h:1208-1229`, `mdb.c:4646-4652`):
  it writes the data pages and syncs them unless the transaction has
  `MDB_NOSYNC` (`mdb.c:4607-4613`), and does not write the meta page. A
  later `mdb_txn_commit` writes only the meta page (`mdb.c:4407-4408`,
  `:4616`). `mdb_env_get_fd` gives the descriptor of the data file
  (`lmdb.h:958`).
- So `MDB_NOSYNC` with one `fdatasync` from outside after the commit
  gives durability once that sync ends. It does not keep the integrity
  if the power fails before the sync ends: the meta page can reach the
  disk before the data pages that it names (`lmdb.h:648-661`). The order
  that keeps the integrity is: `mdb_txn_prepare` with `MDB_NOSYNC`, then
  `fdatasync` from outside, then `mdb_txn_commit` (the meta page, with
  no sync). The meta page of that commit is durable after the next
  `fdatasync`. Not tested in this session; section 15 item 7 tests it.
- If each commit is prepared and synced before the next one begins,
  the pages of the durable snapshot are not reused: the bound of
  `mdb_find_oldest` is the transaction before the one that writes
  (`mdb.c:2645`), and the free pages of that transaction stay. Read from
  the code; not tested.
- The coordinator reports that the state file says the engine of
  slipstreamIO has no operation for a sync yet. A search of
  /home/claude/Claude-state for "fsync" found no such line. Not checked
  in slipstreamIO.
- The client reply at fa14f44 waits for the decide of the `results`
  entry: the second round of plan-execute-verify.md 2.9.1.

## 2. The form of a task

The names in quotes are provisional message names.

1. A client sends a request to any node. A node that is not the leader
   forwards it to the leader and stores nothing (owner 07:16 item 2).
2. The leader gives the request the next log index and sends "work"
   (ballot, index, request, entry CRC) to the nodes that promised.
3. Each node, the leader too, runs the work in log order on its base
   (section 5), and sends "ran" (ballot, index, base CRC, result, result
   CRC) to the leader. This is the answer of plan-execute-verify.md 2.5.
4. The leader selects the result that a majority agrees on, by the rules
   of plan-execute-verify.md 2.6 (the base, try again, the list of the
   nodes that do not agree). The owner's text says "the same result as
   its own"; see question 3.
5. The leader writes the done record (section 3.3) and syncs. The done
   record is the accept of the leader in Paxos terms. Then it sends
   "done" (ballot, index, the done record, chain CRC) to the nodes.
6. A node checks the done record (plan-execute-verify.md 2.7, "accept
   means verify"), writes it and syncs, and sends "synced" (ballot,
   index, chain CRC). A node that refuses sends "refused", as 2.7.6.
7. When a majority, the leader included, synced the done record in the
   ballot, the entry is decided. The leader sends the reply to the
   client at once (owner 07:18). The reply is the chosen reply in the
   done record.
8. The decided index travels in the next message of the leader. It needs
   no sync of its own (section 4.3). The state takes the entry with the
   next commit of the node (section 4.2).

The sync of the done record is the one sync for each task on a node
(section 3.5 says what one sync is with LMDB).
Messages for each task: work, ran, done, synced, and the reply. Syncs in
series on the path of the client: the sync of the leader, then the sync
of a follower (question 4 can make them parallel).

## 3. The one environment

### 3.1 Named databases

The owner named two: metadata and data. Proposal (question 1):

- `meta`: the meta record (section 3.2), the format version (as now),
  the log of done records (keys: the log index), and the client table
  (keys: the session id). The keys of these three kinds have a prefix of
  one byte, so that they never meet.
- `data`: the keys and values of the state machine, as `state` now. No
  key of the library stands in it, so the keys of the machine need no
  prefix.

The node storage stays an environment of its own: the repair count, the
open mark, the proof, the hidden identity and the name of the directory
of the store. No copy from a peer may bring them
(plan-lmdb-only-source.md 2.4 item 8). See question 2.

### 3.2 The meta record

One record with one CRC, as plan-lmdb-only-source.md 2.3 item 2: the
promise, the accepted round, the decided index, the log length, the
chain CRC of the last entry, and now also the applied index and the
chain CRC at the applied index. The applied record of the state
environment and its database `applied` go. The pointer `p` goes with the
verdict function (section 7).

### 3.3 The done record

One log entry for each task: the request (kind, session, request number,
payload), the chosen result (reply and mutations, as
plan-execute-verify.md 2.1.2 gives them), the CRC of the result as the
node that ran it computed it, and the ids of the counted nodes whose
result did not agree. A task with no result that a majority could still
reach gets a done record with a void result, which is the `rejected(k)`
of plan-execute-verify.md 2.1.2. The entry CRC and the chain CRC cover
the whole record.

### 3.4 One write transaction for each step

1. A step is one write transaction of the one environment. It holds the
   done records of the step, the meta record, and the apply of the
   entries that became decided (section 4.2). One commit holds them all.
2. A step that wrote nothing aborts its transaction and syncs nothing.
3. A step that wrote a done record or an apply commits in the form of
   section 3.5.
4. The runs of the work happen in a child transaction of the step
   (section 5.2). The child ends before the parent commits
   (`lmdb.h:1132-1137`).
5. The two write transactions in one thread of plan-lmdb-only-source.md
   2.1 item 3 (owner 2026-10-25 03:53) and their test go. The kept read
   transaction and `MDB_NOTLS` stay.

### 3.5 The sync, outside the library

1. The library makes no system call for a sync. Each step ends with a
   list of groups, in order. A group is "sync the store, then send
   these messages and these client replies". A message in no group
   leaves at once. The binary submits each group to io_uring as an
   `fdatasync` of the descriptor of the store, linked
   (`IOSQE_IO_LINK`) to the sends. A user of the library does the same
   with its own IO. After the sync, the caller tells the library that
   the group is synced, and the library releases what waits on it.
2. The groups:
   - a "synced" ack and a "promise": after the sync of the node;
   - the reply to a client: after the leader counted a majority of
     "synced" acks (no sync of its own, section 2 item 7);
   - "work", "ran", "decide" and the heartbeats: at once;
   - "done" from the leader: after its own sync (07:16), or at once
     while its sync runs (Raft dissertation 10.2.1, proposal of the
     owner, not decided, question 4). The leader counts itself only
     after its own sync, so safety does not change.
3. The commit with LMDB (question 6). The step prepares its write
   transaction with `MDB_NOSYNC` (`mdb_txn_prepare`). The group "sync"
   is the `fdatasync` of the data pages. After it, the library commits
   the meta page with no sync. The meta page is durable after the next
   `fdatasync`. Two forms follow:
   - Q1 (proposal): one `fdatasync` for each step. The messages of a
     step wait on the sync of the next step, which also makes the meta
     page of the step before durable. Under load a step follows at
     once; an idle node makes the next sync at once by itself. This is
     the form of the bench of 2026-10-24 (13 to 32 % less time for each
     message, bench/results/2026-10-24-145849Z-lmdb-txn-nometasync.txt).
   - Q2: two `fdatasync` for each step, data then meta page, linked.
     The messages wait for the second one.
   - Not taken: `MDB_NOSYNC` with one sync after the commit. A power loss
     before the end of the sync can leave a meta page whose data pages
     are not on the disk (section 1).
4. A failed sync throws `sync_error` in the caller's call that reports
   the result of the sync, and the process ends, as now (owner,
   2026-10-21). Nothing repeats the sync.
5. Dependency: slipstreamIO needs an operation for `fdatasync` in its
   engine, with the link to a send. Section 1 says what is not checked.

## 4. The order of log and state

### 4.1 What the old order was for

- plan-lmdb-only-source.md 2.6 item 3: the state commits after the
  commit of the step that raised the decided index. So the state never
  holds an entry that is not decided, also after a crash between the two
  commits.
- 2.4 item 9: a copy from a peer sends the state environment first and
  the log after it, so that the applied index of the copy is not above
  its decided index.
- plan-state-in-lmdb.md 1.4: a start compares the applied chain CRC of
  the state environment with the log, because two commits can leave
  them apart.

### 4.2 What replaces it

1. The state takes an entry only after the entry is decided: after a
   majority synced its done record. The done write does not touch the
   database `data` or the client table (owner 07:19: no node keeps an
   effect that the majority forgot). A done record above the decided
   index is an accepted entry; a later ballot can cut it, and nothing
   of it stands in the state.
2. The apply writes the chosen mutations and the session record of each
   decided entry, the applied index and its chain CRC, in the
   transaction of a step. The applied index and the decided index stand
   in one meta record and commit together. So `applied <= decided <= log
   length` holds in each snapshot of the file.
3. The apply rides in the next commit of the node. Under load that is
   the commit of the next done record, so the apply costs no sync of its
   own. When no done record waits, the node commits the apply alone at
   the end of the step in which it learned the decide (one sync, off the
   path of the client reply). A read through the read index waits until
   the applied index reaches the read index, as now.
4. The order "log first, state second" is gone: there is one commit.

### 4.3 The decided index

1. The decided index is written in the commit of the step that learned
   it, with no sync of its own. The rule "the decide index keeps its own
   synchronous commit" (owner 2026-10-22) changes with this.
2. After a power loss, the stored decided index can be below the value
   that the node had. It is never above it. The done records up to the
   true decided index stand synced on a majority, so no decided entry
   is lost. The node learns the decided index again in the recovery
   after the start (section 8.1).

## 5. The work before the done

### 5.1 Where the record of the work stands (question 5)

The owner's form commits the work with no sync. LMDB documents that
such a commit can corrupt the database at a system crash on a file
system that does not keep the order of writes (section 1). A corrupt
file is a damage, and the repair is a full copy from a peer. So the
plan takes this form provisionally:

- P1 (provisional): the work is not committed. The request and the own
  result of each task in work stay in memory until the done record. A
  crash forgets them, as the owner allows (07:19). The node keeps no
  durable value in memory; the rule "no answer comes from a copy in
  memory" (owner 2026-10-24 14:44) covers the durable values.
- P0 (the text of the owner): the work is committed with `MDB_NOSYNC`.
  A start deletes each work record in its first write transaction. The
  risk of a corrupt file stays: section 3.5 item 3 shows that each
  commit needs the sync of its data pages before its meta page. A
  work record that rides in the commit of a step that syncs anyway
  costs no sync; a step with only work records would need one.
- P2: the work is committed with `MDB_NOMETASYNC`. LMDB keeps the
  integrity. Each such commit syncs the data once, so a task costs two
  syncs on each node.

### 5.2 The base and the run

1. The base of the task at index `q` is the committed state, then the
   chosen results of the done records above the applied index, then the
   own results of the tasks in work below `q` (plan-execute-verify.md
   2.4.1). The own results are a guess.
2. In each step, the node opens one child transaction of the step,
   writes the base into it, runs the new tasks in log order, and aborts
   the child. Under P0 and P2 the own results also go into the work
   records of the step.
3. The open state transaction over many steps (plan-state-in-lmdb.md 5,
   `open_`) goes: one environment allows one writer, and a child cannot
   outlive the commit of its parent. Its cost moves to the writes of the
   base in each step; section 15 measures it.
4. When a chosen result differs from the own result, or a task is void,
   the node runs again each task in work above it, on the new base, and
   sends "ran" again (plan-execute-verify.md 2.4.3).
5. A cut of the log (a new ballot, `accept_sync`, recovery, repair)
   drops the tasks in work above the cut. Nothing in the state needs an
   abort, because the state holds only decided entries.

### 5.3 Two tasks in flight

The leader sends "work" for each new request at once. Each node runs
the tasks in log order, each on the base of section 5.2. The leader
counts a "ran" for `q` only on the selected base (plan-execute-verify.md
2.6.2). Done records are written in log order.

### 5.4 No bound from the library

`max_in_flight` goes from `node_config`, from the model and from the
plans. The tasks in flight are bounded by what the user sets: the
bounded queue, the limit of open requests for each certificate (owner
2026-10-21) and the size of the store. The library sets no bound of its
own.

## 6. Group commit

1. A step takes every message and request that waits when it begins
   (plan-lmdb-only-source.md 2.7).
2. One sync covers every done record that waits: the leader writes the
   done records of all tasks whose selection is ready, in log order, in
   one transaction. A follower writes every "done" of the batch in one
   transaction.
3. While one sync runs, new messages wait for the next step. So the
   batch grows with the cost of the sync.

## 7. Execute-Verify: what changes

1. The `results(from, to, list)` and `rejected(k)` entries go. The done
   record of each task holds its result (section 3.3). A request is
   never decided without its result.
2. The verdict function `V` and the pointer `p` (2.3) go. The chosen
   result of the entry at `k` is the result in that entry. The applied
   index (2.3.3) is the decided index after the apply.
3. The second round (2.9.1) goes. The client waits for one round that
   syncs (done, synced) after the round that runs (work, ran). Before:
   the round of the request and the round of the `results` entry both
   synced on each node.
4. Accept means verify (2.7) applies to the done record. A node syncs a
   done record only after its check. So every durable entry was checked
   by its node, and `verified_idx` (2.7.4, 2.7.5) goes: a promise
   carries only done records.
5. Try again (2.6.4), the list (2.6.5), the minority (2.8), the rounding
   of floats (2.2.2a) and the read back (2.3.5) stay as they are.
6. The leader appends at most one verdict entry in each step (2.6.5)
   goes: a step writes every done record that is ready.
7. A StopSign waits until no task is in work (2.3.6, now with the tasks
   of section 5).

## 8. Recovery, repair and copy

### 8.1 Recovery

1. After each start, a node votes only after f + 1 answers (VR
   Revisited), as now. The tasks in work are gone (P1) or deleted (P0).
2. A node whose done record above the decided index was not synced by a
   majority keeps it as an accepted entry of its ballot. The next
   leader keeps it or cuts it by the rule of the highest accepted round
   (Omni-Paxos). The state never held it (section 4.2).
3. A client that waited asks again. A run of the retry sees the session
   record in its base: an entry that is decided gives the stored reply;
   an entry that the majority forgot runs again (owner 07:19).

### 8.2 Repair and copy

1. The repair by entries (plan-lmdb-only-source.md 2.4 items 1 to 7)
   stays. It writes the received done records and the meta record in one
   transaction.
2. 2.4 item 10: when the chain CRC at the applied index differs from the
   repaired log, the node takes the full copy of item 3. Else it keeps
   its state.
3. The full copy after `MDB_BAD_CHECKSUM` (2.4 item 9): the peer runs
   `mdb_env_copyfd2` without `MDB_CP_COMPACT` over the one environment.
   The copy is one snapshot of the log, the state, the client table and
   the meta record. The order "state first, then log" goes. The receiver
   checks the copy as now and also checks `applied <= decided <= log
   length` in it. The switch through the name of the directory in the
   node storage stays.
4. A copy for a new node (owner 2026-10-24) is the same copy.

## 9. Start checks

In one read transaction:

1. The meta record and its CRC, now with the applied index and its chain
   CRC.
2. `applied <= decided <= log length`. A violation is a damage.
3. The chain CRC at the applied index equals the one in the meta record.
   A difference is a damage. `paxos_errc::state_differs_from_log` (not a
   damage, plan-state-in-lmdb.md 1.4) goes, because one commit cannot
   leave log and state apart.
4. The step check, the continuity and the scan of the whole log as now.
5. Under P0, the first write transaction deletes the work records.

## 10. The CRCs

1. The entry CRC and the chain CRC cover the done record (section 3.3).
2. "work" carries the CRC of the request; "ran" carries the CRC of the
   result, as now. "done" carries the entry CRC and the chain CRC.
   "synced" carries the chain CRC at its index.
3. The meta record has one CRC over all its fields (section 3.2).
4. The state values have no CRC of their own; the read back of
   plan-execute-verify.md 2.3.5 stays.

## 11. The public interface

1. `node::with` takes one store (log and state) and the node storage, in
   place of three storages. The class `state_storage` goes; its views
   stay (`write_view`, `read_view`).
2. The store opens from a directory and a size. No LMDB name stands in
   the interface: not "environment", not "named database", not
   "map size" (the open question on `map_size` stays open).
3. `node_config` loses `max_in_flight`.
4. The `state_machine` concept does not change.

## 12. Migration

No release exists, and no store of an older format exists outside the
tests. The format version goes up, and the library refuses an older
format, as plan-lmdb-only-source.md 2.9 says. No converter is needed.

## 13. The model

### 13.1 Form

A new module `model/tla/OneEnvironment.tla` (provisional name) models
the form of section 2. `SequencePaxos.tla` models two commits for each
entry and the verdict entries; it stays as the model of the agreement
rules of Execute-Verify. The new module models what this plan changes:
when a write is durable.

- Durable, kept up to the last sync of the node: `promise`, `acc`,
  `log`, `syn` (the log length at the last sync), `durdec`, `state`,
  `applied`.
- Volatile: `dec`, `work`, `status`, `lead`, `answers`.
- Ghost: `chosen` (each decided index with its entry), `replied` (each
  client reply), `crashes`.
- A crash drops everything after the last sync: the log falls to `syn`,
  the decided index to `durdec`, the tasks in work go (owner 07:16, the
  model rule).
- The work is not durable (P1; also the worst case of P0, where every
  work record is lost). A result is a function of the entries before
  it, so the model has no result values; the agreement rules are those
  of `SequencePaxos.tla`.
- A request can be proposed again (a client retry). The state is the
  fold of the applied entries: each request takes effect once.

### 13.2 Properties

- `ChosenOnce`: each index is chosen with one request, and each client
  reply names that request (no confirmed entry is lost).
- `DecidedAgree` (S2), `DecidedIsChosen`.
- `StateIsFold`: the state of each node is the fold of the chosen log up
  to its applied index, each request once (owner 07:19).
- `AppliedIsDecided`, `TypeOK` (applied index at most the durable
  decided index).
- Covers, each must be violated: `NoForgottenDone` (a node holds a
  synced done record that is not chosen at its index; the case of owner
  07:19), `NoRetryChosen` (a retry of a request is chosen at a second
  index), `NoDecidedLost` (a restarted node holds a decided index below
  the chosen log).
- Mutations, each must break its property: `ApplyAtDone` (the done
  write applies the state): `StateIsFold`; `ReplyAtLeaderSync` (the
  reply after the sync of the leader alone): `ChosenOnce`;
  `DoneWithoutSync` (a node acks "synced" before its sync):
  `ChosenOnce` or `DecidedAgree`. `NoRecovery` (a restarted node votes
  at once) is a question: does safety in this form rest on the
  recovery?

### 13.3 Runs

Nodes {n1, n2, n3}, Requests {r1, r2}, MaxBallot 3, MaxLog 2, MaxCrash
1, BFS where it ends, at most three TLC at a time, one worker each. The
results are in `model/results/<time>-one-environment-runs-1.md`.

### 13.4 What the model does not show

- That LMDB keeps the integrity of the file. P0 is outside the model.
- The agreement rules of Execute-Verify; `SequencePaxos.tla` rounds 1
  to 3 cover them.
- Group commit: the model writes one done record in each action, a
  subset of the behaviours of a commit of several.
- A lying disk (stale meta, rollback): plan-lmdb-only-source.md rounds 5
  to 15 cover them in `SequencePaxos.tla`.

## 14. Tests

- "the apply and the decided index commit in one transaction";
- "a done record above the decided index leaves the state unchanged";
- "a leader that syncs done and dies before a majority leaves no effect
  on any node after the next ballot";
- "the client reply leaves only after a majority acked synced";
- "a retry of a decided request gets the stored reply";
- "a retry of a forgotten request runs once";
- "one sync covers every done record of a step";
- "a step that writes nothing syncs nothing";
- "a node that follows writes one sync for each task under load"
  (counts syncs through the bench counter);
- "the full copy is one snapshot with applied <= decided <= log length";
- "a start with applied above decided is a damage";
- "the runs of a step leave no write in the store";
- "a chosen result that differs makes the node run the later tasks
  again";
- the tests of plan-execute-verify.md for the done record in place of
  the verdict entries.
- Removed: the tests of two write transactions in one thread, of the
  open state transaction over many steps, of `max_in_flight`, and of
  `state_differs_from_log`.

## 15. What the bench measures before and after

By the rules "Measure before, measure after": both arms built the same
way in the image localhost/cxx-matrix-fuse, each arm in its own binary,
run alternately, 7 rounds, medians compared, the A/A of each case
first. The arm before is fa14f44. No number is written before the run.

1. `write_lmdb` at 64 B, 1 KiB and 64 KiB; `read_lmdb`; `commit_lmdb`.
2. `write_batch_size_lmdb` at 1, 16 and 256.
3. The pipeline cases at 0 ms and 20 ms delay, 10000/s and 20000/s, the
   work put and loop. The arm before takes `max_in_flight` 1, 8 and 64;
   the arm after has no such argument.
4. `execute_verify_int` and `execute_verify_float`.
5. New counters in every cluster case: syncs for each task and node,
   commits for each task, messages for each task. The goal of the owner
   is one sync for each task and node.
6. A hypothesis file outside the tree before the code: the cost of the
   base in a child transaction (section 5.2) against the open
   transaction of fa14f44, for 1, 8 and 64 tasks in work; and the cost of
   a commit with `MDB_NOSYNC`, with `MDB_NOMETASYNC` and of no commit
   (P0, P2, P1).
7. A test file outside the tree: a power cut on the FUSE file system
   error_fs between `mdb_txn_prepare` with `MDB_NOSYNC`, the
   `fdatasync` and `mdb_txn_commit`, at each point, and the open after
   it; the same for `MDB_NOSYNC` with one sync after the commit.
   Not checked: whether error_fs can drop the writes after the last
   sync; if not, it gets that switch first.
8. The forms Q1 and Q2 of section 3.5 against the normal commit:
   `commit_lmdb`, `write_lmdb`, `write_batch_size_lmdb`, with the sync
   through io_uring in the bench.

## 16. Rules of older plans that change

- plan-lmdb-only-source.md: 2.1 item 3 (two write transactions in one
  thread); 2.2 (the entry states `in_work` and `committed`: the log
  holds done records, and the decided index gives the decide); 2.3 item
  2 (the meta record gets the applied index); 2.4 item 9 (the order of
  the copy) and item 10 (the full copy is one environment); 2.6 items 1,
  3, 4, 5, 7 and 8 (own environment, commit after the decide, run before
  the decide in an open transaction, the pipeline with `max_in_flight`,
  the abort on a cut, two environments without `MDB_WRITEMAP`: now one);
  question 11 and 12.
- plan-state-in-lmdb.md: 1.1 (`state_storage`, own environment), 1.3,
  1.4 (`state_differs_from_log`), 1.5, 3.2 and 3.3 (the state commit),
  5.1 to 5.5 (the open transaction, the bound, two write transactions).
- plan-execute-verify.md: 2.1.2 and 2.1.3 (verdict entries), 2.3 (V and
  `p`), 2.4.2 (the open transaction holds the base), 2.4.4
  (`max_in_flight`), 2.6.5 (one verdict entry in each step), 2.7.4 and
  2.7.5 (`verified_idx`), 2.9.1 (the second round), 4.4 and 4.6 (model
  variables of the verdict entries).
- State file, decisions in force: "an entry counts as stored only
  after a synchronous LMDB commit" (now: after the sync of section 3.5);
  "the decide index keeps its own
  synchronous commit" (2026-10-22); "the log has its own LMDB
  environment, apart from the state; each node has one batch in work at
  a time"; "two write transactions (log and state) in one thread"
  (2026-10-25 03:53).

## 17. Provisional names

`OneEnvironment`, `meta`, `data`, done record, "work", "ran", "done",
"synced", "refused", `syn`, `durdec`, `NoForgottenDone`,
`NoRetryChosen`, `NoDecidedLost`, `ApplyAtDone`, `ReplyAtLeaderSync`,
`DoneWithoutSync`, `NoRecovery`, P0, P1, P2. Paxos names the done record
an accept, and the "synced" message an accepted.

## 18. Questions for the owner

1. The two named databases: proposal `meta` (meta record, log, client
   table) and `data` (the keys of the machine only). Option: the log and
   the client table in `data` with a prefix. Option: three or four named
   databases.
2. The node storage (repair count, identity, the name of the store
   directory) stays a second environment, because no copy may bring it.
   Proposal: keep it. Option: a third named database in the one
   environment, left out of each copy; `mdb_env_copyfd2` copies the
   whole environment, so this needs a copy of our own.
3. The leader writes done when a majority agrees. The text of 07:16
   says "the same result as its own". Proposal: the majority rule of
   plan-execute-verify.md 2.6 (the leader is one of many); a leader whose
   own result differs runs again and throws when it still differs.
   Option: the text as given; then a leader with a wrong result decides
   nothing until a leader change.
4. The leader syncs, then sends "done" (07:16). The owner proposed
   (07:30) that the leader sends "done" while its sync runs (Raft
   dissertation 10.2.1); it counts itself only after its sync. Safety
   does not change, and the client path loses one sync in series.
   Proposal: take it. Option: the order of 07:16.
5. The work record (section 5.1). Proposal: P1, no commit, the work stays
   in memory. Option P0, `MDB_NOSYNC` as given, with the risk of a
   corrupt file at a power loss on a file system that does not keep the
   order of writes. Option P2, `MDB_NOMETASYNC`, two syncs for each task.
6. The commit with LMDB (section 3.5 item 3). LMDB needs the data pages
   on the disk before the meta page that names them. Proposal Q1: one
   `fdatasync` for each step, and the messages of a step leave after
   the sync of the next step. Option Q2: two `fdatasync` for each step,
   and the messages leave after the second. Both wait for the test of
   section 15 item 7.
