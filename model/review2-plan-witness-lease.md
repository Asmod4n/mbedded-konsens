Verdict: approve with changes

# Second review: plan-witness-lease

Source: `model/plan-witness-lease.md` at commit 9c673eb, the model
`model/tla/SequencePaxos.tla` at commit 5ea2ff9, the first review
`model/review-plan-witness-lease.md`, the cfgs in `model/tla/` and the
result files named in section 6 of the plan. TLC and Apalache were not
run. No claim below is checked by a run. Line numbers refer to
`model/tla/SequencePaxos.tla` at 5ea2ff9.

The form of the lease in section 2 is safe in the model. The one-step
grant is sound (finding 5 says why). The faults are in the model plan:
one guard blocks node 6 when no force happened, two far properties
cannot break, two properties are not well defined, and the regression
does not reach the changed guards. The build can start after the
changes of findings 1 to 9. Findings 10 to 16 are notes for the code
and questions for the owner.

## Status of the 18 findings of the first review

| # | Subject | Status |
|---|---|---|
| 1 | Base not named | Fixed. Counts predate 5ea2ff9: new 2 |
| 2 | Lease blocks node 6 | Fixed. Guard form open: new 7 |
| 3 | Wait misses decisions | Fixed |
| 4 | Read check in ReadReply | Fixed |
| 5 | Read oracle breaks alone | Fixed |
| 6 | LeaseFromReply cannot break | Answered. Far property still cannot break: new 5 |
| 7 | Two guards without mutation | Fixed |
| 8 | Expected violations stop MC_w | Fixed |
| 9 | Old decision not defined | Partly. Current max, not "ever held": new 4 |
| 10 | Variables missing | Fixed. Type of `lease_request`: new 8 |
| 11 | Absolute time | Fixed |
| 12 | Bounds of MC_w | Partly. 15 constants not given: new 9 |
| 13 | Witness clock | Fixed |
| 14 | Place of the drift factor | Fixed |
| 15 | Follower check differs | Answered. Card to the owner |
| 16 | Epoch has no source | Fixed. Direct property of the mutation: new 6 |
| 17 | Names without source | Answered. Marked provisional, no list: new 16 |
| 18 | Guards for the regression | Partly. Changed guards not conditioned: new 1 |

## New findings

1. **The wait guard blocks node 6 when no force happened.**
   Place: section 3, "LeaderCountsOwnPromise and LeaderCountsOwnAccepted
   for `i \in NextConfigNodes` require `wait_remaining = 0`", and
   "ReadStart at the forced node requires `wait_remaining = 0`".
   Fault: `wait_remaining` is `-1` before ForceSingle, and with `Witness`
   FALSE it stays `-1`. LeaderCountsOwnPromise and
   LeaderCountsOwnAccepted (lines 1578, 1592) also serve node 6 after a
   normal reconfiguration (BuildForServer, line 1553; MC_d1 has
   MaxReconfigure 1 and NextConfigNodes {6}). With the guard
   `= 0`, node 6 never becomes leader in Accept and never serves a read
   in d1, d1_reach and d2. The counts change, and MC_d1_reach finds no
   violation. In MC_w, if MaxReconfigure > 0, the normal path is
   blocked too.
   Change: write each changed guard so that it holds when nothing was
   forced and when `Witness` is FALSE: `wait_remaining <= 0` at node 6,
   and `~Witness \/ lease_remaining[i] > 0` at the old nodes. Section 3
   states that each changed guard in an existing action is TRUE with
   `Witness` FALSE.

2. **The regression does not reach the changed guards, and two counts
   cannot be compared.**
   Place: section 6, first item.
   Fault: MC_base_monotone, MC_e, MC_f and MC_d0 have
   `NextConfigNodes = {}`. They reach neither LeaderCountsOwn*,
   ForceSingle nor the wait. The d0 and d1 runs
   (`20261018T174440Z-d0.md`, `20261018T190030Z-d1.md`) stopped at the
   time limit of 1 h, at level 9 and 11. A count at a time limit depends
   on the speed of the machine, so equal counts are not a criterion.
   The base_monotone, f and e counts come from runs at 08:02 to 08:53,
   before commit 5ea2ff9 (23:33). Not checked here: whether the
   addendum changes these counts. With `NextConfigNodes = {}` the new
   `build_source` is an empty function, so a change is not expected.
   Change: run the regression with complete runs that reach node 6 and
   ForceSingle: MC_d1_reach (the violation and the length of its
   trace), and copies of MC_d1 and MC_d2 at MaxLog 1 that finish
   breadth-first. Run each cfg on 5ea2ff9 and on the new model, one
   after the other, and compare the distinct states. Keep
   base_monotone, e and f as they are. Add the Apalache type check of
   APA_F6c, because the plan changes the type aliases (`read`, new
   variables).

3. **NoConcurrentWriters cannot break by ForcedWithoutWait.**
   Place: section 4, NoConcurrentWriters; section 5, row
   ForcedWithoutWait.
   Fault: the ghost `old_decided_after_wait` is set by an old decision
   after the wait. With ForcedWithoutWait, node 6 decides during the
   wait, the old leader decides during the wait, and after the wait the
   lease stops each old decision. Then `old_decided_after_wait` stays
   FALSE and the invariant holds. In the correct model,
   NoConcurrentWriters is also implied by OldDecidedFrozen alone, so it
   adds no check. It does not say "no overlap" either.
   Change: replace the ghost by `old_decided_after_forced`: set when
   `old_decided_max` rises in a state where `forced_has_decided` is
   TRUE. NoConcurrentWriters == `~old_decided_after_forced`. Trace for
   ForcedWithoutWait: LeaseRequest, LeaseGrant (lease 2), ForceSingle
   (wait 2), node 6 decides the undecided suffix of `first_log`, the
   old leader decides index 1. Each other row of section 5 still breaks
   it: the old decision then comes after the wait, and node 6 decides
   first.

4. **OldDecidedFrozen uses the current maximum.**
   Place: section 4, OldDecidedFrozen; section 3, ghost
   `old_decided_max`.
   Fault: "the highest `decided_idx` of a node in `Nodes`" falls by
   Wipe (line 1196), Rollback (line 1182) and AcceptSync with
   `DecidedIdxMonotone` FALSE (line 701). It then rises again when a
   Decide, AcceptDecide or AcceptSync in flight reaches the node. That
   is a false violation after the wait. The first review asked for the
   highest value that an old node ever held. With that form the
   property is well defined with messages in flight: each such message
   carries a value at most the decided index of a leader at the send
   (lines 521, 622, 646, 824, 889, and the resend of Decide in
   ResendMessageTimeout). The plan also does not say where the ghosts
   change.
   Change: `old_decided_max' = Max(old_decided_max, max of decided_idx'[k] for k \in Nodes)`.
   Put the update of every new ghost in one definition, conjoined to
   GhostUpdate (line 1008). The actions without GhostUpdate (ReadStart,
   StartReadRound, ReadReply, SendDeactivated, Halt) list the ghosts in
   UNCHANGED. `forced_has_decided` uses the same rule as WaitHonoured:
   `decided_idx'[s] > forced_decided_at_force`.

5. **LeaseFromReply cannot break LeasesDisjoint.**
   Place: section 3, LeaseGrant; section 5, row LeaseFromReply.
   The one-step grant is sound for the correct form. A request at time
   r and a grant at time g give the model node the lease over [g, r + L).
   The real node holds it over [reply, r + L), which is a subset. The
   model node 6 waits from ForceSingle, the real node from the reply of
   the raise, which is not earlier. So the model allows more behaviours
   than the code.
   Fault: in one step, the time of the reply equals the time of the
   grant, and every positive grant is before ForceSingle. With
   LeaseFromReply the lease is `LeaseLength` at a time before the force,
   and the wait is `LeaseLength` from the force. Tick lowers both. The
   lease reaches 0 not later than the wait. LeasesDisjoint and
   NoConcurrentWriters hold. The fault of the mutation needs a reply
   that arrives after the force, and the model has no such reply.
   Change, proposal: split LeaseGrant into LeaseGrant (at the witness:
   checks the epoch, puts the answer in `lease_reply[i]`) and
   LeaseReply(i) (at the node: sets `lease_remaining[i]` from the value
   of `lease_request[i]` at the reply, which Tick has lowered). Tick can
   run between them. Trace for the mutation: LeaseRequest, LeaseGrant,
   ForceSingle (wait 2), Tick, Tick (wait 0), LeaseReply with lease 2:
   LeasesDisjoint breaks. Other option: keep one step, and write in
   section 5 that the far property of LeaseFromReply cannot break and
   why. Finding 10 needs the split too.

6. **The direct property of AdoptEpochFromRefusal is not defined, and
   GrantOnlyCurrentEpoch cannot break by it.**
   Place: section 5, row AdoptEpochFromRefusal.
   Fault: the mutation writes `witness_epoch` into `node_epoch[i]` at a
   refusal. At the next grant, `node_epoch[i] = witness_epoch` holds, so
   GrantOnlyCurrentEpoch holds. "GrantOnlyCurrentEpoch with `node_epoch`
   of the configuration" names no formula.
   Change: add the invariant
   `EpochFromConfiguration == \A i \in Nodes : node_epoch[i] = 0`, and
   the action property "`node_epoch[s]` for `s \in NextConfigNodes`
   changes only in ForceSingle". Trace: ForceSingle, LeaseRequest,
   LeaseGrant (refused, mutation writes 1).

7. **The form of the lease guard in HandleAccepted is not stated.**
   Place: section 3, "the decide branch of HandleAccepted for
   `i \in Nodes` requires `lease_remaining[i] > 0`".
   Fault: two forms differ. (a) An enabling conjunct: Receive of the
   Accepted is disabled, the message stays at the head of the FIFO
   channel (line 1055) and blocks each later message from that peer.
   (b) A part of the IF condition at line 819: the leader takes the
   ELSE branch, records `accepted_indexes` and decides nothing. Then a
   later grant decides nothing, because no step decides a chosen index
   without a new Accepted. With MaxLog 1 the entry then stays undecided
   at that leader. Each form hides different cases, and the code must
   do what the model does.
   Change: choose (b) and add one action for `i \in Nodes`, in the form
   of LeaderCountsOwnAccepted: a leader in Accept with a valid lease
   decides the highest chosen index above `decided_idx[i]`. The code
   then decides at the grant what is chosen. LeaseHonoured covers both
   actions. Or choose (a) and state that the code holds the Accepted
   until the lease is valid.

8. **`lease_request` mixes a string and an integer, and
   LeaseWithinGrant mixes three properties.**
   Place: section 3, `lease_request`; section 4, LeaseWithinGrant;
   section 5, row LeaseFromReply.
   Fault: `none` or an integer has no Apalache type. LeaseWithinGrant
   has a type bound (`lease_remaining[i] <= LeaseLength`), which no
   mutation can break, an action part, which repeats
   GrantOnlyCurrentEpoch, and the strengthened form
   `lease_remaining <= lease_request`, which has no meaning as an
   invariant because the grant sets `lease_request` to `none`.
   Change: `lease_request` is `-1` or `0..LeaseLength`, as
   `wait_remaining`. The type bound goes into a type invariant. Write
   LeaseWithinGrant as one action property: the step that sets the
   lease from a request gives `lease_remaining'[i] <= lease_request[i]`.

9. **The cfg of MC_w is still not complete.**
   Place: section 6, MC_w.
   Fault: MaxWipe, MaxRollback, DecidedIdxMonotone, MaxReconfigure,
   MaxLoss, MaxDuplicate, MaxTimeout, Cap, MaxCounter, MaxSession,
   RuleR, ClientSessions, MaxClientSend, MaxRequest, StaleReadNode and
   Mutation are not given. MaxReconfigure decides whether finding 1
   shows in MC_w. MaxWipe, MaxRollback and DecidedIdxMonotone decide
   whether finding 4 shows. The cfgs of MC_w_reach and MC_w_loss are not
   given.
   Change: give all constants of MC_w, MC_w_reach and MC_w_loss.
   Proposal: MaxWipe 0, MaxRollback 0, DecidedIdxMonotone TRUE,
   MaxReconfigure 0 in MC_w and 1 in one copy, the rest as in MC_d2.
   With MaxLog 1, node 6 decides only the undecided suffix of
   `first_log`. MC_w_reach must reach that case: node 1 accepts v1
   alone as leader, enters stage 1; node 2 decides v2 at index 1 under
   its lease; node 6 decides v1 after the wait. That needs
   MaxElection 3.

10. **A reply can match the wrong request.**
    Place: section 2, "the time of the request"; section 3,
    LeaseRequest.
    Fault: the code renews every 60 s, so several requests can be in
    flight. If the node takes the time of the last request for a reply
    to an earlier request, the lease can end after the wait. Example:
    request A at r1 is granted before the force, request B at r2 after
    the force is refused later, the reply of A arrives first and the
    node sets r2 + L. The model has one request per node and one step
    for the grant, so it cannot show this.
    Change: section 2 states that each reply carries the id of its
    request and of the boot, and that the node takes the time of that
    request. LeaseRequest requires `lease_request[i] = -1`. With the
    split of finding 5 the mutation "reply takes the time of the last
    request" can be added if the model keeps two requests per node.
    Proposal: one request in the model, the rule in section 2.

11. **The clock of the lease is not named.**
    Place: section 2, "monotonic clocks".
    Fault: on Linux, CLOCK_MONOTONIC does not advance while the system
    is suspended; CLOCK_BOOTTIME does (clock_gettime(2)). A leader that
    is suspended after its check holds its lease longer in real time
    than the wait of node 6. A pause between the check and the effect
    (the write of `decided_idx`, the Decide, the reply to the client)
    has the same result. The model has no such pause, because the check
    and the decision are one step. Not checked here: the platform of
    the nodes, and the behaviour of each clock when a virtual machine
    is paused.
    Change, for the owner: name the clock (proposal: CLOCK_BOOTTIME on
    Linux). Section 7 states that the drift factor also covers the
    longest pause between the check and the effect, and that a longer
    pause breaks the lease. This is the known limit of a lease without
    a fencing token (Burrows, "The Chubby lock service", OSDI 2006,
    sequencers). It is an argument for the open card: a check at each
    node is the form that fences.

12. **A restart of node 6 during the wait is not stated for the code.**
    Place: section 2, the wait; section 6, the copy with MaxCrash 1.
    Fault: Crash keeps `wait_remaining`, so the model lets node 6 keep
    the elapsed time across a boot. A monotonic clock cannot. The model
    then allows more than the code, which is safe. The plan does not
    say what the code does.
    Change: section 2 states that node 6 starts a full wait again after
    a boot during the wait.

13. **A crash between the raise and the local write is not stated.**
    Place: section 2, ForceSingle.
    Fault: in the model the raise, the epoch of node 6 and the copy of
    the log are one step. In the code, node 6 can crash after the
    witness has raised the epoch and before it stores its own epoch.
    The retry then sends a compare-and-set from 0 to 1, and the witness
    already holds 1.
    Change: section 2 states that a retry of the raise accepts the
    epoch that it wanted, and that node 6 stores its epoch before it
    sends the raise.

14. **The epoch of the witness must not go back, and the check of a
    lower epoch does not cover the old nodes.**
    Place: section 7, last item; WitnessForgetsEpoch.
    Fault: an old node stores epoch 0. A witness that comes back with
    epoch 0 (a restored backup, a new disk) grants it a lease, and the
    node-side check sees no lower epoch. WitnessForgetsEpoch shows the
    result. Section 7 does not say this.
    Change: section 7 states that a witness that has lost its epoch is
    outside the fault model, and that the answers of the witness are
    authenticated, so that no other host can give a grant.

15. **Tick and LeaseRequest are enabled in too many states.**
    Place: section 3, Tick and LeaseRequest.
    Fault: Tick with no positive duration changes nothing, so it is a
    stuttering step. TLC adds no state, but each simulation step on it
    is lost. LeaseRequest at each leader in each phase, at each time,
    multiplies the states. The upper bound of the new values is
    3^3 for the leases, 4^3 for the requests and 4 for the wait, 6912
    combinations. This is a bound, not a measurement. Stuttering is no
    fault here: the plan checks safety only, with no fairness.
    Change: Tick requires `Witness` and one positive duration.
    LeaseRequest requires `Witness`, a leader in Accept, and no pending
    request. The plan names the growth as a thing to measure in MC_w,
    not to estimate.

16. **Names: the provisional list for the owner is missing.**
    Place: section 4, "Provisional names, from the sources in review
    17 where one exists".
    Fault: the plan takes no name from a source and does not say which
    name has one. Lease (Gray and Cheriton, SOSP 1989) and epoch
    (Chubby, ZooKeeper) have a source. Tick, LeaseHonoured,
    WaitHonoured, OldDecidedFrozen, NoConcurrentWriters,
    LeasesDisjoint, StaleReadAfterForce and the mutation names have
    none that was checked here.
    Change: one list in the plan: each name, its source or "no source,
    for the owner". It goes with the open questions to the owner.

## Notes

- After the wait, node 6 decides with no lease. That holds for one
  force. If the cluster of node 6 grows later, its leaders need a lease
  with epoch 1 against a second stage 2. Section 7 can state this.
- Reads: a round that starts under a lease and ends after node 6 has
  decided is linearizable at its start. StaleReadAfterForce takes its
  flag at ReadStart, so it gives no false violation for such a read.

## Check of version 3

Verdict: approve with changes

Source: `model/plan-witness-lease.md` at commit 22e3317. TLC and java
were not run. This check reads only the changed parts.

| # | Change | Status |
|---|---|---|
| 1 | Guards TRUE without force and with `Witness` FALSE | Applied |
| 2 | Complete regression runs, both arms, Apalache check | Applied. Note b |
| 3 | `old_decided_after_forced` | Applied. Fault A touches it |
| 4 | Historical maximum, one ghost update | Applied. Fault A |
| 5 | LeaseGrant and LeaseReply split | Applied. Trace breaks LeasesDisjoint |
| 6 | EpochFromConfiguration | Applied |
| 7 | Form (b) and LeaderDecidesChosen | Applied. Fault B |
| 8 | `lease_request` as `-1`, LeaseWithinGrant as action | Applied |
| 9 | Full cfg of MC_w | Applied. Note c |

The split of finding 5 is correct. A grant before the force sets
`lease_request` at most `wait_remaining`, and Tick lowers both. So at
the reply the lease is 0 when the wait is 0. With LeaseFromReply the
trace LeaseRequest, LeaseGrant, ForceSingle, Tick, Tick, LeaseReply
gives lease 2 with wait 0.

### Faults

A. **WaitHonoured and `forced_has_decided` fire without a force.**
   Place: section 3, ghost updates; section 4, WaitHonoured.
   Fault: neither formula requires that ForceSingle has happened.
   BuildForServer (line 1553) sets `decided_idx[6]` to `Len(l)`, and
   after a normal reconfiguration LeaderCountsOwnAccepted raises it,
   with `wait_remaining = -1`. If `forced_decided_at_force` starts at
   0, WaitHonoured breaks in the correct model, and
   `forced_has_decided` becomes TRUE with no force. That happens in the
   copy of MC_w with MaxReconfigure 1. In the ForceSingle step itself,
   `decided_idx'[s]` rises to `Min(decided_idx[i], Len(l))`, and the
   plan does not say whether "above `forced_decided_at_force`" reads the
   primed or the unprimed value. With the unprimed value, the step
   breaks WaitHonoured and sets `forced_has_decided`.
   Change: both formulas start with `force_single.node # 0`, unprimed.
   Then the ForceSingle step and each step before it are excluded:
   `force_single.node # 0 /\ decided_idx'[s] > forced_decided_at_force => wait_remaining = 0`,
   and the same condition in the update of `forced_has_decided`. The
   plan gives the initial value of `forced_decided_at_force`.

B. **LeaderDecidesChosen: its lease guard has no mutation, and the form
   is open.**
   Place: section 3, LeaderDecidesChosen; section 5.
   Fault: each new guard gets a mutation. DecideWithoutLease removes
   only the check in HandleAccepted. "In the form of
   LeaderCountsOwnAccepted" can be read as a call of HandleAccepted
   with an Accepted from `i` itself. That call writes the chosen index
   into `accepted_indexes[i]` (line 818) and can lower the index of the
   leader.
   Change: DecideWithoutLease removes the lease check in both actions.
   LeaderDecidesChosen sets `decided_idx[i]` and sends Decide to
   `PromisedFollowers`, as the decide branch at lines 822-828 does, and
   does not write `accepted_indexes`.

### Notes

a. The trace of ForcedWithoutWait in section 5 is correct with
   `old_decided_after_forced`.
b. The arm on 5ea2ff9 has no constants `Witness` and `LeaseLength`.
   TLC does not take a cfg that assigns a constant the model does not
   declare (not checked here), and `check_constants.py` reports them
   as unknown. The regression needs one cfg for each arm, the same in
   all other lines.
c. MC_d2 lists NoTwoWriters, S1Force, S2Force and R1Force, and it has no
   VIEW. "MC_w as MC_d2" must give its own INVARIANTS, PROPERTIES and
   VIEW lines. Otherwise MC_w stops at the expected loss, and the new
   variables are not in the view that is used.
