# Plan: recovery after a restart of every node (power-loss mode)

Source: the owner, 2026-10-19 09:12 (UTC+2), settled again 2026-10-21
09:57: "Wenn's keinen Leader nach nem Stromausfall gibt, wird nur der
letzte Eintrag verglichen." The state file of mbedded-konsens holds the
form. This plan gets one review before the build.

## 1. Facts checked in this session

- `SequencePaxos.tla:436` `RecoveryComplete`: a node completes recovery
  only with f + 1 answers, one of them from the primary of the highest
  ballot among the answers.
- `SequencePaxos.tla:1106-1111` `Dispatch`: a recovering node drops
  every message except RecoveryResponse, Recovery too. So when every
  node recovers, no node answers, and no node completes. This is the
  open case "Recovery after a restart of every node" of the state file.
- `SequencePaxos.tla:421-427` `RecoveryResponseMsg`: a node that is no
  primary sends `n_accepted = DefaultBallot`, an empty log and
  `decided_idx = 0`.
- `SequencePaxos.tla:1331-1333` `ResendMessageTimeout`: Recovery goes
  again only to the peers that have not answered. The code sends it to
  every peer (found 2026-10-19, owner: "nur notieren", fix after v0.1).
- `SequencePaxos.tla:948` `HandleRecoveryResponse` sets `decided_idx`
  to the value of the primary. The code takes the maximum of its own
  and the primary's value (the S7 form, found 2026-10-19).
- TigerBeetle (docs.tigerbeetle.com/about/internals/vsr, read
  2026-10-21) has no recovery protocol of VR Revisited. A replica that
  restarts into the status `recovering_head` asks the primary of its
  view (`request_start_view`). A replica keeps
  its view state in the superblock, in several copies, and trusts it
  after a restart. A restart of every replica then is an ordinary view
  change. The page does not describe that case in words; the source is
  not checked. TigerBeetle gives no form for a cluster that trusts no
  disk after a start, so the form of the owner stands.

## 2. Form

1. Every RecoveryResponse carries the state of the sender: `promise`,
   `n_accepted`, the log (in the model; the code sends only
   `accepted_idx` and the CRC of the last entry), `decided_idx`, and a
   new field `recovering`. `primary` stays as it is.
2. A recovering node answers Recovery too, with `recovering = TRUE` and
   `primary = FALSE`. It changes no state when it answers.
3. A node completes recovery in one of two ways:
   - as today: f + 1 answers, one of them from the primary of the
     highest ballot;
   - power-loss mode, new: an answer from every peer of the
     configuration, and no answer is from a primary. The source is the
     node with the highest (`n_accepted`, `accepted_idx`) among the
     answers and the node itself. The node takes the log of the source
     (the code copies the part after the common prefix as AcceptSync),
     the highest promise of all answers and its own, and the maximum of
     its own `decided_idx` and that of the source. It then goes on as
     today: (Follower, Recover), PrepareReq to every peer.
4. The mode starts by itself; there is no flag. A constant
   `PowerLoss` (default FALSE) switches it in the model, so every run
   of today keeps its state space.
5. With the change, the two fixes of section 1 enter the model:
   Recovery goes again to every peer, and `decided_idx` takes the
   maximum.

## 3. Properties

- S1 to S7 as today.
- New invariant `LogMatching`: two voting nodes that are not wiped,
  with the same `n_accepted` and the same `accepted_idx`, hold the same
  log. This is the reason why an equal (round, `accepted_idx`) with a
  different CRC is a hardware fault (owner, 2026-10-21).
- New reach check `MC_p_reach`: the negation of "every node crashed
  once and every node completed recovery" must be violated. It shows
  that the case is reached.

## 4. Runs

All with Nodes = {1, 2, 3}, Recovery = TRUE, PowerLoss = TRUE.

| Run | Faults | Expectation |
|---|---|---|
| MC_p | MaxCrash = 3 | S1-S7 and LogMatching hold |
| MC_p_rollback | MaxCrash = 2, MaxRollback = 1 | S1-S7 and LogMatching hold |
| MC_p_reach | MaxCrash = 3 | the reach invariant is violated |
| MC_mut_PowerLossMajority | as MC_p_rollback, completion with a majority instead of every peer | S1 or S2 is violated |
| MC_c_crash, MC_F6c | as today, PowerLoss = FALSE | same result as before the change |

Order: a simulation of 15 minutes for each run first. A breadth-first
run follows only where it can finish. Each finished run is pushed.

## 5. Not in this plan

The code of the library, the transfer of the part after the common
prefix (the model copies the whole log), the hash chain for a large
database, the liveness model L.

## 6. Order

1. Plan and review.
2. The two fixes of section 1, MC_c_crash and MC_F6c as regression.
3. The power-loss mode, the runs of section 4.
4. State file; then the plan for the code.

## 7. Changes after the review

The review (approve with changes, 12 findings) is applied as follows.

- R1: with more than f rolled-back disks, a decided entry can be gone
  from every node. Today the recovery waits for ever in that case; the
  mode goes on without the entry. Question to the owner (2026-10-21).
  Provisional: the mode goes on, and the documentation states that the
  cluster is safe up to f disks that lose writes at the same time. A
  run `MC_p_rollback2` (MaxRollback = 2) shows the limit; its expected
  result is a violation of S2.
- R2: the mutation completes with f answers (one peer in n = 3), not
  with a majority. Expected: a violation of S1 or S2 with one rollback.
- R3: S7 is left out of every run with a rollback, as in the F6 runs.
  S4 and S6 hold trivially without a reconfiguration.
- R4: no new invariant. S5 with AcceptedIdxIsLogLength already states
  that equal (round, `accepted_idx`) means an equal log, and it is the
  reason for the hardware fault on a different CRC.
- R5: the runs MC_p* state every constant: Cap = 2, MaxTimeout = 1,
  MaxBallot = 2, MaxLog = 1, MaxElection = 2, MaxLoss = 0,
  MaxDuplicate = 0, MaxReconfigure = 0, SingleNode = FALSE, Witness =
  FALSE, ClientSessions = FALSE, ReadIndex = FALSE.
- R6: regression: MC_F6b, MC_c_crash and MC_d2_crash before and after
  the two fixes, compared by result and by distinct states. One run of
  MC_c_crash with MaxTimeout = 1 checks the resend to every peer.
- R7: a ghost set holds each node that completed in power-loss mode
  while every node was recovering. The reach invariant states that the
  set stays empty; MC_p_reach expects its violation.
- R8: `PowerLoss` guards every part of section 2: the full state in an
  answer of a node that is no primary, the field `recovering` in
  `EmptyMessage`, and the answer of a recovering node. The node takes
  `n_accepted` from the source too.
- R9: liveness limits, stated and not checked: a node in single-node
  mode, a deactivated node and a node wiped under rule R do not answer,
  so the mode does not complete while one of them is in the
  configuration. The mode stays out of runs with a reconfiguration.
- R10: the plan for the code states that the nonce is fresh after each
  start, also after a power loss.
- R11: each run reports its distinct states, its depth, its time and
  how often the power-loss branch fired. A simulation without a
  violation is not a complete run.
- R12: the names `PowerLoss`, `MC_p*` and the name of the mutation are
  provisional; the owner decides them.
