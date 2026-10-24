# LMDB as the only source: simulation runs, round 7

Model f1a5a6a: PeerCheck (plan 2.3 item 6). Each node keeps, in
memory, the highest ballot that each peer sent it and the highest
accepted index of an Accepted in that ballot, and drops a message of
that peer with a lower value. Every other check on. Tool and flags as
round 2, `-seed 7`, 15 min limit, three runs at a time on 4
processors. Invariants S1 S2 S3 S4, properties S2Grows
BuildOnlyFromRunning.

| Config | Kind | PeerCheck | States | Result |
|---|---|---|---|---|
| MC_r_stale_peer | stale_meta | TRUE | 214303 | S1 violated |
| MC_r_rollback_peer | rollback | TRUE | 55138 | S1 violated |
| MC_r_meta_peer | the four of round 4 | TRUE | 4126280 | no violation, stopped by the limit |

- MC_r_stale_peer: the trace of round 5. Node 2 promises ballot 21 to
  node 1, goes back to promise 0, and promises ballot 12 to node 3.
  Node 3 has seen nothing higher from node 2, so it takes the message.
  Node 1 sees the lower ballot of node 2 only after the two decisions.
- MC_r_rollback_peer: node 3 reports v1 at index 1 in ballot 12 to
  leader 2, goes back to the empty state, and then sends only higher
  ballots (23). The check compares the accepted index only inside one
  ballot, so no node sees a step back.
- So the check by the peers finds neither trace of round 5. It finds
  only a step back that the same peer saw, in the same ballot or by a
  lower ballot.
- A simulation without a violation is not a complete run.
