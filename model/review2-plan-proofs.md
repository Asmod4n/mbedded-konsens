# Review 2: plan-proofs.md (branch model-proofs, ecf5048)

2026-10-27. Read: plan-proofs.md (version 2 and its later commits 6e28bd1 to
ecf5048), review-plan-proofs.md, plan.md, SequencePaxos.tla (parts named
below), MC_F6c.cfg, MC_mut_S1/S2b/S5.cfg, MC_a_F4/F6b/base.cfg, VoteProof.tla
in the scratchpad, plan-ble-liveness.md and BallotLeaderElection.tla
(ble-liveness, 9bd962f), both rule files. No run. Not checked: Voting.tla
(trusted from review 1), Generalized Paxos.

## 1. Findings of review 1

| # | State | Why |
|---|-------|-----|
| 1 | fixed | VoteProof.tla named and proved here (fact 4). |
| 2 | fixed | 3.4 builds on ble-liveness. |
| 3 | fixed | 3.3 checks the reason; cites :1554-1558, :758-762 (correct). |
| 4 | fixed | The directed run and TraceStart are gone. |
| 5 | fixed | Wrapper with history variables; VIEW named. |
| 6 | fixed | Vote is a set of prefix-compatible logs. See 2.1 on the premise. |
| 7 | partly | Scope by feature, not by constants. No rerun of MC_base, no measure of the history cost. |
| 8 | not | S1, S2 are still theorems of the abstract module; S2 is misquoted; no map of S5, S2b. See 2.3. |
| 9 | partly | Bounds named; no initial count; ballots of the model are sparse (2.6). |
| 10 | fixed | 3.1 mutations break S1/S2 from Init; 3.2 names three that exist. |
| 11 | partly | Obligation stated; no cfg named; wrong for a wiped node (2.4); 3 nodes cannot show it (2.5). |
| 12 | fixed | Order: 3.1 steps 1-2, 3.2, then TLAPS. |
| 13 | partly | (b) moved to the BLE plan, but model L cannot show it (2.8). The load is not written into each result. |
| 14 | fixed | Name decided by the owner (Q2). refs/ verbatim. Data marked. Q3 decided. |
| 15 | not | No Ballot constant with an order. StopSign, S4, S6 not excluded by name. Fact 1 still gives ballot 1 for MC_a_F4; MC_a_F4.cfg has MaxBallot = 2. |

## 2. New findings

2.1. 3.1, "a duplicate AcceptSync can make the log shorter". Fault: the premise
is not shown, and the plan does not say what follows from it. Why: plan.md:137-140
says a duplicate AcceptSync lowers decided_idx (S7), not the log. Channels are
FIFO and Duplicate appends the copy at the tail (SequencePaxos.tla:2131-2138).
A shorter AcceptSync is sent before the AcceptDecide that the node later holds,
so its copy arrives before any later Prepare; HandleAcceptSync acts only in
(Follower, Prepare) (:1129). I found no path to a shorter log. If one exists,
a Promise reports the shorter log at the same n_accepted (:1039-1043), and the
new leader can drop a chosen entry. Fix: before 3.1 step 1, check in the
wrapper `LogExtendsOwnVotes`: the log of each node extends every log in its
history at n_accepted. If it holds, the vote at b is one log that grows
(simpler abstraction). If it fails, it is a safety question for the owner.

2.2. 3.1, "the set of all logs that the node held at b". Fault: "at b" is not
defined. Why: between Promise and AcceptSync, promise = b but the log belongs
to n_accepted < b (:1051-1058). The leader also votes: HandleMajorityPromises
sets its log and n_accepted (:1082-1084). Fix: key the history by n_accepted'
and record the leader. Record only nodes with Votes(i) (:417). Define the
abstract `chosen` for logs: l is chosen at b when each node of a quorum held a
log that extends l at b.

2.3. 3.1, theorems and step 3. Fault: S1, S2 and S2Grows use decided_idx and
decided_history (:3605-3607, :3648, :1449-1460). The abstract module has neither.
"Inv => S1 /\ S2" cannot be stated there. S2 is "prefix of", not "compatible
with" (:3607). Fix: the abstract theorem is "chosen logs are pairwise
compatible" (VT3 form, VoteProof.tla:932). Add a model lemma "each decided
prefix is chosen", checked by TLC with the mapping. Section 2 then says what
is unbounded: the abstract algorithm only. The mapping stays bounded.

2.4. 3.1, sketched invariant. Fault: it is not inductive. Why: "each vote at b
extends every log chosen below b" is a consequence. A node with maxBal <= c
can still vote at c and make a conflicting log chosen. VoteProof closes this
with VInv2 (SafeAt), VInv3 and VInv4 (maxBal < b => no vote at b),
VoteProof.tla:429-612. Fix: state Inv as the log forms of VInv2, VInv3, VInv4
with TypeOK. Write SafeAt for logs and prove its stability (SafeAtProp,
:273) again; the proof of VoteProof is not reused as it stands.

2.5. 3.2, F4 and F6 obligation. Faults: (a) Wipe sets promise to 0 (:1776); a
wiped node then accepts below its old promise, so the obligation fails with
no fault of safety. (b) After VR recovery, promise = Max(own, primary)
(:1350). With 3 nodes the recovery majority is both peers, and it holds every
promise that counted; the obligation cannot fail there. With 5 nodes a
Promise in flight can be counted after the node took a lower promise. (c) A
Promise that no leader counts can make it fail while S1 holds. Fix: restrict
to Votes(i); name the cfg (MC_F6b, and a 5-node cfg); say that a failure goes
to the owner with S1/S2 of the same run, as an open safety question.

2.6. 3.1 step 2 and 3.2. Fault: ballots are n * PidBase + p (:321-322). "Ballot
4" does not match. SafeAt is recursive over all ballots below b, and TLC
evaluates it at each step of the property check. Fix: Ballot a constant set
with its order; the mapping uses the ballots in use (finding 15).

2.7. 3.2 first scope. Fault: no constant list. MC_mut_S5 needs MaxLoss = 1;
Recovery = TRUE with MaxCrash > 0 replaces the log by the log of the primary
(:1356-1359), a new vote with no abstract step; ExecuteVerify makes the vote
the prefix up to AccBound (:646), not the log. Fix: list each constant of the
mapping cfgs, as review 1 item 7 asked.

2.8. 3.3 and 3.4. Faults: (a) the 3.3 mutation needs a new Mutation value in
RecoveryComplete (:760), but section 6 forbids a change of SequencePaxos.tla.
(b) Model L step 1 has no log and no rollback (plan-ble-liveness.md:54-84);
it cannot show F6c. (c) 3.4 adds a section to a reviewed plan on another
branch. Fix: (a) override RecoveryComplete in the cfg (`<-`) in a wrapper.
(b) put the liveness of F6c into the composed step (plan-ble-liveness.md
section 4) and say so. (c) that plan change gets its own review.

2.9. 3.1 step 2, tools. Fault: Apalache runs from the scratchpad, not from a
package. The owner allowed TLAPS only (fact, card 19:49). The state file
lists Apalache as open (mbedded-konsens.md:850). Fix: ask the owner (Q4).

2.10. 3.3. Note: the invariant can be shown inductive with Apalache in one
step (with recovering peers silent, :1556). That gives a result with no
bound on the run length, which the 15 min runs do not.

## 3. Questions (proposed)

- Q4: Apalache from its origin for the model, as TLAPS? Proposal: yes.
- Q5: if `LogExtendsOwnVotes` (2.1) or the promise obligation (2.5) fails,
  stop and report before the proof? Proposal: yes.

## 4. Verdict

Approve with changes. The direction and the order hold. 2.3 and 2.4 must be
fixed before 3.1 step 1; 2.1, 2.2 and 2.7 before 3.2; 2.5 before step 5 of
the order; 2.8 (a) before step 1; Q4 before Apalache runs.
