# Plan: one small run for each question

Source: the request of the owner of 2026-10-19 06:13 ("kleinteiliger
den Proof erarbeiten"). The large cfgs combine several features and
several faults. Their breadth-first runs end at the time limit with no
result. This plan splits them into small cfgs. Each cfg asks one
question. The model does not change; each cfg sets the switches that
exist (Recovery, ClientSessions, ReadIndex, SingleNode, Witness,
MaxReconfigure) and the fault bounds.

## 1. Rule for each run

- Each cfg starts from MC_base_monotone (3 nodes, Values {v1, v2},
  MaxBallot 2, MaxLog 2, MaxElection 2, ValueSymmetry, VIEW View) and
  changes only the lines that its question needs.
- First a simulation of 15 min (`-simulate -deadlock -depth 60`).
- Then a breadth-first run where the table says BFS, with `-deadlock
  -coverage 1` and the limit of the table.
- A question counts as checked only by a complete breadth-first run.
  The result file names its bounds.
- Each question has a mutation that breaks its property in the same
  cfg (simulation, 15 min). Existing mutations are used.
- One run at a time, 3 workers.

## 2. Questions

This version applies the review `model/review-plan-split-20261019.md`
(reject, one small fix per question). Earlier results show that one
fault at MaxBallot 1 and MaxLog 1 already needs 30 min to 2 h
breadth-first (a-F3 55 M states in 60 min, a-F1-loss 86 M in 1 h 54
min). So a breadth-first question has one fault, MaxBallot 1, MaxLog 1
and a limit of 2 h. A question that needs more is a simulation only
and an Apalache candidate; its result file says so.

Order: first what library v0 needs, then v0.1, then v1.

| Id | Question | Cfg | Mode | Properties | Mutation | For |
|---|---|---|---|---|---|---|
| Q1a | Core with one crash | MaxBallot 1, MaxLog 1, MaxCrash 1 | BFS 2 h | S1 S2 S2b S3 S5 S7 | S1 with DecidedIdxMonotone TRUE | v0 |
| Q1b | Core with one loss | MaxBallot 1, MaxLog 1, MaxLoss 1 | BFS 2 h | as Q1a | S5 (SeqNumUnchecked) | v0 |
| Q1c | Core with one duplicate | MaxBallot 1, MaxLog 1, MaxDuplicate 1 | BFS 2 h | as Q1a | S5 | v0 |
| Q1d | Core with crash, loss and duplicate | MaxBallot 2, MaxLog 2, all three faults | simulation | as Q1a | S1 | v0 |
| Q2 | Resend after a timeout | MaxBallot 1, MaxLog 1, MaxTimeout 1, MaxLoss 1 | BFS 2 h | as Q1a | S5 | v0 |
| Q3 | Recovery after a rollback | MaxRollback 1, Recovery TRUE (counter-arm: Recovery FALSE must break S2) | BFS 2 h, then Apalache candidate | S1 S2 S2b S3 S5 | counter-arm | v0.1 |
| Q3s | Recovery with a rollback and a loss | Q3 and MaxLoss 1 | simulation | as Q3 | counter-arm | v0.1 |
| Q5 | Client sessions with a leader change | ClientSessions TRUE, MaxClientSend 3, MaxRequest 1, MaxLog 3, MaxBallot 1, MaxElection 2 | BFS 2 h, then Apalache candidate | E1 and the core properties | E1 | v0.1 |
| Q6 | Client sessions with a crash | Q5 and MaxCrash 1 | simulation | as Q5 | E1 | v0.1 |
| Q7 | Read index with a leader change | the bounds of MC_e | BFS 2 h | R1 ReplyOnlyIfUnchanged and the core properties | R1 | v0.1 |
| Q8 | Read index with a crash | Q7 and Recovery TRUE, MaxCrash 1 | simulation | ReplyOnlyIfUnchanged R1 | ReadBeforeServing | v0.1 |
| Q9 | Reconfiguration, no fault | MC_c with MaxCrash 0, MaxWipe 0, MaxLoss 0, MaxLog 2, Values {v1}, SYMMETRY and VIEW | BFS 2 h | S4 S6 and the core properties | S4, S6 | v1 |
| Q10 | Reconfiguration with a crash | Q9 and MaxCrash 1 | simulation | BuildOnlyFromRunning S6 | BuildFromRecovering | v1 |
| Q11 | Stage 1 | MC_d1 with MaxReadOnly 1, ReadIndex FALSE, without the five read properties, with ReadOnlyWritesNothing | BFS 2 h | the write properties of d1 | ReadOnlyProposes | v1 |
| Q12 | Stage 2 | MC_d2 with MaxLog 1, MaxCrash 0, MaxWipe 0, MaxLoss 0, Cap 1 | BFS 2 h | the properties of d2 | ForceWithoutStage1 | v1 |
| Q13 | Witness lease, writes only | MC_w with ReadIndex FALSE, MaxRead 0, MaxElection 2, without the read properties | BFS 2 h | the write properties of MC_w | DecideWithoutLease | v1 |
| Q14 | Witness lease, reads | MC_w; the mutation runs first at MaxElection 2, else MaxElection 3 with MaxRead 1 | simulation | ReadRoundHonoursLease StaleReadAfterForce | ReadWithoutLease | v1 |

Q1a to Q1c replace the old runs a-F3, a-F1-loss and a-F1-duplicate,
which ran on an older model.

## 3. What this plan does not do

- No change to the model.
- No liveness and no BLE. Both need model L.
- No Apalache. It comes after this plan, for a question whose
  breadth-first run does not finish at the lowered bound.

## 4. Load

The runs start after the builds of library v0. While two build agents
run, the load is already four.
