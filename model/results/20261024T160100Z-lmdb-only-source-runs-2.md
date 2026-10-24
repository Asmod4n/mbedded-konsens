# LMDB as the only source: simulation runs, round 2

Model e50abc9: VerifyCrc compares the log prefix that a report carries
before a leader counts an accepted and before a node raises its decided
index. ChainCheck makes the step check find a lost last entry (plan
section 2.3). Tool: TLC 2026.10.20 (nightly jar), one worker,
`-XX:ParallelGCThreads=1`, `-simulate -deadlock -depth 60 -seed 7`,
15 min limit, three runs at a time on 4 processors. Runs from a copy of
model/tla outside the repository. Invariants S1 S2 S3 S4, properties
S2Grows BuildOnlyFromRunning. S5 and S2b do not hold under a running
fault by definition of the fault.

| Config | Kind | VerifyCrc | ChainCheck | States | Result |
|---|---|---|---|---|---|
| MC_r_entry | entry | TRUE | FALSE | 13272 | S1 violated |
| MC_r_entry_chain | entry | TRUE | TRUE | 5636174 | no violation, stopped by the limit |
| MC_r_entry_chain_noverify | entry | FALSE | TRUE | 5548140 | no violation, stopped by the limit |

- S1 (MC_r_entry): node 1 loses an entry after it reported it, and
  votes on. It later decides index 1 with v2, while nodes 2 and 3 have
  decided v1 at index 1. The earlier smoke run of the same config gave
  S2 with the same cause.
- With ChainCheck, the step check finds the lost entry before the node
  takes its next step, so the node stops voting.
- VerifyCrc did not change the result of the entry kind once
  ChainCheck is on. Round 3 asks the same for all kinds.
- A simulation without a violation is not a complete run.
