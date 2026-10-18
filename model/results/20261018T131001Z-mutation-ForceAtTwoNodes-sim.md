# TLC run mutation-ForceAtTwoNodes-sim, 20261018T131001Z

Counter-check of plan-fixes-20261018 item 16: mutation ForceAtTwoNodes, random simulation (-simulate -deadlock -depth 60, seed 1, limit 15 min, SYMMETRY and VIEW removed). Expected: a violation of ReadOnlyEndsBeforeForce. Mode: simulation.

## Configuration

Spec: `model/tla/SequencePaxos.tla`. Config file `model/tla/MC_mut_ForceAtTwoNodes.sim.cfg`:

```
CONSTANTS
  Nodes = {1, 2, 3}
  Values = {v1, v2}
  MaxBallot = 1
  MaxLog = 1
  MaxCounter = 3
  MaxSession = 2
  Cap = 1
  MaxCrash = 0
  MaxWipe = 0
  MaxLoss = 0
  MaxDuplicate = 0
  MaxTimeout = 0
  MaxElection = 2
  RuleR = TRUE
  DecidedIdxMonotone = TRUE
  MaxRollback = 0
  Recovery = TRUE
  Mutation = "ForceAtTwoNodes"
  ClientSessions = FALSE
  MaxClientSend = 0
  MaxRequest = 0
  ReadIndex = FALSE
  MaxRead = 0
  NextConfigNodes = {6}
  MaxReconfigure = 0
  SingleNode = TRUE
  MaxForceSingle = 1
  MaxReadOnly = 2
  MaxHalt = 2
  StaleReadNode = FALSE
SPECIFICATION Spec
CONSTRAINT Bounds
PROPERTIES ReadOnlyEndsBeforeForce
```

## Tools

- TLC2 Version 2.19 of 08 August 2024 (rev: 5a47802)
- Running Random Simulation with seed 1 with 3 workers on 4 cores with 9102MB heap and 64MB offheap memory [pid: 6293] (Linux 6.18.44-fc-v139 amd64, Ubuntu 21.0.12.1 x86_64).
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
- Wall time: 00s (start 20261018T131001Z, end 20261018T131002Z, UTC)

## Result

```
Error: Action property ReadOnlyEndsBeforeForce is violated.
Error: The behavior up to this point is:
```

## Trace, changed variables per step

```
```

## Trace, full TLC output

```
Error: The behavior up to this point is:
State 1: <Initial predicate>
/\ decided_ballot = <<0>>
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
/\ reads = << >>
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
  <<3, 2>> :> <<>> @@
  <<6, 1>> :> <<>> @@
  <<6, 2>> :> <<>> @@
  <<6, 3>> :> <<>> )
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

State 2: <Next line 1726, col 6 to line 1726, col 31 of module SequencePaxos>
/\ decided_ballot = <<0>>
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
/\ reads = << >>
/\ running = {1, 2}
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
  <<3, 2>> :> <<>> @@
  <<6, 1>> :> <<>> @@
  <<6, 2>> :> <<>> @@
  <<6, 3>> :> <<>> )
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
/\ halts = 1
/\ log = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ cached_promise_message = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)

State 3: <Next line 1705, col 6 to line 1705, col 62 of module SequencePaxos>
/\ decided_ballot = <<0>>
/\ wiped = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ promise = (1 :> 0 @@ 2 :> 12 @@ 3 :> 0 @@ 6 :> 0)
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
/\ role = (1 :> "Follower" @@ 2 :> "Leader" @@ 3 :> "Follower" @@ 6 :> "Follower")
/\ phase = (1 :> "None" @@ 2 :> "Prepare" @@ 3 :> "None" @@ 6 :> "None")
/\ reads = << >>
/\ running = {1, 2}
/\ crashes = 0
/\ current_seq_num = ( 1 :> [session |-> 0, counter |-> 0] @@
  2 :> [session |-> 0, counter |-> 0] @@
  3 :> [session |-> 0, counter |-> 0] @@
  6 :> [session |-> 0, counter |-> 0] )
/\ proposed = {}
/\ chan = (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<>> @@ <<2, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 1>> :> <<>> @@ <<3, 2>> :> <<>> @@ <<6, 1>> :> <<>> @@ <<6, 2>> :> <<>> @@ <<6, 3>> :> <<>>)
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
/\ leader_state = (1 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 2 :> [n_leader |-> 12, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 3 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 6 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>])
/\ rolled_back = {}
/\ nonce = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ n_accepted = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ halts = 1
/\ log = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ cached_promise_message = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)

State 4: <Next line 1726, col 6 to line 1726, col 31 of module SequencePaxos>
/\ decided_ballot = <<0>>
/\ wiped = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ promise = (1 :> 0 @@ 2 :> 12 @@ 3 :> 0 @@ 6 :> 0)
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
/\ role = (1 :> "Follower" @@ 2 :> "Leader" @@ 3 :> "Follower" @@ 6 :> "Follower")
/\ phase = (1 :> "None" @@ 2 :> "Prepare" @@ 3 :> "None" @@ 6 :> "None")
/\ reads = << >>
/\ running = {2}
/\ crashes = 0
/\ current_seq_num = ( 1 :> [session |-> 0, counter |-> 0] @@
  2 :> [session |-> 0, counter |-> 0] @@
  3 :> [session |-> 0, counter |-> 0] @@
  6 :> [session |-> 0, counter |-> 0] )
/\ proposed = {}
/\ chan = (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<>> @@ <<2, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 1>> :> <<>> @@ <<3, 2>> :> <<>> @@ <<6, 1>> :> <<>> @@ <<6, 2>> :> <<>> @@ <<6, 3>> :> <<>>)
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
/\ leader_state = (1 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 2 :> [n_leader |-> 12, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 3 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 6 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>])
/\ rolled_back = {}
/\ nonce = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ n_accepted = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ halts = 2
/\ log = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ cached_promise_message = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)

State 5: <Next line 1724, col 6 to line 1724, col 60 of module SequencePaxos>
/\ decided_ballot = <<0>>
/\ wiped = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ promise = (1 :> 0 @@ 2 :> 12 @@ 3 :> 0 @@ 6 :> 0)
/\ read_only = (1 :> FALSE @@ 2 :> TRUE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ wipes = 0
/\ accepted_idx = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ stage1_ghost = <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>
/\ recovery_responses = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ read_pending = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ read_round = (1 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 2 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 3 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 6 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}])
/\ buffered_proposals = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ read_only_entries = 1
/\ force_single = [log |-> <<>>, node |-> 0, decided |-> <<>>, history |-> <<>>]
/\ recovering = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ decided_idx = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ build_source = (6 :> 0)
/\ role = (1 :> "Follower" @@ 2 :> "Follower" @@ 3 :> "Follower" @@ 6 :> "Follower")
/\ phase = (1 :> "None" @@ 2 :> "Recover" @@ 3 :> "None" @@ 6 :> "None")
/\ reads = << >>
/\ running = {2}
/\ crashes = 0
/\ current_seq_num = ( 1 :> [session |-> 0, counter |-> 0] @@
  2 :> [session |-> 0, counter |-> 0] @@
  3 :> [session |-> 0, counter |-> 0] @@
  6 :> [session |-> 0, counter |-> 0] )
/\ proposed = {}
/\ chan = (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<>> @@ <<2, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 1>> :> <<>> @@ <<3, 2>> :> <<>> @@ <<6, 1>> :> <<>> @@ <<6, 2>> :> <<>> @@ <<6, 3>> :> <<>>)
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
/\ leader_state = (1 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 2 :> [n_leader |-> 12, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 3 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 6 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>])
/\ rolled_back = {}
/\ nonce = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ n_accepted = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ halts = 2
/\ log = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ cached_promise_message = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)

State 6: <Next line 1723, col 6 to line 1723, col 74 of module SequencePaxos>
/\ decided_ballot = <<0>>
/\ wiped = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ promise = (1 :> 0 @@ 2 :> 12 @@ 3 :> 0 @@ 6 :> 0)
/\ read_only = (1 :> FALSE @@ 2 :> TRUE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ wipes = 0
/\ accepted_idx = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ stage1_ghost = <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>
/\ recovery_responses = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ read_pending = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ read_round = (1 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 2 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 3 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 6 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}])
/\ buffered_proposals = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ read_only_entries = 1
/\ force_single = [log |-> <<>>, node |-> 2, decided |-> <<>>, history |-> <<>>]
/\ recovering = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ decided_idx = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ build_source = (6 :> 0)
/\ role = (1 :> "Follower" @@ 2 :> "Follower" @@ 3 :> "Follower" @@ 6 :> "Follower")
/\ phase = (1 :> "None" @@ 2 :> "Recover" @@ 3 :> "None" @@ 6 :> "None")
/\ reads = << >>
/\ running = {6}
/\ crashes = 0
/\ current_seq_num = ( 1 :> [session |-> 0, counter |-> 0] @@
  2 :> [session |-> 0, counter |-> 0] @@
  3 :> [session |-> 0, counter |-> 0] @@
  6 :> [session |-> 0, counter |-> 0] )
/\ proposed = {}
/\ chan = (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<>> @@ <<2, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 1>> :> <<>> @@ <<3, 2>> :> <<>> @@ <<6, 1>> :> <<>> @@ <<6, 2>> :> <<>> @@ <<6, 3>> :> <<>>)
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
/\ leader_state = (1 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 2 :> [n_leader |-> 12, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 3 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 6 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>])
/\ rolled_back = {}
/\ nonce = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ n_accepted = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ halts = 2
/\ log = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ cached_promise_message = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)

State 7: <Next line 1725, col 6 to line 1725, col 58 of module SequencePaxos>
/\ decided_ballot = <<0>>
/\ wiped = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ promise = (1 :> 0 @@ 2 :> 12 @@ 3 :> 0 @@ 6 :> 0)
/\ read_only = (1 :> FALSE @@ 2 :> TRUE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ wipes = 0
/\ accepted_idx = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ stage1_ghost = <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>
/\ recovery_responses = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ read_pending = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ read_round = (1 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 2 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 3 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 6 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}])
/\ buffered_proposals = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ read_only_entries = 1
/\ force_single = [log |-> <<>>, node |-> 2, decided |-> <<>>, history |-> <<>>]
/\ recovering = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ decided_idx = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ build_source = (6 :> 0)
/\ role = (1 :> "Follower" @@ 2 :> "Follower" @@ 3 :> "Follower" @@ 6 :> "Follower")
/\ phase = (1 :> "None" @@ 2 :> "Recover" @@ 3 :> "None" @@ 6 :> "None")
/\ reads = << >>
/\ running = {6}
/\ crashes = 0
/\ current_seq_num = ( 1 :> [session |-> 0, counter |-> 0] @@
  2 :> [session |-> 0, counter |-> 0] @@
  3 :> [session |-> 0, counter |-> 0] @@
  6 :> [session |-> 0, counter |-> 0] )
/\ proposed = {}
/\ chan = (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<>> @@ <<2, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 1>> :> <<>> @@ <<3, 2>> :> <<>> @@ <<6, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 0, type |-> "Deactivated", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<6, 2>> :> <<>> @@ <<6, 3>> :> <<>>)
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
/\ leader_state = (1 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 2 :> [n_leader |-> 12, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 3 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 6 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>])
/\ rolled_back = {}
/\ nonce = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ n_accepted = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ halts = 2
/\ log = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ cached_promise_message = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)

State 8: <Next line 1705, col 6 to line 1705, col 62 of module SequencePaxos>
/\ decided_ballot = <<0>>
/\ wiped = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ promise = (1 :> 0 @@ 2 :> 12 @@ 3 :> 0 @@ 6 :> 16)
/\ read_only = (1 :> FALSE @@ 2 :> TRUE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ wipes = 0
/\ accepted_idx = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ stage1_ghost = <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>
/\ recovery_responses = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ read_pending = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ read_round = (1 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 2 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 3 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 6 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}])
/\ buffered_proposals = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ read_only_entries = 1
/\ force_single = [log |-> <<>>, node |-> 2, decided |-> <<>>, history |-> <<>>]
/\ recovering = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ decided_idx = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ build_source = (6 :> 0)
/\ role = (1 :> "Follower" @@ 2 :> "Follower" @@ 3 :> "Follower" @@ 6 :> "Leader")
/\ phase = (1 :> "None" @@ 2 :> "Recover" @@ 3 :> "None" @@ 6 :> "Prepare")
/\ reads = << >>
/\ running = {6}
/\ crashes = 0
/\ current_seq_num = ( 1 :> [session |-> 0, counter |-> 0] @@
  2 :> [session |-> 0, counter |-> 0] @@
  3 :> [session |-> 0, counter |-> 0] @@
  6 :> [session |-> 0, counter |-> 0] )
/\ proposed = {}
/\ chan = (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<>> @@ <<2, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 1>> :> <<>> @@ <<3, 2>> :> <<>> @@ <<6, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 0, type |-> "Deactivated", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<6, 2>> :> <<>> @@ <<6, 3>> :> <<>>)
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
/\ elections = 2
/\ leader_state = (1 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 2 :> [n_leader |-> 12, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 3 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 6 :> [n_leader |-> 16, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 6], max_promise_sync |-> <<>>])
/\ rolled_back = {}
/\ nonce = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ n_accepted = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ halts = 2
/\ log = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ cached_promise_message = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)

State 9: <Next line 1725, col 6 to line 1725, col 58 of module SequencePaxos>
/\ decided_ballot = <<0>>
/\ wiped = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ promise = (1 :> 0 @@ 2 :> 12 @@ 3 :> 0 @@ 6 :> 16)
/\ read_only = (1 :> FALSE @@ 2 :> TRUE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ wipes = 0
/\ accepted_idx = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ stage1_ghost = <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>
/\ recovery_responses = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ read_pending = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ read_round = (1 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 2 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 3 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 6 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}])
/\ buffered_proposals = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ read_only_entries = 1
/\ force_single = [log |-> <<>>, node |-> 2, decided |-> <<>>, history |-> <<>>]
/\ recovering = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ decided_idx = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ build_source = (6 :> 0)
/\ role = (1 :> "Follower" @@ 2 :> "Follower" @@ 3 :> "Follower" @@ 6 :> "Leader")
/\ phase = (1 :> "None" @@ 2 :> "Recover" @@ 3 :> "None" @@ 6 :> "Prepare")
/\ reads = << >>
/\ running = {6}
/\ crashes = 0
/\ current_seq_num = ( 1 :> [session |-> 0, counter |-> 0] @@
  2 :> [session |-> 0, counter |-> 0] @@
  3 :> [session |-> 0, counter |-> 0] @@
  6 :> [session |-> 0, counter |-> 0] )
/\ proposed = {}
/\ chan = (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<>> @@ <<2, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 1>> :> <<>> @@ <<3, 2>> :> <<>> @@ <<6, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 0, type |-> "Deactivated", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<6, 2>> :> <<>> @@ <<6, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 0, type |-> "Deactivated", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>>)
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
/\ elections = 2
/\ leader_state = (1 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 2 :> [n_leader |-> 12, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 3 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 6 :> [n_leader |-> 16, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 6], max_promise_sync |-> <<>>])
/\ rolled_back = {}
/\ nonce = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ n_accepted = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ halts = 2
/\ log = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ cached_promise_message = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)

State 10: <Next line 1727, col 6 to line 1727, col 41 of module SequencePaxos>
/\ decided_ballot = <<0>>
/\ wiped = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ promise = (1 :> 0 @@ 2 :> 12 @@ 3 :> 0 @@ 6 :> 16)
/\ read_only = (1 :> FALSE @@ 2 :> TRUE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ wipes = 0
/\ accepted_idx = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ stage1_ghost = <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>
/\ recovery_responses = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ read_pending = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ read_round = (1 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 2 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 3 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 6 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}])
/\ buffered_proposals = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ read_only_entries = 1
/\ force_single = [log |-> <<>>, node |-> 2, decided |-> <<>>, history |-> <<>>]
/\ recovering = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> TRUE @@ 6 :> FALSE)
/\ decided_idx = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ build_source = (6 :> 0)
/\ role = (1 :> "Follower" @@ 2 :> "Follower" @@ 3 :> "Follower" @@ 6 :> "Leader")
/\ phase = (1 :> "None" @@ 2 :> "Recover" @@ 3 :> "Recover" @@ 6 :> "Prepare")
/\ reads = << >>
/\ running = {3, 6}
/\ crashes = 0
/\ current_seq_num = ( 1 :> [session |-> 0, counter |-> 0] @@
  2 :> [session |-> 0, counter |-> 0] @@
  3 :> [session |-> 0, counter |-> 0] @@
  6 :> [session |-> 0, counter |-> 0] )
/\ proposed = {}
/\ chan = (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<>> @@ <<2, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 1, reads |-> {}, n |-> 0, type |-> "Recovery", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 2>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 1, reads |-> {}, n |-> 0, type |-> "Recovery", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<6, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 0, type |-> "Deactivated", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<6, 2>> :> <<>> @@ <<6, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 0, type |-> "Deactivated", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>>)
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
/\ elections = 2
/\ leader_state = (1 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 2 :> [n_leader |-> 12, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 3 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 6 :> [n_leader |-> 16, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 6], max_promise_sync |-> <<>>])
/\ rolled_back = {}
/\ nonce = (1 :> 0 @@ 2 :> 0 @@ 3 :> 1 @@ 6 :> 0)
/\ n_accepted = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ halts = 2
/\ log = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ cached_promise_message = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)

State 11: <Next line 1724, col 6 to line 1724, col 60 of module SequencePaxos>
/\ decided_ballot = <<0>>
/\ wiped = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ promise = (1 :> 0 @@ 2 :> 12 @@ 3 :> 0 @@ 6 :> 16)
/\ read_only = (1 :> FALSE @@ 2 :> TRUE @@ 3 :> TRUE @@ 6 :> FALSE)
/\ wipes = 0
/\ accepted_idx = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ stage1_ghost = <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>
/\ recovery_responses = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ read_pending = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ read_round = (1 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 2 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 3 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 6 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}])
/\ buffered_proposals = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ read_only_entries = 2
/\ force_single = [log |-> <<>>, node |-> 2, decided |-> <<>>, history |-> <<>>]
/\ recovering = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ decided_idx = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ build_source = (6 :> 0)
/\ role = (1 :> "Follower" @@ 2 :> "Follower" @@ 3 :> "Follower" @@ 6 :> "Leader")
/\ phase = (1 :> "None" @@ 2 :> "Recover" @@ 3 :> "Recover" @@ 6 :> "Prepare")
/\ reads = << >>
/\ running = {3, 6}
/\ crashes = 0
/\ current_seq_num = ( 1 :> [session |-> 0, counter |-> 0] @@
  2 :> [session |-> 0, counter |-> 0] @@
  3 :> [session |-> 0, counter |-> 0] @@
  6 :> [session |-> 0, counter |-> 0] )
/\ proposed = {}
/\ chan = (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<>> @@ <<2, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 1, reads |-> {}, n |-> 0, type |-> "Recovery", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 2>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 1, reads |-> {}, n |-> 0, type |-> "Recovery", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<6, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 0, type |-> "Deactivated", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<6, 2>> :> <<>> @@ <<6, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 0, type |-> "Deactivated", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>>)
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
/\ elections = 2
/\ leader_state = (1 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 2 :> [n_leader |-> 12, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 3 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 6 :> [n_leader |-> 16, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 6], max_promise_sync |-> <<>>])
/\ rolled_back = {}
/\ nonce = (1 :> 0 @@ 2 :> 0 @@ 3 :> 1 @@ 6 :> 0)
/\ n_accepted = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ halts = 2
/\ log = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ cached_promise_message = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)

State 12: <Next line 1727, col 6 to line 1727, col 41 of module SequencePaxos>
/\ decided_ballot = <<0>>
/\ wiped = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ promise = (1 :> 0 @@ 2 :> 12 @@ 3 :> 0 @@ 6 :> 16)
/\ read_only = (1 :> FALSE @@ 2 :> TRUE @@ 3 :> TRUE @@ 6 :> FALSE)
/\ wipes = 0
/\ accepted_idx = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ stage1_ghost = <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>
/\ recovery_responses = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ read_pending = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ read_round = (1 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 2 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 3 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 6 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}])
/\ buffered_proposals = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ read_only_entries = 2
/\ force_single = [log |-> <<>>, node |-> 2, decided |-> <<>>, history |-> <<>>]
/\ recovering = (1 :> TRUE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ decided_idx = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ build_source = (6 :> 0)
/\ role = (1 :> "Follower" @@ 2 :> "Follower" @@ 3 :> "Follower" @@ 6 :> "Leader")
/\ phase = (1 :> "Recover" @@ 2 :> "Recover" @@ 3 :> "Recover" @@ 6 :> "Prepare")
/\ reads = << >>
/\ running = {1, 3, 6}
/\ crashes = 0
/\ current_seq_num = ( 1 :> [session |-> 0, counter |-> 0] @@
  2 :> [session |-> 0, counter |-> 0] @@
  3 :> [session |-> 0, counter |-> 0] @@
  6 :> [session |-> 0, counter |-> 0] )
/\ proposed = {}
/\ chan = (<<1, 2>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 1, reads |-> {}, n |-> 0, type |-> "Recovery", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<1, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 1, reads |-> {}, n |-> 0, type |-> "Recovery", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 1, reads |-> {}, n |-> 0, type |-> "Recovery", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 2>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 1, reads |-> {}, n |-> 0, type |-> "Recovery", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<6, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 0, type |-> "Deactivated", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<6, 2>> :> <<>> @@ <<6, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 0, type |-> "Deactivated", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>>)
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
/\ elections = 2
/\ leader_state = (1 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 2 :> [n_leader |-> 12, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 3 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 6 :> [n_leader |-> 16, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 6], max_promise_sync |-> <<>>])
/\ rolled_back = {}
/\ nonce = (1 :> 1 @@ 2 :> 0 @@ 3 :> 1 @@ 6 :> 0)
/\ n_accepted = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ halts = 2
/\ log = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ cached_promise_message = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)

State 13: <Next line 1724, col 6 to line 1724, col 60 of module SequencePaxos>
/\ decided_ballot = <<0>>
/\ wiped = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ promise = (1 :> 0 @@ 2 :> 12 @@ 3 :> 0 @@ 6 :> 16)
/\ read_only = (1 :> FALSE @@ 2 :> TRUE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ wipes = 0
/\ accepted_idx = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ stage1_ghost = <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> 0]>>
/\ recovery_responses = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ read_pending = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ read_round = (1 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 2 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 3 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 6 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}])
/\ buffered_proposals = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ read_only_entries = 2
/\ force_single = [log |-> <<>>, node |-> 2, decided |-> <<>>, history |-> <<>>]
/\ recovering = (1 :> TRUE @@ 2 :> FALSE @@ 3 :> TRUE @@ 6 :> FALSE)
/\ decided_idx = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ build_source = (6 :> 0)
/\ role = (1 :> "Follower" @@ 2 :> "Follower" @@ 3 :> "Follower" @@ 6 :> "Leader")
/\ phase = (1 :> "Recover" @@ 2 :> "Recover" @@ 3 :> "Recover" @@ 6 :> "Prepare")
/\ reads = << >>
/\ running = {1, 3, 6}
/\ crashes = 0
/\ current_seq_num = ( 1 :> [session |-> 0, counter |-> 0] @@
  2 :> [session |-> 0, counter |-> 0] @@
  3 :> [session |-> 0, counter |-> 0] @@
  6 :> [session |-> 0, counter |-> 0] )
/\ proposed = {}
/\ chan = (<<1, 2>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 1, reads |-> {}, n |-> 0, type |-> "Recovery", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<1, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 1, reads |-> {}, n |-> 0, type |-> "Recovery", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 1, reads |-> {}, n |-> 0, type |-> "Recovery", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 2>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 1, reads |-> {}, n |-> 0, type |-> "Recovery", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<6, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 0, type |-> "Deactivated", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<6, 2>> :> <<>> @@ <<6, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 0, type |-> "Deactivated", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>>)
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
/\ elections = 2
/\ leader_state = (1 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 2 :> [n_leader |-> 12, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 3 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 6 :> [n_leader |-> 16, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 6], max_promise_sync |-> <<>>])
/\ rolled_back = {}
/\ nonce = (1 :> 1 @@ 2 :> 0 @@ 3 :> 2 @@ 6 :> 0)
/\ n_accepted = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ halts = 2
/\ log = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ cached_promise_message = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)

The number of states generated: 920
Simulation using seed 1 and aril 0
Progress: 927 states checked.
Finished in 00s at (2026-10-18 13:10:02)
exit 13

```

