# Plan: the storage is the only source of the durable values

Source: the owner, 2026-10-24 14:44 and 14:45: the only source of data
is the storage; no answer comes from memory; only the log knows the log. The
owner decided at 14:46 that this change comes before the start checks
(doc/plan-start-checks.md on branch start-checks).

Order of the work: this plan, then the model of section 3 and its TLC
runs, then one review of plan and model by a second agent, then the
go of the owner, then the code of section 2.

The owner, 19:38: outside a transaction, a node knows only the state
of its state machine (role, phase); every fact about the log is read
in a transaction.

The owner, 14:49: the storage runs in its fast and safe modes. So a
step runs in one write transaction of the storage (section 2).

The owner, 20:48 and 20:50: nothing is read from the storage except
through the storage. The node reads no page back from the disk itself.
The former form of this section is in the git history at ebd9f42.

The form of a client work, from the owner, 15:27 to 15:39, 15:55, 20:29
and 20:47:

- A client sends work to any node. That node forwards it to the
  leader.
- The leader appends one entry with the state "in work". There is one
  entry, not two: its state changes to "committed" later.
- A follower reports the CRC of the entry and of its result. The leader
  sets "committed" only when a majority reports the same values. The
  decided index goes through the same check.
- The state of the state machine changes only after the decide, from
  the replicated log. There is no second log.
- A step takes all the work that waits (group commit, no fixed wait).
- A node finds an entry that is lost after its report through a chain
  CRC at each step (card, 15:55).
- The README names the offsite node.

## 1. Facts checked in this session

Line numbers are at commit 06a52bf. `git diff 06a52bf HEAD -- include
src` is empty, so they apply to the head of this branch.

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
- `handle_majority_promises` sets the decided index from
  `get_max_decided_idx` of the promises (`:758`, `:768`), with no CRC.
- The read ranges use `accepted_idx_` as their end (`:501`, `:524`,
  `:612`, `:891`, `:987`).
- `node` reads `get_promise()` between steps (`node.hpp:127`, `:221`,
  `:374`, `:466`).
- `node` keeps the client table and the applied index in memory and
  applies a decided entry to `machine_` in memory (`node.hpp:435-450`).
  It answers a read from `machine_` (`:476-485`). The `state_machine`
  concept has no transaction (`state_machine.hpp:11-14`).
- One step is the calls of `handle` for a batch and then `commit`
  (`sequence_paxos.hpp:276-301`). `commit` writes the waiting ops in one
  transaction (`:295`). Messages leave through `take_outgoing_messages`,
  which throws while ops wait (`:459-463`). Some handlers write the
  waiting ops in the middle of a step before they read
  (`get_storage_after_writing_waiting_ops`, `:591-595`;
  `handle_majority_promises` `:766`; `complete_recovery` `:1071`).
- `persistent_storage` opens one read transaction for each getter
  (`src/persistent_storage.cpp:454`, `:499`, `:515`, `:538`). The four
  values and the last CRC are read in separate transactions.
- `get_log_len` is the count of entries (`read_log_len`, `:192-200`),
  not the last key plus one. Keys count from 0, so the decided index `d`
  means that the entries with the keys 0 to `d - 1` are decided.
- The benches of a read-back from the disk (162320Z, 165704Z, 165906Z,
  195244Z, 203303Z in bench/results/) and of the page cache (163112Z)
  stay as records. The owner decided against the read-back at 20:50, so
  the plan uses none of their numbers.
- The model has no copy of `promise`, `n_accepted` and `decided_idx`:
  they are durable variables (`model/tla/SequencePaxos.tla:238`).
  `accepted_idx` is a volatile variable (`:239`). Two invariants say
  that it equals the log length: `AcceptedIdxIsLogLength` (`:2481`) and
  `AcceptedIdxIsLogLen` (`:2493`).
- A key that is lost while the node runs is not found by the code at
  06a52bf: the node sends the value of its field
  (doc/review-plan-start-checks-4.md, finding 1; not checked again).

The former form of this section is in the git history at ebd9f42.

## 2. Change of the code

### 2.1 The storage is the only source

1. `sequence_paxos` has no field for promise, accepted round, decided
   index or log length, and no list of waiting ops. The four values
   come from the storage.
2. A step runs in one write transaction of the storage. The first call of a step
   begins it. Every read and every write of the step goes through it,
   so a read sees the writes of the same step. `commit` commits it
   once, and the messages of the step leave only after that commit,
   as now (`take_outgoing_messages`, `:459-463`). The writes in the
   middle of a step (`:591-595`, `:766`, `:1071`) go: they exist only
   so that a later read sees the write.
3. Between two steps, a read (the public getters, `node.hpp:127`,
   `:221`, `:374`, `:466`) runs in a read transaction and calls the step
   check of item 5 first, so no value leaves the node without the
   check. The write transaction of the log and the open write
   transaction of the state (2.6 item 5) stand in one thread. The owner
   keeps this form, with a test (2026-10-25 03:53; section 4).
4. In one transaction, the storage gives the log length as the last
   key plus one, and compares it with the count of entries through the
   continuity function of the start-checks plan (its item 2.1). When
   they differ, the read returns `missing_entry`.
5. The checks of `with` in the start-checks plan (its item 2.4), the
   state check of 2.2, the chain check of 2.3 and the mark of 2.3
   item 7 run at the first read of each step and again as its last read
   before the commit, not only at the start. One function makes them.
   A damage that they find makes the node stop voting. It then repairs
   its log from a peer (2.4) and runs the recovery of VR Revisited, as
   at a start (owner, 2026-10-24 19:21). The model of this is round 8.
6. A read that returns a damage code of the storage
   (plan-sqlite-storage.md 6.1) is a damage of item 5 (owner,
   2026-10-24 21:27). Every other read error of the storage throws a
   type derived from `std::system_error` (owner, 2026-10-24 14:39).
7. `cached_promise_message_` (`:728`) is built again from the storage on a
   resend (`:440`), not kept.
8. The storage concept (`storage.hpp:121-129`) changes from
   `write_atomically(ops)` and separate getters to a transaction that
   reads and writes. The type is `storage::transaction` (owner,
   2026-10-24 19:23); `konsens::transaction` (`node.hpp:70`) stays.
9. A write error of a step throws `write_error`, as now.
10. Every new error branch is `[[unlikely]]`.
11. The node reads no page back from the disk itself (owner, 20:50).

The former form of this section is in the git history at ebd9f42.

### 2.2 The state of an entry

1. An entry has a state field: `in_work` or `committed`. The names are
   provisional.
2. An entry is appended with `in_work`. Each step that raises the
   decided index from `o` to `d` sets `committed` on the entries with
   the keys `o` to `d - 1`, in the same transaction. These are all the
   writers of the decided index: the decide in `accept_decide`,
   `accept_sync` and `decide`, the majority of promises
   (`handle_majority_promises`), the end of recovery
   (`complete_recovery`), the build of a new node, the repair of 2.4 and
   the start in single-node mode. Each has a test.
3. The step check reads the entry with the key `d - 1`, which is
   `committed`, and the entry with the key `d`, which is `in_work`. With
   `d = 0` it reads only the second; with no entry at the key `d` it
   reads only the first.
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
   index, the log length and the chain CRC of the last entry, with one
   CRC over all of them. Now these are three keys
   (`src/persistent_storage.cpp:31-33`). The writers of the meta record
   are: the seed node at its first start, a node at the end of its
   recovery, a node built for a new configuration, the repair of 2.4,
   and every step that changes one of its values. The write of an entry
   and the write of the meta record are in one transaction. The check
   "a node with no meta record is damaged" starts after the first write
   of the node. A meta record whose CRC fails is a damage. Round 3 of
   the model showed why: a node with an empty log that loses its
   promise breaks S1, and no other check finds it. The owner chose this
   form, 2026-10-24 19:19.
3. The step check reads the last entry and the meta record in its
   transaction and compares the index and the chain CRC. This finds a
   lost last entry. A lost entry in the middle leaves both equal; the
   continuity check of 2.1 item 4 finds it.
4. The chain CRC of entry `k` stands for the prefix up to `k`. The model
   compares the prefix itself (`PrefixAt`, `VerifyCrc`). The code
   compares the stored chain CRC at `k`. A changed entry below `k` whose
   stored chain CRC at `k` stays the same (the old leaf page of item 5)
   is found by the model and not by the code. So the results of
   `VerifyCrc` hold only for faults that keep the stored chain
   consistent with the entries.
5. What no local check finds:
   - the disk gives back an older meta record with a valid CRC while
     the log stays (`stale_meta`);
   - older versions of the entries and the meta record together
     (`rollback`);
   - an older version of a leaf page in the middle of the log, with the
     same keys, entries of an earlier ballot and a valid page checksum.
   Model round 5 checks the first two. At a start, the recovery of VR Revisited covers
   them; while the node runs, items 6 and 7 cover part of the first two.
   The third is not modelled. The README states all three (owner,
   2026-10-24 19:19: the README warns).
6. Each node keeps, for each peer, the highest promise and the highest
   accepted index that the peer reported on its present connection, in
   memory. The connection keeps the order (owner, 2026-10-20: the
   election requires TCP). A new connection starts with no values. A
   message of the peer with a lower value makes the node refuse it and
   report the proof. This finds only a step back that a peer saw. The
   README says this, and recommends a drive with power-loss protection
   or with its write cache off (owner, 2026-10-24 19:19). The model of
   this check is round 7. It holds only with no loss and no duplicate
   (`ASSUME PeerCheck => MaxLoss = 0 /\ MaxDuplicate = 0`). It does not
   find the traces of round 5.
7. The state machine of each node keeps, in memory, the highest
   promise, the highest pair of accepted round and log length, and the
   highest decided index that its own transactions read or wrote (owner,
   2026-10-24 19:51). It is a check, not a source: each step reads the
   storage and compares. A value below the mark is a damage (2.1 item 5). A
   restart forgets the mark; the recovery covers that case. The repair
   of 2.4 sets the mark from the repaired state. A node of a new
   configuration starts with no mark. TigerBeetle keeps `commit_max` in
   memory with the invariant that it never decreases
   (src/vsr/replica.zig:445 at 0450c32). The model of this check is
   round 8, with no crash, wipe, resend, loss, reconfiguration, StopSign
   or forced single node. Round 9 adds them.

The former form of this section is in the git history at ebd9f42.

### 2.4 The repair from a peer

The owner chose this form, 2026-10-24 21:27. Items 1 to 7 hold for a
damage that the storage reads without error, such as a chain CRC that
differs. Item 9 holds for a damage code of the storage.

1. A node that found a damage sends a recovery request, as at a start.
2. A peer that votes and does not recover answers with its log length
   and its chain CRC at each index that the requester asks for. It reads
   them in one read transaction and runs the step check in it first. A
   peer whose check fails does not answer and repairs itself.
3. The requester finds the first index where its chain CRC differs, or
   where its log ends. It asks for the entries from that index to the
   end of the peer's log.
4. The peer sends them from one read transaction, after the step check.
5. The requester checks the chain CRC of each received entry against
   the chain CRC of the entry before it, before it writes. In one write
   transaction it cuts its log at the first differing index, writes the
   entries, writes the meta record with the promise and the decided
   index of the peer, and sets its mark from that state.
6. Then it runs the recovery of VR Revisited and votes again only after
   it.
7. With no peer that answers, the node stays out of the vote.
8. A node repairs itself automatically once after a damage that the
   checks of 2.1 item 5 find (owner, 2026-10-24 20:55 to 20:57). The
   count is durable. The operator then gets a notice and can allow one
   more automatic repair at the next damage. Without that, the node
   stays out of the vote at the next damage, reports the proof and waits
   for the operator. A restart, a crash, a sleep of the machine or a
   lost network is no damage: the recovery after each start has no
   limit. A damage that the start checks find counts too (owner,
   2026-10-25 03:54): commits of the storage are atomic, so with honest
   hardware a start finds no damage. The model has this as
   `StartDamageCounts` (round 14). The count stands in the node storage,
   which also holds the hidden identity of the node. No repair copies or replaces it, so a copy
   from a peer brings neither the count nor the identity of the peer.
   A rollback of the node storage can give back an older count; the
   README states this. The model keeps `repairs` outside the durable
   tuple, so no fault changes it. The model does not show the operator
   who lets a waiting node vote again: a node in `refused` stays there.
9. After a damage code of the storage, the node replaces the whole
   store with a full copy from the peer (plan-sqlite-storage.md 7.5).
   The requester syncs the copy and its directory, and checks the copy
   with the step check before the switch in the node storage (item 8).
10. A repair by entries (items 1 to 7) keeps the state when its applied
    entries agree with the decided prefix of the repaired log; the chain
    CRC at the applied index shows it. Else it also takes the state of
    the peer, in the form of item 9. When the kept state is ahead of the decided index of the peer,
    the node keeps it: its entries are decided (model `StateFollows`,
    `StateIsDecided`, round 13).

The former form of this section is in the git history at ebd9f42.

### 2.5 The vote

1. `accepted` carries the chain CRC of the accepted index. A `decide`,
   an `accept_decide` and an `accept_sync` carry the chain CRC of the
   decided index of the leader. The recovery response, `new_state` and
   `promise` carry the chain CRC of their last index and of their
   decided index.
2. The leader counts an `accepted` only when its chain CRC equals the
   chain CRC of its own log at that index. It decides only from such
   reports.
3. A follower raises its decided index to `d` only when its own chain
   CRC at `d` equals the one in the message. When they differ, that is a
   damage of 2.1 item 5 (owner, 2026-10-24 21:27).
4. `handle_majority_promises` raises the decided index to the highest
   decided index of the promises only when the chain CRC at that index
   in the promise equals its own after the sync.
5. With the disk cache of the drive on, a read can come from that cache
   and not from the medium. The README says this, as it says for a
   volatile cache now.

### 2.6 The state machine in the storage

1. The state of the state machine and the client table stand in the
   storage (owner, 2026-10-24 20:47). The applied index stands there
   too.
2. `apply` and `query` of the `state_machine` concept take a
   `storage::transaction`. The node answers a read in a read transaction
   of the storage.
3. A decided entry changes the state in a write transaction of the
   storage. It commits after the decide, so after the commit of the
   step that raised the decided index (Jepsen decision of 2026-10-17
   11:24, owner 20:29).
4. Execute-Verify: the leader and each follower run the work of an
   entry before the decide, in a write transaction of the state that
   they do not commit. The entry carries the result and its CRC. A
   user error aborts only a nested transaction, and the entry records
   the error as its result.
5. The leader proposes a batch before the decide of the batches before
   it. Each node runs the entries in the order of the log, on one write
   transaction of the state that holds the effects of the entries before
   them. When a decide covers only part of the batches that ran, the
   node aborts that transaction, runs the decided entries again in a
   new one, commits it, and runs the entries that are not decided again
   on top of it. A step still takes all the work that waits. Of three
   forms this one gave the lowest latency at the same throughput
   (median 21.5 ms against 33.5 to 33.9 ms for one batch in flight, at 20 ms
   round trip and 10000 requests/s, g++ 16 and clang++ 22; at 1 ms round
   trip 2.2 ms against 2.9 ms; bench/results/2026-10-24-231534Z-lmdb-
   repair-and-pipeline.txt). That bench ran one get and one put of a
   counter for each request, 64 B log entries, a decide simulated as
   send time plus round trip, no followers, and up to 64 batches in
   flight. At each partial decide it ran every batch that was not
   decided again, so one request can run up to that bound times. The
   bound is `max_in_flight` (provisional name), a member of the
   configuration struct. Not measured yet: the runs for each request,
   the processor time, and a costlier work of an entry; section 5
   measures them before the bound gets a default.
6. The client gets the result that stands in the decided entry. The run
   after the decide gives the same result, because the entry runs on the
   same state: the state after the entries before it in the log. At the
   commit of the state, the node compares the CRC of each result with
   the CRC in the entry. A difference is a damage of 2.1 item 5 (owner,
   2026-10-25 03:54): the node stops voting, reports the entry and both
   CRCs, and takes the state and the log from a peer by 2.4 item 9. It
   counts toward the automatic repair of 2.4 item 8.
7. A later `accept_sync` that cuts an entry (`sequence_paxos.hpp:804-810`)
   leaves no effect. The node that sent the work sees that the leader is
   gone and sends the work to the new leader again; the session and the
   request number keep it from a second run (owner, 2026-10-17). One
   rule covers every cut: before each run and each commit of the state,
   the node compares the chain CRC of the last entry that ran in the
   open state transaction with the chain CRC of its log at that index.
   When they differ, a write cut or replaced the log below the end of
   the open transaction, and the node aborts it. The writes that cut
   are `accept_sync`, the sync of the leader in
   `handle_majority_promises`, the repair of 2.4 items 5 and 9, and the
   recovery. The node then runs the entries after the applied index
   again. A run counts only in the ballot in which the node accepted
   the entry (model rounds 12 and 13, `StateCurrent`, mutation
   `NoAbortOnCut`).

The former form of this section is in the git history at ebd9f42.

### 2.7 Group commit

1. A step takes every request that waits when it begins. No step waits
   for more work.
2. While one step syncs, new requests wait for the next step. So the
   batch grows with the cost of the sync.
3. The bounded queue and the refusal of the backpressure decision stay
   as they are.

### 2.8 The README

The README names the offsite node: a node at a second site, so that
one site that fails does not take a majority of the copies. It states
the cases of 2.3 items 5 and 6 and of 2.5 item 5.

### 2.9 Not in this change

The wire changes in these places only: the CRC fields of 2.5 item 1,
the result in `accepted` (2.6 item 4), and the messages of the repair
(2.4). The file format changes through 2.2, 2.3 and 2.6, so the format
version goes up. No store of an older format exists outside the tests,
so the library refuses an older format and does not convert it. Item 10
of the start-checks plan ("no change to the file format") changes with
this.

## 3. Model

### 3.1 Change

- `accepted_idx` stays in `volatile`. `AcceptedIdxIsLogLen` checks in
  each run that it equals the log length; rounds 8 to 13 found no
  difference. It goes, with both invariants, in the change of the model
  that follows the go of the owner (owner rule of 19:38).
- `RunningStorageFault(i, kind)`: the durable state of a running node
  changes, with the kinds of the start-checks model (`missing_entry`,
  `missing_accepted`, `missing_promise`, `missing_decided`) and two
  more: `stale_meta` and `rollback` (2.3 item 5). A `missing_entry` at
  the last position is a lost last entry.
- `StepChecks` (Bool): the checks of 2.1 items 4 and 5 at each step. A
  damage they find makes the node leave `running`, as `stop_voting`
  does.
- `VerifyCrc` (Bool): the comparisons of 2.5, with the log prefix in the
  field `last` as a stand-in for the chain CRC.
- `ChainCheck` (Bool): the chain check of 2.3 item 3. With it, the step
  check finds a lost last entry.
- `MetaRecord` (Bool): the meta record of 2.3 item 2. With it, the step
  check finds a lost promise, accepted round or decided index, and a
  `stale_meta` whose log differs from the present log. It does not find
  a `stale_meta` with the same log, nor a `rollback`.
- The node takes its next step on the damaged state, because the model
  has no copy of it. `StepCheck` lets the node take one more step before
  the check finds the damage; the code checks before each step, so the
  model allows more than the code.
- Not modelled: the state field of 2.2, and the old leaf page of 2.3
  item 5. The model treats the state field as a function of the decided
  index; the tests of 2.2 item 2 check the writers.
- `HighWater` (Bool): the mark of 2.3 item 7. `HighWaterHolds` checks
  that no step without a damage falls below the mark.
- `RepairFromPeer` (Bool): a node that finds a damage while it runs
  takes the durable state of a node that votes and does not recover,
  and restarts with the recovery. With no such node, it leaves
  `running`. The model takes the whole state in one action; 2.4 takes
  only the entries that differ. The model sets the mark from the copy.
- `VerifyPromise` (Bool, round 9): the check of 2.5 item 4.
- `repairs` and `AutoRepairs` (round 9): the durable count of automatic
  repairs of 2.4 item 8, and its limit 1. `OperatorAllowsRepair(i)`
  resets the count. The cover `NoRepairLimitReached` must fail with two
  faults. It also fails when no peer can be the source, so it does not
  show by itself that the limit was reached.
- `VerifyPromise` has no model evidence: no run showed a promise whose
  verified decided index differs from the highest one, and the check
  compares with the log after the sync. It stays as a defence.
- `StateInStorage` (round 13): `applied[i]`, the entries of the committed
  state, and `in_txn[i]`, the entries that ran in the open state
  transaction. `StateRun` runs the next entry of the log, up to
  `MaxInFlight` entries before their decide. `StateApply` commits up to
  the decided index and keeps the rest. `StateAbort` and `StateCurrent`
  are the rule of 2.6 item 7. `StateFollows` gives both to every step
  of the protocol: a crash or a recovery aborts the open transaction, a
  wipe loses the state, a repair keeps or copies the state as 2.4 item
  10 says (`StateRepair` "copy"; "keep" leaves out the copy). Invariants:
  `StateIsDecided` (the committed state holds only decided entries, in
  order) and `OpenRunsOnLog`. Covers: `NoTwoInFlight`,
  `NoPartialDecide`, `NoCutOfOpen`. The mutation `NoAbortOnCut` must
  break `StateIsDecided`. A result is a function of the entries before
  it, so the same entries give the same result. The client table is
  part of the state and is not modelled apart. The state has no faults
  in the model.
- `EntryRunsOnOnePrefix` (round 12): a node that accepted in the ballot
  of a decided entry, and holds the same value at that index, holds the
  same entries before it. Review 3 found that this follows from S5.

### 3.2 Questions for the model

- Does a lost promise of a running node with an empty log break S2b?
  Round 1: yes (model/results/20261024T153400Z-lmdb-only-source-runs-1.md).
- Does a lost last entry of a running node break a safety property?
  Round 2: S1 breaks with `VerifyCrc` and no `ChainCheck` (`MC_r_entry`).
- Do `ChainCheck` and `VerifyCrc` keep S1 to S4 for every kind? Round 3:
  no, a lost promise with an empty log breaks S1. Round 4: yes, with
  `MetaRecord`.
- Is `VerifyCrc` still needed with `ChainCheck`? Round 4: no result
  changed; it stays for Execute-Verify.
- Do `stale_meta` and `rollback` of a running node break S1 to S4 with
  every check on? Round 5.
- Does a resend (`MaxTimeout = 1`) change round 4? Round 5.
- Do the mark and the repair from a peer keep S1 to S4 against
  `stale_meta` and `rollback`? Round 8: no violation in five simulations
  of 15 min (model/results/20261024T195900Z-lmdb-only-source-runs-8.md).
- Round 9: does `HighWaterHolds` hold, with `RunningFaults` FALSE, in
  the configs of the full model: crash, wipe, resend, loss, duplicate,
  a second `AcceptSync` in one ballot, reconfiguration, StopSign, forced
  single node, client sessions?
- Round 9: does a repair from a source with an undetected damage break
  S1 to S4 (`MaxStorageFault = 2`)?
- Round 9: does the decide at the majority of promises keep S1 to S4
  with `VerifyPromise`?
- Rounds 10 and 11: which form of `MismatchAction` keeps S1 to S4? With
  every check on, no mismatch is reached; without the high-water mark,
  S1 fails before a node acts on a mismatch. So the model does not tell
  the forms apart. "damage" stays because it uses the repair of 2.4;
  this choice has no model evidence.
- Round 12: does each node that accepted in the ballot of a decided
  entry hold the same entries before it?
- Round 13: does the committed state hold only decided entries, with
  batches in flight, a cut of the open transaction, a crash, loss and
  duplicates, faults with a repair, and client sessions with a resend?
  Does it fail without the abort on a cut?

### 3.3 Configurations and runs

Nodes {1, 2, 3}, Values {v1, v2}, MaxBallot 2, MaxLog 2, one fault,
except where round 9 names two. Rounds 12 and 13 take Values
{v1, v2, v3}, MaxLog 3 and `MaxInFlight` 2. With running faults, S5 and S2b do not
hold by definition of the fault, so these runs check S1 S2 S3 S4 and
the properties `S2Grows` `BuildOnlyFromRunning`. Each run is a
simulation of 15 min, at most three at a time. Results go to
`model/results/<time>-lmdb-only-source-runs-<n>.md` and are pushed
after each round.

### 3.4 What the model does not show

- That the code reads the storage as the model says. The tests of
  section 4 show it.
- The cost of the reads. Section 5 measures it.

The former form of this section is in the git history at ebd9f42.

## 4. Tests

- "a key lost while the node runs is found at the next step";
- "a last entry lost while the node runs is found by the chain check";
- "a promise lost while the node runs is found at the next step" for a
  log that is not empty;
- "a read in a step sees the writes of the same step";
- "no message leaves before the commit of its step";
- "a resent promise carries the stored values";
- "a page with a wrong checksum stops the vote";
- "a user error aborts only its nested transaction";
- "a write transaction of the log and an open write transaction of the
  state stand in one thread over many steps", with commits, aborts and
  a reader in each environment;
- "a result after the decide that differs from the entry stops the
  vote";
- "a damage that the start checks find counts toward the automatic
  repair";
- "a leader does not decide on an accepted with another chain CRC";
- "each writer of the decided index sets the state of its entries", one
  test for each writer of 2.2 item 2;
- "a getter between two steps runs the step check";
- "recovery compares the chain CRC, not the entry CRC";
- "a value below the mark stops the vote";
- "a repair takes only the entries that differ, and sets the mark";
- "a repair refuses an entry whose chain CRC does not follow";
- "the majority of promises does not decide on another chain CRC";
- "a state change commits only after the decide";
- "a read of the state comes from a read transaction".

## 5. Checks of the build

- g++ 16, image localhost/cxx-matrix-fuse, Release, all tests.
- counter_stress seeds 1 to 300 with `--cbor --power-loss 50`.
- The cost, by the rules of a measurement, before and after, in every
  build: the commit of a step, a read after a step, and the throughput
  for batch sizes 1, 16 and 256. No number is written before the run.
- The pipeline of 2.6 item 5 with followers, in every build: the runs
  for each request, the processor time and the latency, for a work of
  one counter put and for a costlier work, at several values of
  `max_in_flight`. The default of `max_in_flight` comes from this.

## 6. Provisional names

`RunningStorageFault`, `StepChecks`, `VerifyCrc`, `ChainCheck`,
`MetaRecord`, `HighWater`, `RepairFromPeer`, `PeerCheck`,
`VerifyPromise`, `StateInStorage`, `MaxInFlight`, `max_in_flight`,
`StateRepair`, `applied`, `in_txn`, `StateCurrent`, the node storage
(2.4 item 8), `repairs`, `AutoRepairs`,
`OperatorAllowsRepair`, `stale_meta`, `rollback`, `in_work`,
`committed`, meta record, chain CRC, step check, high-water mark. None
has a source outside this tree. In Kafka, "high watermark" names the
offset of the last committed record, so that word has another meaning
there. The owner decides the names.

## 7. Open questions for the owner

1. Decided, 2026-10-24 19:19: one meta record with one CRC (2.3 item 2).
2. Decided, 2026-10-24 19:19: for a `stale_meta` or a `rollback` while
   the node runs (2.3 item 5), the README warns, and the peers check
   (2.3 item 6).
3. Decided, 2026-10-24 19:21: a node that stopped voting while it ran
   repairs from a peer (2.1 item 5).
4. Decided, 2026-10-24 19:23: `storage::transaction` (2.1 item 8).
5. Decided, 2026-10-24 19:45: page checksums beside the CRC of each
   entry and the chain CRC. The former form of this decision is in the git
   history at ebd9f42. A checksum on each page with SQLite
   is an open point (plan-sqlite-storage.md section 9).
6. Decided, 2026-10-24 20:29: the state changes only after the decide,
   from the log; no second log (2.6 item 3).
7. Decided, 2026-10-24 20:47 and 20:50: the state machine and the client
   table stand in the storage (2.6); the node reads no page back from the disk
   itself (2.1 item 11). With this, the pointer rounding of review 2,
   finding 6, does not arise.
8. Decided, 2026-10-24 21:27: a follower whose chain CRC at `d` differs
   from the leader's has a damage (2.5 item 3).
9. Decided, 2026-10-24 21:27: a damage code of the storage is a damage
   that the repair fixes (2.1 item 6).
10. Decided, 2026-10-24 21:27: the repair takes only the entries that
    differ (2.4). Decided, 20:55 to 20:57: one automatic repair, then
    the operator (2.4 item 8). After a damage code of the storage, a
    full copy replaces the whole file (2.4 item 9).
11. Decided after the bench of 2026-10-24 23:15: batches in flight
    before the decide, with a run again after a partial decide (2.6
    item 5).
12. Decided, 2026-10-25 03:53: two write transactions, of the log and
    of the state, in one thread, with a test (2.1 item 3).
13. Decided, 2026-10-25 03:54: a damage that the start checks find
    counts toward the one automatic repair (2.4 item 8).
14. Decided, 2026-10-25 03:54: a result after the decide that differs
    from the entry is a damage, with the repair of 2.4 (2.6 item 6).

The owner, 21:27 and 21:30, left these four to the session: take what
the storage gives, and where it gives no direct form, work out three
proposals and test which is best. Items 9 and 10 take the form of the
storage. For item 11 the storage gives no form; three forms were
measured. For item 8
the model ran three forms (keep, damage, stop) in rounds 10 and 11. No
run reached a mismatch with every check on, and without the high-water
mark S1 failed before any node acted on one
(model/results/20261025T000000Z-lmdb-only-source-runs-11.md). So the
model does not tell the forms apart. "damage" stays, because it uses
the repair of 2.4.
