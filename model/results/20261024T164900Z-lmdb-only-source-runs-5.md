# LMDB as the only source: simulation runs, round 5

Model 378b61d: two new kinds of RunningStorageFault from review 1,
finding 4. `stale_meta`: the disk gives back an earlier meta record with
a valid CRC, and the log stays. `rollback`: the disk gives back earlier
versions of the log and of the meta record together. Both draw the
earlier state from `past_durable`. Every check on (StepChecks,
VerifyCrc, ChainCheck, MetaRecord). Tool and flags as round 2,
`-seed 7`, 15 min limit, three runs at a time on 4 processors.
Invariants S1 S2 S3 S4, properties S2Grows BuildOnlyFromRunning.

| Config | Kind | MaxTimeout | States | Result |
|---|---|---|---|---|
| MC_r_stale | stale_meta | 0 | 214585 | S1 violated |
| MC_r_rollback | rollback | 0 | 55229 | S1 violated |
| MC_r_meta_timeout | the four of round 4 | 1 | 5891635 | no violation, stopped by the limit |

- S1 (MC_r_stale): node 2 promises ballot 21 with an empty log. Its
  meta record goes back to the earlier one (promise 0); the log is
  empty before and after, so no check finds it. Node 2 then promises
  ballot 12 and decides v2 at index 1 with node 3. Nodes 1 and 3 later
  decide v1 at index 1 in ballot 21.
- S1 (MC_r_rollback): node 3 accepts v1 at index 1 from leader 2 and
  reports it. Its log and meta record go back to the empty state. Node
  3 then accepts v2 at index 1 from leader 3's ballot 23 with node 1,
  and they decide v2, while node 2 has decided v1.
- So a running node whose disk gives back an older consistent state
  breaks S1 with every local check on. The read-back of plan 2.4 finds
  such a loss only in the window between the commit and the read.
- A resend (MaxTimeout = 1) changed no result of round 4.
- A simulation without a violation is not a complete run.
