# TLC runs of Execute-Verify with a reconfiguration

Plan: doc/plan-reconfiguration.md 2.3 and 4.1. Spec:
model/tla/EVReconfigure.tla (extends SequencePaxos.tla). Configs:
MC_ev_sessions.sim.cfg with NextConfigNodes {6, 7, 8}, MaxReconfigure
1, MaxElection 2. TLC2 Version 2.19 of 08 August 2024 (rev: 5a47802),
3 workers on 4 cores, OpenJDK 21.0.12.1. Simulation: -simulate -depth
80 -seed 1 -deadlock, limit 900 s.

| Run | Model | Expected | Result |
|---|---|---|---|
| MC_ev_reconf.sim | before 8af157c | violated | StopSignCoversRequests violated at state 17 |
| MC_ev_reconf_mut_StopSignUnchecked.sim | 8af157c | violated | StopSignCoversRequests violated |
| MC_ev_reconf_cover.sim | 8af157c | violated | CoverReconfigured violated |
| MC_ev_reconf.sim | 8af157c | no error | no error, 4,012,556 states checked, stopped by the limit |

The trace before the fix: node 1 forwards a StopSign to node 2, the
leader in its prepare phase. Node 2 also buffers a RegisterClient
entry. At the accept sync it appends the entry and then the StopSign,
and the StopSign is decided before the entry has a result.

The fix: the leader checks StopSignAllowed on a forwarded StopSign in
its accept phase and on a buffered StopSign at the accept sync
(LeaderChecksStopSign). Without Execute-Verify the check is TRUE, so
earlier runs keep their result.
