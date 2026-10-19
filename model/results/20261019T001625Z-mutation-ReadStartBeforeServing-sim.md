# TLC run mutation-ReadStartBeforeServing-sim, 20261019T001625Z

Counter-check of plan-fixes-20261018-addendum: mutation ReadStartBeforeServing against the direct property ReadStartOnlyWhenServing, random simulation (-simulate -deadlock -depth 60, seed 1, limit 15 min). Expected: a violation of ReadStartOnlyWhenServing. Mode: simulation.

## Configuration

Spec: `model/tla/SequencePaxos.tla`. Config file `model/tla/MC_mut_ReadStartBeforeServing.sim.cfg`:

```
CONSTANTS
  Nodes = {1, 2, 3}
  Values = {v1, v2}
  MaxBallot = 1
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
  Mutation = "ReadStartBeforeServing"
  ClientSessions = FALSE
  MaxClientSend = 0
  MaxRequest = 0
  ReadIndex = TRUE
  MaxRead = 1
  NextConfigNodes = {6}
  MaxReconfigure = 1
  SingleNode = TRUE
  MaxForceSingle = 0
  MaxReadOnly = 0
  MaxHalt = 0
  StaleReadNode = FALSE
SPECIFICATION Spec
CONSTRAINT Bounds
PROPERTIES ReadStartOnlyWhenServing
```

## Tools

- TLC2 Version 2.19 of 08 August 2024 (rev: 5a47802)
- Running Random Simulation with seed 1 with 3 workers on 4 cores with 9102MB heap and 64MB offheap memory [pid: 3432] (Linux 6.18.44-fc-v139 amd64, Ubuntu 21.0.12.1 x86_64).
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
- Wall time: 03s (start 20261019T001625Z, end 20261019T001628Z, UTC)

## Result

```
Error: Action property ReadStartOnlyWhenServing is violated.
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
/\ wiped = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ promise = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ read_only = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ wipes = 0
/\ accepted_idx = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ stage1_ghost = <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>
/\ recovery_responses = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ read_pending = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ read_round = (1 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 2 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 3 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 6 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}])
/\ buffered_proposals = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ read_only_entries = 0
/\ force_single = [log |-> <<>>, node |-> 0, decided |-> <<>>, history |-> <<>>]
/\ recovering = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ decided_idx = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ build_source = (6 :> 0)
/\ role = (1 :> "Follower" @@ 2 :> "Follower" @@ 3 :> "Follower" @@ 6 :> "Follower")
/\ phase = (1 :> "None" @@ 2 :> "None" @@ 3 :> "None" @@ 6 :> "None")
/\ reads = <<[crashes |-> 0, rolled_back |-> {}, state |-> "none", read_index |-> 0, node |-> 0, decided_at_start |-> <<>>, result |-> <<>>, stale |-> FALSE, contract_stale_ok |-> FALSE, ballot |-> 0, decided_new_at_start |-> <<>>]>>
/\ running = {1, 2, 3}
/\ crashes = 0
/\ current_seq_num = ( 1 :> [session |-> 0, counter |-> 0] @@
  2 :> [session |-> 0, counter |-> 0] @@
  3 :> [session |-> 0, counter |-> 0] @@
  6 :> [session |-> 0, counter |-> 0] )
/\ proposed = {}
/\ chan = ( <<1, 2>> :> <<>> @@
  <<1, 3>> :> <<>> @@
  <<2, 1>> :> <<>> @@
  <<2, 3>> :> <<>> @@
  <<3, 1>> :> <<>> @@
  <<3, 2>> :> <<>> )
/\ client = [session |-> 0, request_number |-> 1, sends |-> 0]
/\ past_durable = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ buffered_stopsign = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ reconfigurations = 0
/\ decided_history = <<>>
/\ losses = 0
/\ deactivated = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ first_log = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ timeouts = 0
/\ duplicates = 0
/\ elections = 0
/\ leader_state = (1 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 2 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 3 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 6 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>])
/\ rolled_back = {}
/\ nonce = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ n_accepted = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ halts = 0
/\ log = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ cached_promise_message = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)

State 2: <Next line 1705, col 6 to line 1705, col 62 of module SequencePaxos>
/\ decided_ballot = <<0, 0>>
/\ wiped = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ promise = (1 :> 0 @@ 2 :> 0 @@ 3 :> 13 @@ 6 :> 0)
/\ read_only = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ wipes = 0
/\ accepted_idx = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ stage1_ghost = <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>
/\ recovery_responses = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ read_pending = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ read_round = (1 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 2 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 3 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 6 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}])
/\ buffered_proposals = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ read_only_entries = 0
/\ force_single = [log |-> <<>>, node |-> 0, decided |-> <<>>, history |-> <<>>]
/\ recovering = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ decided_idx = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ build_source = (6 :> 0)
/\ role = (1 :> "Follower" @@ 2 :> "Follower" @@ 3 :> "Leader" @@ 6 :> "Follower")
/\ phase = (1 :> "None" @@ 2 :> "None" @@ 3 :> "Prepare" @@ 6 :> "None")
/\ reads = <<[crashes |-> 0, rolled_back |-> {}, state |-> "none", read_index |-> 0, node |-> 0, decided_at_start |-> <<>>, result |-> <<>>, stale |-> FALSE, contract_stale_ok |-> FALSE, ballot |-> 0, decided_new_at_start |-> <<>>]>>
/\ running = {1, 2, 3}
/\ crashes = 0
/\ current_seq_num = ( 1 :> [session |-> 0, counter |-> 0] @@
  2 :> [session |-> 0, counter |-> 0] @@
  3 :> [session |-> 0, counter |-> 0] @@
  6 :> [session |-> 0, counter |-> 0] )
/\ proposed = {}
/\ chan = (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<>> @@ <<2, 1>> :> <<>> @@ <<2, 3>> :> <<>> @@ <<3, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 2>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>>)
/\ client = [session |-> 0, request_number |-> 1, sends |-> 0]
/\ past_durable = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ buffered_stopsign = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ reconfigurations = 0
/\ decided_history = <<>>
/\ losses = 0
/\ deactivated = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ first_log = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ timeouts = 0
/\ duplicates = 0
/\ elections = 1
/\ leader_state = (1 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 2 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 3 :> [n_leader |-> 13, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 6 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>])
/\ rolled_back = {}
/\ nonce = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ n_accepted = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ halts = 0
/\ log = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ cached_promise_message = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)

State 3: <Next line 1720, col 6 to line 1720, col 38 of module SequencePaxos>
/\ decided_ballot = <<0, 0>>
/\ wiped = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ promise = (1 :> 0 @@ 2 :> 0 @@ 3 :> 13 @@ 6 :> 0)
/\ read_only = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ wipes = 0
/\ accepted_idx = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ stage1_ghost = <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>
/\ recovery_responses = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ read_pending = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ read_round = (1 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 2 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 3 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 6 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}])
/\ buffered_proposals = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ read_only_entries = 0
/\ force_single = [log |-> <<>>, node |-> 0, decided |-> <<>>, history |-> <<>>]
/\ recovering = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ decided_idx = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ build_source = (6 :> 0)
/\ role = (1 :> "Follower" @@ 2 :> "Follower" @@ 3 :> "Leader" @@ 6 :> "Follower")
/\ phase = (1 :> "None" @@ 2 :> "None" @@ 3 :> "Prepare" @@ 6 :> "None")
/\ reads = <<[crashes |-> 0, rolled_back |-> {}, state |-> "none", read_index |-> 0, node |-> 0, decided_at_start |-> <<>>, result |-> <<>>, stale |-> FALSE, contract_stale_ok |-> FALSE, ballot |-> 0, decided_new_at_start |-> <<>>]>>
/\ running = {1, 2, 3}
/\ crashes = 0
/\ current_seq_num = ( 1 :> [session |-> 0, counter |-> 0] @@
  2 :> [session |-> 0, counter |-> 0] @@
  3 :> [session |-> 0, counter |-> 0] @@
  6 :> [session |-> 0, counter |-> 0] )
/\ proposed = {[session |-> 0, type |-> "StopSign", request_number |-> 0]}
/\ chan = (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<>> @@ <<2, 1>> :> <<>> @@ <<2, 3>> :> <<>> @@ <<3, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 2>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>>)
/\ client = [session |-> 0, request_number |-> 1, sends |-> 0]
/\ past_durable = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ buffered_stopsign = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<[session |-> 0, type |-> "StopSign", request_number |-> 0]>> @@ 6 :> <<>>)
/\ reconfigurations = 1
/\ decided_history = <<>>
/\ losses = 0
/\ deactivated = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ first_log = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ timeouts = 0
/\ duplicates = 0
/\ elections = 1
/\ leader_state = (1 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 2 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 3 :> [n_leader |-> 13, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 6 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>])
/\ rolled_back = {}
/\ nonce = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ n_accepted = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ halts = 0
/\ log = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ cached_promise_message = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)

State 4: <Next line 1706, col 6 to line 1706, col 51 of module SequencePaxos>
/\ decided_ballot = <<0, 0>>
/\ wiped = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ promise = (1 :> 0 @@ 2 :> 0 @@ 3 :> 13 @@ 6 :> 0)
/\ read_only = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ wipes = 0
/\ accepted_idx = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ stage1_ghost = <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>
/\ recovery_responses = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ read_pending = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ read_round = (1 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 2 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 3 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 6 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}])
/\ buffered_proposals = (1 :> <<>> @@ 2 :> <<v2>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ read_only_entries = 0
/\ force_single = [log |-> <<>>, node |-> 0, decided |-> <<>>, history |-> <<>>]
/\ recovering = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ decided_idx = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ build_source = (6 :> 0)
/\ role = (1 :> "Follower" @@ 2 :> "Follower" @@ 3 :> "Leader" @@ 6 :> "Follower")
/\ phase = (1 :> "None" @@ 2 :> "None" @@ 3 :> "Prepare" @@ 6 :> "None")
/\ reads = <<[crashes |-> 0, rolled_back |-> {}, state |-> "none", read_index |-> 0, node |-> 0, decided_at_start |-> <<>>, result |-> <<>>, stale |-> FALSE, contract_stale_ok |-> FALSE, ballot |-> 0, decided_new_at_start |-> <<>>]>>
/\ running = {1, 2, 3}
/\ crashes = 0
/\ current_seq_num = ( 1 :> [session |-> 0, counter |-> 0] @@
  2 :> [session |-> 0, counter |-> 0] @@
  3 :> [session |-> 0, counter |-> 0] @@
  6 :> [session |-> 0, counter |-> 0] )
/\ proposed = {v2, [session |-> 0, type |-> "StopSign", request_number |-> 0]}
/\ chan = (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<>> @@ <<2, 1>> :> <<>> @@ <<2, 3>> :> <<>> @@ <<3, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 2>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>>)
/\ client = [session |-> 0, request_number |-> 1, sends |-> 0]
/\ past_durable = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ buffered_stopsign = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<[session |-> 0, type |-> "StopSign", request_number |-> 0]>> @@ 6 :> <<>>)
/\ reconfigurations = 1
/\ decided_history = <<>>
/\ losses = 0
/\ deactivated = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ first_log = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ timeouts = 0
/\ duplicates = 0
/\ elections = 1
/\ leader_state = (1 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 2 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 3 :> [n_leader |-> 13, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 6 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>])
/\ rolled_back = {}
/\ nonce = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ n_accepted = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ halts = 0
/\ log = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ cached_promise_message = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)

State 5: <Receive line 1056, col 3 to line 1071, col 16 of module SequencePaxos>
/\ decided_ballot = <<0, 0>>
/\ wiped = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ promise = (1 :> 13 @@ 2 :> 0 @@ 3 :> 13 @@ 6 :> 0)
/\ read_only = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ wipes = 0
/\ accepted_idx = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ stage1_ghost = <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>
/\ recovery_responses = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ read_pending = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ read_round = (1 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 2 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 3 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 6 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}])
/\ buffered_proposals = (1 :> <<>> @@ 2 :> <<v2>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ read_only_entries = 0
/\ force_single = [log |-> <<>>, node |-> 0, decided |-> <<>>, history |-> <<>>]
/\ recovering = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ decided_idx = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ build_source = (6 :> 0)
/\ role = (1 :> "Follower" @@ 2 :> "Follower" @@ 3 :> "Leader" @@ 6 :> "Follower")
/\ phase = (1 :> "Prepare" @@ 2 :> "None" @@ 3 :> "Prepare" @@ 6 :> "None")
/\ reads = <<[crashes |-> 0, rolled_back |-> {}, state |-> "none", read_index |-> 0, node |-> 0, decided_at_start |-> <<>>, result |-> <<>>, stale |-> FALSE, contract_stale_ok |-> FALSE, ballot |-> 0, decided_new_at_start |-> <<>>]>>
/\ running = {1, 2, 3}
/\ crashes = 0
/\ current_seq_num = ( 1 :> [session |-> 0, counter |-> 0] @@
  2 :> [session |-> 0, counter |-> 0] @@
  3 :> [session |-> 0, counter |-> 0] @@
  6 :> [session |-> 0, counter |-> 0] )
/\ proposed = {v2, [session |-> 0, type |-> "StopSign", request_number |-> 0]}
/\ chan = (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 1>> :> <<>> @@ <<2, 3>> :> <<>> @@ <<3, 1>> :> <<>> @@ <<3, 2>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>>)
/\ client = [session |-> 0, request_number |-> 1, sends |-> 0]
/\ past_durable = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ buffered_stopsign = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<[session |-> 0, type |-> "StopSign", request_number |-> 0]>> @@ 6 :> <<>>)
/\ reconfigurations = 1
/\ decided_history = <<>>
/\ losses = 0
/\ deactivated = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ first_log = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ timeouts = 0
/\ duplicates = 0
/\ elections = 1
/\ leader_state = (1 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 2 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 3 :> [n_leader |-> 13, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 6 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>])
/\ rolled_back = {}
/\ nonce = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ n_accepted = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ halts = 0
/\ log = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ cached_promise_message = (1 :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)

State 6: <Receive line 1056, col 3 to line 1071, col 16 of module SequencePaxos>
/\ decided_ballot = <<0, 0>>
/\ wiped = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ promise = (1 :> 13 @@ 2 :> 13 @@ 3 :> 13 @@ 6 :> 0)
/\ read_only = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ wipes = 0
/\ accepted_idx = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ stage1_ghost = <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>
/\ recovery_responses = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ read_pending = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ read_round = (1 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 2 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 3 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 6 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}])
/\ buffered_proposals = (1 :> <<>> @@ 2 :> <<v2>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ read_only_entries = 0
/\ force_single = [log |-> <<>>, node |-> 0, decided |-> <<>>, history |-> <<>>]
/\ recovering = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ decided_idx = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ build_source = (6 :> 0)
/\ role = (1 :> "Follower" @@ 2 :> "Follower" @@ 3 :> "Leader" @@ 6 :> "Follower")
/\ phase = (1 :> "Prepare" @@ 2 :> "Prepare" @@ 3 :> "Prepare" @@ 6 :> "None")
/\ reads = <<[crashes |-> 0, rolled_back |-> {}, state |-> "none", read_index |-> 0, node |-> 0, decided_at_start |-> <<>>, result |-> <<>>, stale |-> FALSE, contract_stale_ok |-> FALSE, ballot |-> 0, decided_new_at_start |-> <<>>]>>
/\ running = {1, 2, 3}
/\ crashes = 0
/\ current_seq_num = ( 1 :> [session |-> 0, counter |-> 0] @@
  2 :> [session |-> 0, counter |-> 0] @@
  3 :> [session |-> 0, counter |-> 0] @@
  6 :> [session |-> 0, counter |-> 0] )
/\ proposed = {v2, [session |-> 0, type |-> "StopSign", request_number |-> 0]}
/\ chan = (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 1>> :> <<>> @@ <<2, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 1>> :> <<>> @@ <<3, 2>> :> <<>>)
/\ client = [session |-> 0, request_number |-> 1, sends |-> 0]
/\ past_durable = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ buffered_stopsign = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<[session |-> 0, type |-> "StopSign", request_number |-> 0]>> @@ 6 :> <<>>)
/\ reconfigurations = 1
/\ decided_history = <<>>
/\ losses = 0
/\ deactivated = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ first_log = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ timeouts = 0
/\ duplicates = 0
/\ elections = 1
/\ leader_state = (1 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 2 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 3 :> [n_leader |-> 13, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 6 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>])
/\ rolled_back = {}
/\ nonce = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ n_accepted = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ halts = 0
/\ log = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ cached_promise_message = (1 :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ 2 :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ 3 :> <<>> @@ 6 :> <<>>)

State 7: <Next line 1706, col 6 to line 1706, col 51 of module SequencePaxos>
/\ decided_ballot = <<0, 0>>
/\ wiped = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ promise = (1 :> 13 @@ 2 :> 13 @@ 3 :> 13 @@ 6 :> 0)
/\ read_only = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ wipes = 0
/\ accepted_idx = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ stage1_ghost = <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>
/\ recovery_responses = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ read_pending = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ read_round = (1 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 2 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 3 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 6 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}])
/\ buffered_proposals = (1 :> <<>> @@ 2 :> <<v2>> @@ 3 :> <<v1>> @@ 6 :> <<>>)
/\ read_only_entries = 0
/\ force_single = [log |-> <<>>, node |-> 0, decided |-> <<>>, history |-> <<>>]
/\ recovering = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ decided_idx = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ build_source = (6 :> 0)
/\ role = (1 :> "Follower" @@ 2 :> "Follower" @@ 3 :> "Leader" @@ 6 :> "Follower")
/\ phase = (1 :> "Prepare" @@ 2 :> "Prepare" @@ 3 :> "Prepare" @@ 6 :> "None")
/\ reads = <<[crashes |-> 0, rolled_back |-> {}, state |-> "none", read_index |-> 0, node |-> 0, decided_at_start |-> <<>>, result |-> <<>>, stale |-> FALSE, contract_stale_ok |-> FALSE, ballot |-> 0, decided_new_at_start |-> <<>>]>>
/\ running = {1, 2, 3}
/\ crashes = 0
/\ current_seq_num = ( 1 :> [session |-> 0, counter |-> 0] @@
  2 :> [session |-> 0, counter |-> 0] @@
  3 :> [session |-> 0, counter |-> 0] @@
  6 :> [session |-> 0, counter |-> 0] )
/\ proposed = {v1, v2, [session |-> 0, type |-> "StopSign", request_number |-> 0]}
/\ chan = (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 1>> :> <<>> @@ <<2, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 1>> :> <<>> @@ <<3, 2>> :> <<>>)
/\ client = [session |-> 0, request_number |-> 1, sends |-> 0]
/\ past_durable = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ buffered_stopsign = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<[session |-> 0, type |-> "StopSign", request_number |-> 0]>> @@ 6 :> <<>>)
/\ reconfigurations = 1
/\ decided_history = <<>>
/\ losses = 0
/\ deactivated = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ first_log = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ timeouts = 0
/\ duplicates = 0
/\ elections = 1
/\ leader_state = (1 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 2 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 3 :> [n_leader |-> 13, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 6 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>])
/\ rolled_back = {}
/\ nonce = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ n_accepted = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ halts = 0
/\ log = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ cached_promise_message = (1 :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ 2 :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ 3 :> <<>> @@ 6 :> <<>>)

State 8: <Receive line 1056, col 3 to line 1071, col 16 of module SequencePaxos>
/\ decided_ballot = <<0, 0>>
/\ wiped = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ promise = (1 :> 13 @@ 2 :> 13 @@ 3 :> 13 @@ 6 :> 0)
/\ read_only = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ wipes = 0
/\ accepted_idx = (1 :> 0 @@ 2 :> 0 @@ 3 :> 2 @@ 6 :> 0)
/\ stage1_ghost = <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>
/\ recovery_responses = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ read_pending = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ read_round = (1 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 2 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 3 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 6 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}])
/\ buffered_proposals = (1 :> <<>> @@ 2 :> <<v2>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ read_only_entries = 0
/\ force_single = [log |-> <<>>, node |-> 0, decided |-> <<>>, history |-> <<>>]
/\ recovering = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ decided_idx = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ build_source = (6 :> 0)
/\ role = (1 :> "Follower" @@ 2 :> "Follower" @@ 3 :> "Leader" @@ 6 :> "Follower")
/\ phase = (1 :> "Prepare" @@ 2 :> "Prepare" @@ 3 :> "Accept" @@ 6 :> "None")
/\ reads = <<[crashes |-> 0, rolled_back |-> {}, state |-> "none", read_index |-> 0, node |-> 0, decided_at_start |-> <<>>, result |-> <<>>, stale |-> FALSE, contract_stale_ok |-> FALSE, ballot |-> 0, decided_new_at_start |-> <<>>]>>
/\ running = {1, 2, 3}
/\ crashes = 0
/\ current_seq_num = ( 1 :> [session |-> 0, counter |-> 0] @@
  2 :> [session |-> 0, counter |-> 0] @@
  3 :> [session |-> 0, counter |-> 0] @@
  6 :> [session |-> 0, counter |-> 0] )
/\ proposed = {v1, v2, [session |-> 0, type |-> "StopSign", request_number |-> 0]}
/\ chan = (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 1>> :> <<>> @@ <<2, 3>> :> <<>> @@ <<3, 1>> :> <<>> @@ <<3, 2>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "AcceptSync", log_sync |-> <<[suffix |-> <<v1, [session |-> 0, type |-> "StopSign", request_number |-> 0]>>, sync_idx |-> 0]>>, seq_num |-> [session |-> 1, counter |-> 1], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>>)
/\ client = [session |-> 0, request_number |-> 1, sends |-> 0]
/\ past_durable = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ buffered_stopsign = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ reconfigurations = 1
/\ decided_history = <<>>
/\ losses = 0
/\ deactivated = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ first_log = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ timeouts = 0
/\ duplicates = 0
/\ elections = 1
/\ leader_state = (1 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 2 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 3 :> [n_leader |-> 13, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 1, counter |-> 1] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 2 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 2], max_promise_sync |-> <<>>] @@ 6 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>])
/\ rolled_back = {}
/\ nonce = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ n_accepted = (1 :> 0 @@ 2 :> 0 @@ 3 :> 13 @@ 6 :> 0)
/\ halts = 0
/\ log = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<v1, [session |-> 0, type |-> "StopSign", request_number |-> 0]>> @@ 6 :> <<>>)
/\ cached_promise_message = (1 :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ 2 :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ 3 :> <<>> @@ 6 :> <<>>)

State 9: <Receive line 1056, col 3 to line 1071, col 16 of module SequencePaxos>
/\ decided_ballot = <<0, 0>>
/\ wiped = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ promise = (1 :> 13 @@ 2 :> 13 @@ 3 :> 13 @@ 6 :> 0)
/\ read_only = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ wipes = 0
/\ accepted_idx = (1 :> 0 @@ 2 :> 2 @@ 3 :> 2 @@ 6 :> 0)
/\ stage1_ghost = <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>
/\ recovery_responses = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ read_pending = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ read_round = (1 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 2 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 3 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 6 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}])
/\ buffered_proposals = (1 :> <<>> @@ 2 :> <<v2>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ read_only_entries = 0
/\ force_single = [log |-> <<>>, node |-> 0, decided |-> <<>>, history |-> <<>>]
/\ recovering = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ decided_idx = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ build_source = (6 :> 0)
/\ role = (1 :> "Follower" @@ 2 :> "Follower" @@ 3 :> "Leader" @@ 6 :> "Follower")
/\ phase = (1 :> "Prepare" @@ 2 :> "Accept" @@ 3 :> "Accept" @@ 6 :> "None")
/\ reads = <<[crashes |-> 0, rolled_back |-> {}, state |-> "none", read_index |-> 0, node |-> 0, decided_at_start |-> <<>>, result |-> <<>>, stale |-> FALSE, contract_stale_ok |-> FALSE, ballot |-> 0, decided_new_at_start |-> <<>>]>>
/\ running = {1, 2, 3}
/\ crashes = 0
/\ current_seq_num = ( 1 :> [session |-> 0, counter |-> 0] @@
  2 :> [session |-> 1, counter |-> 1] @@
  3 :> [session |-> 0, counter |-> 0] @@
  6 :> [session |-> 0, counter |-> 0] )
/\ proposed = {v1, v2, [session |-> 0, type |-> "StopSign", request_number |-> 0]}
/\ chan = (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 1>> :> <<>> @@ <<2, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 2, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "Accepted", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 1>> :> <<>> @@ <<3, 2>> :> <<>>)
/\ client = [session |-> 0, request_number |-> 1, sends |-> 0]
/\ past_durable = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ buffered_stopsign = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ reconfigurations = 1
/\ decided_history = <<>>
/\ losses = 0
/\ deactivated = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ first_log = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ timeouts = 0
/\ duplicates = 0
/\ elections = 1
/\ leader_state = (1 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 2 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 3 :> [n_leader |-> 13, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 1, counter |-> 1] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 2 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 2], max_promise_sync |-> <<>>] @@ 6 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>])
/\ rolled_back = {}
/\ nonce = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ n_accepted = (1 :> 0 @@ 2 :> 13 @@ 3 :> 13 @@ 6 :> 0)
/\ halts = 0
/\ log = (1 :> <<>> @@ 2 :> <<v1, [session |-> 0, type |-> "StopSign", request_number |-> 0]>> @@ 3 :> <<v1, [session |-> 0, type |-> "StopSign", request_number |-> 0]>> @@ 6 :> <<>>)
/\ cached_promise_message = (1 :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)

State 10: <Receive line 1056, col 3 to line 1071, col 16 of module SequencePaxos>
/\ decided_ballot = <<13, 13>>
/\ wiped = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ promise = (1 :> 13 @@ 2 :> 13 @@ 3 :> 13 @@ 6 :> 0)
/\ read_only = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ wipes = 0
/\ accepted_idx = (1 :> 0 @@ 2 :> 2 @@ 3 :> 2 @@ 6 :> 0)
/\ stage1_ghost = <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>
/\ recovery_responses = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ read_pending = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ read_round = (1 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 2 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 3 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 6 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}])
/\ buffered_proposals = (1 :> <<>> @@ 2 :> <<v2>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ read_only_entries = 0
/\ force_single = [log |-> <<>>, node |-> 0, decided |-> <<>>, history |-> <<>>]
/\ recovering = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ decided_idx = (1 :> 0 @@ 2 :> 0 @@ 3 :> 2 @@ 6 :> 0)
/\ build_source = (6 :> 0)
/\ role = (1 :> "Follower" @@ 2 :> "Follower" @@ 3 :> "Leader" @@ 6 :> "Follower")
/\ phase = (1 :> "Prepare" @@ 2 :> "Accept" @@ 3 :> "Accept" @@ 6 :> "None")
/\ reads = <<[crashes |-> 0, rolled_back |-> {}, state |-> "none", read_index |-> 0, node |-> 0, decided_at_start |-> <<>>, result |-> <<>>, stale |-> FALSE, contract_stale_ok |-> FALSE, ballot |-> 0, decided_new_at_start |-> <<>>]>>
/\ running = {1, 2, 3}
/\ crashes = 0
/\ current_seq_num = ( 1 :> [session |-> 0, counter |-> 0] @@
  2 :> [session |-> 1, counter |-> 1] @@
  3 :> [session |-> 0, counter |-> 0] @@
  6 :> [session |-> 0, counter |-> 0] )
/\ proposed = {v1, v2, [session |-> 0, type |-> "StopSign", request_number |-> 0]}
/\ chan = (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 1>> :> <<>> @@ <<2, 3>> :> <<>> @@ <<3, 1>> :> <<>> @@ <<3, 2>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 2, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "Decide", log_sync |-> <<>>, seq_num |-> [session |-> 1, counter |-> 2], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>>)
/\ client = [session |-> 0, request_number |-> 1, sends |-> 0]
/\ past_durable = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ buffered_stopsign = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ reconfigurations = 1
/\ decided_history = <<v1, [session |-> 0, type |-> "StopSign", request_number |-> 0]>>
/\ losses = 0
/\ deactivated = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ first_log = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ timeouts = 0
/\ duplicates = 0
/\ elections = 1
/\ leader_state = (1 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 2 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 3 :> [n_leader |-> 13, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 1, counter |-> 2] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 2 @@ 3 :> 2 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 2], max_promise_sync |-> <<>>] @@ 6 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>])
/\ rolled_back = {}
/\ nonce = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ n_accepted = (1 :> 0 @@ 2 :> 13 @@ 3 :> 13 @@ 6 :> 0)
/\ halts = 0
/\ log = (1 :> <<>> @@ 2 :> <<v1, [session |-> 0, type |-> "StopSign", request_number |-> 0]>> @@ 3 :> <<v1, [session |-> 0, type |-> "StopSign", request_number |-> 0]>> @@ 6 :> <<>>)
/\ cached_promise_message = (1 :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)

State 11: <Next line 1721, col 6 to line 1721, col 75 of module SequencePaxos>
/\ decided_ballot = <<13, 13>>
/\ wiped = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ promise = (1 :> 13 @@ 2 :> 13 @@ 3 :> 13 @@ 6 :> 0)
/\ read_only = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ wipes = 0
/\ accepted_idx = (1 :> 0 @@ 2 :> 2 @@ 3 :> 2 @@ 6 :> 2)
/\ stage1_ghost = <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>
/\ recovery_responses = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ read_pending = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ read_round = (1 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 2 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 3 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 6 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}])
/\ buffered_proposals = (1 :> <<>> @@ 2 :> <<v2>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ read_only_entries = 0
/\ force_single = [log |-> <<>>, node |-> 0, decided |-> <<>>, history |-> <<>>]
/\ recovering = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ decided_idx = (1 :> 0 @@ 2 :> 0 @@ 3 :> 2 @@ 6 :> 2)
/\ build_source = (6 :> 3)
/\ role = (1 :> "Follower" @@ 2 :> "Follower" @@ 3 :> "Leader" @@ 6 :> "Follower")
/\ phase = (1 :> "Prepare" @@ 2 :> "Accept" @@ 3 :> "Accept" @@ 6 :> "None")
/\ reads = <<[crashes |-> 0, rolled_back |-> {}, state |-> "none", read_index |-> 0, node |-> 0, decided_at_start |-> <<>>, result |-> <<>>, stale |-> FALSE, contract_stale_ok |-> FALSE, ballot |-> 0, decided_new_at_start |-> <<>>]>>
/\ running = {1, 2, 3, 6}
/\ crashes = 0
/\ current_seq_num = ( 1 :> [session |-> 0, counter |-> 0] @@
  2 :> [session |-> 1, counter |-> 1] @@
  3 :> [session |-> 0, counter |-> 0] @@
  6 :> [session |-> 0, counter |-> 0] )
/\ proposed = {v1, v2, [session |-> 0, type |-> "StopSign", request_number |-> 0]}
/\ chan = (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 1>> :> <<>> @@ <<2, 3>> :> <<>> @@ <<3, 1>> :> <<>> @@ <<3, 2>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 2, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "Decide", log_sync |-> <<>>, seq_num |-> [session |-> 1, counter |-> 2], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>>)
/\ client = [session |-> 0, request_number |-> 1, sends |-> 0]
/\ past_durable = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ buffered_stopsign = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ reconfigurations = 1
/\ decided_history = <<v1, [session |-> 0, type |-> "StopSign", request_number |-> 0]>>
/\ losses = 0
/\ deactivated = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ first_log = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<v1, [session |-> 0, type |-> "StopSign", request_number |-> 0]>>)
/\ timeouts = 0
/\ duplicates = 0
/\ elections = 1
/\ leader_state = (1 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 2 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 3 :> [n_leader |-> 13, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 1, counter |-> 2] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 2 @@ 3 :> 2 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 2], max_promise_sync |-> <<>>] @@ 6 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>])
/\ rolled_back = {}
/\ nonce = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ n_accepted = (1 :> 0 @@ 2 :> 13 @@ 3 :> 13 @@ 6 :> 0)
/\ halts = 0
/\ log = (1 :> <<>> @@ 2 :> <<v1, [session |-> 0, type |-> "StopSign", request_number |-> 0]>> @@ 3 :> <<v1, [session |-> 0, type |-> "StopSign", request_number |-> 0]>> @@ 6 :> <<v1, [session |-> 0, type |-> "StopSign", request_number |-> 0]>>)
/\ cached_promise_message = (1 :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)

State 12: <Next line 1705, col 6 to line 1705, col 62 of module SequencePaxos>
/\ decided_ballot = <<13, 13>>
/\ wiped = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ promise = (1 :> 13 @@ 2 :> 13 @@ 3 :> 13 @@ 6 :> 16)
/\ read_only = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ wipes = 0
/\ accepted_idx = (1 :> 0 @@ 2 :> 2 @@ 3 :> 2 @@ 6 :> 2)
/\ stage1_ghost = <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>
/\ recovery_responses = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ read_pending = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ read_round = (1 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 2 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 3 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 6 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}])
/\ buffered_proposals = (1 :> <<>> @@ 2 :> <<v2>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ read_only_entries = 0
/\ force_single = [log |-> <<>>, node |-> 0, decided |-> <<>>, history |-> <<>>]
/\ recovering = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ decided_idx = (1 :> 0 @@ 2 :> 0 @@ 3 :> 2 @@ 6 :> 2)
/\ build_source = (6 :> 3)
/\ role = (1 :> "Follower" @@ 2 :> "Follower" @@ 3 :> "Leader" @@ 6 :> "Leader")
/\ phase = (1 :> "Prepare" @@ 2 :> "Accept" @@ 3 :> "Accept" @@ 6 :> "Prepare")
/\ reads = <<[crashes |-> 0, rolled_back |-> {}, state |-> "none", read_index |-> 0, node |-> 0, decided_at_start |-> <<>>, result |-> <<>>, stale |-> FALSE, contract_stale_ok |-> FALSE, ballot |-> 0, decided_new_at_start |-> <<>>]>>
/\ running = {1, 2, 3, 6}
/\ crashes = 0
/\ current_seq_num = ( 1 :> [session |-> 0, counter |-> 0] @@
  2 :> [session |-> 1, counter |-> 1] @@
  3 :> [session |-> 0, counter |-> 0] @@
  6 :> [session |-> 0, counter |-> 0] )
/\ proposed = {v1, v2, [session |-> 0, type |-> "StopSign", request_number |-> 0]}
/\ chan = (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 1>> :> <<>> @@ <<2, 3>> :> <<>> @@ <<3, 1>> :> <<>> @@ <<3, 2>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 2, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "Decide", log_sync |-> <<>>, seq_num |-> [session |-> 1, counter |-> 2], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>>)
/\ client = [session |-> 0, request_number |-> 1, sends |-> 0]
/\ past_durable = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ buffered_stopsign = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ reconfigurations = 1
/\ decided_history = <<v1, [session |-> 0, type |-> "StopSign", request_number |-> 0]>>
/\ losses = 0
/\ deactivated = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ first_log = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<v1, [session |-> 0, type |-> "StopSign", request_number |-> 0]>>)
/\ timeouts = 0
/\ duplicates = 0
/\ elections = 2
/\ leader_state = (1 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 2 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 3 :> [n_leader |-> 13, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 1, counter |-> 2] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 2 @@ 3 :> 2 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 2], max_promise_sync |-> <<>>] @@ 6 :> [n_leader |-> 16, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 2, accepted_idx |-> 2, state |-> "Promised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 2, accepted_idx |-> 2, pid |-> 6], max_promise_sync |-> <<>>])
/\ rolled_back = {}
/\ nonce = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ n_accepted = (1 :> 0 @@ 2 :> 13 @@ 3 :> 13 @@ 6 :> 0)
/\ halts = 0
/\ log = (1 :> <<>> @@ 2 :> <<v1, [session |-> 0, type |-> "StopSign", request_number |-> 0]>> @@ 3 :> <<v1, [session |-> 0, type |-> "StopSign", request_number |-> 0]>> @@ 6 :> <<v1, [session |-> 0, type |-> "StopSign", request_number |-> 0]>>)
/\ cached_promise_message = (1 :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)

State 13: <Receive line 1056, col 3 to line 1071, col 16 of module SequencePaxos>
/\ decided_ballot = <<13, 13>>
/\ wiped = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ promise = (1 :> 13 @@ 2 :> 13 @@ 3 :> 13 @@ 6 :> 16)
/\ read_only = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ wipes = 0
/\ accepted_idx = (1 :> 0 @@ 2 :> 2 @@ 3 :> 2 @@ 6 :> 2)
/\ stage1_ghost = <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>
/\ recovery_responses = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ read_pending = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ read_round = (1 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 2 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 3 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 6 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}])
/\ buffered_proposals = (1 :> <<>> @@ 2 :> <<v2>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ read_only_entries = 0
/\ force_single = [log |-> <<>>, node |-> 0, decided |-> <<>>, history |-> <<>>]
/\ recovering = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ decided_idx = (1 :> 0 @@ 2 :> 0 @@ 3 :> 2 @@ 6 :> 2)
/\ build_source = (6 :> 3)
/\ role = (1 :> "Follower" @@ 2 :> "Follower" @@ 3 :> "Leader" @@ 6 :> "Leader")
/\ phase = (1 :> "Prepare" @@ 2 :> "Accept" @@ 3 :> "Accept" @@ 6 :> "Prepare")
/\ reads = <<[crashes |-> 0, rolled_back |-> {}, state |-> "none", read_index |-> 0, node |-> 0, decided_at_start |-> <<>>, result |-> <<>>, stale |-> FALSE, contract_stale_ok |-> FALSE, ballot |-> 0, decided_new_at_start |-> <<>>]>>
/\ running = {1, 2, 3, 6}
/\ crashes = 0
/\ current_seq_num = ( 1 :> [session |-> 0, counter |-> 0] @@
  2 :> [session |-> 1, counter |-> 1] @@
  3 :> [session |-> 0, counter |-> 0] @@
  6 :> [session |-> 0, counter |-> 0] )
/\ proposed = {v1, v2, [session |-> 0, type |-> "StopSign", request_number |-> 0]}
/\ chan = (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<>> @@ <<2, 1>> :> <<>> @@ <<2, 3>> :> <<>> @@ <<3, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 2, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "AcceptSync", log_sync |-> <<[suffix |-> <<v1, [session |-> 0, type |-> "StopSign", request_number |-> 0]>>, sync_idx |-> 0]>>, seq_num |-> [session |-> 1, counter |-> 1], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 2>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 2, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "Decide", log_sync |-> <<>>, seq_num |-> [session |-> 1, counter |-> 2], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>>)
/\ client = [session |-> 0, request_number |-> 1, sends |-> 0]
/\ past_durable = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ buffered_stopsign = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ reconfigurations = 1
/\ decided_history = <<v1, [session |-> 0, type |-> "StopSign", request_number |-> 0]>>
/\ losses = 0
/\ deactivated = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ first_log = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<v1, [session |-> 0, type |-> "StopSign", request_number |-> 0]>>)
/\ timeouts = 0
/\ duplicates = 0
/\ elections = 2
/\ leader_state = (1 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 2 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 3 :> [n_leader |-> 13, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 1, counter |-> 1] @@ 2 :> [session |-> 1, counter |-> 2] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 2 @@ 3 :> 2 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 2], max_promise_sync |-> <<>>] @@ 6 :> [n_leader |-> 16, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 2, accepted_idx |-> 2, state |-> "Promised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 2, accepted_idx |-> 2, pid |-> 6], max_promise_sync |-> <<>>])
/\ rolled_back = {}
/\ nonce = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ n_accepted = (1 :> 0 @@ 2 :> 13 @@ 3 :> 13 @@ 6 :> 0)
/\ halts = 0
/\ log = (1 :> <<>> @@ 2 :> <<v1, [session |-> 0, type |-> "StopSign", request_number |-> 0]>> @@ 3 :> <<v1, [session |-> 0, type |-> "StopSign", request_number |-> 0]>> @@ 6 :> <<v1, [session |-> 0, type |-> "StopSign", request_number |-> 0]>>)
/\ cached_promise_message = (1 :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)

State 14: <Next line 1717, col 6 to line 1717, col 57 of module SequencePaxos>
/\ decided_ballot = <<13, 13>>
/\ wiped = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ promise = (1 :> 13 @@ 2 :> 13 @@ 3 :> 13 @@ 6 :> 16)
/\ read_only = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ wipes = 0
/\ accepted_idx = (1 :> 0 @@ 2 :> 2 @@ 3 :> 2 @@ 6 :> 2)
/\ stage1_ghost = <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>
/\ recovery_responses = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ read_pending = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {1})
/\ read_round = (1 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 2 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 3 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 6 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}])
/\ buffered_proposals = (1 :> <<>> @@ 2 :> <<v2>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ read_only_entries = 0
/\ force_single = [log |-> <<>>, node |-> 0, decided |-> <<>>, history |-> <<>>]
/\ recovering = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ decided_idx = (1 :> 0 @@ 2 :> 0 @@ 3 :> 2 @@ 6 :> 2)
/\ build_source = (6 :> 3)
/\ role = (1 :> "Follower" @@ 2 :> "Follower" @@ 3 :> "Leader" @@ 6 :> "Leader")
/\ phase = (1 :> "Prepare" @@ 2 :> "Accept" @@ 3 :> "Accept" @@ 6 :> "Prepare")
/\ reads = <<[crashes |-> 0, rolled_back |-> {}, state |-> "pending", read_index |-> 0, node |-> 6, decided_at_start |-> <<v1, [session |-> 0, type |-> "StopSign", request_number |-> 0]>>, result |-> <<>>, stale |-> FALSE, contract_stale_ok |-> FALSE, ballot |-> 0, decided_new_at_start |-> <<>>]>>
/\ running = {1, 2, 3, 6}
/\ crashes = 0
/\ current_seq_num = ( 1 :> [session |-> 0, counter |-> 0] @@
  2 :> [session |-> 1, counter |-> 1] @@
  3 :> [session |-> 0, counter |-> 0] @@
  6 :> [session |-> 0, counter |-> 0] )
/\ proposed = {v1, v2, [session |-> 0, type |-> "StopSign", request_number |-> 0]}
/\ chan = (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<>> @@ <<2, 1>> :> <<>> @@ <<2, 3>> :> <<>> @@ <<3, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 2, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "AcceptSync", log_sync |-> <<[suffix |-> <<v1, [session |-> 0, type |-> "StopSign", request_number |-> 0]>>, sync_idx |-> 0]>>, seq_num |-> [session |-> 1, counter |-> 1], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 2>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 2, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "Decide", log_sync |-> <<>>, seq_num |-> [session |-> 1, counter |-> 2], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>>)
/\ client = [session |-> 0, request_number |-> 1, sends |-> 0]
/\ past_durable = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ buffered_stopsign = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ reconfigurations = 1
/\ decided_history = <<v1, [session |-> 0, type |-> "StopSign", request_number |-> 0]>>
/\ losses = 0
/\ deactivated = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ first_log = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<v1, [session |-> 0, type |-> "StopSign", request_number |-> 0]>>)
/\ timeouts = 0
/\ duplicates = 0
/\ elections = 2
/\ leader_state = (1 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 2 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 3 :> [n_leader |-> 13, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 1, counter |-> 1] @@ 2 :> [session |-> 1, counter |-> 2] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 2 @@ 3 :> 2 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 2], max_promise_sync |-> <<>>] @@ 6 :> [n_leader |-> 16, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 2, accepted_idx |-> 2, state |-> "Promised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 2, accepted_idx |-> 2, pid |-> 6], max_promise_sync |-> <<>>])
/\ rolled_back = {}
/\ nonce = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ n_accepted = (1 :> 0 @@ 2 :> 13 @@ 3 :> 13 @@ 6 :> 0)
/\ halts = 0
/\ log = (1 :> <<>> @@ 2 :> <<v1, [session |-> 0, type |-> "StopSign", request_number |-> 0]>> @@ 3 :> <<v1, [session |-> 0, type |-> "StopSign", request_number |-> 0]>> @@ 6 :> <<v1, [session |-> 0, type |-> "StopSign", request_number |-> 0]>>)
/\ cached_promise_message = (1 :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)

The number of states generated: 114360
Simulation using seed 1 and aril 0
Progress: 114560 states checked.
Finished in 03s at (2026-10-19 00:16:28)
exit 13

```

