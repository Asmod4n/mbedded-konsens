# LMDB as the only source: simulation runs, round 4

Model aba4ba1: MetaRecord adds one meta record with one CRC over every
durable value, written when the node is created. Tool and flags as
round 2, `-seed 7`, 15 min limit, three runs at a time on 4
processors. Invariants S1 S2 S3 S4, properties S2Grows
BuildOnlyFromRunning.

| Config | Kinds | VerifyCrc | ChainCheck | MetaRecord | States | Result |
|---|---|---|---|---|---|---|
| MC_r_meta | all four | TRUE | TRUE | TRUE | 5415476 | no violation, stopped by the limit |
| MC_r_meta_noverify | all four | FALSE | TRUE | TRUE | 5279891 | no violation, stopped by the limit |
| MC_r_promise_meta | promise | TRUE | TRUE | TRUE | 5468614 | no violation, stopped by the limit |

- With the chain CRC and the meta record, the step check finds each
  modelled fault of a running node before its next step. The node then
  stops voting. The runs show that the stop of one node keeps S1 to S4.
- VerifyCrc did not change a result with ChainCheck and MetaRecord on.
  It stays in the plan for Execute-Verify: it compares the results of
  the nodes, which no local check sees.
- Not modelled: a disk that gives back an older version of both the
  entries and the meta record while the node runs.
- A simulation without a violation is not a complete run.
