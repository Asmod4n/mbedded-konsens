# Plan: Models for Sequence Paxos with Ballot Leader Election

Version 3, 2026-10-16. Version 1 was rejected (12 findings). Version 2
was approved with changes (13 findings). Section 10 overrides all
other sections where they contradict each other.
Owner: "yes, and for it build a Quint and/or TLA+ model that we prove
first, as Jepsen does."

## 1. Goal

The models describe the protocol that the owner chose. The protocol
has three parts: the log of Omni-Paxos (Sequence Paxos), the election
of Omni-Paxos (Ballot Leader Election, BLE), and the change of the
configuration through StopSign. The models check the protocol under
the faults that Jepsen makes. This happens before any line of C++ is
written.

## 2. Source of the names and the rules

- Origin: github.com/haraldng/omnipaxos, commit 532ff79 (2026-10-06),
  Apache-2.0. Read: ballot_leader_election.rs,
  sequence_paxos/{leader,follower,mod}.rs, storage/internal_storage.rs
  (sync_log), util.rs (LeaderState, PromiseMetaData,
  SequenceNumber::check_msg_status).
- Names from this source: Ballot (config_id, n, priority, pid),
  Prepare, Promise, AcceptSync, AcceptDecide, Accepted, Decide,
  PrepareReq, NotAccepted, StopSign, SequenceNumber (session,
  counter), HeartbeatRequest, HeartbeatReply, happy, decided_idx,
  accepted_idx, n_accepted, promise, Role, Phase.
- Rule for a lost disk: Paxos Made Live (Chandra et al. 2007), the
  section about disk corruption.
- Not read: the EuroSys 2023 paper. The Rust source is not copied
  into the repository; refs/omnipaxos/ORIGIN.md names the commit.

## 3. Two models

### 3.1 Model S (safety), TLA+/TLC, later Quint/Apalache

- No BLE. Each node can call `handle_leader(b)` for each ballot
  b > promise with its own pid. This allows more behaviour than BLE
  allows. It is sufficient for safety.
- A handler is one atomic step: one durable write, as
  `write_atomically` in `sync_log`, and then its messages. No
  batching. A leader counts its own accepted_idx only after the
  write.
- Channels: one FIFO channel for each ordered pair. The channel can
  lose messages. SequenceNumber (session, counter) is in the model.
  `check_msg_status` decides as in the source (Expected,
  DroppedPreceding → reconnected → PrepareReq, Outdated). An
  additional action duplicates a message. The copy goes to the end of
  the channel.
- For equal (n_accepted, accepted_idx), the source selects the first
  Promise. In the model, the selection is nondeterministic.
- Configurations: up to 2, node IDs from 4. The transition is an
  action of its own. It copies the decided log of a node of the old
  configuration that decided the StopSign. It never copies from a
  history variable.

Faults:

- F1 Loss and duplication on a channel (see above).
- F3 Crash with the durable state kept. The node loses leader_state,
  outgoing messages and buffered proposals. It restarts in
  Phase::Recover, as in the source.
- F4 Loss of the disk. The node loses all of its durable state.
  - Rule R (our design, after Paxos Made Live): A node with empty
    storage does not vote. It sends no Promise and no Accepted. It
    counts in no majority. It votes again only after a decided
    configuration adds it under a new incarnation (pid, incarnation).
    The start of a cluster writes the storage of the first nodes in a
    step of its own. Thus no node must detect whether it is new or
    has lost data.
  - The source does not have this rule. After a wipe, a node in the
    source looks like a new node (Phase::None, Ballot::default).
- Compaction (owner, 2026-10-16: deterministic, state as small as
  possible, no unbounded growth):
  - Snapshot as in the source (`snapshot`, `create_log_sync` with
    decided_snapshot, `sync_log` with Trim and SetSnapshot).
  - Deterministic rule (our design): The step that moves decided_idx
    past a multiple of K replaces the decided prefix up to this
    multiple with the snapshot. It does this in the same atomic
    write. Thus each node compacts at the same indexes. Equal prefixes
    give equal snapshots.
  - Owner: The user of the library says what the state means. Form of
    the source: the trait `Snapshot` with `create(entries)` and
    `merge(delta)`. In the model, the snapshot is a pure function. It
    folds the decided prefix. The entries are pairs (key, value) with
    "last value wins". Thus a snapshot is smaller than the prefix. S8
    checks that the snapshot is equal to the fold of the prefix in
    `decided_history`.
  - Trim of the source (`trim`, bound `min_all_accepted_idx`) is an
    action of its own. The source uses accepted. The documentation
    says decided. The model checks whether the bound of the source is
    safe.
  - A follower with a decided_idx below the compacted_idx of the
    leader gets the snapshot in AcceptSync.
- F5 Wrong checksum: the file system reports it, and the process
  ends. In the model, this is the same as F4.

### 3.2 Model L (liveness), TLA+/TLC only

- Model S plus BLE with priority in the ballot.
- Before GST: all faults. A flag `stable` stops loss, crash and change
  of the partition. After that, nothing changes.
- After GST, `hb_timeout` is possible only when each reply of each
  connected peer has arrived.
- Weak fairness on delivery, `hb_timeout` and
  `resend_message_timeout`.
- A client sends a proposal again until it is decided. The source
  sends it only once (`forward_proposals`).
- Liveness runs start at GST from fixed initial states. They use no
  state constraint and no symmetry. When a run reaches a bound, the
  result says so.

## 4. Properties

Safety (model S):

- S1 For two nodes that vote, the shorter decided prefix is a prefix
  of the longer one.
- S2 The variable `decided_history` only grows. It holds each entry
  that was ever decided. Each decided prefix of a node that votes is
  compatible with it.
- S2b Each node in (Leader, Accept) has a log that extends
  `decided_history` of its configuration.
  - Note (model): The model narrows S2b to the entries that were
    decided at a ballot not higher than the ballot of the leader
    (`decided_ballot[k] <= leader_state[i].n_leader` in
    model/tla/SequencePaxos.tla). The form of this plan fails for any
    stale leader that is still in Accept. See model/results/.
- S3 Only proposed values are decided.
- S4 After a decided StopSign, the old configuration decides no
  further entry.
- S5 For each ballot and index, all accepted entries are equal.
- S6 The first log of configuration c+1 extends each entry that was
  decided in c.
- S7 decided_idx never decreases at a node that did not lose its
  disk. A duplicated AcceptSync after a new Prepare with the same
  ballot (branch Phase::Recover in `handle_prepare`) can violate this.
  Such a find is a finding about Omni-Paxos.
  - Note (model): The constant DecidedIdxMonotone = TRUE is the form
    that this project uses. With it, the follower keeps
    max(old, new) decided_idx in AcceptSync. FALSE is the form of the
    Omni-Paxos source. It violates S7 (results file
    model/results/20261016T183659Z-a-base.md).
- S8 A snapshot up to index i is equal at all nodes. It is equal to
  the decided prefix up to i in `decided_history`.
- S9 A trim removes no entry that a node that votes still needs and
  that no snapshot covers.
- B1 (size) decided_idx - compacted_idx < K at each node. The part
  that is not decided depends on the proposals in flight. The bounds
  of the model limit it.
- Nodes that do not vote (empty storage, F4) are excluded from S1, S2
  and S7.

Liveness (model L), under the assumption of Omni-Paxos "there is a
server that is connected to a majority":

- L1 A leader in this majority reaches Phase::Accept, and decided_idx
  increases.
- L2 The three cases from the blog, as fixed sets of edges:
  quorum-loss and constrained with 5 nodes, chained with 3 nodes. L1
  holds in each. If TLC refutes this, it is a finding about
  Omni-Paxos.

## 5. Counter-proof

- Run G: model S with F4 and without rule R. Expected violation of
  S2: A and B accept v1 and decide it. B loses its disk. C sends
  Prepare with ballot 2. C gets a Promise from B and from C, both with
  n_accepted default. C proposes v2 and decides it. 3 nodes, ballot 2,
  log 1, 2 values.
- Run G has failed if TLC finds no violation. The trace is also a
  Quint test.

## 6. Bounds

- Model S: 3 nodes, 2 values, ballot n up to 3, log up to 3 (one
  entry plus StopSign is not sufficient for the branch with `min(...)`
  in `send_accsync`), session and counter up to 2, channel up to 2
  messages, crashes and wipes up to 1 each, K = 2. Then 2
  configurations with 4 IDs.
- Model L: 5 nodes, one value, the three sets of edges.
- At most 3 of 4 processors (TLC -workers 3).

## 7. What is measured and written down

For each TLC run, one file with a time stamp: the configuration file
with the bounds, the TLC version, the Java version, the number of
distinct states, the diameter, the wall time, the result, and the
trace if there is a violation. For each Quint run: the version, the
number of runs, the steps for each run, the seeds, the result.

End condition for each stage:

- Stage 1 (model S, TLC): done when all invariants hold at the bounds
  of section 6 and run G finds its violation. If a run takes longer
  than 2 h, the bound is lowered, and this is written down.
- Stage 2 (model L, TLC): done when L1 and L2 are decided, confirmed
  or refuted.
- Stage 3 (Quint): model S in Quint, `quint run` with 100 000 runs of
  50 steps each, run G as a test. Done when no violation occurs and
  run G fails.
- Stage 4 (inductive invariant with Apalache) gets a plan of its own.
  Candidates are checked in TLC with Init == Inv before.

Version 1 named snapshots as stage 4. They now belong to stage 1.

## 8. Location and tools

- Provisional: /mnt/project-files/actor-raft/model/.
- TLC 2.19 (tla2tools.jar from GitHub), Quint 0.33.0 (npm), Java 21,
  Node 22. None of these is a package of the distribution.

## 9. Questions to the owner

- Q1 Name of the repository: mbedded-konsens (owner, 18:32).
- Q2 Tools from outside the distribution for the models.
- Q3 Is Apalache sufficient for a fixed number of nodes, or is TLAPS
  necessary?
- Q5 How does the user say what the state means? a) a pure function
  as `Snapshot::create`/`merge` of Omni-Paxos; b) an annotation for
  each field (last value wins, counter, set), and the library folds
  by itself. Proposal: a as the base form, b later.
- Q4 Rule R is new: A node with empty storage does not vote until a
  configuration adds it under a new incarnation.

## 10. Changes from review 2 (these override)

1. Model L: After GST, one round of heartbeats is one atomic action.
   `hb_round` is kept modulo 2. Invariant n < MaxN in the liveness
   runs. Its violation reports a bound that was reached.
2. Trim without snapshot: `get_suffix` below compacted_idx ends in a
   panic in the source. The model has an error state for this. S9 now
   reads: No node reaches the error state. `accepted_indexes` is not
   reset in `reset_promise` and `lost_promise`, as in the source. The
   effective log is snapshot ⊕ suffix. Trim runs only in a run of its
   own (check of the hazard). The mechanism of compaction is the
   deterministic snapshot.
3. Rule K is our design, not the source. It applies at each place
   where decided_idx increases: `handle_accepted`,
   `update_decided_idx_and_get_accepted_idx`, `sync_log` at leader
   and follower. The StopSign does not count as an entry (the source:
   `get_decided_idx_without_stopsign`). `create_log_sync` sends a
   full snapshot up to ⌊decided/K⌋·K and the suffix from there.
4. Each (node, config_id) has its own storage and its own channels.
   The node discards a message with a different config_id. The
   incarnation is part of the identity of sender and receiver.
5. A node that does not vote sends no HeartbeatReply.
6. Counter up to the log bound + 2. A state that is cut off is
   counted and is in the result.
7. Window W (our design): A leader rejects a proposal when
   accepted_idx - decided_idx ≥ W. New invariant B2: accepted_idx -
   compacted_idx < K + W.
8. S9 as in item 2.
9. Model S in three runs, each with its own time: (a) F1, F3, F4, one
   configuration; (b) plus compaction; (c) plus reconfiguration. In
   addition, run G and the trim run.
10. Model L: The faults before GST only select the initial states.
11. A send to a full channel is a loss.
12. F5: After it detects a wrong checksum, the node deletes all of
    its storage. After that, rule R applies.
13. The start of the cluster writes a promise that is set. A crash
    after that leads to Phase::Recover.
14. Each node has a ghost variable with all of its decided prefix. It
    is not part of the durable state, and no action reads it. S1, S2,
    S2b and S6 check it. S8 checks the snapshot. Reason: A fold "last
    value wins" is not injective and hides a changed entry.
15. S8: The snapshot of each node that votes, up to i, is equal to
    fold(decided_history[1..i]). Without a snapshot, it is the empty
    map.
16. Law for the user: merge(create(a), create(b)) = create(a ++ b).
    The model checks it as an invariant of its own. The model takes
    the delta path of `sync_log` at least once. An arm with a merge
    that breaks the law must violate S8, as run G does.
17. The StopSign is not part of the fold. S8 and rule K count only
    entries of the log.
18. Bounds for compaction: 2 keys, 2 values, K smaller than the bound
    of the log.
19. B3: Cardinality(DOMAIN snapshot) ≤ |Keys|.
20. Q5 b is a new design with no source in Omni-Paxos. The proposal
    stays a.
21. Owner (LMDB thread, 2026-10-16): The user sets a maximum file
    size. Compaction runs so that it is not visible in the latency.
    Consequence for the model: The snapshot can occur at each decided
    index, not only at multiples of K. This includes rule K and a
    trigger by size. S8 applies to each index and does not change. B1
    applies only in the run with rule K. The reduction of the file
    (copy with MDB_CP_COMPACT) is local and is not part of the
    protocol. No model checks latency. It is measured on the C++
    code. If the file reaches the bound nevertheless, the leader
    rejects new proposals (window W). It does not break the bound.
22. Owner (LMDB thread, 17:42): Nothing is fixed for the file. LMDB
    stays LMDB and is observed in a development environment. The
    model keeps the snapshot in the protocol. The rejection of new
    proposals at the reached file size (item 21) is provisional. It
    depends on this observation.
23. From the independent review (bewertung.md), proposed, the owner
    decides:
    - S10: An installed snapshot never lowers promise and n_accepted.
      The snapshot in the model holds only the folded state, never
      promise or ballot. A snapshot as a full LMDB env would break
      this.
    - F6 Rolled-back disk: A node returns to an older, valid durable
      state (lost fsync, backup). Rule R detects only empty storage.
      Expectation: run F6 violates S2, as run G does. A defence needs
      a witness outside the node (for example: the other nodes record
      the highest promise of each node). This is a new design.
    - A snapshot at the same indexes on all nodes affects all of them
      at the same time. Item 21 already allows different indexes for
      each node. The model checks both forms.
24. Owner (17:49): LMDB is a detail of the implementation. Only the
    owner of the DB knows about it. Consequence: A snapshot travels
    as CBOR of the folded state, never as an LMDB file
    (mdb_env_copyfd2 is not used for InstallSnapshot). Thus S10 holds
    by the form. The model checks it nevertheless.
25. Owner (review thread, 17:52): The library does check by itself:
    CRC32C by hardware instruction, for each log entry, the meta DB
    (promise) and the snapshot block. Index and kind are part of the
    checksum. Consequence for the model: F5 is detected before a value
    is used. A detected fault counts provisionally as loss of all of
    the storage (rule R). A repair of only the bad entry from other
    nodes (CTRL, FAST 2018) is a separate, later design. A
    rolled-back, valid state (F6) has correct checksums. Thus this
    check does not detect it.
26. Owner (17:58): Put recovery from VR Revisited into the model, and
    check by a run whether it solves the case F6. Form (VR Revisited,
    section 4.3, names from the source: Recovery, RecoveryResponse,
    nonce): After each restart, a node votes again only after it sent
    a Recovery message with a fresh nonce and got f+1 matching
    RecoveryResponse messages. One of them must come from the current
    leader. The node takes their state (promise, log, decided_idx)
    before it gives a vote. Applied to Sequence Paxos: The response
    carries promise, n_accepted, accepted_idx and the suffix.
    Runs:
    - F6a without recovery, one node rolls back. Expectation: S2
      breaks.
    - F6b with recovery, one node rolls back. Expectation: all holds.
    - F6c with recovery, two of three nodes roll back at the same
      time. Expectation: S2 breaks. This is the limit of the defence.
    If a result differs from the expectation, this is a finding.
    Note: A commit that was not complete before the power failure is
    not a rollback. The node never confirmed it, because it replies
    only after fsync. F6 occurs only when a confirmed commit is
    missing (fsync gave a false result, backup).
27. Addition to 26: The fault action is Rollback(i). The durable
    state of i goes back to an earlier state of its own (promise,
    n_accepted, log, decided_idx) with a valid CRC. For this, the
    model keeps for each node the sequence of its earlier durable
    states. Liveness at the simultaneous restart of a majority is
    checked separately in model L (Vanlightly found gaps there). A
    full cluster from a backup is intended and is not a fault. The
    model excludes it.
28. Owner (18:06):
    - Compaction is postponed until it becomes necessary. Run (b) and
      items 14 to 19, 21 and B1 to B3 are suspended. The window W
      stays only as an idea.
    - Names and forms from Rust are allowed (also the source
      Omni-Paxos). etcd stays no model.
    - A client that crashed: its object is released through
      heartbeats. This belongs to the actor part, not to model S.
    - An entry counts as stored only after the synchronous commit of
      LMDB (default, with no flags). step blocks the calling thread.
      The library starts no thread. In the model, this is the atomic
      action from item 12 of review 1.
    Order now: run a and G, then F6a/b/c, then (c) reconfiguration,
    then model L, then Quint.
29. Owner (18:22, confirmed 18:23): explicit single-node mode for
    defective hardware (details in ../bewertung.md).
    - Case 1, the majority is alive: normal reconfiguration by
      StopSign down to one node. This belongs to run (c).
    - Case 2, the majority is dead: action ForceSingle(i), a command
      of the operator. It creates a new configuration with only i.
      Old nodes come back only with deleted storage under a new
      incarnation (rule R).
    - Run (d): Expectation 1: S1 holds across ForceSingle. No old node
      makes a second majority with the old configuration.
      Expectation 2: S2 breaks if and only if i did not have all
      decided entries. The model checks that the warning of the
      command (decided_idx and accepted_idx of i) describes this case
      correctly: If accepted_idx of i ≥ the highest index ever
      decided, nothing is lost.
    - Read: read index as the default, local read as an explicit
      choice (proposal, not confirmed). Not in model S.
30. Owner (18:28): A read in the cluster goes only through a read
    index (one round to the majority, batched, with no clock). A
    local read with no round occurs only in single-node mode.
    Proposal for the model, run (e): an action Read. It gets a read
    index from a majority. It replies only when decided_idx of the
    reader reaches that index. Invariant R1: A read sees each entry
    that was decided before the read started. Counter-check: A read
    with no round in the cluster must violate R1.
31. Owner (18:29 to 18:32): A client connects to any node. A follower
    gets the read index from the leader and forwards writes. Client
    session: the session ID is the index of the login entry in the
    log. The client numbers its requests. The state records the last
    number and the reply for each session. The end of a session
    (failure of the heartbeats) is an entry in the log.
    Proposal run (f): A client sends one request several times
    through different nodes. Invariant E1: Each (session, number) is
    applied at most once. A counter-check without the record of the
    number must violate E1.
    Repository: mbedded-konsens.

## 11. Checks of the checks (owner, 2026-10-17)

A run shows only that this model holds these properties within these
bounds. Three means make the gaps smaller.

1. Mutation. Each property has at least one counter-check: a model with
   a deliberate fault, on which the run must fail. Done so far: G (no
   rule R, S2 violated), F6a (no recovery, S2 violated),
   DecidedIdxMonotone = FALSE (S7 violated), and the constant Mutation
   with one fault each for S1, S2b, S3, S5, E1 and R1 (all six
   violated).
2. Bounds. For cases with no bound: inductive invariants with Apalache
   or proofs with TLAPS (open question Q3).
3. Code against model. Trace validation: the C++ code records each
   step, and TLC checks that the trace is allowed by the model. Tests in
   the manner of Jepsen come in addition.
