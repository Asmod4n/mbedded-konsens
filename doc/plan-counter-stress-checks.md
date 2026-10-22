# Plan: counter_stress checks S2b, S3 and S5, also on LMDB

Source: the open work of the state file: counter_stress checks only
S1, S2 and S7, and runs on memory only. The owner, 2026-10-22 11:09
(UTC+2): take the next steps, from easy to complex. This plan gets one
review before the build.

## 1. Facts checked in this session

- examples/counter_stress.cpp:516-541 `check_decided_logs` checks S7
  (the decided index never falls) and S1 (two nodes decide the same
  entry at one index). It builds `decided_history` from the decided
  suffix of each node.
- test/simulation.hpp:779-850 `check` holds S7, S1, S2, S2b, S3, S5 and
  the check of one execution, in the form of the model. S2b compares
  the log of a leader in Accept with the decided history up to the
  last entry decided in a round not above its leader round
  (`decided_ballot`). S3 asks a set of proposed entries. S5 compares
  the logs of two nodes with the same nonzero accepted round.
- counter_stress runs on `konsens::memory_storage` only
  (counter_stress.cpp:56, 344). A restart keeps the storage (353-365);
  a power loss restarts every node (367-376).
- test/test_simulation_lmdb.cpp:31-49 opens one `persistent_storage`
  per node in a temporary directory and reopens it at a restart.

## 2. Change

1. S2b, S3 and S5 in counter_stress, each in the form of
   test/simulation.hpp:779-850, with the name of the property in the
   failure text.
   - S2b: `check_decided_logs` records, for each new entry of
     `decided_history`, the accepted round of the node that decided
     it. A leader in Accept must hold the decided history up to the
     last entry with a round not above its leader round.
   - S3: the stress keeps the fields of each entry that a client call
     proposed (register and increment). Each entry of
     `decided_history` is in that set.
   - S5: two nodes with the same nonzero accepted round have logs where
     one is a prefix of the other.
2. A flag `--lmdb`: each node runs on `persistent_storage` in its own
   temporary directory; a restart and a power loss close the env and
   open it again. The stress becomes a template on the storage type;
   `main` picks the type from the flag. A run as root is refused by the
   library, so the flag works only as a user that is not root.
3. The default run, the output line and the exit codes stay as they
   are.

## 3. Tests

- Seeds 1 to 300: plain, `--cbor`, `--power-loss 50`, and `--lmdb`
  (with and without `--cbor`), as a user that is not root.
- One mutation for each new check, outside the tree: a leader that
  drops the last decided entry (S2b), a client entry with a changed
  payload (S3), a follower that accepts a different entry in the same
  round (S5). Each makes at least one of seeds 1 to 300 fail with the
  name of its property.
- Debug with ASan and UBSan, and Release; all tests.

## 4. Measurement

The run time of seeds 1 to 300, plain, before and after, in the result
of the build. No claim of speed is made.

## 5. Names

`--lmdb` after the library it selects. No new public name.
