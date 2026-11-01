# Plan: a first start happens once

Source: the owner, 2026-11-01 (card "Lücke beim ersten Start jetzt
schließen?", answer "Ja, mit Test"). Base: main bb60fbb.

## 1. The fault

Jepsen, ParticleDB 1.1.4, issue 112: after misdirected writes the node
found no data, reported "starting fresh" and served an empty database.

The same start exists here. `kind_of_the_start` (`node.hpp:76-81`)
returns `start_kind::first` when the caller sets
`node_config::first_start` and the store holds no promise. A store file
that is deleted, truncated to 0 bytes or new holds no promise: `persistent_storage::open`
creates the schema and writes `meta_record{}` into an empty file
(`check_or_write_format`, `persistent_storage.cpp:1153-1166`). A file of zero bytes does not open as a database, and a damage retires
the node (`node.hpp:833-843`); those cases are safe. So a node
whose store was lost starts as a new first node when the caller leaves
`first_start` set. It votes with an empty log and no promise. That is
the start that the model forbids: only `Init` starts a seed node with no
recovery (`SequencePaxos.tla:1900`). The mutation `StartAsNewNode` is
that start and breaks S2b (model/results/20261024T131421Z-start-checks-runs-7.md).

doc/plan-start-checks.md item 6 already names the rule: a seed starts
with no recovery only when the store is new, "no meta key and no hidden
identity", and calls the form "no meta key" provisional until a record
outside the store exists. doc/plan-clean-stop.md question 1 names the
same case after a lost first sync.

## 2. The change

1. `node_storage` keeps one more record, `first_start_record`
   (provisional name): the pid of the node, with a CRC32C, in the form
   of `clean_stop_mark`. `get_first_start_record()` reads it,
   `write_the_first_start_record()` writes it. node.db is opened with
   `synchronous=FULL` (`persistent_storage.cpp:2088`). A record with a
   wrong size or CRC is an error of the read; `node::with` returns it
   and does not start, as it does for the mark. A damaged record never
   reads as a missing one.
2. `kind_of_the_start` takes the record as one more argument.
   - A record of another pid: the node storage belongs to another node,
     and the start retires, as a mark of another node does.
   - The caller sets `first_start` and a record exists: it throws
     `std::logic_error`, as it does today for a store with a promise.
     The message names both conditions.
   - A mark that agrees with the store still gives `clean` first, and a
     build still comes before both.
3. `node::with` reads the record before the kind. It writes the record
   as its last step that can fail, before `release_the_output`, for a
   start of kind `first` and for a start of kind `clean` that found no
   record. So a node storage of an older version gets the record at
   its next clean start, and an error of a step before it does not
   cost the directory its first start.
4. The library never deletes the record.

## 3. What this does not cover

- A loss of node.db and of the store together (a new disk, or a new
  data directory of konsensd) with `first_start` set looks like a true
  first start. Only a check against the peers (rule R, the hidden
  identity) finds that. The operator rule stays: a seed that gets a new
  disk loses the seed field first.
- A build start (configuration_id above 1 and no config_first_idx)
  ignores `first_start` and the record. A built node that loses its
  store builds again with an empty promise. This is the same loss on
  the build path, and it stays open.
- A crash after the record and before the first promise: the next start
  with `first_start` set is refused. The node has sent nothing; the
  operator deletes the directory and starts again. This refusal is safe.
- examples/three_nodes.cpp starts a stopped node again with
  `first_start` set and without `node::stop()` (`:80`, `:161`, `:474`).
  That throws today already; ctest runs only `three_nodes --help`.

## 4. Tests

In test_clean_stop:

- "a first start on a node storage that started before is refused, also
  with a new store": a new empty store and the old store, both with
  `first_start`, give `std::logic_error`; a start without the flag
  retires. The old store is the rule of before; the new store is the
  new rule. Against the code without the check, the new store does not
  throw (checked).
- "a first start writes the first start record, and a clean start
  writes it when it is missing".
- "a node storage with the first start record of another pid retires
  the node".
- "a damaged first start record stops the start": a wrong CRC gives
  `crc_mismatch`, a short record gives `malformed_record`.

No harness starts a directory twice with `first_start` (review 1,
item 11). test_clean_stop "a mark that does not agree with the store"
swaps two node storages and still retires.

## 5. Model

No model change. The model already starts a seed only in `Init`, and
`StartAsNewNode` shows what breaks without the rule. The change makes
the code hold an assumption of the model.

## 6. Checks

Release g++ 16.2.1 (Tumbleweed, podman): 32 of 32 tests. The CI runs
every build.

## 7. Review 1 and changes

A second agent reviewed version 1. No finding blocked. Taken: the
wording "deleted, truncated or new" (item 2); the build path in section 3
(item 4); the record is written last (item 5); a clean start writes a
missing record (item 7); the pid is read and a foreign pid retires
(item 8); a damaged record is an error (item 9); std::logic_error stays,
with a message that names both conditions (item 10); three_nodes.cpp in
section 3 (item 12); the tests of item 13 that apply to this form.
Not taken: a sync of the directory after node.db is made (item 6,
not checked); node.db already exists before the first start writes the
record, because `node_storage::open` made it.
