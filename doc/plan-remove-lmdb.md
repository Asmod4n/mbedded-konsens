# Plan: LMDB is removed from mbedded-konsens

Source: the owner, 2026-10-30 18:53 ("lmdb bitte aus allen streichen"),
18:55 (card "Alles": old plans are rewritten too, measurement results
stay as they are), 18:56 (only mbedded-konsens). SQLite replaced LMDB
on 2026-10-26 (doc/plan-sqlite-storage.md). The code has no LMDB
dependency: no submodule, no link, no include (git grep on main
ebd9f42 over CMakeLists.txt, .gitmodules and the sources).

Base: main ebd9f42. Branch: remove-lmdb. Each step below is one commit.

## 1. What stays as it is

- bench/results/* and model/results/*, their text and their file names.
  They are the records of what was measured and checked.
- bench/arms/*.patch: each one is the code of an arm of a result.
- The history of git. A deleted plan stays readable there.
- Branch names. The owner deletes branches; step 8 lists them.
- In the state file, each wrong assumption stays whole (rule "A state
  file holds what is in force"). An entry that names LMDB keeps its
  words.

## 2. The bench program

bench/konsens_bench.cpp: nine of the 33 measure functions end in `_lmdb`
(measure_commit_lmdb, measure_get_entries_lmdb, measure_read_lmdb,
measure_read_lmdb_threads, measure_write_batch_lmdb,
measure_write_batch_lmdb_threads, measure_write_batch_size_lmdb,
measure_write_lmdb, measure_write_lmdb_threads) and their `_ops`
constants. They measure persistent_storage, which is SQLite. The suffix
goes: measure_commit, measure_get_entries, ... No name collides with the
other 24 functions or the other `_ops` constants (review). The rename
goes through clangd in the build container. The case names in the table
(:2064-2075, the check at :2098) lose the suffix in the same commit.
A case gets its store directory today when its name matches `*lmdb*`
(bench/run.sh:144, the usage text at konsens_bench.cpp:2121). Both
places name the cases that need a store explicitly. bench/run.sh: the
mount /lmdb and the directory lmdb become /storage and storage, and the
texts at :17, 37-38, 49, 136, 142, 316-333 and 399 change with them.
The decision at run.sh:331-332 ("the names stay until the two stores
are compared") ends by the owner's word of 2026-10-30 18:53. An arm
built before this commit knows only the old case names, so it is not
comparable through run.sh; run.sh says so.

## 3. Comments of the tests

test/test_client.cpp, test_persistent_storage.cpp,
test_sequence_paxos.cpp, test_wire.cpp: a comment that cites a plan
cites its new file name (step 5). A comment that states an LMDB fact
(overflow pages at test_persistent_storage.cpp:595) says the fact of
SQLite in its place, or the sentence goes where it holds no fact of the
current code. test_wire.cpp:66 keeps "ISO SQL transaction" as the
source of the name transaction and drops "LMDB MDB_txn"; the entry for
`mdb_txn` at :78 goes, because src has no mdb_txn.

## 4. The TLA+ model

model/tla: the constant StateInLmdb becomes StateInStorage in
SequencePaxos.tla, APA_F6c.tla and every MC_*.cfg. The source of the
word is the code (persistent_storage, storage.hpp) and the model, which
says "storage" 49 times (RunningStorageFault). Comments that cite
plan-lmdb-only-source.md cite the new name. Comments that say "LMDB"
for the store say "the storage". OneEnvironment.tla:3 changes too. No
language server for TLA+ is in an image (not checked further), so this
is a text change; the diff is read before the commit. Check:
model/tla/check_constants.py over every cfg, SANY parses
SequencePaxos.tla, and TLC runs MC_base.cfg with the bounds of its last
run, and its result equals that run. The five *.base.cfg files are
checked against SequencePaxos.base.tla (the model at 5ea2ff9), which has
no StateInLmdb; check_constants.py reports that already before this
plan, and the plan does not count it as new.

## 5. Plans and reviews in doc/

- Deleted, because their subject is an LMDB mechanism:
  plan-hide-lmdb.md, plan-lmdb-converter.md, plan-lmdb-submodule.md,
  plan-simulation-on-lmdb.md. Before the deletion, each decision in
  force in them goes into the state file: from
  plan-simulation-on-lmdb.md:128-130 and :169 ASan once before each
  merge at -O1, the split of test_simulation, the error_fs control
  file; from plan-lmdb-converter.md:3-6 the converter decision, which
  waits for the owner (card of 2026-10-30) because its form under
  SQLite is a design question. plan-sqlite-storage.md:150-155, which
  cites the deleted files, is rewritten to name the rules that stay.
- Renamed, and LMDB replaced by the storage where the decision holds:
  plan-lmdb-only-source.md -> plan-storage-only-source.md (with its
  three reviews), plan-state-in-lmdb.md -> plan-state-in-storage.md. A
  sentence that describes only an LMDB mechanism (MDB_REMAP_CHUNKS,
  page checksums, mdb_env_copyfd2) goes, and the section says once that
  the LMDB form is in the history before this commit.
- Every other plan, review and doc/storage-fault-matrix.md: each
  mention is replaced by the database or by SQLite, or the sentence
  goes where it holds no fact in force.
- doc/img/start.svg, write.svg: "LMDB commit" becomes "SQLite commit".

## 6. model/plan.md

Same as step 5. README.md has no mention.

## 7. The state file

Claude-state mbedded-konsens.md: each decision in force names the
database, not LMDB. Section 1 holds. The shortened file is compared
with its old text before the push.

## 8. List for the owner

The branches whose names carry lmdb (hide-lmdb, lmdb-converter,
lmdb-only-source, lmdb-single-source, lmdb-submodule and others on
origin), for the owner to delete or keep.

## 9. Check

`git grep -i lmdb` outside step 1 finds nothing. The build of the bench
program and the tests whose comments changed compile; a comment change
needs no test run.

Open work outside this plan: the branch write-path has files that main
does not have (doc/plan-write-path.md); its LMDB mentions change on that
branch as a task of its own.

## Data that the plan leaves on the disk

The worktree wt-lmdb in the scratchpad (made again by git worktree),
the build directory of the bench program (made again by a build
command), the TLC output of the MC_base run (states/ and trace files)
and the SANY log (made again by a TLC run). All of it is deleted after
the owner sees the list.

Review: doc/review-plan-remove-lmdb.md, approved after fixes F1-F10,
which this text carries.
