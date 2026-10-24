# LMDB as the only source: simulation runs, round 3

Model e50abc9, as round 2. Tool and flags as round 2, `-seed 7`.
Invariants S1 S2 S3 S4, properties S2Grows BuildOnlyFromRunning.

| Config | Kinds | VerifyCrc | ChainCheck | States | Result |
|---|---|---|---|---|---|
| MC_r_chain | all four | TRUE | TRUE | 436505 | S1 violated |
| MC_r_chain_noverify | all four | FALSE | TRUE | 48341 | S1 violated |
| MC_r_promise_chain | promise | TRUE | TRUE | 618268 | S1 violated |

All three traces have one cause:

1. Node 1 promises a ballot of node 2 or 3 while its log is empty.
2. Node 1 loses its promise while it runs. With an empty log, no check
   finds it.
3. Node 1 becomes leader of a lower ballot with one other node and
   decides v1 or v2 at index 1.
4. The other majority decides the other value at index 1.

The chain CRC does not cover the promise. Round 4 models one meta
record that holds every durable value with one CRC, written when the
node is created (MetaRecord, plan section 2.3).
- A simulation without a violation is not a complete run.
