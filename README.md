# mbedded-konsens

mbedded-konsens is a consensus library in C++. It follows the form of
Omni-Paxos. It has three parts:

- Sequence Paxos replicates the log.
- Ballot Leader Election elects the leader.
- StopSign changes the configuration.

The library does no network IO. A separate binary does the IO. The
library keeps its state in an internal store.

## Design

- The library takes C++ types only. It has no C API and starts no
  threads.
- An entry counts as stored only after a synchronous commit to disk.
- After each start, a node votes only after f+1 answers.
- Reads go through a read index. An operator can enable a stale read
  for clients.
- A single-node mode starts only by a flag at the start of the process.
  Stage 1 reads only. Stage 2 reads and writes at one node.
- A wrong use of the public API does not compile. What the compiler
  cannot check is checked at run time. A use that neither can check is
  not offered.
- Stored memory grows with the active state, not with the number of
  changes.

## Current state

The repository holds formal models in TLA+ only. There is no C++ code
yet.

- TLC checks safety under message loss, duplicate messages, a crash
  and a disk rollback, at lowered bounds.
- The model found that the Omni-Paxos source lets `decided_idx` fall
  after a late Promise. The fix keeps the higher value. It is reported
  upstream.
- Each property has a mutation of the model that makes TLC find a
  violation.
- Reconfiguration and single-node mode are checked by simulation only.

## Layout

| Path | Content |
|---|---|
| `model/plan.md` | The plan of the models: properties, faults, bounds and the decisions of the owner. |
| `model/tla/` | The TLA+ model of Sequence Paxos and the TLC configuration files. |
| `model/results/` | One file for each TLC run, with the bounds, the result and the trace of a violation. |
| `model/results/logs-*/` | The raw logs of a batch of runs. |
| `refs/omnipaxos/` | The repository and the commit of the Omni-Paxos source that the models follow. |
| `refs/papers.md` | References to the papers that the models use. |

## License

Apache License 2.0. See `LICENSE`.
