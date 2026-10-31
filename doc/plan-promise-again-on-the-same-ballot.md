# Plan: a follower promises the same ballot again

Owner card 2026-10-31 11:27 UTC: fix it ("Beheben"), first in the
model, then in the code.

## 1. The fault

Simulation seed 410 of test_simulation_clients, in the base and on
accept-carries-the-result, with g++ 15, g++ 16 and clang 22.

1. A follower sends `prepare_req` (`reconnected`,
   sequence_paxos.hpp). The network of the simulation duplicates it.
2. The leader takes the first copy: `handle_preparereq` calls
   `reset_promise(from)` and `send_prepare`. The follower promises,
   takes the accept sync and is in phase accept.
3. The leader takes the second copy: it forgets the promise of the
   follower again and sends a prepare of the same ballot.
4. The follower is in phase accept with `promise == prep.n`.
   `handle_prepare` takes a prepare of the same ballot only in phase
   recover, so it ignores the prepare.
5. The leader sends no accept to a node without a promise. The node
   sends nothing that makes the leader send again. The link stays dead.

Omni-Paxos has the same two conditions (follower.rs `handle_prepare`,
leader.rs `handle_preparereq`), not checked against the current
release.

## 2. The change

`handle_prepare` takes a prepare of the ballot that it promised when
the node is a follower, in every phase:

```cpp
if (!(m->promise < prep.n || (m->promise == prep.n && state_.first == role::follower))) {
```

The rest of `handle_prepare` stays: the node writes the promise, sends
its promise and goes to phase prepare. In phase prepare it takes no
accept and no decide, and it takes the next accept sync.

A stale accept sync of the same ballot stays out by `applied_sync_`
(session of the sequence number, `handle_acceptsync`). A stale accept
or decide stays out by the phase check and by `current_seq_num_`.

The guard `role::follower` is equal to the guard `Owner(m.b) # j` of
the model: no node sends a prepare to itself
(`get_preparable_peers`).

An accept sync of the accepted ballot that is not longer than the log
changes no entry. This is the consensus of Raft: a follower truncates
only at a conflict and never on entries that it already holds, because
an outdated AppendEntries would take back entries (Raft, Figure 2,
AppendEntries receiver step 3, https://raft.github.io/raft.pdf; Students'
Guide to Raft, https://thesquareplanet.com/blog/students-guide-to-raft/;
openraft log replication, which calls the truncation unsafe). Inside one
ballot the log of the leader only grows, so a sync of that ballot has no
conflict with the own log.

Review 2026-10-31 (one round, approve, nothing blocking): applied
items 3, 5, 6 and 7. Open from it: after a restart, `applied_sync_` is
empty, so a pre-crash accept sync of the same ballot passes; this path
is in the base too, and the model now reaches it.

## 3. Model

AcceptWithResult on this branch:

- `ResetPromise(i, j)`: the leader in every phase forgets the promise
  of j (handle_preparereq). The prepare of the ballot is in msgs
  already, and msgs is a set, so every message can come again.
- `HandlePrepare`: with `NoPromiseAgain = FALSE`, a node that is not
  the owner of the ballot promises the same ballot again. It is then in
  phase "promised" (`lead[j].phase`) until the next accept sync, and
  takes no accept and no decide.
- Switch `NoPromiseAgain = TRUE` is the old form.

The model checks safety only. The dead link is a fault of liveness,
so the model does not show it. The cover `NoPromiseAgainReached`
(MC_aw_cover_promised.cfg) must be violated: it shows that the new
branch is reached.

The model has no sessions of the accept sync, so it lets more accept
syncs through than the code.

Runs: stage1_l1, stage1_c0_r1, stage1_recovery_l1 (BFS). The model
has no sessions of the accept sync. When TLC shows a stale accept sync
of the same ballot, the session goes into the model.

## 4. Test

test_sequence_paxos: a follower in phase accept that gets a prepare of
its ballot answers with a promise, and takes the accept sync that
follows. After the new promise, an accept and a decide of the old session do not
change the log. A leader in phase accept that gets the promise again
sends one accept sync. Seed 410 of test_simulation_clients then ends
green, and stays as a fixed case of the simulation test.

## 5. Checks

Release with g++ 16: test_sequence_paxos, test_simulation_clients,
test_simulation, test_reconfiguration. The CI runs every build.

## 6. Data on the disk

TLC metadirs under /mnt/zrun/aw (deleted after each run); logs in the
scratchpad aw-logs.
