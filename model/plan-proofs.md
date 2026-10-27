# Plan: proofs and the open checks of the model

The owner asked on 2026-10-27 (19:42) that the open checks of the model
are finished: the proofs, run F6c, and liveness (model L). The owner
allowed TLAPS for the proofs, from its origin and only for the model
(card, 19:49).

## 1. Facts checked in this session

1. An inventory of model/tla and model/results (2026-10-27) found:
   - No complete breadth-first run reaches the bounds of plan.md
     section 6. The largest complete runs use ballot 2 and log 2 with
     no fault (MC_base), or ballot 1 and log 1 with one fault each
     (MC_a_F1_dup, MC_a_F1_loss, MC_a_F3, MC_a_F4, MC_F6b).
   - S4 and S6 (reconfiguration), the properties of the single-node
     mode and of the witness lease, and every fault of the start
     checks, the store and Execute-Verify are checked only by
     simulation or by breadth-first runs that stopped.
   - F6c expects a violation of S2. Four stopped breadth-first runs,
     one simulation of 60 min and two Apalache runs found none.
   - No spec has fairness, a temporal property or BLE. Model L does
     not exist.
   - No spec has an inductive invariant, a THEOREM or a proof step.
   - The complete runs of 2026-10-16 to 2026-10-18 ran on older forms
     of SequencePaxos.tla. Only MC_base_monotone, MC_f and MC_F6b ran
     again later.
2. model/tla/SequencePaxos.tla has 4075 lines and 141 definitions.
3. TLAPS 1.6.0-pre (tlapm 5f5037f) runs here. A proof with SMT passes.
4. tlaplus/Examples has `specifications/Paxos/Voting.tla` by Leslie
   Lamport. It holds the TLAPS proof that each value that is chosen
   is the same (`THEOREM Invariance == Spec => []Inv`, with
   `Inv == TypeOK /\ VotesSafe /\ OneValuePerBallot`). It decides one
   value, not a log.

## 2. Why bounds cannot finish the proof

A breadth-first run is complete only for its bounds. At the bounds of
plan.md section 6, no run ended in 2 h, and the runs grow with each
feature of the model. A proof without bounds needs an inductive
invariant and TLAPS. Model checking stays the tool that finds faults
and the tool that shows a fault breaks a property (mutations, run G).

## 3. The form

### 3.1 Core: the voting of Sequence Paxos without bounds

- A new module `SequencePaxosVoting.tla` states what Sequence Paxos
  does in the words of Voting.tla: ballots, promises, accepted logs
  per ballot, and decided prefixes. A node accepts a whole log at a
  ballot, not one value. Faults that do not touch the durable state
  (loss, duplicates, reordering) are absent there, because the voting
  has no messages.
- Properties as theorems: S1 (decided prefixes agree) and S2 (the
  decided history only grows), for any number of nodes, ballots and
  entries.
- The inductive invariant follows Voting.tla: each accepted log at
  ballot b extends every log that is chosen at a ballot below b
  (`SafeAt` for logs), and one ballot has one log per prefix.
- Steps:
  1. Write the module and the candidate invariant `Inv`.
  2. TLC with `Init == Inv` and small bounds checks that `Inv` is
     inductive, and that each mutation of the module breaks it.
  3. Apalache checks `Init => Inv` and `Inv /\ Next => Inv'` for fixed
     sizes (3 nodes, ballot 4, log 3).
  4. TLAPS proves `Spec => []Inv` and `Inv => S1 /\ S2`.

### 3.2 Refinement: the model implements the core

- A refinement mapping from SequencePaxos.tla to
  SequencePaxosVoting.tla: votes are the accepted logs that are
  durable, promises are the durable promises, and the decided prefix
  is decided_idx.
- TLC checks `SequencePaxos!Spec => SequencePaxosVoting!Spec` under the
  mapping as a property, with the bounds of the complete runs that
  exist (MC_base, MC_a_F*, MC_F6b) and by simulation for the rest.
- A fault that ends voting (F4, a rollback in F6) maps to a node that
  leaves the voting and comes back under rule R or VR recovery. The
  first version maps only the runs without F4 and F6. F4 and F6 come
  next, as their own step.
- This links the proof of 3.1 to the 4075 lines of the model. The
  link is checked with bounds; the core is proved without bounds.

### 3.3 F6c

- F6c expects that S2 breaks when two of three nodes roll back. No run
  found the break. A directed run starts TLC from a state that the
  plan names (`TraceStart`, as in tla/power-loss): two nodes accepted
  and decided entry 1, then both roll back before a third node saw
  the decision.
- If TLC finds the break, F6c is confirmed as the limit of the defence,
  and the trace goes into the result file.
- If TLC proves that no break is reachable from that state, that is a
  finding about the model or about the expectation. It goes to the
  owner.

### 3.4 Model L (liveness)

- The form of plan.md section 3.2: model S plus BLE with priority in
  the ballot, a flag `stable` (GST), weak fairness on delivery,
  `hb_timeout` and `resend_message_timeout`.
- L1 and L2 as in plan.md section 4, with TLC. Runs start at GST from
  fixed initial states, with no symmetry. 5 nodes, one value, the
  three sets of edges of L2.
- TLAPS does not prove liveness in this plan.

## 4. Mutations

Each new property gets a mutation that must break it:

- 3.1: a node accepts at a ballot below its promise; a leader takes a
  log that is not the longest of the highest accepted ballot; two logs
  that differ at one prefix at one ballot. Each breaks `Inv` in TLC.
- 3.2: the mapping with a node that accepts without a durable write.
  It must break the refinement.
- 3.4: BLE without priority in the ballot, and a timeout that fires
  before replies arrive after GST. Each must break L1.

## 5. Runs

- TLC with 3 workers, one run at a time (the load rule). Each run has
  a time limit of 2 h; a run that reaches it is written down as
  stopped, with its bounds lowered for the next run.
- One result file per run, as plan.md section 7 states.
- TLAPS: each proof obligation proved or not, with the prover and its
  time. A proof file is checked again in full before each commit.

## 6. What this plan does not do

- No change of SequencePaxos.tla, except the refinement mapping and the
  TraceStart of F6c.
- No proof of liveness with TLAPS.
- No proof of Execute-Verify, the witness lease, the single-node mode
  or the start checks without bounds. They get a plan of their own
  after 3.1 and 3.2 hold.
- No Quint (stage 3 of plan.md stays open).

## 7. Order

1. 3.3 F6c (small, settles an old open run).
2. 3.1 core with TLC and Apalache, then TLAPS.
3. 3.2 refinement without F4 and F6, then with them.
4. 3.4 model L.

## 8. Data of this plan

- TLAPS in the session scratchpad (tlaps/dist), fetched again from
  its origin if lost; sha256 in the state file.
- Copies of Voting.tla, Paxos.tla and Consensus.tla in the scratchpad
  (tla-examples). The repository keeps only a reference: the origin
  URL and the commit.
- Result files in model/results.
