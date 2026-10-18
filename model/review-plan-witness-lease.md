Verdict: reject

# Review: plan-witness-lease

Source: `model/plan-witness-lease.md`, the model
`model/tla/SequencePaxos.tla`, the newer working model
`fix20261018/SequencePaxos.tla` in the scratchpad of the session, the
cfgs of run d, and the state file `mbedded-konsens.md`. TLC was not run.
No claim below is checked by a run. Line numbers refer to
`model/tla/SequencePaxos.tla`.

The form of the lease in section 2 of the plan is safe for decisions in
the old cluster. The faults are in the model plan: two checks block or
miss the forced node, the read check is in the wrong action, two
mutations cannot break their property, and the run of MC_w stops at a
violation that the plan already expects. A new version of the plan
needs a second review.

## Findings

1. **The base model is not named.**
   Place: header, section 6.
   Fault: `model/tla/SequencePaxos.tla` (09:21) is older than the
   working model of plan-fixes-20261018 (scratchpad, 13:39). The newer
   model has `build_source`, six direct properties, `ReadStartBeforeServing`
   and `EachReadSeesDecided` with `~reads[r].stale`. "The state counts
   of the last runs" come from the newer model.
   Change: name the base file and its commit, and the result files
   whose counts the regression must match.

2. **The lease check blocks the forced node.**
   Place: section 3, "HandleAccepted and LeaderCountsOwnAccepted
   (decide only with a valid lease)".
   Fault: LeaderCountsOwnAccepted requires `ClusterNodes(i) = {i}`
   (line 1590). In MC_w only node 6 meets it. It calls HandleAccepted
   (line 1598). Node 6 holds no lease. With the check in either action,
   node 6 never decides. Then NoConcurrentWriters and WaitHonoured hold
   with no case. ForcedWithoutWait cannot break NoConcurrentWriters.
   Change: put the lease check only in the decide branch of
   HandleAccepted (line 816), for `i \in Nodes`. Put the wait only in
   LeaderCountsOwnAccepted, for `i \in NextConfigNodes`. The coverage of
   MC_w must show both branches above zero.

3. **The wait misses a part of the decisions of the forced node.**
   Place: section 3, "the decide steps of the forced node"; section 4,
   "the first decision of the forced node".
   Fault: ForceSingle sets `decided_idx[s]` to
   `Min(decided_idx[i], Len(l))` and `accepted_idx[s]` to `Len(l)`
   (lines 1621-1622). Node 6 then decides the undecided suffix of
   `first_log` through LeaderCountsOwnPromise and
   LeaderCountsOwnAccepted, with no new proposal. If "decides" means
   `decided_idx[s] > Len(first_log[s])`, as in NoTwoWriters
   (line 1919), this decision passes the wait. It can conflict: the
   old cluster can decide a different entry at the same index.
   Change: a decision of node 6 is each rise of `decided_idx[s]` above
   its value at ForceSingle. The wait guards each such rise. WaitHonoured
   and NoConcurrentWriters use this definition.
   Question for the owner: SingleNodeServes (line 448) lets node 6
   serve reads at once after ForceSingle when `first_log` is decided.
   Proposal: node 6 serves no read before the wait ends.

4. **The read check is in the wrong action.**
   Place: sections 2 and 3, ReadReply.
   Fault: ReadReply (line 1511) runs at `reads[r].node`. That is often
   a follower. A follower holds no lease. So a follower read never
   completes, or the check reads the time of another node. The read
   index is fixed in StartReadRound (line 1490). A read can be placed
   at that step: read start <= round start < lease end <= first
   decision of node 6.
   Change: the lease check goes into StartReadRound, for
   `i \in Nodes`. ReadReply stays as it is.

5. **ReadWithoutLease has an oracle that breaks without the mutation.**
   Place: section 5, last row.
   Fault: in MC_w the old cluster decides during the wait. These
   entries extend `decided_history`. The decided prefix of node 6 does
   not hold them. A read at node 6 then breaks EachReadSeesDecided (in
   the newer model the same as R1) with no mutation.
   Change: direct property ReadRoundHonoursLease: a step that starts a
   read round at `i \in Nodes` has a valid lease. Far property: the
   read record takes a ghost flag "node 6 had decided at the start".
   No read at an old node with that flag and with `~stale` reaches
   "done".

6. **LeaseFromReply cannot break its property.**
   Place: section 3, RenewLease; section 5.
   Fault: RenewLease is one step. Request, grant and reply have the
   same time, so the time of the reply equals the time of the request.
   Also, LeaseHonoured reads the lease end that the mutation sets.
   LeaseHonoured holds even when the renewal has several steps.
   Change: split the renewal into LeaseRequest(i) (reads the clock and
   keeps the time), the grant at the witness (checks the epoch, sets
   the expiry), and LeaseReply(i) (sets the end from the kept time).
   Tick can run between them. Direct property LeaseWithinGrant: the
   lease end of each node is at most the witness expiry of the grant
   that made it. Trace that breaks it: grant at 0 (expiry 2), Tick,
   reply at 1 with end 3, ForceSingle at 1 (remaining 1), Tick, node 6
   decides at 2, the old leader decides at 2.

7. **Two guards have no mutation.**
   Place: section 5.
   Fault: the rule of plan-fixes item 16 gives each new guard a
   mutation. The epoch check at the grant and the raise in ForceSingle
   have none.
   Change: WitnessGrantsAnyEpoch, with GrantOnlyCurrentEpoch as direct
   property. ForceWithoutRaise, with
   `ForceRaisesEpoch == [][force_single.node = 0 /\ force_single'.node # 0 => witness'.epoch > witness.epoch]_vars`.

8. **Expected violations stop the run of MC_w.**
   Place: section 4, NoTwoWriters; section 6.
   Fault: NoTwoWriters breaks in MC_w. Trace: renewal, ForceSingle
   with remaining time 2, the old leader decides index 1 during the
   wait, Tick, Tick, node 6 becomes leader, proposes and decides
   index 1. MaxLog 1 is enough. S1Force, S2Force and R1Force break by
   the same loss when NoForceLoss holds. TLC stops at the first
   violation. The breadth-first run of 1 h then checks nothing else.
   Change: MC_w lists only properties that hold. A cfg MC_w_loss must
   violate NoTwoWriters, in the form of MC_d1_reach. The plan states to
   the owner: the lease does not remove the loss of stage 2. It limits
   the loss to the decisions in the rest of the lease.

9. **"An old node decides" is not defined.**
   Place: section 4, NoConcurrentWriters, ghost `old_decide_time`.
   Fault: a Decide, AcceptDecide or AcceptSync in flight raises the
   `decided_idx` of a follower late. HandleMajorityPromises raises it
   at a new leader (line 637). If the ghost counts each rise, a value
   decided under the lease looks like a late decision. That is a false
   violation.
   Change: an old decision is the decide branch of HandleAccepted at a
   leader in Accept, `i \in Nodes`. Add OldDecidedFrozen (see the
   answer to question 1). It covers every path.

10. **Variables are missing.**
    Place: section 3.
    Fault: the guard of the wait needs the remaining time at node 6.
    That is state, not a ghost. NoConcurrentWriters needs the time of
    the first decision of node 6. Finding 6 needs the pending request
    and the grant expiry. The plan names none of these. The plan does
    not say if the lease of a node is durable. A monotonic clock starts
    again at boot, so a durable lease end is not valid after a start.
    Change: add these variables. The lease is volatile. Restart
    (line 1139) clears it. Each new constant and variable gets its
    `@type` line. The APA wrapper gets the new OVERRIDE names.
    `check_constants.py` then requires the three constants in every
    cfg, also in each MC_mut cfg.

11. **The state space grows with absolute time.**
    Place: section 3, `now`, `MaxTime`, `force_time`,
    `old_decide_time`.
    Fault: `now`, each lease end, the expiry and the ghost times enter
    the view. Tick can run in each state, so a state at time t repeats
    at t+1 with each time shifted. The d2 cfgs did not finish
    breadth-first at MaxLog 2 within 1 h (plan-fixes item 13). Not
    measured here. At `now = MaxTime` no lease ends, so node 6 cannot
    decide in those behaviours.
    Change: keep each time as a remaining duration in
    `0..LeaseLength`. Tick lowers each positive value by one. No `now`
    and no `MaxTime`. The ghost times become booleans. Add
    `LeasesDisjoint == force_single.node # 0 /\ wait_remaining = 0 => \A i \in Nodes : lease_remaining[i] = 0`.
    It needs no decision to break.

12. **The bounds of MC_w are not complete.**
    Place: section 6.
    Fault: earlier bounds hid the case (MaxReadOnly 0, MaxBallot).
    ForceSingle requires `read_only[i]` (line 1615). `elections` is one
    counter for the old leader and node 6 (line 1081).
    Change: give the full cfg in the plan: MaxReadOnly 1, MaxElection 3
    (old leader, new old leader after the force, node 6), MaxBallot 1
    (a higher pid takes over), Values 2, MaxLog 1, ReadIndex TRUE,
    MaxRead 1, MaxHalt 0, Recovery TRUE. A reach cfg with ghost
    counters must reach: an old decision during the wait, a decision
    of node 6 after the wait, a refused renewal after the force.

13. **WitnessRestart shows nothing, and the witness clock is open.**
    Place: section 3, WitnessRestart; section 2, `expiry`.
    Fault: WitnessRestart changes no variable. TLC treats it as a
    stuttering step and adds no state. The safety of the remaining time
    depends on the clock of the witness across its restarts. The plan
    does not say which clock the witness uses.
    Change, for the owner: node 6 waits a full LeaseLength, times the
    drift factor, from the reply to the raise. The witness keeps only
    the durable epoch and changes it by compare-and-set. The witness
    then has no expiry, and its clock is not part of the safety
    argument. The cost: stage 2 waits up to one LeaseLength. If the
    owner keeps the expiry, the plan names the clock of the witness and
    says what it returns after a restart. Proposal: a full LeaseLength.

14. **The drift factor has no place.**
    Place: section 7 against section 2.
    Fault: section 7 says the code waits LeaseLength times a factor.
    Section 2 says node 6 waits the remaining time. Neither says which
    side applies the factor. The factor must cover the clock of the
    leader and the clock of the side that waits.
    Change: name one place for the factor.

15. **The plan differs from the owner decision on followers.**
    Place: section 2, "A follower needs no clock".
    Fault: the state file says: "Each node accepts an Accept only while
    the lease is valid by its own monotonic clock."
    Change: the plan states the difference. The owner confirms it. See
    question 1.

16. **The epoch of a node has no source.**
    Place: section 2, "for its epoch". Owner decision "a node rejects a
    lower epoch".
    Fault: the plan does not say where a node gets its epoch. If an old
    node takes the epoch of a refusal and asks with it, the witness
    grants the lease. The node-side check of a lower epoch is not in
    the plan. With one force it protects no step.
    Change: the epoch of a node comes from its configuration: 0 for an
    old node, the raised epoch for node 6, stored durably at
    ForceSingle. It never comes from a reply of the witness. Mutation
    AdoptEpochFromRefusal. On a refusal the old leader drops its lease
    at once. Section 7 states where the node-side check acts.

17. **Names have no source.**
    Place: sections 3 to 5.
    Fault: Tick, RenewLease, WitnessRestart, LeaseHonoured,
    WaitHonoured, NoConcurrentWriters, `force_time` and
    `old_decide_time` are made here.
    Change: take names from sources and mark the others provisional
    for the owner. Candidates, not checked here: lease (Gray and
    Cheriton, SOSP 1989), witness (Paris, 1986), `now` and timers
    (Lamport, "Real Time is Really Simple", 2005).

18. **The regression needs guards.**
    Place: section 3, "With Witness FALSE the model is unchanged".
    Change: Tick, the renewal steps and WitnessRestart require
    `Witness`. Each new variable keeps its initial value when `Witness`
    is FALSE. The new mutation names enter the ASSUME of `Mutation`
    (line 1761).

## Answers to section 8

**Question 1: is the lease of the leader enough?**
Yes, for decisions and for reads with the check of finding 4. Each rise
of `decided_idx` at an old node is bounded by a decision of a leader:
AcceptDecide carries `decided_idx` of the leader (line 521). Decide
carries the new decided index (lines 824, 1244). UpdateDecided takes at
most that value (line 716). AcceptSync carries `MaxDecided` of the
promises (lines 622, 646). HandleRecoveryResponse copies the primary
(line 889). A new leader that takes `decided_idx` from promises
(line 637) takes only an index that a node already decided. It decides
a new index only through HandleAccepted, so the lease check applies.
A chosen entry that no leader decided is decided only through
HandleAccepted. A follower check is not needed. It would bring the
clocks of the followers into the safety argument. The model must check
this claim and not assume it:
`OldDecidedFrozen == [][wait_over => old_decided_max' = old_decided_max]_vars`,
where `old_decided_max` is the highest `decided_idx` that an old node
ever held. The owner decides the difference to the decision of finding
15.

**Question 2: is one global time sound?**
It is sound for these properties under three conditions. The clocks
run at the same rate; this is the assumption of section 7. No node
compares a time that another node read; only durations cross nodes.
Section 2 keeps this condition. A restart resets nothing that the
model keeps. Global time breaks the third condition: a monotonic clock
starts again at boot, and `now` does not. The model cannot show a lease
or an expiry kept across a restart. The form with remaining durations
(finding 11) has no absolute time. A time of another node then cannot
be written. Finding 13 removes the clock of the witness.

**Question 3: does NoConcurrentWriters say what the owner asked?**
Yes, in the time of the model, after findings 3 and 9. It then says:
each old decision comes before each decision of node 6. The write
periods do not overlap. It needs the ghost of the first decision of
node 6, which the plan does not list. It does not cover reads; finding
5 does. In real time it holds only with the drift factor. Add
LeasesDisjoint (finding 11) as the form over one state.
