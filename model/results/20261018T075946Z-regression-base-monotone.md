# TLC run regression-base-monotone, 20261018T075946Z

Regression run of plan-fixes-20261018 section 5 after items 1 to 7 and 11. Config MC_base_monotone with MaxHalt = 0 and StaleReadNode = FALSE added (item 11). Expected: the counts of run 20261017T121020Z-a-base-monotone-runs-e-f-off (13489555 distinct states, depth 35, no error).

## Configuration

Spec: `model/tla/SequencePaxos.tla`. Config file `model/tla/MC_base_monotone.cfg`:

```
CONSTANTS
  Nodes = {1, 2, 3}
  Values = {v1, v2}
  MaxBallot = 2
  MaxLog = 2
  MaxCounter = 3
  MaxSession = 2
  Cap = 2
  MaxCrash = 0
  MaxWipe = 0
  MaxLoss = 0
  MaxDuplicate = 0
  MaxTimeout = 0
  MaxElection = 2
  RuleR = TRUE
  DecidedIdxMonotone = TRUE
  MaxRollback = 0
  Recovery = FALSE
  Mutation = "none"
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
SPECIFICATION Spec
CONSTRAINT Bounds
SYMMETRY ValueSymmetry
VIEW View
INVARIANTS AcceptedIdxIsLogLength S1 S2 S2b S3 S5
PROPERTIES S2Grows S7
```

## Tools

- TLC2 Version 2.19 of 08 August 2024 (rev: 5a47802)
- Running breadth-first search Model-Checking with fp 75 and seed 4823071276373251829 with 3 workers on 4 cores with 9102MB heap and 64MB offheap memory [pid: 574] (Linux 6.18.44-fc-v139 amd64, Ubuntu 21.0.12.1 x86_64, MSBDiskFPSet, DiskStateQueue).
- Java:

```
openjdk version "21.0.12.1" 2026-08-18
OpenJDK Runtime Environment (build 21.0.12.1+1-1-24.04.4-Ubuntu)
OpenJDK 64-Bit Server VM (build 21.0.12.1+1-1-24.04.4-Ubuntu, mixed mode, sharing)
```

## Numbers

- States generated: 4092207
- Distinct states: 1384534
- States left on queue: 513315
- Diameter (depth of the state graph search): 14
- Wall time: 02min 49s (start 20261018T075946Z, end 20261018T080237Z, UTC)

## Result

```
Error: Deadlock reached.
Error: The behavior up to this point is:
```

## Trace, changed variables per step

```
```

## Trace, full TLC output

```
Error: The behavior up to this point is:
State 1: <Initial predicate>
/\ decided_ballot = <<0, 0>>
/\ wiped = <<FALSE, FALSE, FALSE>>
/\ promise = <<0, 0, 0>>
/\ read_only = <<FALSE, FALSE, FALSE>>
/\ wipes = 0
/\ accepted_idx = <<0, 0, 0>>
/\ stage1_ghost = <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>
/\ recovery_responses = <<{}, {}, {}>>
/\ read_pending = <<{}, {}, {}>>
/\ read_round = <<[reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}]>>
/\ buffered_proposals = <<<<>>, <<>>, <<>>>>
/\ read_only_entries = 0
/\ force_single = [log |-> <<>>, node |-> 0, decided |-> <<>>, history |-> <<>>]
/\ recovering = <<FALSE, FALSE, FALSE>>
/\ decided_idx = <<0, 0, 0>>
/\ role = <<"Follower", "Follower", "Follower">>
/\ phase = <<"None", "None", "None">>
/\ reads = << >>
/\ running = {1, 2, 3}
/\ crashes = 0
/\ current_seq_num = << [session |-> 0, counter |-> 0],
   [session |-> 0, counter |-> 0],
   [session |-> 0, counter |-> 0] >>
/\ proposed = {}
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
/\ decided_history = <<>>
/\ losses = 0
/\ deactivated = <<FALSE, FALSE, FALSE>>
/\ first_log = <<<<>>, <<>>, <<>>>>
/\ timeouts = 0
/\ duplicates = 0
/\ elections = 0
/\ leader_state = <<[n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>]>>
/\ rolled_back = {}
/\ nonce = <<0, 0, 0>>
/\ n_accepted = <<0, 0, 0>>
/\ halts = 0
/\ log = <<<<>>, <<>>, <<>>>>
/\ cached_promise_message = <<<<>>, <<>>, <<>>>>

State 2: <Next line 1700, col 6 to line 1700, col 62 of module SequencePaxos>
/\ decided_ballot = <<0, 0>>
/\ wiped = <<FALSE, FALSE, FALSE>>
/\ promise = <<11, 0, 0>>
/\ read_only = <<FALSE, FALSE, FALSE>>
/\ wipes = 0
/\ accepted_idx = <<0, 0, 0>>
/\ stage1_ghost = <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>
/\ recovery_responses = <<{}, {}, {}>>
/\ read_pending = <<{}, {}, {}>>
/\ read_round = <<[reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}]>>
/\ buffered_proposals = <<<<>>, <<>>, <<>>>>
/\ read_only_entries = 0
/\ force_single = [log |-> <<>>, node |-> 0, decided |-> <<>>, history |-> <<>>]
/\ recovering = <<FALSE, FALSE, FALSE>>
/\ decided_idx = <<0, 0, 0>>
/\ role = <<"Leader", "Follower", "Follower">>
/\ phase = <<"Prepare", "None", "None">>
/\ reads = << >>
/\ running = {1, 2, 3}
/\ crashes = 0
/\ current_seq_num = << [session |-> 0, counter |-> 0],
   [session |-> 0, counter |-> 0],
   [session |-> 0, counter |-> 0] >>
/\ proposed = {}
/\ chan = (<<1, 2>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<1, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 1>> :> <<>> @@ <<2, 3>> :> <<>> @@ <<3, 1>> :> <<>> @@ <<3, 2>> :> <<>>)
/\ client = [session |-> 0, request_number |-> 1, sends |-> 0]
/\ past_durable = <<{}, {}, {}>>
/\ buffered_stopsign = <<<<>>, <<>>, <<>>>>
/\ reconfigurations = 0
/\ decided_history = <<>>
/\ losses = 0
/\ deactivated = <<FALSE, FALSE, FALSE>>
/\ first_log = <<<<>>, <<>>, <<>>>>
/\ timeouts = 0
/\ duplicates = 0
/\ elections = 1
/\ leader_state = <<[n_leader |-> 11, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>]>>
/\ rolled_back = {}
/\ nonce = <<0, 0, 0>>
/\ n_accepted = <<0, 0, 0>>
/\ halts = 0
/\ log = <<<<>>, <<>>, <<>>>>
/\ cached_promise_message = <<<<>>, <<>>, <<>>>>

State 3: <Receive line 1053, col 3 to line 1068, col 16 of module SequencePaxos>
/\ decided_ballot = <<0, 0>>
/\ wiped = <<FALSE, FALSE, FALSE>>
/\ promise = <<11, 11, 0>>
/\ read_only = <<FALSE, FALSE, FALSE>>
/\ wipes = 0
/\ accepted_idx = <<0, 0, 0>>
/\ stage1_ghost = <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>
/\ recovery_responses = <<{}, {}, {}>>
/\ read_pending = <<{}, {}, {}>>
/\ read_round = <<[reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}]>>
/\ buffered_proposals = <<<<>>, <<>>, <<>>>>
/\ read_only_entries = 0
/\ force_single = [log |-> <<>>, node |-> 0, decided |-> <<>>, history |-> <<>>]
/\ recovering = <<FALSE, FALSE, FALSE>>
/\ decided_idx = <<0, 0, 0>>
/\ role = <<"Leader", "Follower", "Follower">>
/\ phase = <<"Prepare", "Prepare", "None">>
/\ reads = << >>
/\ running = {1, 2, 3}
/\ crashes = 0
/\ current_seq_num = << [session |-> 0, counter |-> 0],
   [session |-> 0, counter |-> 0],
   [session |-> 0, counter |-> 0] >>
/\ proposed = {}
/\ chan = (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 3>> :> <<>> @@ <<3, 1>> :> <<>> @@ <<3, 2>> :> <<>>)
/\ client = [session |-> 0, request_number |-> 1, sends |-> 0]
/\ past_durable = <<{}, {}, {}>>
/\ buffered_stopsign = <<<<>>, <<>>, <<>>>>
/\ reconfigurations = 0
/\ decided_history = <<>>
/\ losses = 0
/\ deactivated = <<FALSE, FALSE, FALSE>>
/\ first_log = <<<<>>, <<>>, <<>>>>
/\ timeouts = 0
/\ duplicates = 0
/\ elections = 1
/\ leader_state = <<[n_leader |-> 11, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>]>>
/\ rolled_back = {}
/\ nonce = <<0, 0, 0>>
/\ n_accepted = <<0, 0, 0>>
/\ halts = 0
/\ log = <<<<>>, <<>>, <<>>>>
/\ cached_promise_message = <<<<>>, <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>>, <<>>>>

State 4: <Receive line 1053, col 3 to line 1068, col 16 of module SequencePaxos>
/\ decided_ballot = <<0, 0>>
/\ wiped = <<FALSE, FALSE, FALSE>>
/\ promise = <<11, 11, 11>>
/\ read_only = <<FALSE, FALSE, FALSE>>
/\ wipes = 0
/\ accepted_idx = <<0, 0, 0>>
/\ stage1_ghost = <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>
/\ recovery_responses = <<{}, {}, {}>>
/\ read_pending = <<{}, {}, {}>>
/\ read_round = <<[reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}]>>
/\ buffered_proposals = <<<<>>, <<>>, <<>>>>
/\ read_only_entries = 0
/\ force_single = [log |-> <<>>, node |-> 0, decided |-> <<>>, history |-> <<>>]
/\ recovering = <<FALSE, FALSE, FALSE>>
/\ decided_idx = <<0, 0, 0>>
/\ role = <<"Leader", "Follower", "Follower">>
/\ phase = <<"Prepare", "Prepare", "Prepare">>
/\ reads = << >>
/\ running = {1, 2, 3}
/\ crashes = 0
/\ current_seq_num = << [session |-> 0, counter |-> 0],
   [session |-> 0, counter |-> 0],
   [session |-> 0, counter |-> 0] >>
/\ proposed = {}
/\ chan = (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<>> @@ <<2, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 3>> :> <<>> @@ <<3, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 2>> :> <<>>)
/\ client = [session |-> 0, request_number |-> 1, sends |-> 0]
/\ past_durable = <<{}, {}, {}>>
/\ buffered_stopsign = <<<<>>, <<>>, <<>>>>
/\ reconfigurations = 0
/\ decided_history = <<>>
/\ losses = 0
/\ deactivated = <<FALSE, FALSE, FALSE>>
/\ first_log = <<<<>>, <<>>, <<>>>>
/\ timeouts = 0
/\ duplicates = 0
/\ elections = 1
/\ leader_state = <<[n_leader |-> 11, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>]>>
/\ rolled_back = {}
/\ nonce = <<0, 0, 0>>
/\ n_accepted = <<0, 0, 0>>
/\ halts = 0
/\ log = <<<<>>, <<>>, <<>>>>
/\ cached_promise_message = <<<<>>, <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>>, <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>>>>

State 5: <Next line 1700, col 6 to line 1700, col 62 of module SequencePaxos>
/\ decided_ballot = <<0, 0>>
/\ wiped = <<FALSE, FALSE, FALSE>>
/\ promise = <<21, 11, 11>>
/\ read_only = <<FALSE, FALSE, FALSE>>
/\ wipes = 0
/\ accepted_idx = <<0, 0, 0>>
/\ stage1_ghost = <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>
/\ recovery_responses = <<{}, {}, {}>>
/\ read_pending = <<{}, {}, {}>>
/\ read_round = <<[reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}]>>
/\ buffered_proposals = <<<<>>, <<>>, <<>>>>
/\ read_only_entries = 0
/\ force_single = [log |-> <<>>, node |-> 0, decided |-> <<>>, history |-> <<>>]
/\ recovering = <<FALSE, FALSE, FALSE>>
/\ decided_idx = <<0, 0, 0>>
/\ role = <<"Leader", "Follower", "Follower">>
/\ phase = <<"Prepare", "Prepare", "Prepare">>
/\ reads = << >>
/\ running = {1, 2, 3}
/\ crashes = 0
/\ current_seq_num = << [session |-> 0, counter |-> 0],
   [session |-> 0, counter |-> 0],
   [session |-> 0, counter |-> 0] >>
/\ proposed = {}
/\ chan = (<<1, 2>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 21, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<1, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 21, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 3>> :> <<>> @@ <<3, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 2>> :> <<>>)
/\ client = [session |-> 0, request_number |-> 1, sends |-> 0]
/\ past_durable = <<{}, {}, {}>>
/\ buffered_stopsign = <<<<>>, <<>>, <<>>>>
/\ reconfigurations = 0
/\ decided_history = <<>>
/\ losses = 0
/\ deactivated = <<FALSE, FALSE, FALSE>>
/\ first_log = <<<<>>, <<>>, <<>>>>
/\ timeouts = 0
/\ duplicates = 0
/\ elections = 2
/\ leader_state = <<[n_leader |-> 21, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>]>>
/\ rolled_back = {}
/\ nonce = <<0, 0, 0>>
/\ n_accepted = <<0, 0, 0>>
/\ halts = 0
/\ log = <<<<>>, <<>>, <<>>>>
/\ cached_promise_message = <<<<>>, <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>>, <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>>>>

State 6: <Next line 1701, col 6 to line 1701, col 51 of module SequencePaxos>
/\ decided_ballot = <<0, 0>>
/\ wiped = <<FALSE, FALSE, FALSE>>
/\ promise = <<21, 11, 11>>
/\ read_only = <<FALSE, FALSE, FALSE>>
/\ wipes = 0
/\ accepted_idx = <<0, 0, 0>>
/\ stage1_ghost = <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>
/\ recovery_responses = <<{}, {}, {}>>
/\ read_pending = <<{}, {}, {}>>
/\ read_round = <<[reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}]>>
/\ buffered_proposals = <<<<>>, <<>>, <<>>>>
/\ read_only_entries = 0
/\ force_single = [log |-> <<>>, node |-> 0, decided |-> <<>>, history |-> <<>>]
/\ recovering = <<FALSE, FALSE, FALSE>>
/\ decided_idx = <<0, 0, 0>>
/\ role = <<"Leader", "Follower", "Follower">>
/\ phase = <<"Prepare", "Prepare", "Prepare">>
/\ reads = << >>
/\ running = {1, 2, 3}
/\ crashes = 0
/\ current_seq_num = << [session |-> 0, counter |-> 0],
   [session |-> 0, counter |-> 0],
   [session |-> 0, counter |-> 0] >>
/\ proposed = {v1}
/\ chan = (<<1, 2>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 21, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<1, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 21, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0], [promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 0, type |-> "ProposalForward", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<v1>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 3>> :> <<>> @@ <<3, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 2>> :> <<>>)
/\ client = [session |-> 0, request_number |-> 1, sends |-> 0]
/\ past_durable = <<{}, {}, {}>>
/\ buffered_stopsign = <<<<>>, <<>>, <<>>>>
/\ reconfigurations = 0
/\ decided_history = <<>>
/\ losses = 0
/\ deactivated = <<FALSE, FALSE, FALSE>>
/\ first_log = <<<<>>, <<>>, <<>>>>
/\ timeouts = 0
/\ duplicates = 0
/\ elections = 2
/\ leader_state = <<[n_leader |-> 21, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>]>>
/\ rolled_back = {}
/\ nonce = <<0, 0, 0>>
/\ n_accepted = <<0, 0, 0>>
/\ halts = 0
/\ log = <<<<>>, <<>>, <<>>>>
/\ cached_promise_message = <<<<>>, <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>>, <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>>>>

State 7: <Receive line 1053, col 3 to line 1068, col 16 of module SequencePaxos>
/\ decided_ballot = <<0, 0>>
/\ wiped = <<FALSE, FALSE, FALSE>>
/\ promise = <<21, 21, 11>>
/\ read_only = <<FALSE, FALSE, FALSE>>
/\ wipes = 0
/\ accepted_idx = <<0, 0, 0>>
/\ stage1_ghost = <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>
/\ recovery_responses = <<{}, {}, {}>>
/\ read_pending = <<{}, {}, {}>>
/\ read_round = <<[reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}]>>
/\ buffered_proposals = <<<<>>, <<>>, <<>>>>
/\ read_only_entries = 0
/\ force_single = [log |-> <<>>, node |-> 0, decided |-> <<>>, history |-> <<>>]
/\ recovering = <<FALSE, FALSE, FALSE>>
/\ decided_idx = <<0, 0, 0>>
/\ role = <<"Leader", "Follower", "Follower">>
/\ phase = <<"Prepare", "Prepare", "Prepare">>
/\ reads = << >>
/\ running = {1, 2, 3}
/\ crashes = 0
/\ current_seq_num = << [session |-> 0, counter |-> 0],
   [session |-> 0, counter |-> 0],
   [session |-> 0, counter |-> 0] >>
/\ proposed = {v1}
/\ chan = (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 21, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0], [promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 0, type |-> "ProposalForward", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<v1>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 3>> :> <<>> @@ <<3, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 2>> :> <<>>)
/\ client = [session |-> 0, request_number |-> 1, sends |-> 0]
/\ past_durable = <<{}, {}, {}>>
/\ buffered_stopsign = <<<<>>, <<>>, <<>>>>
/\ reconfigurations = 0
/\ decided_history = <<>>
/\ losses = 0
/\ deactivated = <<FALSE, FALSE, FALSE>>
/\ first_log = <<<<>>, <<>>, <<>>>>
/\ timeouts = 0
/\ duplicates = 0
/\ elections = 2
/\ leader_state = <<[n_leader |-> 21, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>]>>
/\ rolled_back = {}
/\ nonce = <<0, 0, 0>>
/\ n_accepted = <<0, 0, 0>>
/\ halts = 0
/\ log = <<<<>>, <<>>, <<>>>>
/\ cached_promise_message = <<<<>>, <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 21, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>>, <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>>>>

State 8: <Receive line 1053, col 3 to line 1068, col 16 of module SequencePaxos>
/\ decided_ballot = <<0, 0>>
/\ wiped = <<FALSE, FALSE, FALSE>>
/\ promise = <<21, 21, 11>>
/\ read_only = <<FALSE, FALSE, FALSE>>
/\ wipes = 0
/\ accepted_idx = <<0, 0, 0>>
/\ stage1_ghost = <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>
/\ recovery_responses = <<{}, {}, {}>>
/\ read_pending = <<{}, {}, {}>>
/\ read_round = <<[reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}]>>
/\ buffered_proposals = <<<<>>, <<>>, <<>>>>
/\ read_only_entries = 0
/\ force_single = [log |-> <<>>, node |-> 0, decided |-> <<>>, history |-> <<>>]
/\ recovering = <<FALSE, FALSE, FALSE>>
/\ decided_idx = <<0, 0, 0>>
/\ role = <<"Leader", "Follower", "Follower">>
/\ phase = <<"Prepare", "Prepare", "Prepare">>
/\ reads = << >>
/\ running = {1, 2, 3}
/\ crashes = 0
/\ current_seq_num = << [session |-> 0, counter |-> 0],
   [session |-> 0, counter |-> 0],
   [session |-> 0, counter |-> 0] >>
/\ proposed = {v1}
/\ chan = (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 21, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 0, type |-> "ProposalForward", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<v1>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 3>> :> <<>> @@ <<3, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 2>> :> <<>>)
/\ client = [session |-> 0, request_number |-> 1, sends |-> 0]
/\ past_durable = <<{}, {}, {}>>
/\ buffered_stopsign = <<<<>>, <<>>, <<>>>>
/\ reconfigurations = 0
/\ decided_history = <<>>
/\ losses = 0
/\ deactivated = <<FALSE, FALSE, FALSE>>
/\ first_log = <<<<>>, <<>>, <<>>>>
/\ timeouts = 0
/\ duplicates = 0
/\ elections = 2
/\ leader_state = <<[n_leader |-> 21, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>]>>
/\ rolled_back = {}
/\ nonce = <<0, 0, 0>>
/\ n_accepted = <<0, 0, 0>>
/\ halts = 0
/\ log = <<<<>>, <<>>, <<>>>>
/\ cached_promise_message = <<<<>>, <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 21, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>>, <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>>>>

State 9: <Next line 1701, col 6 to line 1701, col 51 of module SequencePaxos>
/\ decided_ballot = <<0, 0>>
/\ wiped = <<FALSE, FALSE, FALSE>>
/\ promise = <<21, 21, 11>>
/\ read_only = <<FALSE, FALSE, FALSE>>
/\ wipes = 0
/\ accepted_idx = <<0, 0, 0>>
/\ stage1_ghost = <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>
/\ recovery_responses = <<{}, {}, {}>>
/\ read_pending = <<{}, {}, {}>>
/\ read_round = <<[reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}]>>
/\ buffered_proposals = <<<<>>, <<>>, <<>>>>
/\ read_only_entries = 0
/\ force_single = [log |-> <<>>, node |-> 0, decided |-> <<>>, history |-> <<>>]
/\ recovering = <<FALSE, FALSE, FALSE>>
/\ decided_idx = <<0, 0, 0>>
/\ role = <<"Leader", "Follower", "Follower">>
/\ phase = <<"Prepare", "Prepare", "Prepare">>
/\ reads = << >>
/\ running = {1, 2, 3}
/\ crashes = 0
/\ current_seq_num = << [session |-> 0, counter |-> 0],
   [session |-> 0, counter |-> 0],
   [session |-> 0, counter |-> 0] >>
/\ proposed = {v1, v2}
/\ chan = (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 21, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 0, type |-> "ProposalForward", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<v1>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 3>> :> <<>> @@ <<3, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0], [promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 0, type |-> "ProposalForward", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<v2>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 2>> :> <<>>)
/\ client = [session |-> 0, request_number |-> 1, sends |-> 0]
/\ past_durable = <<{}, {}, {}>>
/\ buffered_stopsign = <<<<>>, <<>>, <<>>>>
/\ reconfigurations = 0
/\ decided_history = <<>>
/\ losses = 0
/\ deactivated = <<FALSE, FALSE, FALSE>>
/\ first_log = <<<<>>, <<>>, <<>>>>
/\ timeouts = 0
/\ duplicates = 0
/\ elections = 2
/\ leader_state = <<[n_leader |-> 21, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>]>>
/\ rolled_back = {}
/\ nonce = <<0, 0, 0>>
/\ n_accepted = <<0, 0, 0>>
/\ halts = 0
/\ log = <<<<>>, <<>>, <<>>>>
/\ cached_promise_message = <<<<>>, <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 21, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>>, <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>>>>

State 10: <Receive line 1053, col 3 to line 1068, col 16 of module SequencePaxos>
/\ decided_ballot = <<0, 0>>
/\ wiped = <<FALSE, FALSE, FALSE>>
/\ promise = <<21, 21, 21>>
/\ read_only = <<FALSE, FALSE, FALSE>>
/\ wipes = 0
/\ accepted_idx = <<0, 0, 0>>
/\ stage1_ghost = <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>
/\ recovery_responses = <<{}, {}, {}>>
/\ read_pending = <<{}, {}, {}>>
/\ read_round = <<[reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}]>>
/\ buffered_proposals = <<<<>>, <<>>, <<>>>>
/\ read_only_entries = 0
/\ force_single = [log |-> <<>>, node |-> 0, decided |-> <<>>, history |-> <<>>]
/\ recovering = <<FALSE, FALSE, FALSE>>
/\ decided_idx = <<0, 0, 0>>
/\ role = <<"Leader", "Follower", "Follower">>
/\ phase = <<"Prepare", "Prepare", "Prepare">>
/\ reads = << >>
/\ running = {1, 2, 3}
/\ crashes = 0
/\ current_seq_num = << [session |-> 0, counter |-> 0],
   [session |-> 0, counter |-> 0],
   [session |-> 0, counter |-> 0] >>
/\ proposed = {v1, v2}
/\ chan = (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<>> @@ <<2, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 0, type |-> "ProposalForward", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<v1>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 3>> :> <<>> @@ <<3, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0], [promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 0, type |-> "ProposalForward", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<v2>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 2>> :> <<>>)
/\ client = [session |-> 0, request_number |-> 1, sends |-> 0]
/\ past_durable = <<{}, {}, {}>>
/\ buffered_stopsign = <<<<>>, <<>>, <<>>>>
/\ reconfigurations = 0
/\ decided_history = <<>>
/\ losses = 0
/\ deactivated = <<FALSE, FALSE, FALSE>>
/\ first_log = <<<<>>, <<>>, <<>>>>
/\ timeouts = 0
/\ duplicates = 0
/\ elections = 2
/\ leader_state = <<[n_leader |-> 21, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>]>>
/\ rolled_back = {}
/\ nonce = <<0, 0, 0>>
/\ n_accepted = <<0, 0, 0>>
/\ halts = 0
/\ log = <<<<>>, <<>>, <<>>>>
/\ cached_promise_message = <<<<>>, <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 21, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>>, <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 21, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>>>>

State 11: <Receive line 1053, col 3 to line 1068, col 16 of module SequencePaxos>
/\ decided_ballot = <<0, 0>>
/\ wiped = <<FALSE, FALSE, FALSE>>
/\ promise = <<21, 21, 21>>
/\ read_only = <<FALSE, FALSE, FALSE>>
/\ wipes = 0
/\ accepted_idx = <<0, 0, 0>>
/\ stage1_ghost = <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>
/\ recovery_responses = <<{}, {}, {}>>
/\ read_pending = <<{}, {}, {}>>
/\ read_round = <<[reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}]>>
/\ buffered_proposals = <<<<v1>>, <<>>, <<>>>>
/\ read_only_entries = 0
/\ force_single = [log |-> <<>>, node |-> 0, decided |-> <<>>, history |-> <<>>]
/\ recovering = <<FALSE, FALSE, FALSE>>
/\ decided_idx = <<0, 0, 0>>
/\ role = <<"Leader", "Follower", "Follower">>
/\ phase = <<"Prepare", "Prepare", "Prepare">>
/\ reads = << >>
/\ running = {1, 2, 3}
/\ crashes = 0
/\ current_seq_num = << [session |-> 0, counter |-> 0],
   [session |-> 0, counter |-> 0],
   [session |-> 0, counter |-> 0] >>
/\ proposed = {v1, v2}
/\ chan = (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<>> @@ <<2, 1>> :> <<>> @@ <<2, 3>> :> <<>> @@ <<3, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 11, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0], [promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 0, type |-> "ProposalForward", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<v2>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 2>> :> <<>>)
/\ client = [session |-> 0, request_number |-> 1, sends |-> 0]
/\ past_durable = <<{}, {}, {}>>
/\ buffered_stopsign = <<<<>>, <<>>, <<>>>>
/\ reconfigurations = 0
/\ decided_history = <<>>
/\ losses = 0
/\ deactivated = <<FALSE, FALSE, FALSE>>
/\ first_log = <<<<>>, <<>>, <<>>>>
/\ timeouts = 0
/\ duplicates = 0
/\ elections = 2
/\ leader_state = <<[n_leader |-> 21, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>]>>
/\ rolled_back = {}
/\ nonce = <<0, 0, 0>>
/\ n_accepted = <<0, 0, 0>>
/\ halts = 0
/\ log = <<<<>>, <<>>, <<>>>>
/\ cached_promise_message = <<<<>>, <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 21, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>>, <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 21, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>>>>

State 12: <Receive line 1053, col 3 to line 1068, col 16 of module SequencePaxos>
/\ decided_ballot = <<0, 0>>
/\ wiped = <<FALSE, FALSE, FALSE>>
/\ promise = <<21, 21, 21>>
/\ read_only = <<FALSE, FALSE, FALSE>>
/\ wipes = 0
/\ accepted_idx = <<0, 0, 0>>
/\ stage1_ghost = <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>
/\ recovery_responses = <<{}, {}, {}>>
/\ read_pending = <<{}, {}, {}>>
/\ read_round = <<[reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}]>>
/\ buffered_proposals = <<<<v1>>, <<>>, <<>>>>
/\ read_only_entries = 0
/\ force_single = [log |-> <<>>, node |-> 0, decided |-> <<>>, history |-> <<>>]
/\ recovering = <<FALSE, FALSE, FALSE>>
/\ decided_idx = <<0, 0, 0>>
/\ role = <<"Leader", "Follower", "Follower">>
/\ phase = <<"Prepare", "Prepare", "Prepare">>
/\ reads = << >>
/\ running = {1, 2, 3}
/\ crashes = 0
/\ current_seq_num = << [session |-> 0, counter |-> 0],
   [session |-> 0, counter |-> 0],
   [session |-> 0, counter |-> 0] >>
/\ proposed = {v1, v2}
/\ chan = (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<>> @@ <<2, 1>> :> <<>> @@ <<2, 3>> :> <<>> @@ <<3, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 0, type |-> "ProposalForward", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<v2>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 2>> :> <<>>)
/\ client = [session |-> 0, request_number |-> 1, sends |-> 0]
/\ past_durable = <<{}, {}, {}>>
/\ buffered_stopsign = <<<<>>, <<>>, <<>>>>
/\ reconfigurations = 0
/\ decided_history = <<>>
/\ losses = 0
/\ deactivated = <<FALSE, FALSE, FALSE>>
/\ first_log = <<<<>>, <<>>, <<>>>>
/\ timeouts = 0
/\ duplicates = 0
/\ elections = 2
/\ leader_state = <<[n_leader |-> 21, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>]>>
/\ rolled_back = {}
/\ nonce = <<0, 0, 0>>
/\ n_accepted = <<0, 0, 0>>
/\ halts = 0
/\ log = <<<<>>, <<>>, <<>>>>
/\ cached_promise_message = <<<<>>, <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 21, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>>, <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 21, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>>>>

State 13: <Receive line 1053, col 3 to line 1068, col 16 of module SequencePaxos>
/\ decided_ballot = <<0, 0>>
/\ wiped = <<FALSE, FALSE, FALSE>>
/\ promise = <<21, 21, 21>>
/\ read_only = <<FALSE, FALSE, FALSE>>
/\ wipes = 0
/\ accepted_idx = <<0, 0, 0>>
/\ stage1_ghost = <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>
/\ recovery_responses = <<{}, {}, {}>>
/\ read_pending = <<{}, {}, {}>>
/\ read_round = <<[reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}]>>
/\ buffered_proposals = <<<<v1, v2>>, <<>>, <<>>>>
/\ read_only_entries = 0
/\ force_single = [log |-> <<>>, node |-> 0, decided |-> <<>>, history |-> <<>>]
/\ recovering = <<FALSE, FALSE, FALSE>>
/\ decided_idx = <<0, 0, 0>>
/\ role = <<"Leader", "Follower", "Follower">>
/\ phase = <<"Prepare", "Prepare", "Prepare">>
/\ reads = << >>
/\ running = {1, 2, 3}
/\ crashes = 0
/\ current_seq_num = << [session |-> 0, counter |-> 0],
   [session |-> 0, counter |-> 0],
   [session |-> 0, counter |-> 0] >>
/\ proposed = {v1, v2}
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
/\ decided_history = <<>>
/\ losses = 0
/\ deactivated = <<FALSE, FALSE, FALSE>>
/\ first_log = <<<<>>, <<>>, <<>>>>
/\ timeouts = 0
/\ duplicates = 0
/\ elections = 2
/\ leader_state = <<[n_leader |-> 21, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>]>>
/\ rolled_back = {}
/\ nonce = <<0, 0, 0>>
/\ n_accepted = <<0, 0, 0>>
/\ halts = 0
/\ log = <<<<>>, <<>>, <<>>>>
/\ cached_promise_message = <<<<>>, <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 21, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>>, <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 21, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>>>>

```

