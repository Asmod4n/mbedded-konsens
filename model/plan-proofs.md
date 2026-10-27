# Plan: proofs and the open checks of the model

Version 3, after review 1 (model/review-plan-proofs.md) and review 2
(model/review2-plan-proofs.md, approved with changes).

The owner asked on 2026-10-27 (19:42) that the open checks of the model
are finished: the proofs, run F6c, and liveness (model L). The owner
allowed TLAPS for the proofs, from its origin and only for the model
(card, 19:49).

## 1. Facts checked in this session

1. An inventory of model/tla and model/results (2026-10-27) found:
   - No complete breadth-first run reaches the bounds of plan.md
     section 6. The largest complete runs use ballot 2 and log 2 with
     no fault (MC_base), or ballot 1 and log 1 with one fault each
     (MC_a_F1_dup, MC_a_F1_loss, MC_a_F3, MC_F6b), and ballot 2 with
     log 1 (MC_a_F4).
   - S4 and S6 (reconfiguration), the properties of the single-node
     mode and of the witness lease, and every fault of the start
     checks, the store and Execute-Verify are checked only by
     simulation or by breadth-first runs that stopped.
   - No spec on this branch has an inductive invariant, a THEOREM or a
     proof step.
   - The complete runs of 2026-10-16 to 2026-10-18 ran on older forms
     of SequencePaxos.tla. Only MC_base_monotone, MC_f and MC_F6b ran
     again later.
2. model/tla/SequencePaxos.tla has 4075 lines and 141 definitions.
3. TLAPS 1.6.0-pre (tlapm 5f5037f) runs here. A proof with SMT passes.
4. tlaplus/Examples (commit 53955cf, MIT licence):
   - `specifications/Paxos/Voting.tla` states
     `THEOREM Invariance == Spec => []Inv` with no proof steps
     (Voting.tla:178). Only the refinement to Consensus is proved.
   - `specifications/byzpaxos/VoteProof.tla` holds the voting
     algorithm of Paxos with a full TLAPS proof. With
     `specifications/byzpaxos/Consensus.tla` beside it, tlapm 5f5037f
     proves all 809 obligations here in 77 s of wall time
     (`--threads 3`, session VM, 2026-10-27).
5. F6c is answered by the spec. A recovering node drops each message
   except RecoveryResponse (SequencePaxos.tla:1554-1558). Recovery needs
   answers from a majority of nodes that do not recover
   (SequencePaxos.tla:758-762). When two of three nodes roll back, one
   node can answer, so neither node votes again. S2 cannot break; the
   cluster decides nothing more. The state file says this since
   2026-10-17 (mbedded-konsens.md:621-624). The break that F6c expected
   occurs in power-loss mode (run p_rollback2 violates S2b,
   model/results/20261021T091016Z-power-loss-runs.md:12).
6. Model L exists on branch ble-liveness (9bd962f): reviewed plan
   doc/plan-ble-liveness.md, model/tla/BallotLeaderElection.tla with
   weak fairness and a leads-to property, and 8 cfgs. It has no result
   file yet.

Wrong assumptions of version 1 (kept for the state file): version 1
said Voting.tla holds the proof of `Invariance`; it holds none. It said
model L does not exist; it exists on ble-liveness. It said F6c is
open; the spec answers it (fact 5). It named a TraceStart form in
tla/power-loss; that form was never kept in the tree.

## 2. Why bounds cannot finish the proof

A breadth-first run is complete only for its bounds. At the bounds of
plan.md section 6, no run ended in 2 h, and the runs grow with each
feature of the model. A proof without bounds needs an inductive
invariant and TLAPS. Model checking stays the tool that finds faults
and the tool that shows a fault breaks a property (mutations, run G).

## 3. The form

### 3.1 Core: the voting of Sequence Paxos without bounds

- Step 0 (done, fact 4): tlapm proves VoteProof.tla of tlaplus/Examples
  here, in full.
- Step 0a: TLC checks the invariant `LogExtendsOwnVotes` on the
  model: at its ballot n_accepted, the log of a node extends each log
  it held before at that ballot. Review 2 found no path where a
  duplicate AcceptSync makes a log shorter (FIFO, and AcceptSync is
  taken only in Follower/Prepare, SequencePaxos.tla:1129, :2131-2138).
  If the invariant fails, the work stops, and the trace goes to the
  owner as a question about safety.
- A new module `MultiPaxosVoting.tla` states what
  Sequence Paxos does in the words of VoteProof.tla. A vote is a log,
  not a value, and it is keyed by n_accepted. The log of the leader is
  its own vote (SequencePaxos.tla:1082-1084). Only nodes that vote
  count in a quorum. The abstract vote at ballot b is the set of all
  logs that the node held at b; each two of them are prefix-compatible.
- A log is chosen at ballot b when a quorum of voting nodes voted at b
  for logs that each extend it.
- Ballots are an abstract ordered set. The model maps its ballots
  n*10+p onto it (SequencePaxos.tla:321-322).
- Properties as theorems, with the names of SequencePaxos.tla:
  - The main theorem: each two chosen logs are prefix-compatible.
  - S1 (SequencePaxos.tla, `S1`) and S2 (`S2`) follow from it, with
    the lemma that each decided prefix is chosen. This lemma is
    checked by TLC at bounds in 3.2 and is not proved by TLAPS.
  - S2Grows (`S2Grows`, SequencePaxos.tla:3648): `decided_history`
    only grows.
- The inductive invariant takes the log forms of VInv2, VInv3 and
  VInv4 of VoteProof.tla (:429-612): each log that a node votes for at
  ballot b is safe at b, and the logs of one ballot are
  prefix-compatible. The proof that SafeAt is stable is written again
  for logs.
- Steps:
  1. Write the module and the candidate invariant `Inv`.
  2. Apalache checks `Init => Inv` and `Inv /\ Next => Inv'` for fixed
     sizes (3 nodes, ballot 4, log 3). TLC draws states of `TypeOK`
     with RandomSubset and checks `Inv /\ Next => Inv'` on them, as in
     Lamport, "Using TLC to Check Inductive Invariance". TLC with
     `Init == Inv` over all of `TypeOK` cannot end and is not run.
  3. TLAPS proves `Spec => []Inv` and `Inv => S1 /\ S2`.

### 3.2 Refinement: the model implements the core

- A wrapper module extends SequencePaxos.tla. It adds history
  variables: the set of logs each node voted for at each ballot (only
  grows), and the highest promise each node ever gave (only rises).
  The model overwrites the log, n_accepted and, at a wipe, the
  promise; the history variables keep what the abstract module needs.
  The wrapper has no VIEW, or a VIEW that holds the history variables.
- TLC checks the abstract `Spec` under the mapping as a property.
- First scope, as constants: no crash, no wipe, no rollback; MaxLoss
  1, so that the mutation MC_mut_S5 can break the mapping; no
  Execute-Verify; no ForceSingle; no read-only appends; no
  reconfiguration and no StopSign (S4 and S6 are not in scope); no log
  copy of VR recovery. Configs are written new for this scope; the old
  results of MC_a_F4 and MC_F6b do not count, because they ran on
  older forms of the spec.
- The first run measures what the history variables cost: the number
  of initial states and of distinct states with and without them.
- Then F4 and F6. They add one obligation: a node never accepts at a
  ballot below the highest promise it ever gave, also after rule R or
  VR recovery (a recovered node can hold a promise below the lost one,
  SequencePaxos.tla:1350). This obligation is a property of its own,
  checked before the mapping. It holds for nodes that vote only,
  because a wipe sets the promise to 0 (SequencePaxos.tla:1776). It
  can fail with 5 nodes and a promise in flight (compare
  MC_rr_inflight_n5). If it fails, the work stops, and the trace goes
  to the owner.
- Reconfiguration, ForceSingle and read-only appends change the
  quorum. They need their own plan after the first scope holds.
- 3.2 runs at small bounds before step 3 of 3.1, so that a fault in the
  abstraction shows before the proof is written.

### 3.3 F6c

- A new invariant on MC_F6c.cfg: each node that rolled back stays
  recovering (`\A k \in rolled_back : recovering[k]`), checked with the
  existing bounds by simulation for 15 min and breadth-first to its
  time limit. A mutation through a cfg override
  (`RecoveryComplete <- ...`) shows that the invariant can break; the
  spec does not change. Apalache can show that the invariant is
  inductive.
- The result goes to the owner as question Q1: F6c keeps S2 and loses
  liveness.

### 3.4 Model L

- This plan runs the reviewed plan of branch ble-liveness
  (doc/plan-ble-liveness.md) and adds nothing to its model.
- The liveness of F6c and the reuse of a ballot after a rollback with
  BLE get a plan of their own after model L has results.

## 4. Mutations

Each mutation breaks a property in a run from `Init`, not only `Inv`:

- 3.1: a node accepts at a ballot below its promise (breaks S1); a
  leader takes a log that is not the longest of the highest accepted
  ballot (breaks S2); a quorum of f nodes instead of f+1 (breaks S1).
- 3.2: the existing mutations PromiseWithoutAccepted (MC_mut_S1),
  SyncIgnored (MC_mut_S2b) and SeqNumUnchecked (MC_mut_S5),
  SequencePaxos.tla:3434-3438, each break the refinement.
- 3.3: a rolled-back node that votes after an answer from one node
  (breaks the invariant of 3.3).

## 5. Runs

- TLC, Apalache and tlapm each with 3 threads, one of them at a time
  (the load rule).
- By day a run ends within 15 min. At night a run has a time limit of
  2 h (plan.md section 7), and the night runs start and end with no
  question to the owner (Q3). A run that reaches its limit is
  written down as stopped. After three stopped runs on one question,
  the question goes to the owner with the bounds that were tried.
- One result file per run, as plan.md section 7 states. It records the
  number of threads of the run.
- TLAPS: each proof obligation proved or not, with the prover and its
  time. A proof file is checked again in full before each commit.

## 6. What this plan does not do

- No change of SequencePaxos.tla. The wrapper of 3.2 extends it.
- No proof of liveness with TLAPS.
- No proof of Execute-Verify, the witness lease, the single-node mode
  or the start checks without bounds.
- No Quint (stage 3 of plan.md stays open).

## 7. Order

1. 3.1 step 0a (`LogExtendsOwnVotes`) and 3.3 F6c.
2. 3.1 steps 1 and 2.
3. 3.2 first scope at small bounds.
4. 3.1 step 3 (TLAPS).
5. 3.2 with F4 and F6.
6. 3.4 model L on branch ble-liveness.

## 8. Questions to the owner

- Q1: close F6c as "S2 holds, liveness is lost" (fact 5)? Decided:
  yes (owner card, 2026-10-27 20:10).
- Q2: the name of the abstract module. Decided: `MultiPaxosVoting`
  (owner card, 2026-10-27 20:10).
- Q3: the time limit of one model run. Decided (owner, 2026-10-27
  20:11, 20:12): from 06:00 to 22:00 only runs that end within 15 min; longer runs at
  night, with no question to the owner. The clock is the local time
  of the owner (assumed: Central European Time, UTC+1, so the night is
  21:00 to 05:00 UTC).

## 9. Data of this plan

- TLAPS in the session scratchpad (tlaps/dist): made again by the
  download in the state file (URL and sha256). Deleted after the plan.
- Copies of tlaplus/Examples files in the scratchpad (tla-examples):
  deleted after they are in the tree. The tree keeps the files that a
  module uses verbatim in refs/tlaplus-examples/, with an ORIGIN file
  (URL, commit 53955cf, MIT licence), in the commit of the first module
  that uses them.
- Result files in model/results: recorded in the state file.
