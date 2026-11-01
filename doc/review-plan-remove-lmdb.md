# Review of the plan to remove LMDB

Reviewer: a second agent, 2026-10-30, at b0d00ab. Verdict: approve
after fixes.

- F1: bench/run.sh:144 and konsens_bench.cpp:2121 give a store
  directory to a case whose name matches *lmdb*. Name the cases.
- F2: run.sh runs the same case names on every arm; an older arm knows
  only the old names. run.sh:331-332 holds a decision that ends. All
  texts of run.sh change, not only the mount.
- F3: the four plans to delete hold decisions in force (ASan at -O1,
  the split of test_simulation, the error_fs control file, the
  converter). Move them into the state file first; ask the owner about
  the converter under SQLite. Rewrite plan-sqlite-storage.md:150-155.
- F4: OneEnvironment.tla:3 is missed.
- F5: run check_constants.py too; the *.base.cfg fault exists before.
- F6: StateInStorage, not StateInDatabase; file names with "storage".
- F7: 33 measure functions, 24 stay; table lines 2064-2075.
- F8: test_wire.cpp:66 and :78 record name sources; drop the LMDB ones.
- F9: write-path is another task.
- F10: the data list lacks the TLC output and the SANY log.
