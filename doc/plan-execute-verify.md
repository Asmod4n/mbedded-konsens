# Plan: Execute-Verify with the majority (stage D2 of plan-state-in-lmdb.md)

This plan gives the protocol and the model of section 4 of
doc/plan-state-in-lmdb.md, in the form that the owner gave on
2026-10-25 09:26 to 09:30:

- A node that finds a fault in itself throws.
- A peer that delivers a wrong value is asked to try again. When the
  value is still wrong, the peer is told that it is broken.
- The majority shows which value is right.
- One repair runs automatically. Each further repair waits for the
  operator.

The plan has three parts: the protocol (section 1), the damage of the
own log as an exception (section 2) and the model (section 3). Nothing
of it is built before the model runs of section 4 show no violation and
a review approves this plan.

## 1. The protocol

### 1.1 The result of an entry

1. Each entry of kind `register_client`, `client_request` or
   `expire_session` carries the result of its run on the node that
   proposed it: the reply and the mutations (plan-state-in-lmdb.md 4.1).
   The entry CRC and the chain CRC cover the result.
2. A run of an entry on a node gives a result. The node agrees with the
   entry when its result equals the result in the entry (floats within
   1 ULP, every other item byte for byte, owner 2026-10-25 09:22).
3. The assumption: the machine of a node that works gives the same
   result for the same entry on the same state. A node whose machine
   does not work is broken. Fewer than a majority of the nodes are
   broken at one time. Under this assumption, two nodes that work agree
   on each entry, and each majority holds a node that works.

### 1.2 The leader

1. The leader runs each proposal in its open state transaction before it
   appends it, and writes its result into the entry
   (plan-state-in-lmdb.md 4.3).
2. A new leader runs the suffix of its sync after its decided index in
   the same way. For an entry of the sync, the leader does not write a
   result: it compares its result with the result in the entry, as a
   follower does (1.3).

### 1.3 The follower

1. A follower runs each new entry in its open state transaction before
   it appends the entry to its log (AcceptDecide and AcceptSync). It
   appends the entries up to the first entry that it does not agree
   with, and none after it, in this ballot. So the log of a follower
   holds only entries that it agrees with, and `accepted` keeps its
   meaning: an entry in the log is accepted.
2. For the first entry that it does not agree with, the follower sends
   `retry(ballot, index)` (provisional) to the leader. The leader sends
   the entries from that index again. When the follower still does not
   agree, it sends `refused(ballot, index, result digest)` (provisional,
   "you are broken").
3. A follower that is in the minority sees the entry decided: a Decide
   or an AcceptDecide whose decided index covers an index that it
   refused. It knows that its own machine does not work, and it throws
   (2.1).

### 1.4 The majority

1. The leader counts, for each index of its log in its ballot, the
   nodes that refused the entry. The leader counts itself as refusing
   an entry of the sync that it does not agree with (1.2.2), and never
   counts itself for its own proposals.
2. An entry is chosen when a majority accepted it, as now. A node
   accepts only what it agrees with (1.3.1).
3. When a majority refused the entry at index `k` in one ballot and the
   leader does not agree with it, the entry is not chosen in any ballot.
   The reason: a chosen entry has a majority that agreed with it; the
   two majorities share a node; by 1.1.3 that node works in one of the
   two cases, and the node that works in the other majority gives the
   same result. The leader then starts a new ballot, and in its sync it
   puts a no-op entry at index `k` and drops the entries after it
   (provisional "veto"). The no-op carries the new ballot, so the next
   sync takes it over the refused entry (the rule of the highest round).
4. When a majority refused an entry that the leader agrees with, the
   leader is in the minority. It throws (2.1): "you are broken" from the
   majority.
5. When the leader does not agree with an entry of its sync and a
   majority accepted it, the leader is in the minority, and it throws.

### 1.5 The open questions of this part

1. The no-op of 1.4.3 is a new kind of entry (provisional `veto`). It
   applies nothing and answers the client of the refused entry with an
   error (provisional `client_errc::refused`).
2. The leader keeps the counts of 1.4.1 in memory only. A leader that
   crashes loses them; the next leader counts again. This costs
   progress, not safety: the model checks safety only.

## 2. A damage of the own log throws

1. A damage that a step finds (`error_is_a_damage`) throws
   `damage_error` (provisional), a type derived from `std::system_error`
   with the error code. Nothing catches it but `main` and the body of a
   thread. The proof (`hardware_fault`) goes into the message.
2. The start runs the scan of plan-repair-from-peer.md 2.3 over the
   whole log, not only the step check. A damage that the scan finds
   starts the repair from a peer, as `with` does now, with the count of
   the node storage: one automatic repair, each further one after
   `allow_one_more_repair`.
3. The mark of the open repair stays: a crash during a repair does not
   count again.
4. `with` gives no stopped node for a damage any more; it gives a node
   that repairs, or one that waits for the operator.
5. In the model this changes nothing: `DamageFound` is already a restart
   with the durable state of a source that votes (SequencePaxos.tla,
   `DamageFound`, `Restart`). The scan at the start finds every damage
   that a step check finds, so `Detected` keeps its meaning.

## 3. The model

1. A constant `ExecuteVerify` (Boolean). FALSE keeps every present
   configuration as it is.
2. A constant `MaxBroken` (0 or 1) and a variable `broken`, the set of
   nodes whose machine does not work. An action `MachineBreaks(i)` adds a
   node while `Cardinality(broken) < MaxBroken`. A repair does not
   remove a node from `broken`.
3. A variable `tainted`, the set of values whose result is wrong. A
   proposal of a leader in `broken` adds its value. A value is unique
   (`proposed`), so the result belongs to the value.
4. `Agrees(i, v) == (i \in broken) = (v \in tainted)`, for a value `v`
   that carries a result. The no-op and the entries of the sessions
   carry none in the model; every node agrees with them.
5. HandleAcceptDecide and HandleAcceptSync append the longest prefix of
   the entries that the follower agrees with, and send `Refused(n,
   index)` for the first entry that it does not agree with. The retry of
   1.3.2 is not in the model: a second run on the same state gives the
   same result.
6. `HandleRefused(i, j, m)` adds `j` to `refused_by[i][m.index]` in the
   ballot `m.n`. With a majority: the cases 1.4.3 to 1.4.5.
7. The follower in the minority (1.3.3) and the leader in the minority
   (1.4.4, 1.4.5) take `DamageFound`.
8. The veto of 1.4.3: the leader takes a higher ballot through
   `handle_leader`, and HandleMajorityPromises replaces the entry at
   `k` with `VetoEntry(k)` and drops the entries after it.
9. New invariants:
   - `DecidedAgrees`: every decided entry with a result agrees with
     each node that works (`~(i \in broken)`): no tainted value is
     decided.
   - `AppliedAgrees`: the state of a node that works holds no tainted
     value (`applied`).
   - `VetoOnlyRefused`: a veto stands only at an index where no value is
     chosen.
   - S1 to S4, E1 and StateIsDecided stay.
10. Mutations: `AcceptWithoutVerify` (a follower appends what it does
    not agree with) must break `DecidedAgrees`; `VetoWithoutMajority`
    (the leader vetoes with one refusal) must break S1 or S2.

## 4. The runs

Each configuration runs 15 minutes as a simulation first; a BFS runs
only where it can end (owner, 2026-10-19 05:55). At most 3 TLC runs
at a time, with JOBS=1 for builds while they run.

1. `MC_ev_leader`: `MaxBroken = 1`, the broken node can lead, one
   crash, client sessions off.
2. `MC_ev_follower`: `MaxBroken = 1`, the broken node never leads.
3. `MC_ev_state`: as 1, with `StateInLmdb`, `MaxInFlight = 2` and the
   state invariants.
4. The two mutations of 3.10 must each give a violation.
