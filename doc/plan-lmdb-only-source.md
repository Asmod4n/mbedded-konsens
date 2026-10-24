# Plan: LMDB is the only source of the durable values

Source: the owner, 2026-10-24 14:44 and 14:45: "unsere einzige quelle
für daten ist lmdb", "wir antworten nie aus dem speicher", "wir sind
ein distributed log, nur das log kennt das log". The owner decided at
14:46 that this change comes before the start checks
(doc/plan-start-checks.md on branch start-checks).

Order of the work: this plan, then the model of section 3 and its TLC
runs, then one review of plan and model by a second agent, then the
go of the owner, then the code of section 2.

Provisional: the form "one read for each step" (section 2, item 2). It
waits for the answer of the owner to the card of 14:47.

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
- The model already has no copy of `promise`, `n_accepted` and
  `decided_idx`: they are durable variables
  (`model/tla/SequencePaxos.tla:210`). `accepted_idx` is a volatile
  variable (`:211`), and the invariant `AcceptedIdxIsLogLength` says
  that it equals the log length.
- A key that is lost while the node runs is not found by the code at
  06a52bf: the node sends the value of its field
  (doc/review-plan-start-checks-4.md, finding 1).

## 2. Change of the code

1. `sequence_paxos` has no field for promise, accepted round, decided
   index or log length. The four values come from the storage.
2. Provisional form, after the card of 14:47: at the first use in a
   step, the node reads the four values and the last CRC in one read
   transaction. The values of the step are these values with the
   waiting ops of the step applied, by one pure function of
   (stored values, waiting ops). `commit` writes the ops and drops the
   values of the step. Between two steps the node holds none of them.
   A write of the waiting ops in the middle of a step reads the values
   again after the write.
3. The storage gives the four values and the last CRC in one read, as
   one struct. The log length is the last key plus one. The read
   compares it with the count of entries through the continuity
   function of the start-checks plan (its item 2.1). When they
   differ, the read returns `missing_entry`. The struct name is
   provisional.
4. The checks of `with` in the start-checks plan (its item 2.4) run on
   every read of item 3, not only at the start. One function makes
   them.
5. Each public getter (`get_promise`, `get_accepted_round`,
   `get_decided_idx`, `get_accepted_idx`) reads the storage. It throws
   `std::logic_error` while ops wait, as `read_decided_suffix` does
   (`:500`).
6. `cached_promise_message_` (`:728`) is built again from the storage
   on a resend (`:440`), not kept.
7. Every new error branch is `[[unlikely]]`.
8. No change to the protocol, to the wire or to the file format.

## 3. Model

### 3.1 Change

- `accepted_idx` leaves `volatile`. Every use reads `Len(log[i])`.
  `AcceptedIdxIsLogLength` goes, because it holds by definition.
- New action `RunningStorageFault(i, kind)`: the durable state of a
  running node changes, with the kinds of the start-checks model
  (`missing_entry`, `missing_accepted`, `missing_promise`,
  `missing_decided`) and one more, `lost_last_entry`: the last key and
  the count both lose one entry, so the log is consistent and one
  entry shorter.
- `StepChecks` (Bool): TRUE models the checks of items 3 and 4 at
  each step. A damage they find makes the node leave `running`, as
  `stop_voting` does.
- The node takes its next step on the damaged state, because the model
  has no copy of it.

### 3.2 Questions for the model

- Does a lost promise of a running node with an empty log break S2b?
  The checks cannot find it.
- Does `lost_last_entry` of a running node break S2 or S2b? The
  checks cannot find it, because the stored log is consistent. At
  06a52bf the field `accepted_idx_` made the next append fail.
- Do the checks of items 3 and 4 find every other kind before it does
  harm?

### 3.3 Configurations and runs

As the start-checks runs: Nodes {1, 2, 3}, Values {v1, v2},
MaxBallot 2, MaxLog 2, one fault, invariants S1 S2 S2b S3 S4 S5 S6,
properties `S2Grows` `BuildOnlyFromRunning`. One config for each kind,
with and without `StepChecks`. Each run is a simulation of 15 min, at
most three at a time. Results go to
`model/results/<time>-lmdb-only-source-runs-<n>.md` and are pushed
after each round.

### 3.4 What the model does not show

- That the code reads LMDB as the model says. The tests of section 4
  show it.
- The cost of the reads. Section 5 measures it.

## 4. Tests

- "a key lost while the node runs is found at the next step";
- "a promise lost while the node runs is found at the next step" for a
  log that is not empty;
- "the values of a step include its waiting ops";
- "a getter while ops wait throws logic_error";
- "a resent promise carries the stored values".
The tests for the cases of section 3.2 follow the model results.

## 5. Checks of the build

- g++ 16, image localhost/cxx-matrix-fuse, Release, all tests.
- counter_stress seeds 1 to 300 with `--cbor --power-loss 50`.
- The cost of the read at each step: the bench cases of a commit and
  of a read, before and after, in every build, by the rules of a
  measurement. No number is written before the run.

## 6. Provisional names

`RunningStorageFault`, `StepChecks`, `lost_last_entry`, the struct of
item 3. The owner decides the names.
