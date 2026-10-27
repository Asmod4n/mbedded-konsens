# Review: plan-proofs.md (branch model-proofs)

2026-10-27. Read: plan-proofs.md, plan.md, SequencePaxos.tla (parts), the F6c
cfgs and results, power-loss-runs.md, the scratchpad Voting/Paxos/Consensus,
both rule files, mbedded-konsens.md, branch ble-liveness (/home/claude/mk-ble).
Not checked: Generalized Paxos (item 8), tlapm options (item 13). No run.

## Findings

1. Section 1, fact 4. Fault: the copy of Voting.tla holds no proof of
   `THEOREM Invariance`. Why: Voting.tla:178 has no proof steps. Only
   QuorumNonEmpty (:19-20) and the refinement to Consensus (:190-194) have
   steps. Fix: find and name the source of a TLAPS proof of Invariance
   before 3.1 starts.

2. Section 1, fact 1 ("No spec has fairness ... or BLE. Model L does not
   exist"). Fault: branch ble-liveness has this work. Why: /home/claude/mk-ble
   commit 9bd962f holds model/tla/BallotLeaderElection.tla with weak fairness
   (:298-303), a leads-to property (:315), 8 cfgs, and
   doc/plan-ble-liveness.md with one review. Fix: base 3.4 on that plan and
   module. Write the branch into the state file.

3. 3.3. Fault: the spec already answers F6c, and the plan does not cite the
   answer. Why: a recovering node drops each message except RecoveryResponse
   (SequencePaxos.tla:1556-1558). Recovery completes only with Majority
   answers from peers that do not recover (:760-762). When two of three nodes
   roll back, only one peer can answer. The two nodes never vote again, so S2
   cannot break. The F6c simulation file ("Reading") and the state file
   (mbedded-konsens.md:621-624) say this. The expected break occurs in the
   power-loss mode: p_rollback2 violated S2b
   (20261021T091016Z-power-loss-runs.md:12, :20). Fix: replace the directed
   run with a check of the reason, the invariant
   `\A k \in rolled_back : recovering[k]` on MC_F6c.cfg. Tell the owner that
   F6c keeps S2 and loses liveness, and that the break needs PowerLoss = TRUE.
   A run from one start state says nothing about other start states.

4. 3.3 and section 6 ("TraceStart, as in tla/power-loss"). Fault: this form
   is not in the tree. Why: tla/power-loss holds only cfg files. Only a result
   file names the wrapper TraceStart.tla (power-loss-runs.md:9), and the
   wrapper was not kept. Fix: use the TLC trace exploration form that is in
   the tree (`_init`, SequencePaxos_TTrace_1792578485.tla:126), or write the
   wrapper again and list it in section 8.

5. 3.2, mapping of votes and promises. Fault: the mapping cannot hold. Why:
   in Voting, votes only grow (VoteFor, Voting.tla:163) and maxBal only rises
   (IncreaseMaxBal, :145). The model overwrites log and n_accepted at each
   AcceptSync and Rollback. Wipe sets the promise to DefaultBallot
   (SequencePaxos.tla:1776). Paxos.tla takes votes from msgs, which no step
   removes (Paxos.tla:185-187). The channels of the model lose and consume
   messages. Fix: add history variables in a wrapper module that extends
   SequencePaxos.tla and conjoins their update to Next. Keep the set of
   <<ballot, log>> that each node accepted and its highest promise. Then no
   line of SequencePaxos.tla changes. Put the history into the VIEW
   (:4050-4061) or drop the VIEW. Otherwise TLC merges states that differ in
   their history.

6. 3.1 and 3.2, form of a vote. Fault: Voting allows one vote per ballot
   (Voting.tla:159). A follower accepts several logs at one ballot. A
   duplicated AcceptSync can also make that log shorter (plan.md:137-140).
   Fix: an abstract vote is a set of logs per ballot whose members are
   prefix-compatible. Do not keep only the longest log.

7. 3.2, scope. Fault: the first version excludes F4 and F6 but names MC_a_F4
   (MaxWipe = 1) and MC_F6b (MaxRollback = 1) as bounds. Fact 1
   (plan-proofs.md:24-26) says these runs used older forms of the spec. Other
   features break a fixed quorum: ForceSingle decides with one node
   (SequencePaxos.tla:2471-2482), a read-only node appends with no ballot
   (:1678-1681), and Majority follows ClusterNodes (:342). Fix: list the
   constants of the mapping runs (wipe, rollback, reconfiguration, single
   node, read-only and witness off). Run MC_base again on the current spec.
   Name the measure: distinct states and wall time of MC_base with and
   without the history variables.

8. 3.1, theorems. Fault: S2 is stated wrong. Why: S2 says each decided prefix
   of a voting node is a prefix of decided_history (SequencePaxos.tla:3607).
   "The decided history only grows" is S2Grows (:3648). In the abstract
   module, chosen grows by its form. Fix: state S1, "each decided prefix is
   chosen", and a refinement to a consensus on a log as in
   Voting.tla:188-194. Map S5 (:3671) to one log per prefix per ballot and S2b
   (:3655) to SafeAt. Read Lamport, "Generalized Consensus and Paxos"
   (MSR-TR-2005-33): there a vote is a c-struct, and sequences are c-structs.
   Take the names from there.

9. 3.1, step 2. Fault: TLC with Init == Inv cannot end. Why: TypeOK takes
   SUBSET (Ballot \X Log) per node. Ballots 0..2 and logs up to length 2 over
   2 values give 7 logs, 21 pairs, 2^21 sets per node, 2^63 for three. Fix:
   keep Apalache (step 3) as the inductive check. In TLC, draw TypeOK with
   RandomSubset (module Randomization), as in Lamport, "Using TLC to Check
   Inductive Invariance". Write the bounds and the initial count in the plan.

10. Section 4. Fault: a mutation that breaks only Inv shows that Inv is
    tight, not that S1 or S2 needs the rule. The mutation of 3.2 has no form
    in the model: each handler is one atomic durable write (plan.md:42-45).
    A changed mapping tests only the mapping. Fix: each mutation of 3.1
    breaks S1 or S2 in a run from Init. Add a quorum of f nodes. For 3.2, run
    the refinement on PromiseWithoutAccepted (MC_mut_S1), SyncIgnored
    (MC_mut_S2b) and SeqNumUnchecked (MC_mut_S5). Each must break it.

11. 3.2, F4 and F6. Fault: "comes back under rule R" has no abstract step.
    Why: a wiped node votes again only under a new incarnation through a
    decided configuration (plan.md:66-72). The abstract acceptors are fixed.
    F4 alone maps with the history (Votes(i), SequencePaxos.tla:417). After
    recovery the promise is Max(own, primary) (:1350). It can be below a
    promise that the rollback lost. Fix: state the obligation "a node never
    accepts below its highest earlier promise". Check it on MC_F6b before
    the mapping takes F6.

12. Section 7, order. Fault: TLAPS comes before the mapping. If the mapping
    finds a missing abstract step (items 5, 6, 11), the proof is done again.
    Fix: 3.1 steps 1-3, then 3.2 at small bounds, then 3.1 step 4.

13. 3.4 and section 5. Faults: (a) 3.4 leaves out plan.md 10.1 and 10.10,
    which override plan.md 3.2. (b) It leaves out the open items
    mbedded-konsens.md:824-825 (liveness of F6c, ballot reuse after a
    rollback with BLE). (c) Model S at 3 nodes, ballot 1, log 1 does not end
    (F6c runs). Model S with BLE at 5 nodes cannot end, and a stopped
    liveness run checks nothing. (d) "Bounds lowered for the next run" has
    no floor. (e) The load rule names only TLC; tlapm and Apalache start
    more threads. Earlier F6c runs shared the machine at load 4.2 to 4.8
    (mbedded-konsens.md:72). Fix: build model L from the module of item 2
    and a reduced log part, with no SYMMETRY and no VIEW. After three
    stopped runs on one question, ask the owner. Run one tool at a time,
    with at most 3 threads. Write the load before each run into the result.

14. Project rules. (a) SequencePaxosVoting is a made name ("No name is made
    here"). Mark it provisional, or take the name from item 8. (b) Section 8
    keeps only a reference to Voting.tla and Consensus.tla. "A grammar is
    read, not remembered" puts a specification into the tree verbatim, in
    the commit of its first use. Both are MIT. (c) Section 8 does not mark
    its items ("A plan ends with a list of its data"). Add the wrappers and
    TTrace files. (d) The owner set 5 min for build and test (2026-10-27);
    the plan uses 2 h runs. (e) The plan has no list of open questions.

15. 3.1 and 3.4, ballots. Voting takes Ballot == Nat (Voting.tla:27). Model S
    uses n * PidBase + p (SequencePaxos.tla:322). BLE uses (config_id, n,
    priority, pid) (plan.md:25). Fix: make Ballot a constant with a total
    order. Exclude StopSign, S4 and S6 by name in section 6. Minor: fact 1
    gives ballot 1 for MC_a_F4, but MC_a_F4.cfg has MaxBallot = 2.

## Questions for the owner (proposed)

- Q1 Close F6c with the check of item 3 (S2 holds, liveness is lost)? Yes.
- Q2 Name the abstract module from Generalized Paxos? Yes, provisional.
- Q3 Does the 5 min rule hold for model runs? No: at most 2 h per run.

## Verdict

Reject this version. The direction holds. Items 3, 5, 6 and 9 make steps fail
as written. Items 1 and 2 rest on wrong facts. A revised plan gets a review.
