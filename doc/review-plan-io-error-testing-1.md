# Review of plan-io-error-testing.md, round 1

Verdict: approve with changes. All changes are applied in the plan.

- The library opens SQLite in one place with the default VFS. Some
  calls bypass the VFS (::open, the sync by the caller). The default
  mmap size of the distribution's build is not checked.
- The simulation has no path for a node that throws: a raise ends the
  seed. A node that throws must be handled as a crash; this needs a
  change of test/simulation.hpp. Each node has two directories.
- The expectations were too loose. They lacked read_error, the value of
  a full disk, the logic_error after a failed commit, and the range
  confirmed, present, attempted. "Stops voting" is not an allowed end of
  an I/O error.
- Finding: a failed sync of the WAL at a commit comes back as a value
  and becomes write_error; the storage can commit and sync again.
- The sweep reaches the write paths only; the plan must say which
  branches it cannot reach.
- Missing: a checkpoint scenario, node_storage, xGetLastError, atomic
  counters in pAppData, temporary files, a fixed seed with no check
  threads, the owner's consent for a second long run.
