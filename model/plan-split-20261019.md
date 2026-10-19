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
- Then a breadth-first run with a limit of 30 min and `-deadlock
  -coverage 1`. If it does not finish, the run is repeated once with
  one bound lowered by one (MaxLog, then MaxBallot), and the result
  file says which bound.
- A question counts as checked only by a complete breadth-first run.
  The result file names its bounds.
- Each question has a mutation that breaks its property in the same
  cfg (simulation, 15 min). Existing mutations are used.
- One run at a time, 3 workers.

## 2. Questions

Order: first what library v0 needs, then v0.1, then v1.

| Id | Question | Changes to MC_base_monotone | Properties | Mutation | For |
|---|---|---|---|---|---|
| Q1 | Core with one crash, one loss, one duplicate together | MaxCrash 1, MaxLoss 1, MaxDuplicate 1, Cap 2 | S1 S2 S2b S3 S5 S7 | S1 | v0 |
| Q2 | Core with message timeout and resend | MaxTimeout 1, MaxLoss 1 | S1 S2 S2b S3 S5 S7 | S2b | v0 |
| Q3 | Recovery after a crash | Recovery TRUE, MaxCrash 1 | S1 S2 S2b S3 S5 S7 | R1 (mutation form of plan-fixes) | v0.1 |
| Q4 | Recovery after a crash with a loss | Recovery TRUE, MaxCrash 1, MaxLoss 1, MaxLog 1 | as Q3 | as Q3 | v0.1 |
| Q5 | Client sessions with a leader change | ClientSessions TRUE, MaxClientSend 2, MaxRequest 2, MaxElection 2 | E1 and the core properties | E1 | v0.1 |
| Q6 | Client sessions with a crash | Q5 and Recovery TRUE, MaxCrash 1, MaxLog 1 | as Q5 | E1 | v0.1 |
| Q7 | Read index with a leader change | ReadIndex TRUE, MaxRead 1, MaxLog 1 | R1 and the core properties | R1 | v0.1 |
| Q8 | Read index with a crash | Q7 and Recovery TRUE, MaxCrash 1 | as Q7 | ReadBeforeServing | v0.1 |
| Q9 | Reconfiguration, no fault | MC_c with MaxLog 1 | S4 S6 and the core properties | S4, S6 | v1 |
| Q10 | Reconfiguration with a crash | Q9 and MaxCrash 1 | as Q9 | BuildFromRecovering | v1 |
| Q11 | Stage 1 | MC_d1 with MaxLog 1, ReadIndex FALSE | the properties of d1 | ReadOnlyProposes | v1 |
| Q12 | Stage 2 | MC_d2 with MaxLog 1, ReadIndex FALSE | the properties of d2 | ForceWithoutStage1 | v1 |
| Q13 | Witness lease, writes only | MC_w with ReadIndex FALSE, MaxRead 0 | the write properties of MC_w | DecideWithoutLease | v1 |
| Q14 | Witness lease, reads | MC_w with MaxElection 2, MaxLog 1, no wait for a second leader | ReadRoundHonoursLease, StaleReadAfterForce | ReadWithoutLease | v1 |

Q1 replaces the old runs a-F1-loss, a-F1-duplicate and a-F3. These ran
with one fault each, on an older model.

## 3. What this plan does not do

- No change to the model.
- No liveness and no BLE. Both need model L.
- No Apalache. It comes after this plan, for a question whose
  breadth-first run does not finish at the lowered bound.

## 4. Load

The runs start after the builds of library v0. While two build agents
run, the load is already four.
