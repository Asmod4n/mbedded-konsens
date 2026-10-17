# mbedded-konsens

mbedded-konsens is a consensus library in C++. It follows the form of
Omni-Paxos. It has three parts:

- Sequence Paxos replicates the log.
- Ballot Leader Election elects the leader.
- StopSign changes the configuration.

The library does no network IO. A separate binary does the IO. The
library keeps its state in an internal store.

## Current state

The repository holds formal models in TLA+ only. There is no C++ code
yet.

## Layout

| Path | Content |
|---|---|
| `model/plan.md` | The plan of the models: properties, faults, bounds and the decisions of the owner. |
| `model/tla/` | The TLA+ model of Sequence Paxos and the TLC configuration files. |
| `model/results/` | One file for each TLC run, with the bounds, the result and the trace of a violation. |
| `refs/omnipaxos/` | A verbatim copy of the Omni-Paxos source that the models follow, with its license and origin. |
| `refs/papers.md` | References to the papers that the models use. |

## License

Apache License 2.0. See `LICENSE`.
