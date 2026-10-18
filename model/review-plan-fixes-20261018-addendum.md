# Review: plan-fixes-20261018-addendum

Source: the scratchpad model and the six `MC_mut_*` cfgs, read only. TLC was not run, so no claim below is checked by a run.

1. **ForceAtOneNode cannot break.** The mutation ForceAtTwoNodes sits in LeaveReadOnly (line 1663), not in ForceSingle. `force_single.node` is set once and never reset, with or without the mutation. Use this property:
   `ReadOnlyEndsBeforeForce == [][\A i \in Nodes : read_only[i] /\ ~read_only'[i] => force_single.node = 0]_vars`
   It holds unmutated: only LeaveReadOnly clears `read_only`. It breaks after EnterReadOnly(a), EnterReadOnly(b), Halt(b), Halt(c), ForceSingle(a,6), Resume(b) and LeaveReadOnly(b). MaxReadOnly 2 in that cfg permits this sequence.

2. **MaxBallot 2 does not help DeactivatedStillVotes.** ForceSingle needs both other old nodes halted. Both come back through Resume, and with Recovery TRUE they are recovering. No running node answers Recovery, so they stay recovering. Dispatch drops every message at a recovering node, so the mutated branch changes nothing. No node can start ballot 2. Changes:
   - Set MaxCrash 1. A crash restarts a deactivated node with no recovery, because it is Quiet. A Prepare from before the halt then reaches it.
   - Cap 1 hides the Promise: chan<<2,1>> already holds the Recovery that Resume sent, and SendsNothing counts a send to a full channel as nothing. Also add this property, which does not depend on Cap:
   `DeactivatedKeepsDurable == [][\A i \in Nodes : deactivated[i] /\ wipes' = wipes /\ rolled_back' = rolled_back => DurableOfP(i) = DurableOf(i)]_vars`
   - Keep MaxBallot 1.

3. **ReadOnlyWhenServing names the wrong guard.** ReadBeforeServing mutates ReadReply (line 1516). It does not mutate the SingleNodeServes guard of ReadStart, and that cfg sets SingleNode FALSE. Use this property:
   `ReplyOnlyIfUnchanged == [][\A r \in DOMAIN reads : reads[r].state = "indexed" /\ reads'[r].state = "done" => LET i == reads[r].node IN reads[r].crashes = crashes /\ reads[r].rolled_back = rolled_back /\ reads[r].ballot = promise[i]]_vars`
   It needs no ghost, and MaxCrash 1 reaches the break. The ReadStart guard has no mutation yet.

4. **The StopSignLost item is not sound.** That mutation clears `deactivated` on a wipe (line 1213), and DeactivatedStays is already its direct property. The item describes a different guard (a StopSign resend). "Lost without a later resend" depends on future steps, so one state cannot record it. Drop the item.

5. **BuildOnlyFromVoter misquotes the guard.** The mutated conjunct is `j \in running /\ ~recovering[j]` (line 1553); `Votes(j)` is not mutated. The state does not show which j was the source, so the property needs a ghost `build_source \in [NextConfigNodes -> Nodes \cup {0}]`, which only BuildForServer sets:
   `BuildOnlyFromRunning == [][\A s \in NextConfigNodes : build_source'[s] # build_source[s] => LET j == build_source'[s] IN j \in running /\ ~recovering[j]]_vars`
   The ghost must go into `reconfig`, `vars`, View and every explicit UNCHANGED list (BuildForServer, ForceSingle, Halt, Resume, Leave/EnterReadOnly, Reconfigure, Wipe, Receive). The form without a ghost (`\E j` with `log'[s] = DecidedPrefix(j)`) is weaker: a second valid source with the same prefix hides the fault.

6. **ForceOnlyFromStage1 is correct.** It needs no ghost and breaks after Halt, Halt and ForceSingle:
   `ForceOnlyFromStage1 == [][force_single.node = 0 /\ force_single'.node # 0 => read_only[force_single'.node]]_vars`
   A property that restates its guard only shows that the step without the guard is reachable. It does not show that the guard is needed. NoTwoWriters stays intact under ForceWithoutStage1, and the plan must report that to the owner as a finding. Run that mutation against every safety property of the d cfgs. If none breaks, the owner decides if the guard stays.

7. **Missing.**
   - Section 2 says each property "needs a ghost field". Only item 5 needs one, and the plan must say so.
   - Add each new property to the unmutated d cfgs too.
   - The simulation needs `-simulate`, and the coverage run needs `-coverage`.

Verdict: approve with changes
