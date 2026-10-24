# LMDB as the only source: breadth-first run, round 6

Model 378b61d, config MC_r_meta.cfg (every check on, the four kinds of
round 4, SYMMETRY ValueSymmetry, VIEW View). Tool: TLC 2026.10.20
(nightly jar), breadth-first, `-workers 3 -deadlock`, `-Xmx8g`,
`-XX:ParallelGCThreads=3`, limit 60 min. Invariants S1 S2 S3 S4,
properties S2Grows BuildOnlyFromRunning.

Result: stopped after 45 min at depth 15 because the disk of the
session was full (the queue of TLC held 13 GB). 82979480 states
generated, 26585474 distinct, 11714785 left on the queue. No violation
up to that point. The run is not complete.

- The state space of this config does not fit the disk of the session.
  A complete run needs smaller bounds (for example MaxBallot 1 or one
  kind for each run) or a machine with more disk.
