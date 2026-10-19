# Review of plan-split-20261019 (commit d231556)

Verdict: reject. Each question has a fault. In eleven questions the
fault stops the run or the mutation. Each fault has a small change
below.

The reviewer read SequencePaxos.tla, the cfgs MC_base_monotone, MC_c,
MC_d1, MC_d2, MC_w, MC_e, MC_f and all MC_mut_*, and the results in
model/results/. No TLC run was made for this review. A bound that is
marked "not checked" comes from the result files only.

Facts from the results that the findings use:

- a-F3 (one crash, MaxBallot 1, MaxLog 1): 55 M distinct states, 60 min.
- a-F1-loss (one loss, one timeout, MaxBallot 1, MaxLog 1): 86 M
  distinct states, 1 h 54 min.
- a-F1-duplicate (MaxBallot 1, MaxLog 1): 29 M distinct states, 32 min.
- e-crash, c-crash, d2-crash, d2-loss, lease-w, lease-loss: each
  stopped at the 1 h limit with states on the queue.
- f with two calls of handle_leader (MaxBallot 1, MaxLog 3): stopped
  after 20 min, the queue grew.

## Findings

1. Q1. Fault: three faults at MaxBallot 2, MaxLog 2 do not finish. One
   fault alone at MaxBallot 1, MaxLog 1 needs 32 min to 1 h 54 min. A
   lower bound by one does not help. The mutation S1 was shown only with
   DecidedIdxMonotone FALSE and MaxBallot 2. "Cap 2" changes nothing.
   Change: run three cfgs, one fault each, at MaxBallot 1, MaxLog 1,
   with a time limit of 2 h. Run the combination as simulation only.
   Run the S1 mutation with DecidedIdxMonotone TRUE before the run.

2. Q2. Fault: MaxTimeout 1 and MaxLoss 1 at MaxBallot 2, MaxLog 2 contain
   the bounds of a-F1-loss, which needed 1 h 54 min at MaxBallot 1. The
   mutation S2b (SyncIgnored) does not touch the resend path. Change:
   MaxBallot 1, MaxLog 1, limit 2 h. Use the mutation S5 (SeqNumUnchecked).
   Its cfg already has MaxLoss 1, and a loss is what the resend repairs.

3. Q3. Fault: a crash keeps the durable state. With a crash alone,
   Recovery FALSE is safe (a-F3), so no arm breaks. The mutation "R1" is
   LocalRead and needs ReadIndex TRUE. No mutation of the core recovery
   exists. Change: use MaxRollback 1 instead of MaxCrash 1, and leave out
   S7. The counter-arm is the same cfg with Recovery FALSE: F6a shows
   that it breaks S2. F6b at MaxBallot 1, MaxLog 1 did not finish in
   40 min, so mark the breadth-first run as an Apalache candidate.

4. Q4. Fault: as Q3, and a crash with a loss at MaxBallot 2 does not
   finish (a-F3 alone needed 60 min at MaxBallot 1). Change: merge Q4
   into Q3 as a simulation arm with MaxLoss 1.

5. Q5. Fault: E1 breaks only with two Request entries and one
   RegisterClient entry: three sends and three log entries. MaxClientSend
   2 and MaxLog 2 do not permit this, and MaxRequest 2 is not reachable
   with two sends. MaxElection 2 is already in the base. Change:
   MaxClientSend 3, MaxRequest 1, MaxLog 3, MaxBallot 1, as MC_mut_E1
   with MaxElection 2. Run f in this form stopped after 20 min, so mark it
   as an Apalache candidate if the 30 min run does not finish.

6. Q6. Fault: MaxLog 1 holds only the RegisterClient entry. No request
   is in the log, so E1 holds by default and the mutation cannot break
   it. Change: drop Q6, or keep MaxLog 3 and run it as simulation only.

7. Q7. Fault: not checked. Run e at MaxBallot 1, Values {v1} needed
   20 min. MaxBallot 2 and two values probably exceed 30 min. Change:
   take the bounds of MC_e (MaxBallot 1, Values {v1}). Add
   ReplyOnlyIfUnchanged to the properties.

8. Q8. Fault: the mutation ReadBeforeServing breaks ReplyOnlyIfUnchanged,
   its direct property (run 20261019T001622Z). "As Q7" does not name that
   property. e-crash at MaxBallot 1, MaxLog 1, Values {v1} did not finish
   in 1 h. Change: name ReplyOnlyIfUnchanged. Run Q8 as simulation only.

9. Q9. Fault: MC_c has MaxCrash 1, MaxWipe 1, MaxLoss 1. "No fault" is
   not what the cfg says. The stop sign takes one log entry. With MaxLog
   1, no entry can follow it, so the mutation S4 (AppendAfterStopSign)
   cannot break S4. Change: MaxCrash 0, MaxWipe 0, MaxLoss 0, MaxLog 2,
   Values {v1}, as MC_mut_S4. Add SYMMETRY and VIEW, as the base has.

10. Q10. Fault: c-crash (MaxLog 1, Cap 1, one crash) did not finish in
    1 h. Cap 2 is larger. BuildOnlyFromRunning, the direct property of
    BuildFromRecovering, is not named. Change: name BuildOnlyFromRunning.
    Run Q10 as simulation only.

11. Q11. Fault: MC_d1 has MaxReadOnly 0. EnterReadOnly needs
    read_only_entries < MaxReadOnly, so ReadOnlyProposes never runs.
    ReadOnlyWritesNothing, its direct property, is not in MC_d1. With
    ReadIndex FALSE, R1, EachReadSeesDecided, StaleReadIsMarked,
    ReplyOnlyIfUnchanged and ReadStartOnlyWhenServing hold by default.
    Change: MaxReadOnly 1, add ReadOnlyWritesNothing, remove the five read
    properties.

12. Q12. Fault: MC_d2 with MaxLog 1 is MC_d2_log1. It keeps one crash,
    one wipe and one loss. d2-crash and d2-loss with one fault each and
    Cap 1 did not finish in 1 h. Change: MaxCrash 0, MaxWipe 0, MaxLoss 0,
    Cap 1. Keep Recovery TRUE and MaxHalt 2.

13. Q13. Fault: not checked. MC_w did not finish in 1 h (29 M distinct
    states, 19 M on the queue). MaxRead 0 removes the reads but keeps
    MaxElection 3. StaleReadAfterForce and ReadRoundHonoursLease hold by
    default with no read. Change: also MaxElection 2, and remove the two
    read properties.

14. Q14. Fault: "no wait for a second leader" is no switch of the model.
    The plan says that the model does not change. The mutation
    ReadWithoutLease was shown only with MaxElection 3, and an election of
    the forced node also counts in elections. Change: remove that clause.
    Run the mutation at MaxElection 2 first. If it does not break
    ReadRoundHonoursLease, keep MaxElection 3 and lower MaxRead or
    LeaseLength instead.
