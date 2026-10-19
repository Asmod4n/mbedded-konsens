# TLC run split-Q1a-mutation-sim, 20261019T042345Z

Counter-check of plan-split-20261019 (9dfc42d), model d58cce1, 2 workers. Question Q1a with its mutation, random simulation (-simulate -deadlock -depth 60, seed 1, limit 15 min). Expected: a violation.

## Configuration

Spec: `model/tla/SequencePaxos.tla`. Config file `model/tla/Q1a_mut.sim.cfg`:

```
CONSTANTS
  Nodes = {1, 2, 3}
  Values = {v1, v2}
  MaxBallot = 1
  MaxLog = 1
  MaxCounter = 3
  MaxSession = 2
  Cap = 2
  MaxCrash = 1
  MaxWipe = 0
  MaxLoss = 0
  MaxDuplicate = 0
  MaxTimeout = 0
  MaxElection = 2
  RuleR = TRUE
  DecidedIdxMonotone = TRUE
  MaxRollback = 0
  Recovery = FALSE
  Mutation = "PromiseWithoutAccepted"
  ClientSessions = FALSE
  MaxClientSend = 0
  MaxRequest = 0
  ReadIndex = FALSE
  MaxRead = 0
  NextConfigNodes = {}
  MaxReconfigure = 0
  SingleNode = FALSE
  MaxForceSingle = 0
  MaxReadOnly = 0
  MaxHalt = 0
  StaleReadNode = FALSE
  Witness = FALSE
  LeaseLength = 0
SPECIFICATION Spec
CONSTRAINT Bounds
INVARIANTS S1
```

## Tools

- TLC2 Version 2.19 of 08 August 2024 (rev: 5a47802)
- Running Random Simulation with seed 1 with 2 workers on 4 cores with 9102MB heap and 64MB offheap memory [pid: 11212] (Linux 6.18.44-fc-v139 amd64, Ubuntu 21.0.12.1 x86_64).
- Java:

```
openjdk version "21.0.12.1" 2026-08-18
OpenJDK Runtime Environment (build 21.0.12.1+1-1-24.04.4-Ubuntu)
OpenJDK 64-Bit Server VM (build 21.0.12.1+1-1-24.04.4-Ubuntu, mixed mode, sharing)
```

## Numbers

- States generated: n/a
- Distinct states: n/a
- States left on queue: n/a
- Diameter (depth of the state graph search): n/a
- Wall time: 00s (start 20261019T042345Z, end 20261019T042346Z, UTC)

## Result

```
Error: Invariant S1 is violated.
Error: The behavior up to this point is:
Error: TLC threw an unexpected exception.
```

## Trace, changed variables per step

```
```

## Trace, full TLC output

```
Error: The behavior up to this point is:
State 1: <Initial predicate>
/\ decided_ballot = <<0>>
/\ wiped = <<FALSE, FALSE, FALSE>>
/\ lease_remaining = <<0, 0, 0>>
/\ promise = <<0, 0, 0>>
/\ node_epoch = <<0, 0, 0>>
/\ read_only = <<FALSE, FALSE, FALSE>>
/\ wipes = 0
/\ accepted_idx = <<0, 0, 0>>
/\ stage1_ghost = <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>
/\ recovery_responses = <<{}, {}, {}>>
/\ old_decided_after_forced = FALSE
/\ read_pending = <<{}, {}, {}>>
/\ read_round = <<[reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}]>>
/\ buffered_proposals = <<<<>>, <<>>, <<>>>>
/\ read_only_entries = 0
/\ force_single = [log |-> <<>>, node |-> 0, decided |-> <<>>, history |-> <<>>]
/\ recovering = <<FALSE, FALSE, FALSE>>
/\ decided_idx = <<0, 0, 0>>
/\ build_source = << >>
/\ role = <<"Follower", "Follower", "Follower">>
/\ phase = <<"None", "None", "None">>
/\ reads = << >>
/\ old_decided_max = 0
/\ running = {1, 2, 3}
/\ crashes = 0
/\ current_seq_num = << [session |-> 0, counter |-> 0],
   [session |-> 0, counter |-> 0],
   [session |-> 0, counter |-> 0] >>
/\ proposed = {}
/\ forced_decided_at_force = 0
/\ chan = ( <<1, 2>> :> <<>> @@
  <<1, 3>> :> <<>> @@
  <<2, 1>> :> <<>> @@
  <<2, 3>> :> <<>> @@
  <<3, 1>> :> <<>> @@
  <<3, 2>> :> <<>> )
/\ client = [session |-> 0, request_number |-> 1, sends |-> 0]
/\ past_durable = <<{}, {}, {}>>
/\ buffered_stopsign = <<<<>>, <<>>, <<>>>>
/\ reconfigurations = 0
/\ lease_request = <<-1, -1, -1>>
/\ decided_history = <<>>
/\ losses = 0
/\ witness_epoch = 0
/\ forced_has_decided = FALSE
/\ deactivated = <<FALSE, FALSE, FALSE>>
/\ first_log = <<<<>>, <<>>, <<>>>>
/\ wait_remaining = -1
/\ timeouts = 0
/\ duplicates = 0
/\ elections = 0
/\ leader_state = <<[n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>]>>
/\ rolled_back = {}
/\ lease_reply = <<-1, -1, -1>>
/\ nonce = <<0, 0, 0>>
/\ n_accepted = <<0, 0, 0>>
/\ halts = 0
/\ log = <<<<>>, <<>>, <<>>>>
/\ cached_promise_message = <<<<>>, <<>>, <<>>>>

State 2: <Next line 1930, col 6 to line 1930, col 62 of module SequencePaxos>
/\ decided_ballot = <<0>>
/\ wiped = <<FALSE, FALSE, FALSE>>
/\ lease_remaining = <<0, 0, 0>>
/\ promise = <<11, 0, 0>>
/\ node_epoch = <<0, 0, 0>>
/\ read_only = <<FALSE, FALSE, FALSE>>
/\ wipes = 0
/\ accepted_idx = <<0, 0, 0>>
/\ stage1_ghost = <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>
/\ recovery_responses = <<{}, {}, {}>>
/\ old_decided_after_forced = FALSE
/\ read_pending = <<{}, {}, {}>>
/\ read_round = <<[reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}]>>
/\ buffered_proposals = <<<<>>, <<>>, <<>>>>
/\ read_only_entries = 0
/\ force_single = [log |-> <<>>, node |-> 0, decided |-> <<>>, history |-> <<>>]
/\ recovering = <<FALSE, FALSE, FALSE>>
/\ decided_idx = <<0, 0, 0>>
/\ build_source = << >>
/\ role = <<"Leader", "Follower", "Follower">>
/\ phase = <<"Prepare", "None", "None">>
/\ reads = << >>
/\ old_decided_max = 0
/\ running = {1, 2, 3}
/\ crashes = 0
/\ current_seq_num = << [session |-> 0, counter |-> 0],
   [session |-> 0, counter |-> 0],
   [session |-> 0, counter |-> 0] >>
/\ proposed = {}
/\ forced_decided_at_force = 0
/\ chan = (<<1, 2>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<1, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 1>> :> <<>> @@ <<2, 3>> :> <<>> @@ <<3, 1>> :> <<>> @@ <<3, 2>> :> <<>>)
/\ client = [session |-> 0, request_number |-> 1, sends |-> 0]
/\ past_durable = <<{}, {}, {}>>
/\ buffered_stopsign = <<<<>>, <<>>, <<>>>>
/\ reconfigurations = 0
/\ lease_request = <<-1, -1, -1>>
/\ decided_history = <<>>
/\ losses = 0
/\ witness_epoch = 0
/\ forced_has_decided = FALSE
/\ deactivated = <<FALSE, FALSE, FALSE>>
/\ first_log = <<<<>>, <<>>, <<>>>>
/\ wait_remaining = -1
/\ timeouts = 0
/\ duplicates = 0
/\ elections = 1
/\ leader_state = <<[n_leader |-> 11, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 1], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>]>>
/\ rolled_back = {}
/\ lease_reply = <<-1, -1, -1>>
/\ nonce = <<0, 0, 0>>
/\ n_accepted = <<0, 0, 0>>
/\ halts = 0
/\ log = <<<<>>, <<>>, <<>>>>
/\ cached_promise_message = <<<<>>, <<>>, <<>>>>

State 3: <Receive line 1138, col 3 to line 1154, col 16 of module SequencePaxos>
/\ decided_ballot = <<0>>
/\ wiped = <<FALSE, FALSE, FALSE>>
/\ lease_remaining = <<0, 0, 0>>
/\ promise = <<11, 11, 0>>
/\ node_epoch = <<0, 0, 0>>
/\ read_only = <<FALSE, FALSE, FALSE>>
/\ wipes = 0
/\ accepted_idx = <<0, 0, 0>>
/\ stage1_ghost = <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>
/\ recovery_responses = <<{}, {}, {}>>
/\ old_decided_after_forced = FALSE
/\ read_pending = <<{}, {}, {}>>
/\ read_round = <<[reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}]>>
/\ buffered_proposals = <<<<>>, <<>>, <<>>>>
/\ read_only_entries = 0
/\ force_single = [log |-> <<>>, node |-> 0, decided |-> <<>>, history |-> <<>>]
/\ recovering = <<FALSE, FALSE, FALSE>>
/\ decided_idx = <<0, 0, 0>>
/\ build_source = << >>
/\ role = <<"Leader", "Follower", "Follower">>
/\ phase = <<"Prepare", "Prepare", "None">>
/\ reads = << >>
/\ old_decided_max = 0
/\ running = {1, 2, 3}
/\ crashes = 0
/\ current_seq_num = << [session |-> 0, counter |-> 0],
   [session |-> 0, counter |-> 0],
   [session |-> 0, counter |-> 0] >>
/\ proposed = {}
/\ forced_decided_at_force = 0
/\ chan = (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 3>> :> <<>> @@ <<3, 1>> :> <<>> @@ <<3, 2>> :> <<>>)
/\ client = [session |-> 0, request_number |-> 1, sends |-> 0]
/\ past_durable = <<{}, {}, {}>>
/\ buffered_stopsign = <<<<>>, <<>>, <<>>>>
/\ reconfigurations = 0
/\ lease_request = <<-1, -1, -1>>
/\ decided_history = <<>>
/\ losses = 0
/\ witness_epoch = 0
/\ forced_has_decided = FALSE
/\ deactivated = <<FALSE, FALSE, FALSE>>
/\ first_log = <<<<>>, <<>>, <<>>>>
/\ wait_remaining = -1
/\ timeouts = 0
/\ duplicates = 0
/\ elections = 1
/\ leader_state = <<[n_leader |-> 11, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 1], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>]>>
/\ rolled_back = {}
/\ lease_reply = <<-1, -1, -1>>
/\ nonce = <<0, 0, 0>>
/\ n_accepted = <<0, 0, 0>>
/\ halts = 0
/\ log = <<<<>>, <<>>, <<>>>>
/\ cached_promise_message = <<<<>>, <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>>, <<>>>>

State 4: <Next line 1932, col 6 to line 1932, col 32 of module SequencePaxos>
/\ decided_ballot = <<0>>
/\ wiped = <<FALSE, FALSE, FALSE>>
/\ lease_remaining = <<0, 0, 0>>
/\ promise = <<11, 11, 0>>
/\ node_epoch = <<0, 0, 0>>
/\ read_only = <<FALSE, FALSE, FALSE>>
/\ wipes = 0
/\ accepted_idx = <<0, 0, 0>>
/\ stage1_ghost = <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>
/\ recovery_responses = <<{}, {}, {}>>
/\ old_decided_after_forced = FALSE
/\ read_pending = <<{}, {}, {}>>
/\ read_round = <<[reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}]>>
/\ buffered_proposals = <<<<>>, <<>>, <<>>>>
/\ read_only_entries = 0
/\ force_single = [log |-> <<>>, node |-> 0, decided |-> <<>>, history |-> <<>>]
/\ recovering = <<FALSE, FALSE, FALSE>>
/\ decided_idx = <<0, 0, 0>>
/\ build_source = << >>
/\ role = <<"Leader", "Follower", "Follower">>
/\ phase = <<"Prepare", "Recover", "None">>
/\ reads = << >>
/\ old_decided_max = 0
/\ running = {1, 2, 3}
/\ crashes = 1
/\ current_seq_num = << [session |-> 0, counter |-> 0],
   [session |-> 0, counter |-> 0],
   [session |-> 0, counter |-> 0] >>
/\ proposed = {}
/\ forced_decided_at_force = 0
/\ chan = (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0], [promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "PrepareReq", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "PrepareReq", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 1>> :> <<>> @@ <<3, 2>> :> <<>>)
/\ client = [session |-> 0, request_number |-> 1, sends |-> 0]
/\ past_durable = <<{}, {}, {}>>
/\ buffered_stopsign = <<<<>>, <<>>, <<>>>>
/\ reconfigurations = 0
/\ lease_request = <<-1, -1, -1>>
/\ decided_history = <<>>
/\ losses = 0
/\ witness_epoch = 0
/\ forced_has_decided = FALSE
/\ deactivated = <<FALSE, FALSE, FALSE>>
/\ first_log = <<<<>>, <<>>, <<>>>>
/\ wait_remaining = -1
/\ timeouts = 0
/\ duplicates = 0
/\ elections = 1
/\ leader_state = <<[n_leader |-> 11, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 1], max_promise_sync |-> <<>>], [n_leader |-> 11, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>]>>
/\ rolled_back = {}
/\ lease_reply = <<-1, -1, -1>>
/\ nonce = <<0, 0, 0>>
/\ n_accepted = <<0, 0, 0>>
/\ halts = 0
/\ log = <<<<>>, <<>>, <<>>>>
/\ cached_promise_message = <<<<>>, <<>>, <<>>>>

State 5: <Next line 1930, col 6 to line 1930, col 62 of module SequencePaxos>
/\ decided_ballot = <<0>>
/\ wiped = <<FALSE, FALSE, FALSE>>
/\ lease_remaining = <<0, 0, 0>>
/\ promise = <<11, 12, 0>>
/\ node_epoch = <<0, 0, 0>>
/\ read_only = <<FALSE, FALSE, FALSE>>
/\ wipes = 0
/\ accepted_idx = <<0, 0, 0>>
/\ stage1_ghost = <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>
/\ recovery_responses = <<{}, {}, {}>>
/\ old_decided_after_forced = FALSE
/\ read_pending = <<{}, {}, {}>>
/\ read_round = <<[reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}]>>
/\ buffered_proposals = <<<<>>, <<>>, <<>>>>
/\ read_only_entries = 0
/\ force_single = [log |-> <<>>, node |-> 0, decided |-> <<>>, history |-> <<>>]
/\ recovering = <<FALSE, FALSE, FALSE>>
/\ decided_idx = <<0, 0, 0>>
/\ build_source = << >>
/\ role = <<"Leader", "Leader", "Follower">>
/\ phase = <<"Prepare", "Prepare", "None">>
/\ reads = << >>
/\ old_decided_max = 0
/\ running = {1, 2, 3}
/\ crashes = 1
/\ current_seq_num = << [session |-> 0, counter |-> 0],
   [session |-> 0, counter |-> 0],
   [session |-> 0, counter |-> 0] >>
/\ proposed = {}
/\ forced_decided_at_force = 0
/\ chan = (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0], [promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "PrepareReq", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "PrepareReq", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0], [promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 1>> :> <<>> @@ <<3, 2>> :> <<>>)
/\ client = [session |-> 0, request_number |-> 1, sends |-> 0]
/\ past_durable = <<{}, {}, {}>>
/\ buffered_stopsign = <<<<>>, <<>>, <<>>>>
/\ reconfigurations = 0
/\ lease_request = <<-1, -1, -1>>
/\ decided_history = <<>>
/\ losses = 0
/\ witness_epoch = 0
/\ forced_has_decided = FALSE
/\ deactivated = <<FALSE, FALSE, FALSE>>
/\ first_log = <<<<>>, <<>>, <<>>>>
/\ wait_remaining = -1
/\ timeouts = 0
/\ duplicates = 0
/\ elections = 2
/\ leader_state = <<[n_leader |-> 11, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 1], max_promise_sync |-> <<>>], [n_leader |-> 12, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>]>>
/\ rolled_back = {}
/\ lease_reply = <<-1, -1, -1>>
/\ nonce = <<0, 0, 0>>
/\ n_accepted = <<0, 0, 0>>
/\ halts = 0
/\ log = <<<<>>, <<>>, <<>>>>
/\ cached_promise_message = <<<<>>, <<>>, <<>>>>

State 6: <Next line 1931, col 6 to line 1931, col 51 of module SequencePaxos>
/\ decided_ballot = <<0>>
/\ wiped = <<FALSE, FALSE, FALSE>>
/\ lease_remaining = <<0, 0, 0>>
/\ promise = <<11, 12, 0>>
/\ node_epoch = <<0, 0, 0>>
/\ read_only = <<FALSE, FALSE, FALSE>>
/\ wipes = 0
/\ accepted_idx = <<0, 0, 0>>
/\ stage1_ghost = <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>
/\ recovery_responses = <<{}, {}, {}>>
/\ old_decided_after_forced = FALSE
/\ read_pending = <<{}, {}, {}>>
/\ read_round = <<[reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}]>>
/\ buffered_proposals = <<<<>>, <<>>, <<v2>>>>
/\ read_only_entries = 0
/\ force_single = [log |-> <<>>, node |-> 0, decided |-> <<>>, history |-> <<>>]
/\ recovering = <<FALSE, FALSE, FALSE>>
/\ decided_idx = <<0, 0, 0>>
/\ build_source = << >>
/\ role = <<"Leader", "Leader", "Follower">>
/\ phase = <<"Prepare", "Prepare", "None">>
/\ reads = << >>
/\ old_decided_max = 0
/\ running = {1, 2, 3}
/\ crashes = 1
/\ current_seq_num = << [session |-> 0, counter |-> 0],
   [session |-> 0, counter |-> 0],
   [session |-> 0, counter |-> 0] >>
/\ proposed = {v2}
/\ forced_decided_at_force = 0
/\ chan = (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0], [promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "PrepareReq", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "PrepareReq", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0], [promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 1>> :> <<>> @@ <<3, 2>> :> <<>>)
/\ client = [session |-> 0, request_number |-> 1, sends |-> 0]
/\ past_durable = <<{}, {}, {}>>
/\ buffered_stopsign = <<<<>>, <<>>, <<>>>>
/\ reconfigurations = 0
/\ lease_request = <<-1, -1, -1>>
/\ decided_history = <<>>
/\ losses = 0
/\ witness_epoch = 0
/\ forced_has_decided = FALSE
/\ deactivated = <<FALSE, FALSE, FALSE>>
/\ first_log = <<<<>>, <<>>, <<>>>>
/\ wait_remaining = -1
/\ timeouts = 0
/\ duplicates = 0
/\ elections = 2
/\ leader_state = <<[n_leader |-> 11, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 1], max_promise_sync |-> <<>>], [n_leader |-> 12, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>]>>
/\ rolled_back = {}
/\ lease_reply = <<-1, -1, -1>>
/\ nonce = <<0, 0, 0>>
/\ n_accepted = <<0, 0, 0>>
/\ halts = 0
/\ log = <<<<>>, <<>>, <<>>>>
/\ cached_promise_message = <<<<>>, <<>>, <<>>>>

State 7: <Next line 1931, col 6 to line 1931, col 51 of module SequencePaxos>
/\ decided_ballot = <<0>>
/\ wiped = <<FALSE, FALSE, FALSE>>
/\ lease_remaining = <<0, 0, 0>>
/\ promise = <<11, 12, 0>>
/\ node_epoch = <<0, 0, 0>>
/\ read_only = <<FALSE, FALSE, FALSE>>
/\ wipes = 0
/\ accepted_idx = <<0, 0, 0>>
/\ stage1_ghost = <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>
/\ recovery_responses = <<{}, {}, {}>>
/\ old_decided_after_forced = FALSE
/\ read_pending = <<{}, {}, {}>>
/\ read_round = <<[reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}]>>
/\ buffered_proposals = <<<<>>, <<v1>>, <<v2>>>>
/\ read_only_entries = 0
/\ force_single = [log |-> <<>>, node |-> 0, decided |-> <<>>, history |-> <<>>]
/\ recovering = <<FALSE, FALSE, FALSE>>
/\ decided_idx = <<0, 0, 0>>
/\ build_source = << >>
/\ role = <<"Leader", "Leader", "Follower">>
/\ phase = <<"Prepare", "Prepare", "None">>
/\ reads = << >>
/\ old_decided_max = 0
/\ running = {1, 2, 3}
/\ crashes = 1
/\ current_seq_num = << [session |-> 0, counter |-> 0],
   [session |-> 0, counter |-> 0],
   [session |-> 0, counter |-> 0] >>
/\ proposed = {v1, v2}
/\ forced_decided_at_force = 0
/\ chan = (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0], [promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "PrepareReq", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "PrepareReq", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0], [promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 1>> :> <<>> @@ <<3, 2>> :> <<>>)
/\ client = [session |-> 0, request_number |-> 1, sends |-> 0]
/\ past_durable = <<{}, {}, {}>>
/\ buffered_stopsign = <<<<>>, <<>>, <<>>>>
/\ reconfigurations = 0
/\ lease_request = <<-1, -1, -1>>
/\ decided_history = <<>>
/\ losses = 0
/\ witness_epoch = 0
/\ forced_has_decided = FALSE
/\ deactivated = <<FALSE, FALSE, FALSE>>
/\ first_log = <<<<>>, <<>>, <<>>>>
/\ wait_remaining = -1
/\ timeouts = 0
/\ duplicates = 0
/\ elections = 2
/\ leader_state = <<[n_leader |-> 11, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 1], max_promise_sync |-> <<>>], [n_leader |-> 12, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>]>>
/\ rolled_back = {}
/\ lease_reply = <<-1, -1, -1>>
/\ nonce = <<0, 0, 0>>
/\ n_accepted = <<0, 0, 0>>
/\ halts = 0
/\ log = <<<<>>, <<>>, <<>>>>
/\ cached_promise_message = <<<<>>, <<>>, <<>>>>

State 8: <Receive line 1138, col 3 to line 1154, col 16 of module SequencePaxos>
/\ decided_ballot = <<0>>
/\ wiped = <<FALSE, FALSE, FALSE>>
/\ lease_remaining = <<0, 0, 0>>
/\ promise = <<11, 12, 11>>
/\ node_epoch = <<0, 0, 0>>
/\ read_only = <<FALSE, FALSE, FALSE>>
/\ wipes = 0
/\ accepted_idx = <<0, 0, 0>>
/\ stage1_ghost = <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>
/\ recovery_responses = <<{}, {}, {}>>
/\ old_decided_after_forced = FALSE
/\ read_pending = <<{}, {}, {}>>
/\ read_round = <<[reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}]>>
/\ buffered_proposals = <<<<>>, <<v1>>, <<v2>>>>
/\ read_only_entries = 0
/\ force_single = [log |-> <<>>, node |-> 0, decided |-> <<>>, history |-> <<>>]
/\ recovering = <<FALSE, FALSE, FALSE>>
/\ decided_idx = <<0, 0, 0>>
/\ build_source = << >>
/\ role = <<"Leader", "Leader", "Follower">>
/\ phase = <<"Prepare", "Prepare", "Prepare">>
/\ reads = << >>
/\ old_decided_max = 0
/\ running = {1, 2, 3}
/\ crashes = 1
/\ current_seq_num = << [session |-> 0, counter |-> 0],
   [session |-> 0, counter |-> 0],
   [session |-> 0, counter |-> 0] >>
/\ proposed = {v1, v2}
/\ forced_decided_at_force = 0
/\ chan = (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<>> @@ <<2, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0], [promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "PrepareReq", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "PrepareReq", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0], [promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 2>> :> <<>>)
/\ client = [session |-> 0, request_number |-> 1, sends |-> 0]
/\ past_durable = <<{}, {}, {}>>
/\ buffered_stopsign = <<<<>>, <<>>, <<>>>>
/\ reconfigurations = 0
/\ lease_request = <<-1, -1, -1>>
/\ decided_history = <<>>
/\ losses = 0
/\ witness_epoch = 0
/\ forced_has_decided = FALSE
/\ deactivated = <<FALSE, FALSE, FALSE>>
/\ first_log = <<<<>>, <<>>, <<>>>>
/\ wait_remaining = -1
/\ timeouts = 0
/\ duplicates = 0
/\ elections = 2
/\ leader_state = <<[n_leader |-> 11, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 1], max_promise_sync |-> <<>>], [n_leader |-> 12, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>]>>
/\ rolled_back = {}
/\ lease_reply = <<-1, -1, -1>>
/\ nonce = <<0, 0, 0>>
/\ n_accepted = <<0, 0, 0>>
/\ halts = 0
/\ log = <<<<>>, <<>>, <<>>>>
/\ cached_promise_message = <<<<>>, <<>>, <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>>>>

State 9: <Receive line 1138, col 3 to line 1154, col 16 of module SequencePaxos>
/\ decided_ballot = <<0>>
/\ wiped = <<FALSE, FALSE, FALSE>>
/\ lease_remaining = <<0, 0, 0>>
/\ promise = <<11, 12, 11>>
/\ node_epoch = <<0, 0, 0>>
/\ read_only = <<FALSE, FALSE, FALSE>>
/\ wipes = 0
/\ accepted_idx = <<0, 0, 0>>
/\ stage1_ghost = <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>
/\ recovery_responses = <<{}, {}, {}>>
/\ old_decided_after_forced = FALSE
/\ read_pending = <<{}, {}, {}>>
/\ read_round = <<[reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}]>>
/\ buffered_proposals = <<<<>>, <<v1>>, <<v2>>>>
/\ read_only_entries = 0
/\ force_single = [log |-> <<>>, node |-> 0, decided |-> <<>>, history |-> <<>>]
/\ recovering = <<FALSE, FALSE, FALSE>>
/\ decided_idx = <<0, 0, 0>>
/\ build_source = << >>
/\ role = <<"Leader", "Leader", "Follower">>
/\ phase = <<"Accept", "Prepare", "Prepare">>
/\ reads = << >>
/\ old_decided_max = 0
/\ running = {1, 2, 3}
/\ crashes = 1
/\ current_seq_num = << [session |-> 0, counter |-> 0],
   [session |-> 0, counter |-> 0],
   [session |-> 0, counter |-> 0] >>
/\ proposed = {v1, v2}
/\ forced_decided_at_force = 0
/\ chan = (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "AcceptSync", log_sync |-> <<[suffix |-> <<>>, sync_idx |-> 0]>>, seq_num |-> [session |-> 1, counter |-> 1], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0], [promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "PrepareReq", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "PrepareReq", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0], [promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 1>> :> <<>> @@ <<3, 2>> :> <<>>)
/\ client = [session |-> 0, request_number |-> 1, sends |-> 0]
/\ past_durable = <<{}, {}, {}>>
/\ buffered_stopsign = <<<<>>, <<>>, <<>>>>
/\ reconfigurations = 0
/\ lease_request = <<-1, -1, -1>>
/\ decided_history = <<>>
/\ losses = 0
/\ witness_epoch = 0
/\ forced_has_decided = FALSE
/\ deactivated = <<FALSE, FALSE, FALSE>>
/\ first_log = <<<<>>, <<>>, <<>>>>
/\ wait_remaining = -1
/\ timeouts = 0
/\ duplicates = 0
/\ elections = 2
/\ leader_state = <<[n_leader |-> 11, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 1, counter |-> 1]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 3], max_promise_sync |-> <<>>], [n_leader |-> 12, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>]>>
/\ rolled_back = {}
/\ lease_reply = <<-1, -1, -1>>
/\ nonce = <<0, 0, 0>>
/\ n_accepted = <<11, 0, 0>>
/\ halts = 0
/\ log = <<<<>>, <<>>, <<>>>>
/\ cached_promise_message = <<<<>>, <<>>, <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>>>>

State 10: <Receive line 1138, col 3 to line 1154, col 16 of module SequencePaxos>
/\ decided_ballot = <<0>>
/\ wiped = <<FALSE, FALSE, FALSE>>
/\ lease_remaining = <<0, 0, 0>>
/\ promise = <<11, 12, 11>>
/\ node_epoch = <<0, 0, 0>>
/\ read_only = <<FALSE, FALSE, FALSE>>
/\ wipes = 0
/\ accepted_idx = <<0, 0, 0>>
/\ stage1_ghost = <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>
/\ recovery_responses = <<{}, {}, {}>>
/\ old_decided_after_forced = FALSE
/\ read_pending = <<{}, {}, {}>>
/\ read_round = <<[reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}]>>
/\ buffered_proposals = <<<<>>, <<v1>>, <<>>>>
/\ read_only_entries = 0
/\ force_single = [log |-> <<>>, node |-> 0, decided |-> <<>>, history |-> <<>>]
/\ recovering = <<FALSE, FALSE, FALSE>>
/\ decided_idx = <<0, 0, 0>>
/\ build_source = << >>
/\ role = <<"Leader", "Leader", "Follower">>
/\ phase = <<"Accept", "Prepare", "Accept">>
/\ reads = << >>
/\ old_decided_max = 0
/\ running = {1, 2, 3}
/\ crashes = 1
/\ current_seq_num = << [session |-> 0, counter |-> 0],
   [session |-> 0, counter |-> 0],
   [session |-> 1, counter |-> 1] >>
/\ proposed = {v1, v2}
/\ forced_decided_at_force = 0
/\ chan = (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<>> @@ <<2, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0], [promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "PrepareReq", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "PrepareReq", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0], [promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 0, type |-> "ProposalForward", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<v2>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0], [promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "Accepted", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 2>> :> <<>>)
/\ client = [session |-> 0, request_number |-> 1, sends |-> 0]
/\ past_durable = <<{}, {}, {}>>
/\ buffered_stopsign = <<<<>>, <<>>, <<>>>>
/\ reconfigurations = 0
/\ lease_request = <<-1, -1, -1>>
/\ decided_history = <<>>
/\ losses = 0
/\ witness_epoch = 0
/\ forced_has_decided = FALSE
/\ deactivated = <<FALSE, FALSE, FALSE>>
/\ first_log = <<<<>>, <<>>, <<>>>>
/\ wait_remaining = -1
/\ timeouts = 0
/\ duplicates = 0
/\ elections = 2
/\ leader_state = <<[n_leader |-> 11, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 1, counter |-> 1]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 3], max_promise_sync |-> <<>>], [n_leader |-> 12, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>]>>
/\ rolled_back = {}
/\ lease_reply = <<-1, -1, -1>>
/\ nonce = <<0, 0, 0>>
/\ n_accepted = <<11, 0, 11>>
/\ halts = 0
/\ log = <<<<>>, <<>>, <<>>>>
/\ cached_promise_message = <<<<>>, <<>>, <<>>>>

State 11: <Receive line 1138, col 3 to line 1154, col 16 of module SequencePaxos>
/\ decided_ballot = <<0>>
/\ wiped = <<FALSE, FALSE, FALSE>>
/\ lease_remaining = <<0, 0, 0>>
/\ promise = <<11, 12, 11>>
/\ node_epoch = <<0, 0, 0>>
/\ read_only = <<FALSE, FALSE, FALSE>>
/\ wipes = 0
/\ accepted_idx = <<1, 0, 0>>
/\ stage1_ghost = <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>
/\ recovery_responses = <<{}, {}, {}>>
/\ old_decided_after_forced = FALSE
/\ read_pending = <<{}, {}, {}>>
/\ read_round = <<[reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}]>>
/\ buffered_proposals = <<<<>>, <<v1>>, <<>>>>
/\ read_only_entries = 0
/\ force_single = [log |-> <<>>, node |-> 0, decided |-> <<>>, history |-> <<>>]
/\ recovering = <<FALSE, FALSE, FALSE>>
/\ decided_idx = <<0, 0, 0>>
/\ build_source = << >>
/\ role = <<"Leader", "Leader", "Follower">>
/\ phase = <<"Accept", "Prepare", "Accept">>
/\ reads = << >>
/\ old_decided_max = 0
/\ running = {1, 2, 3}
/\ crashes = 1
/\ current_seq_num = << [session |-> 0, counter |-> 0],
   [session |-> 0, counter |-> 0],
   [session |-> 1, counter |-> 1] >>
/\ proposed = {v1, v2}
/\ forced_decided_at_force = 0
/\ chan = (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "AcceptDecide", log_sync |-> <<>>, seq_num |-> [session |-> 1, counter |-> 2], entries |-> <<v2>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0], [promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "PrepareReq", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "PrepareReq", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0], [promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "Accepted", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 2>> :> <<>>)
/\ client = [session |-> 0, request_number |-> 1, sends |-> 0]
/\ past_durable = <<{}, {}, {}>>
/\ buffered_stopsign = <<<<>>, <<>>, <<>>>>
/\ reconfigurations = 0
/\ lease_request = <<-1, -1, -1>>
/\ decided_history = <<>>
/\ losses = 0
/\ witness_epoch = 0
/\ forced_has_decided = FALSE
/\ deactivated = <<FALSE, FALSE, FALSE>>
/\ first_log = <<<<>>, <<>>, <<>>>>
/\ wait_remaining = -1
/\ timeouts = 0
/\ duplicates = 0
/\ elections = 2
/\ leader_state = <<[n_leader |-> 11, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 1, counter |-> 2]>>, accepted_indexes |-> <<1, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 3], max_promise_sync |-> <<>>], [n_leader |-> 12, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>]>>
/\ rolled_back = {}
/\ lease_reply = <<-1, -1, -1>>
/\ nonce = <<0, 0, 0>>
/\ n_accepted = <<11, 0, 11>>
/\ halts = 0
/\ log = <<<<v2>>, <<>>, <<>>>>
/\ cached_promise_message = <<<<>>, <<>>, <<>>>>

State 12: <Receive line 1138, col 3 to line 1154, col 16 of module SequencePaxos>
/\ decided_ballot = <<0>>
/\ wiped = <<FALSE, FALSE, FALSE>>
/\ lease_remaining = <<0, 0, 0>>
/\ promise = <<11, 12, 11>>
/\ node_epoch = <<0, 0, 0>>
/\ read_only = <<FALSE, FALSE, FALSE>>
/\ wipes = 0
/\ accepted_idx = <<1, 0, 0>>
/\ stage1_ghost = <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>
/\ recovery_responses = <<{}, {}, {}>>
/\ old_decided_after_forced = FALSE
/\ read_pending = <<{}, {}, {}>>
/\ read_round = <<[reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}]>>
/\ buffered_proposals = <<<<>>, <<v1>>, <<>>>>
/\ read_only_entries = 0
/\ force_single = [log |-> <<>>, node |-> 0, decided |-> <<>>, history |-> <<>>]
/\ recovering = <<FALSE, FALSE, FALSE>>
/\ decided_idx = <<0, 0, 0>>
/\ build_source = << >>
/\ role = <<"Leader", "Leader", "Follower">>
/\ phase = <<"Accept", "Prepare", "Accept">>
/\ reads = << >>
/\ old_decided_max = 0
/\ running = {1, 2, 3}
/\ crashes = 1
/\ current_seq_num = << [session |-> 0, counter |-> 0],
   [session |-> 0, counter |-> 0],
   [session |-> 1, counter |-> 1] >>
/\ proposed = {v1, v2}
/\ forced_decided_at_force = 0
/\ chan = (<<1, 2>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "AcceptSync", log_sync |-> <<[suffix |-> <<v2>>, sync_idx |-> 0]>>, seq_num |-> [session |-> 1, counter |-> 1], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<1, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "AcceptDecide", log_sync |-> <<>>, seq_num |-> [session |-> 1, counter |-> 2], entries |-> <<v2>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "PrepareReq", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "PrepareReq", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0], [promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "Accepted", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 2>> :> <<>>)
/\ client = [session |-> 0, request_number |-> 1, sends |-> 0]
/\ past_durable = <<{}, {}, {}>>
/\ buffered_stopsign = <<<<>>, <<>>, <<>>>>
/\ reconfigurations = 0
/\ lease_request = <<-1, -1, -1>>
/\ decided_history = <<>>
/\ losses = 0
/\ witness_epoch = 0
/\ forced_has_decided = FALSE
/\ deactivated = <<FALSE, FALSE, FALSE>>
/\ first_log = <<<<>>, <<>>, <<>>>>
/\ wait_remaining = -1
/\ timeouts = 0
/\ duplicates = 0
/\ elections = 2
/\ leader_state = <<[n_leader |-> 11, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 1, counter |-> 1], [session |-> 1, counter |-> 2]>>, accepted_indexes |-> <<1, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 3], max_promise_sync |-> <<>>], [n_leader |-> 12, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>]>>
/\ rolled_back = {}
/\ lease_reply = <<-1, -1, -1>>
/\ nonce = <<0, 0, 0>>
/\ n_accepted = <<11, 0, 11>>
/\ halts = 0
/\ log = <<<<v2>>, <<>>, <<>>>>
/\ cached_promise_message = <<<<>>, <<>>, <<>>>>

State 13: <Receive line 1138, col 3 to line 1154, col 16 of module SequencePaxos>
/\ decided_ballot = <<0>>
/\ wiped = <<FALSE, FALSE, FALSE>>
/\ lease_remaining = <<0, 0, 0>>
/\ promise = <<11, 12, 11>>
/\ node_epoch = <<0, 0, 0>>
/\ read_only = <<FALSE, FALSE, FALSE>>
/\ wipes = 0
/\ accepted_idx = <<1, 0, 0>>
/\ stage1_ghost = <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>
/\ recovery_responses = <<{}, {}, {}>>
/\ old_decided_after_forced = FALSE
/\ read_pending = <<{}, {}, {}>>
/\ read_round = <<[reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}]>>
/\ buffered_proposals = <<<<>>, <<v1>>, <<>>>>
/\ read_only_entries = 0
/\ force_single = [log |-> <<>>, node |-> 0, decided |-> <<>>, history |-> <<>>]
/\ recovering = <<FALSE, FALSE, FALSE>>
/\ decided_idx = <<0, 0, 0>>
/\ build_source = << >>
/\ role = <<"Leader", "Leader", "Follower">>
/\ phase = <<"Accept", "Prepare", "Accept">>
/\ reads = << >>
/\ old_decided_max = 0
/\ running = {1, 2, 3}
/\ crashes = 1
/\ current_seq_num = << [session |-> 0, counter |-> 0],
   [session |-> 0, counter |-> 0],
   [session |-> 1, counter |-> 1] >>
/\ proposed = {v1, v2}
/\ forced_decided_at_force = 0
/\ chan = (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "AcceptDecide", log_sync |-> <<>>, seq_num |-> [session |-> 1, counter |-> 2], entries |-> <<v2>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "PrepareReq", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0], [promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "NotAccepted", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "PrepareReq", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0], [promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "Accepted", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 2>> :> <<>>)
/\ client = [session |-> 0, request_number |-> 1, sends |-> 0]
/\ past_durable = <<{}, {}, {}>>
/\ buffered_stopsign = <<<<>>, <<>>, <<>>>>
/\ reconfigurations = 0
/\ lease_request = <<-1, -1, -1>>
/\ decided_history = <<>>
/\ losses = 0
/\ witness_epoch = 0
/\ forced_has_decided = FALSE
/\ deactivated = <<FALSE, FALSE, FALSE>>
/\ first_log = <<<<>>, <<>>, <<>>>>
/\ wait_remaining = -1
/\ timeouts = 0
/\ duplicates = 0
/\ elections = 2
/\ leader_state = <<[n_leader |-> 11, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 1, counter |-> 1], [session |-> 1, counter |-> 2]>>, accepted_indexes |-> <<1, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 3], max_promise_sync |-> <<>>], [n_leader |-> 12, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>]>>
/\ rolled_back = {}
/\ lease_reply = <<-1, -1, -1>>
/\ nonce = <<0, 0, 0>>
/\ n_accepted = <<11, 0, 11>>
/\ halts = 0
/\ log = <<<<v2>>, <<>>, <<>>>>
/\ cached_promise_message = <<<<>>, <<>>, <<>>>>

State 14: <Receive line 1138, col 3 to line 1154, col 16 of module SequencePaxos>
/\ decided_ballot = <<0>>
/\ wiped = <<FALSE, FALSE, FALSE>>
/\ lease_remaining = <<0, 0, 0>>
/\ promise = <<11, 12, 11>>
/\ node_epoch = <<0, 0, 0>>
/\ read_only = <<FALSE, FALSE, FALSE>>
/\ wipes = 0
/\ accepted_idx = <<1, 0, 0>>
/\ stage1_ghost = <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>
/\ recovery_responses = <<{}, {}, {}>>
/\ old_decided_after_forced = FALSE
/\ read_pending = <<{}, {}, {}>>
/\ read_round = <<[reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}]>>
/\ buffered_proposals = <<<<>>, <<v1>>, <<>>>>
/\ read_only_entries = 0
/\ force_single = [log |-> <<>>, node |-> 0, decided |-> <<>>, history |-> <<>>]
/\ recovering = <<FALSE, FALSE, FALSE>>
/\ decided_idx = <<0, 0, 0>>
/\ build_source = << >>
/\ role = <<"Leader", "Leader", "Follower">>
/\ phase = <<"Accept", "Prepare", "Accept">>
/\ reads = << >>
/\ old_decided_max = 0
/\ running = {1, 2, 3}
/\ crashes = 1
/\ current_seq_num = << [session |-> 0, counter |-> 0],
   [session |-> 0, counter |-> 0],
   [session |-> 1, counter |-> 1] >>
/\ proposed = {v1, v2}
/\ forced_decided_at_force = 0
/\ chan = (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "AcceptDecide", log_sync |-> <<>>, seq_num |-> [session |-> 1, counter |-> 2], entries |-> <<v2>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "PrepareReq", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0], [promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "NotAccepted", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "PrepareReq", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0], [promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 1>> :> <<>> @@ <<3, 2>> :> <<>>)
/\ client = [session |-> 0, request_number |-> 1, sends |-> 0]
/\ past_durable = <<{}, {}, {}>>
/\ buffered_stopsign = <<<<>>, <<>>, <<>>>>
/\ reconfigurations = 0
/\ lease_request = <<-1, -1, -1>>
/\ decided_history = <<>>
/\ losses = 0
/\ witness_epoch = 0
/\ forced_has_decided = FALSE
/\ deactivated = <<FALSE, FALSE, FALSE>>
/\ first_log = <<<<>>, <<>>, <<>>>>
/\ wait_remaining = -1
/\ timeouts = 0
/\ duplicates = 0
/\ elections = 2
/\ leader_state = <<[n_leader |-> 11, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 1, counter |-> 1], [session |-> 1, counter |-> 2]>>, accepted_indexes |-> <<1, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 3], max_promise_sync |-> <<>>], [n_leader |-> 12, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>]>>
/\ rolled_back = {}
/\ lease_reply = <<-1, -1, -1>>
/\ nonce = <<0, 0, 0>>
/\ n_accepted = <<11, 0, 11>>
/\ halts = 0
/\ log = <<<<v2>>, <<>>, <<>>>>
/\ cached_promise_message = <<<<>>, <<>>, <<>>>>

Error: TLC threw an unexpected exception.
This was probably caused by an error in the spec or model.
See the User Output or TLC Console for clues to what happened.
The exception was a java.lang.ArrayIndexOutOfBoundsException
: Index 1 out of bounds for length 1
Finished in 00s at (2026-10-19 04:23:46)
exit 255

```

