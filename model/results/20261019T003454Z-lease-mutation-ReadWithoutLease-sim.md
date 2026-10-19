# TLC run lease-mutation-ReadWithoutLease-sim, 20261019T003454Z

Counter-check of plan-witness-lease (8b7cafd), model d58cce1. Mutation ReadWithoutLease, random simulation (-simulate -deadlock -depth 60, seed 1, limit 15 min, VIEW removed). Expected: a violation of ReadRoundHonoursLease. Mode: simulation.

## Configuration

Spec: `model/tla/SequencePaxos.tla`. Config file `model/tla/MC_mut_ReadWithoutLease.sim.cfg`:

```
CONSTANTS
  Nodes = {1, 2, 3}
  Values = {v1, v2}
  MaxBallot = 1
  MaxLog = 1
  MaxCounter = 3
  MaxSession = 2
  Cap = 2
  MaxCrash = 0
  MaxWipe = 0
  MaxLoss = 0
  MaxDuplicate = 0
  MaxTimeout = 0
  MaxElection = 3
  RuleR = TRUE
  DecidedIdxMonotone = TRUE
  MaxRollback = 0
  Recovery = TRUE
  Mutation = "ReadWithoutLease"
  ClientSessions = FALSE
  MaxClientSend = 0
  MaxRequest = 0
  ReadIndex = TRUE
  MaxRead = 1
  NextConfigNodes = {6}
  MaxReconfigure = 0
  SingleNode = TRUE
  MaxForceSingle = 1
  MaxReadOnly = 1
  MaxHalt = 0
  StaleReadNode = FALSE
  Witness = TRUE
  LeaseLength = 2
SPECIFICATION Spec
CONSTRAINT Bounds
INVARIANTS StaleReadAfterForce
PROPERTIES ReadRoundHonoursLease
```

## Tools

- TLC2 Version 2.19 of 08 August 2024 (rev: 5a47802)
- Running Random Simulation with seed 1 with 3 workers on 4 cores with 9102MB heap and 64MB offheap memory [pid: 4864] (Linux 6.18.44-fc-v139 amd64, Ubuntu 21.0.12.1 x86_64).
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
- Wall time: 00s (start 20261019T003454Z, end 20261019T003454Z, UTC)

## Result

```
Error: Action property ReadRoundHonoursLease is violated.
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
/\ lease_remaining = <<0, 0, 0>>
/\ promise = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ node_epoch = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ read_only = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ wipes = 0
/\ accepted_idx = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ stage1_ghost = <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>
/\ recovery_responses = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ old_decided_after_forced = FALSE
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
/\ reads = <<[crashes |-> 0, rolled_back |-> {}, state |-> "none", read_index |-> 0, node |-> 0, decided_at_start |-> <<>>, result |-> <<>>, stale |-> FALSE, contract_stale_ok |-> FALSE, ballot |-> 0, decided_new_at_start |-> <<>>, forced_decided |-> FALSE]>>
/\ old_decided_max = 0
/\ running = {1, 2, 3}
/\ crashes = 0
/\ current_seq_num = ( 1 :> [session |-> 0, counter |-> 0] @@
  2 :> [session |-> 0, counter |-> 0] @@
  3 :> [session |-> 0, counter |-> 0] @@
  6 :> [session |-> 0, counter |-> 0] )
/\ proposed = {}
/\ forced_decided_at_force = 0
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
/\ lease_request = <<-1, -1, -1>>
/\ decided_history = <<>>
/\ losses = 0
/\ witness_epoch = 0
/\ forced_has_decided = FALSE
/\ deactivated = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ first_log = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ wait_remaining = -1
/\ timeouts = 0
/\ duplicates = 0
/\ elections = 0
/\ leader_state = (1 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 2 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 3 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 6 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>])
/\ rolled_back = {}
/\ lease_reply = <<-1, -1, -1>>
/\ nonce = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ n_accepted = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ halts = 0
/\ log = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ cached_promise_message = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)

State 2: <Next line 1930, col 6 to line 1930, col 62 of module SequencePaxos>
/\ decided_ballot = <<0>>
/\ wiped = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ lease_remaining = <<0, 0, 0>>
/\ promise = (1 :> 0 @@ 2 :> 12 @@ 3 :> 0 @@ 6 :> 0)
/\ node_epoch = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ read_only = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ wipes = 0
/\ accepted_idx = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ stage1_ghost = <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>
/\ recovery_responses = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ old_decided_after_forced = FALSE
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
/\ reads = <<[crashes |-> 0, rolled_back |-> {}, state |-> "none", read_index |-> 0, node |-> 0, decided_at_start |-> <<>>, result |-> <<>>, stale |-> FALSE, contract_stale_ok |-> FALSE, ballot |-> 0, decided_new_at_start |-> <<>>, forced_decided |-> FALSE]>>
/\ old_decided_max = 0
/\ running = {1, 2, 3}
/\ crashes = 0
/\ current_seq_num = ( 1 :> [session |-> 0, counter |-> 0] @@
  2 :> [session |-> 0, counter |-> 0] @@
  3 :> [session |-> 0, counter |-> 0] @@
  6 :> [session |-> 0, counter |-> 0] )
/\ proposed = {}
/\ forced_decided_at_force = 0
/\ chan = (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<>> @@ <<2, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 1>> :> <<>> @@ <<3, 2>> :> <<>> @@ <<6, 1>> :> <<>> @@ <<6, 2>> :> <<>> @@ <<6, 3>> :> <<>>)
/\ client = [session |-> 0, request_number |-> 1, sends |-> 0]
/\ past_durable = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ buffered_stopsign = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ reconfigurations = 0
/\ lease_request = <<-1, -1, -1>>
/\ decided_history = <<>>
/\ losses = 0
/\ witness_epoch = 0
/\ forced_has_decided = FALSE
/\ deactivated = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ first_log = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ wait_remaining = -1
/\ timeouts = 0
/\ duplicates = 0
/\ elections = 1
/\ leader_state = (1 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 2 :> [n_leader |-> 12, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 3 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 6 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>])
/\ rolled_back = {}
/\ lease_reply = <<-1, -1, -1>>
/\ nonce = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ n_accepted = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ halts = 0
/\ log = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ cached_promise_message = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)

State 3: <Receive line 1138, col 3 to line 1154, col 16 of module SequencePaxos>
/\ decided_ballot = <<0>>
/\ wiped = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ lease_remaining = <<0, 0, 0>>
/\ promise = (1 :> 0 @@ 2 :> 12 @@ 3 :> 12 @@ 6 :> 0)
/\ node_epoch = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ read_only = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ wipes = 0
/\ accepted_idx = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ stage1_ghost = <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>
/\ recovery_responses = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ old_decided_after_forced = FALSE
/\ read_pending = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ read_round = (1 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 2 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 3 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 6 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}])
/\ buffered_proposals = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ read_only_entries = 0
/\ force_single = [log |-> <<>>, node |-> 0, decided |-> <<>>, history |-> <<>>]
/\ recovering = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ decided_idx = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ build_source = (6 :> 0)
/\ role = (1 :> "Follower" @@ 2 :> "Leader" @@ 3 :> "Follower" @@ 6 :> "Follower")
/\ phase = (1 :> "None" @@ 2 :> "Prepare" @@ 3 :> "Prepare" @@ 6 :> "None")
/\ reads = <<[crashes |-> 0, rolled_back |-> {}, state |-> "none", read_index |-> 0, node |-> 0, decided_at_start |-> <<>>, result |-> <<>>, stale |-> FALSE, contract_stale_ok |-> FALSE, ballot |-> 0, decided_new_at_start |-> <<>>, forced_decided |-> FALSE]>>
/\ old_decided_max = 0
/\ running = {1, 2, 3}
/\ crashes = 0
/\ current_seq_num = ( 1 :> [session |-> 0, counter |-> 0] @@
  2 :> [session |-> 0, counter |-> 0] @@
  3 :> [session |-> 0, counter |-> 0] @@
  6 :> [session |-> 0, counter |-> 0] )
/\ proposed = {}
/\ forced_decided_at_force = 0
/\ chan = (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<>> @@ <<2, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 3>> :> <<>> @@ <<3, 1>> :> <<>> @@ <<3, 2>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<6, 1>> :> <<>> @@ <<6, 2>> :> <<>> @@ <<6, 3>> :> <<>>)
/\ client = [session |-> 0, request_number |-> 1, sends |-> 0]
/\ past_durable = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ buffered_stopsign = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ reconfigurations = 0
/\ lease_request = <<-1, -1, -1>>
/\ decided_history = <<>>
/\ losses = 0
/\ witness_epoch = 0
/\ forced_has_decided = FALSE
/\ deactivated = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ first_log = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ wait_remaining = -1
/\ timeouts = 0
/\ duplicates = 0
/\ elections = 1
/\ leader_state = (1 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 2 :> [n_leader |-> 12, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 3 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 6 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>])
/\ rolled_back = {}
/\ lease_reply = <<-1, -1, -1>>
/\ nonce = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ n_accepted = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ halts = 0
/\ log = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ cached_promise_message = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ 6 :> <<>>)

State 4: <Next line 1949, col 6 to line 1949, col 60 of module SequencePaxos>
/\ decided_ballot = <<0>>
/\ wiped = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ lease_remaining = <<0, 0, 0>>
/\ promise = (1 :> 0 @@ 2 :> 12 @@ 3 :> 12 @@ 6 :> 0)
/\ node_epoch = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ read_only = (1 :> TRUE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ wipes = 0
/\ accepted_idx = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ stage1_ghost = <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>
/\ recovery_responses = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ old_decided_after_forced = FALSE
/\ read_pending = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ read_round = (1 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 2 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 3 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 6 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}])
/\ buffered_proposals = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ read_only_entries = 1
/\ force_single = [log |-> <<>>, node |-> 0, decided |-> <<>>, history |-> <<>>]
/\ recovering = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ decided_idx = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ build_source = (6 :> 0)
/\ role = (1 :> "Follower" @@ 2 :> "Leader" @@ 3 :> "Follower" @@ 6 :> "Follower")
/\ phase = (1 :> "Recover" @@ 2 :> "Prepare" @@ 3 :> "Prepare" @@ 6 :> "None")
/\ reads = <<[crashes |-> 0, rolled_back |-> {}, state |-> "none", read_index |-> 0, node |-> 0, decided_at_start |-> <<>>, result |-> <<>>, stale |-> FALSE, contract_stale_ok |-> FALSE, ballot |-> 0, decided_new_at_start |-> <<>>, forced_decided |-> FALSE]>>
/\ old_decided_max = 0
/\ running = {1, 2, 3}
/\ crashes = 0
/\ current_seq_num = ( 1 :> [session |-> 0, counter |-> 0] @@
  2 :> [session |-> 0, counter |-> 0] @@
  3 :> [session |-> 0, counter |-> 0] @@
  6 :> [session |-> 0, counter |-> 0] )
/\ proposed = {}
/\ forced_decided_at_force = 0
/\ chan = (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<>> @@ <<2, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 3>> :> <<>> @@ <<3, 1>> :> <<>> @@ <<3, 2>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<6, 1>> :> <<>> @@ <<6, 2>> :> <<>> @@ <<6, 3>> :> <<>>)
/\ client = [session |-> 0, request_number |-> 1, sends |-> 0]
/\ past_durable = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ buffered_stopsign = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ reconfigurations = 0
/\ lease_request = <<-1, -1, -1>>
/\ decided_history = <<>>
/\ losses = 0
/\ witness_epoch = 0
/\ forced_has_decided = FALSE
/\ deactivated = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ first_log = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ wait_remaining = -1
/\ timeouts = 0
/\ duplicates = 0
/\ elections = 1
/\ leader_state = (1 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 2 :> [n_leader |-> 12, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 3 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 6 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>])
/\ rolled_back = {}
/\ lease_reply = <<-1, -1, -1>>
/\ nonce = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ n_accepted = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ halts = 0
/\ log = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ cached_promise_message = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ 6 :> <<>>)

State 5: <Next line 1948, col 6 to line 1948, col 74 of module SequencePaxos>
/\ decided_ballot = <<0>>
/\ wiped = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ lease_remaining = <<0, 0, 0>>
/\ promise = (1 :> 0 @@ 2 :> 12 @@ 3 :> 12 @@ 6 :> 0)
/\ node_epoch = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 1)
/\ read_only = (1 :> TRUE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ wipes = 0
/\ accepted_idx = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ stage1_ghost = <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>
/\ recovery_responses = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ old_decided_after_forced = FALSE
/\ read_pending = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ read_round = (1 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 2 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 3 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 6 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}])
/\ buffered_proposals = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ read_only_entries = 1
/\ force_single = [log |-> <<>>, node |-> 1, decided |-> <<>>, history |-> <<>>]
/\ recovering = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ decided_idx = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ build_source = (6 :> 0)
/\ role = (1 :> "Follower" @@ 2 :> "Leader" @@ 3 :> "Follower" @@ 6 :> "Follower")
/\ phase = (1 :> "Recover" @@ 2 :> "Prepare" @@ 3 :> "Prepare" @@ 6 :> "None")
/\ reads = <<[crashes |-> 0, rolled_back |-> {}, state |-> "none", read_index |-> 0, node |-> 0, decided_at_start |-> <<>>, result |-> <<>>, stale |-> FALSE, contract_stale_ok |-> FALSE, ballot |-> 0, decided_new_at_start |-> <<>>, forced_decided |-> FALSE]>>
/\ old_decided_max = 0
/\ running = {2, 3, 6}
/\ crashes = 0
/\ current_seq_num = ( 1 :> [session |-> 0, counter |-> 0] @@
  2 :> [session |-> 0, counter |-> 0] @@
  3 :> [session |-> 0, counter |-> 0] @@
  6 :> [session |-> 0, counter |-> 0] )
/\ proposed = {}
/\ forced_decided_at_force = 0
/\ chan = (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<>> @@ <<2, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 3>> :> <<>> @@ <<3, 1>> :> <<>> @@ <<3, 2>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<6, 1>> :> <<>> @@ <<6, 2>> :> <<>> @@ <<6, 3>> :> <<>>)
/\ client = [session |-> 0, request_number |-> 1, sends |-> 0]
/\ past_durable = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ buffered_stopsign = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ reconfigurations = 0
/\ lease_request = <<-1, -1, -1>>
/\ decided_history = <<>>
/\ losses = 0
/\ witness_epoch = 1
/\ forced_has_decided = FALSE
/\ deactivated = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ first_log = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ wait_remaining = 2
/\ timeouts = 0
/\ duplicates = 0
/\ elections = 1
/\ leader_state = (1 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 2 :> [n_leader |-> 12, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 3 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 6 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>])
/\ rolled_back = {}
/\ lease_reply = <<-1, -1, -1>>
/\ nonce = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ n_accepted = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ halts = 0
/\ log = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ cached_promise_message = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ 6 :> <<>>)

State 6: <Next line 1942, col 6 to line 1942, col 57 of module SequencePaxos>
/\ decided_ballot = <<0>>
/\ wiped = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ lease_remaining = <<0, 0, 0>>
/\ promise = (1 :> 0 @@ 2 :> 12 @@ 3 :> 12 @@ 6 :> 0)
/\ node_epoch = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 1)
/\ read_only = (1 :> TRUE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ wipes = 0
/\ accepted_idx = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ stage1_ghost = <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>
/\ recovery_responses = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ old_decided_after_forced = FALSE
/\ read_pending = (1 :> {} @@ 2 :> {1} @@ 3 :> {} @@ 6 :> {})
/\ read_round = (1 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 2 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 3 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 6 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}])
/\ buffered_proposals = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ read_only_entries = 1
/\ force_single = [log |-> <<>>, node |-> 1, decided |-> <<>>, history |-> <<>>]
/\ recovering = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ decided_idx = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ build_source = (6 :> 0)
/\ role = (1 :> "Follower" @@ 2 :> "Leader" @@ 3 :> "Follower" @@ 6 :> "Follower")
/\ phase = (1 :> "Recover" @@ 2 :> "Prepare" @@ 3 :> "Prepare" @@ 6 :> "None")
/\ reads = <<[crashes |-> 0, rolled_back |-> {}, state |-> "pending", read_index |-> 0, node |-> 2, decided_at_start |-> <<>>, result |-> <<>>, stale |-> FALSE, contract_stale_ok |-> FALSE, ballot |-> 0, decided_new_at_start |-> <<>>, forced_decided |-> FALSE]>>
/\ old_decided_max = 0
/\ running = {2, 3, 6}
/\ crashes = 0
/\ current_seq_num = ( 1 :> [session |-> 0, counter |-> 0] @@
  2 :> [session |-> 0, counter |-> 0] @@
  3 :> [session |-> 0, counter |-> 0] @@
  6 :> [session |-> 0, counter |-> 0] )
/\ proposed = {}
/\ forced_decided_at_force = 0
/\ chan = (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<>> @@ <<2, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 3>> :> <<>> @@ <<3, 1>> :> <<>> @@ <<3, 2>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<6, 1>> :> <<>> @@ <<6, 2>> :> <<>> @@ <<6, 3>> :> <<>>)
/\ client = [session |-> 0, request_number |-> 1, sends |-> 0]
/\ past_durable = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ buffered_stopsign = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ reconfigurations = 0
/\ lease_request = <<-1, -1, -1>>
/\ decided_history = <<>>
/\ losses = 0
/\ witness_epoch = 1
/\ forced_has_decided = FALSE
/\ deactivated = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ first_log = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ wait_remaining = 2
/\ timeouts = 0
/\ duplicates = 0
/\ elections = 1
/\ leader_state = (1 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 2 :> [n_leader |-> 12, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 3 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 6 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>])
/\ rolled_back = {}
/\ lease_reply = <<-1, -1, -1>>
/\ nonce = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ n_accepted = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ halts = 0
/\ log = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ cached_promise_message = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ 6 :> <<>>)

State 7: <Receive line 1138, col 3 to line 1154, col 16 of module SequencePaxos>
/\ decided_ballot = <<0>>
/\ wiped = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ lease_remaining = <<0, 0, 0>>
/\ promise = (1 :> 0 @@ 2 :> 12 @@ 3 :> 12 @@ 6 :> 0)
/\ node_epoch = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 1)
/\ read_only = (1 :> TRUE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ wipes = 0
/\ accepted_idx = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ stage1_ghost = <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>
/\ recovery_responses = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ old_decided_after_forced = FALSE
/\ read_pending = (1 :> {} @@ 2 :> {1} @@ 3 :> {} @@ 6 :> {})
/\ read_round = (1 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 2 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 3 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 6 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}])
/\ buffered_proposals = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ read_only_entries = 1
/\ force_single = [log |-> <<>>, node |-> 1, decided |-> <<>>, history |-> <<>>]
/\ recovering = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ decided_idx = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ build_source = (6 :> 0)
/\ role = (1 :> "Follower" @@ 2 :> "Leader" @@ 3 :> "Follower" @@ 6 :> "Follower")
/\ phase = (1 :> "Recover" @@ 2 :> "Accept" @@ 3 :> "Prepare" @@ 6 :> "None")
/\ reads = <<[crashes |-> 0, rolled_back |-> {}, state |-> "pending", read_index |-> 0, node |-> 2, decided_at_start |-> <<>>, result |-> <<>>, stale |-> FALSE, contract_stale_ok |-> FALSE, ballot |-> 0, decided_new_at_start |-> <<>>, forced_decided |-> FALSE]>>
/\ old_decided_max = 0
/\ running = {2, 3, 6}
/\ crashes = 0
/\ current_seq_num = ( 1 :> [session |-> 0, counter |-> 0] @@
  2 :> [session |-> 0, counter |-> 0] @@
  3 :> [session |-> 0, counter |-> 0] @@
  6 :> [session |-> 0, counter |-> 0] )
/\ proposed = {}
/\ forced_decided_at_force = 0
/\ chan = (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<>> @@ <<2, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "AcceptSync", log_sync |-> <<[suffix |-> <<>>, sync_idx |-> 0]>>, seq_num |-> [session |-> 1, counter |-> 1], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 1>> :> <<>> @@ <<3, 2>> :> <<>> @@ <<6, 1>> :> <<>> @@ <<6, 2>> :> <<>> @@ <<6, 3>> :> <<>>)
/\ client = [session |-> 0, request_number |-> 1, sends |-> 0]
/\ past_durable = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ buffered_stopsign = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ reconfigurations = 0
/\ lease_request = <<-1, -1, -1>>
/\ decided_history = <<>>
/\ losses = 0
/\ witness_epoch = 1
/\ forced_has_decided = FALSE
/\ deactivated = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ first_log = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ wait_remaining = 2
/\ timeouts = 0
/\ duplicates = 0
/\ elections = 1
/\ leader_state = (1 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 2 :> [n_leader |-> 12, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 1, counter |-> 1] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 3], max_promise_sync |-> <<>>] @@ 3 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 6 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>])
/\ rolled_back = {}
/\ lease_reply = <<-1, -1, -1>>
/\ nonce = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ n_accepted = (1 :> 0 @@ 2 :> 12 @@ 3 :> 0 @@ 6 :> 0)
/\ halts = 0
/\ log = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ cached_promise_message = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ 6 :> <<>>)

State 8: <Tick line 1826, col 3 to line 1836, col 29 of module SequencePaxos>
/\ decided_ballot = <<0>>
/\ wiped = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ lease_remaining = <<0, 0, 0>>
/\ promise = (1 :> 0 @@ 2 :> 12 @@ 3 :> 12 @@ 6 :> 0)
/\ node_epoch = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 1)
/\ read_only = (1 :> TRUE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ wipes = 0
/\ accepted_idx = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ stage1_ghost = <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>
/\ recovery_responses = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ old_decided_after_forced = FALSE
/\ read_pending = (1 :> {} @@ 2 :> {1} @@ 3 :> {} @@ 6 :> {})
/\ read_round = (1 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 2 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 3 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 6 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}])
/\ buffered_proposals = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ read_only_entries = 1
/\ force_single = [log |-> <<>>, node |-> 1, decided |-> <<>>, history |-> <<>>]
/\ recovering = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ decided_idx = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ build_source = (6 :> 0)
/\ role = (1 :> "Follower" @@ 2 :> "Leader" @@ 3 :> "Follower" @@ 6 :> "Follower")
/\ phase = (1 :> "Recover" @@ 2 :> "Accept" @@ 3 :> "Prepare" @@ 6 :> "None")
/\ reads = <<[crashes |-> 0, rolled_back |-> {}, state |-> "pending", read_index |-> 0, node |-> 2, decided_at_start |-> <<>>, result |-> <<>>, stale |-> FALSE, contract_stale_ok |-> FALSE, ballot |-> 0, decided_new_at_start |-> <<>>, forced_decided |-> FALSE]>>
/\ old_decided_max = 0
/\ running = {2, 3, 6}
/\ crashes = 0
/\ current_seq_num = ( 1 :> [session |-> 0, counter |-> 0] @@
  2 :> [session |-> 0, counter |-> 0] @@
  3 :> [session |-> 0, counter |-> 0] @@
  6 :> [session |-> 0, counter |-> 0] )
/\ proposed = {}
/\ forced_decided_at_force = 0
/\ chan = (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<>> @@ <<2, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "AcceptSync", log_sync |-> <<[suffix |-> <<>>, sync_idx |-> 0]>>, seq_num |-> [session |-> 1, counter |-> 1], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 1>> :> <<>> @@ <<3, 2>> :> <<>> @@ <<6, 1>> :> <<>> @@ <<6, 2>> :> <<>> @@ <<6, 3>> :> <<>>)
/\ client = [session |-> 0, request_number |-> 1, sends |-> 0]
/\ past_durable = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ buffered_stopsign = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ reconfigurations = 0
/\ lease_request = <<-1, -1, -1>>
/\ decided_history = <<>>
/\ losses = 0
/\ witness_epoch = 1
/\ forced_has_decided = FALSE
/\ deactivated = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ first_log = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ wait_remaining = 1
/\ timeouts = 0
/\ duplicates = 0
/\ elections = 1
/\ leader_state = (1 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 2 :> [n_leader |-> 12, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 1, counter |-> 1] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 3], max_promise_sync |-> <<>>] @@ 3 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 6 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>])
/\ rolled_back = {}
/\ lease_reply = <<-1, -1, -1>>
/\ nonce = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ n_accepted = (1 :> 0 @@ 2 :> 12 @@ 3 :> 0 @@ 6 :> 0)
/\ halts = 0
/\ log = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ cached_promise_message = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ 6 :> <<>>)

State 9: <Tick line 1826, col 3 to line 1836, col 29 of module SequencePaxos>
/\ decided_ballot = <<0>>
/\ wiped = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ lease_remaining = <<0, 0, 0>>
/\ promise = (1 :> 0 @@ 2 :> 12 @@ 3 :> 12 @@ 6 :> 0)
/\ node_epoch = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 1)
/\ read_only = (1 :> TRUE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ wipes = 0
/\ accepted_idx = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ stage1_ghost = <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>
/\ recovery_responses = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ old_decided_after_forced = FALSE
/\ read_pending = (1 :> {} @@ 2 :> {1} @@ 3 :> {} @@ 6 :> {})
/\ read_round = (1 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 2 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 3 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 6 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}])
/\ buffered_proposals = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ read_only_entries = 1
/\ force_single = [log |-> <<>>, node |-> 1, decided |-> <<>>, history |-> <<>>]
/\ recovering = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ decided_idx = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ build_source = (6 :> 0)
/\ role = (1 :> "Follower" @@ 2 :> "Leader" @@ 3 :> "Follower" @@ 6 :> "Follower")
/\ phase = (1 :> "Recover" @@ 2 :> "Accept" @@ 3 :> "Prepare" @@ 6 :> "None")
/\ reads = <<[crashes |-> 0, rolled_back |-> {}, state |-> "pending", read_index |-> 0, node |-> 2, decided_at_start |-> <<>>, result |-> <<>>, stale |-> FALSE, contract_stale_ok |-> FALSE, ballot |-> 0, decided_new_at_start |-> <<>>, forced_decided |-> FALSE]>>
/\ old_decided_max = 0
/\ running = {2, 3, 6}
/\ crashes = 0
/\ current_seq_num = ( 1 :> [session |-> 0, counter |-> 0] @@
  2 :> [session |-> 0, counter |-> 0] @@
  3 :> [session |-> 0, counter |-> 0] @@
  6 :> [session |-> 0, counter |-> 0] )
/\ proposed = {}
/\ forced_decided_at_force = 0
/\ chan = (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<>> @@ <<2, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "AcceptSync", log_sync |-> <<[suffix |-> <<>>, sync_idx |-> 0]>>, seq_num |-> [session |-> 1, counter |-> 1], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 1>> :> <<>> @@ <<3, 2>> :> <<>> @@ <<6, 1>> :> <<>> @@ <<6, 2>> :> <<>> @@ <<6, 3>> :> <<>>)
/\ client = [session |-> 0, request_number |-> 1, sends |-> 0]
/\ past_durable = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ buffered_stopsign = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ reconfigurations = 0
/\ lease_request = <<-1, -1, -1>>
/\ decided_history = <<>>
/\ losses = 0
/\ witness_epoch = 1
/\ forced_has_decided = FALSE
/\ deactivated = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ first_log = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ wait_remaining = 0
/\ timeouts = 0
/\ duplicates = 0
/\ elections = 1
/\ leader_state = (1 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 2 :> [n_leader |-> 12, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 1, counter |-> 1] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 3], max_promise_sync |-> <<>>] @@ 3 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 6 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>])
/\ rolled_back = {}
/\ lease_reply = <<-1, -1, -1>>
/\ nonce = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ n_accepted = (1 :> 0 @@ 2 :> 12 @@ 3 :> 0 @@ 6 :> 0)
/\ halts = 0
/\ log = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ cached_promise_message = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ 6 :> <<>>)

State 10: <Next line 1931, col 6 to line 1931, col 51 of module SequencePaxos>
/\ decided_ballot = <<0>>
/\ wiped = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ lease_remaining = <<0, 0, 0>>
/\ promise = (1 :> 0 @@ 2 :> 12 @@ 3 :> 12 @@ 6 :> 0)
/\ node_epoch = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 1)
/\ read_only = (1 :> TRUE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ wipes = 0
/\ accepted_idx = (1 :> 0 @@ 2 :> 1 @@ 3 :> 0 @@ 6 :> 0)
/\ stage1_ghost = <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>
/\ recovery_responses = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ old_decided_after_forced = FALSE
/\ read_pending = (1 :> {} @@ 2 :> {1} @@ 3 :> {} @@ 6 :> {})
/\ read_round = (1 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 2 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 3 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 6 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}])
/\ buffered_proposals = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ read_only_entries = 1
/\ force_single = [log |-> <<>>, node |-> 1, decided |-> <<>>, history |-> <<>>]
/\ recovering = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ decided_idx = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ build_source = (6 :> 0)
/\ role = (1 :> "Follower" @@ 2 :> "Leader" @@ 3 :> "Follower" @@ 6 :> "Follower")
/\ phase = (1 :> "Recover" @@ 2 :> "Accept" @@ 3 :> "Prepare" @@ 6 :> "None")
/\ reads = <<[crashes |-> 0, rolled_back |-> {}, state |-> "pending", read_index |-> 0, node |-> 2, decided_at_start |-> <<>>, result |-> <<>>, stale |-> FALSE, contract_stale_ok |-> FALSE, ballot |-> 0, decided_new_at_start |-> <<>>, forced_decided |-> FALSE]>>
/\ old_decided_max = 0
/\ running = {2, 3, 6}
/\ crashes = 0
/\ current_seq_num = ( 1 :> [session |-> 0, counter |-> 0] @@
  2 :> [session |-> 0, counter |-> 0] @@
  3 :> [session |-> 0, counter |-> 0] @@
  6 :> [session |-> 0, counter |-> 0] )
/\ proposed = {v2}
/\ forced_decided_at_force = 0
/\ chan = (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<>> @@ <<2, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "AcceptSync", log_sync |-> <<[suffix |-> <<>>, sync_idx |-> 0]>>, seq_num |-> [session |-> 1, counter |-> 1], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0], [promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "AcceptDecide", log_sync |-> <<>>, seq_num |-> [session |-> 1, counter |-> 2], entries |-> <<v2>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 1>> :> <<>> @@ <<3, 2>> :> <<>> @@ <<6, 1>> :> <<>> @@ <<6, 2>> :> <<>> @@ <<6, 3>> :> <<>>)
/\ client = [session |-> 0, request_number |-> 1, sends |-> 0]
/\ past_durable = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ buffered_stopsign = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ reconfigurations = 0
/\ lease_request = <<-1, -1, -1>>
/\ decided_history = <<>>
/\ losses = 0
/\ witness_epoch = 1
/\ forced_has_decided = FALSE
/\ deactivated = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ first_log = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ wait_remaining = 0
/\ timeouts = 0
/\ duplicates = 0
/\ elections = 1
/\ leader_state = (1 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 2 :> [n_leader |-> 12, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 1, counter |-> 2] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 1 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 3], max_promise_sync |-> <<>>] @@ 3 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 6 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>])
/\ rolled_back = {}
/\ lease_reply = <<-1, -1, -1>>
/\ nonce = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ n_accepted = (1 :> 0 @@ 2 :> 12 @@ 3 :> 0 @@ 6 :> 0)
/\ halts = 0
/\ log = (1 :> <<>> @@ 2 :> <<v2>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ cached_promise_message = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ 6 :> <<>>)

State 11: <Next line 1954, col 6 to line 1954, col 82 of module SequencePaxos>
/\ decided_ballot = <<0>>
/\ wiped = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ lease_remaining = <<0, 0, 0>>
/\ promise = (1 :> 0 @@ 2 :> 12 @@ 3 :> 12 @@ 6 :> 0)
/\ node_epoch = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 1)
/\ read_only = (1 :> TRUE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ wipes = 0
/\ accepted_idx = (1 :> 0 @@ 2 :> 1 @@ 3 :> 0 @@ 6 :> 0)
/\ stage1_ghost = <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>
/\ recovery_responses = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ old_decided_after_forced = FALSE
/\ read_pending = (1 :> {} @@ 2 :> {1} @@ 3 :> {} @@ 6 :> {})
/\ read_round = (1 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 2 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 3 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 6 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}])
/\ buffered_proposals = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ read_only_entries = 1
/\ force_single = [log |-> <<>>, node |-> 1, decided |-> <<>>, history |-> <<>>]
/\ recovering = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ decided_idx = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ build_source = (6 :> 0)
/\ role = (1 :> "Follower" @@ 2 :> "Leader" @@ 3 :> "Follower" @@ 6 :> "Follower")
/\ phase = (1 :> "Recover" @@ 2 :> "Accept" @@ 3 :> "Prepare" @@ 6 :> "None")
/\ reads = <<[crashes |-> 0, rolled_back |-> {}, state |-> "pending", read_index |-> 0, node |-> 2, decided_at_start |-> <<>>, result |-> <<>>, stale |-> FALSE, contract_stale_ok |-> FALSE, ballot |-> 0, decided_new_at_start |-> <<>>, forced_decided |-> FALSE]>>
/\ old_decided_max = 0
/\ running = {2, 3, 6}
/\ crashes = 0
/\ current_seq_num = ( 1 :> [session |-> 0, counter |-> 0] @@
  2 :> [session |-> 0, counter |-> 0] @@
  3 :> [session |-> 0, counter |-> 0] @@
  6 :> [session |-> 0, counter |-> 0] )
/\ proposed = {v2}
/\ forced_decided_at_force = 0
/\ chan = (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<>> @@ <<2, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "AcceptSync", log_sync |-> <<[suffix |-> <<>>, sync_idx |-> 0]>>, seq_num |-> [session |-> 1, counter |-> 1], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0], [promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "AcceptDecide", log_sync |-> <<>>, seq_num |-> [session |-> 1, counter |-> 2], entries |-> <<v2>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 1>> :> <<>> @@ <<3, 2>> :> <<>> @@ <<6, 1>> :> <<>> @@ <<6, 2>> :> <<>> @@ <<6, 3>> :> <<>>)
/\ client = [session |-> 0, request_number |-> 1, sends |-> 0]
/\ past_durable = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ buffered_stopsign = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ reconfigurations = 0
/\ lease_request = <<-1, 2, -1>>
/\ decided_history = <<>>
/\ losses = 0
/\ witness_epoch = 1
/\ forced_has_decided = FALSE
/\ deactivated = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ first_log = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ wait_remaining = 0
/\ timeouts = 0
/\ duplicates = 0
/\ elections = 1
/\ leader_state = (1 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 2 :> [n_leader |-> 12, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 1, counter |-> 2] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 1 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 3], max_promise_sync |-> <<>>] @@ 3 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 6 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>])
/\ rolled_back = {}
/\ lease_reply = <<-1, -1, -1>>
/\ nonce = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ n_accepted = (1 :> 0 @@ 2 :> 12 @@ 3 :> 0 @@ 6 :> 0)
/\ halts = 0
/\ log = (1 :> <<>> @@ 2 :> <<v2>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ cached_promise_message = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ 6 :> <<>>)

State 12: <Next line 1931, col 6 to line 1931, col 51 of module SequencePaxos>
/\ decided_ballot = <<0>>
/\ wiped = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ lease_remaining = <<0, 0, 0>>
/\ promise = (1 :> 0 @@ 2 :> 12 @@ 3 :> 12 @@ 6 :> 0)
/\ node_epoch = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 1)
/\ read_only = (1 :> TRUE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ wipes = 0
/\ accepted_idx = (1 :> 0 @@ 2 :> 1 @@ 3 :> 0 @@ 6 :> 0)
/\ stage1_ghost = <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>
/\ recovery_responses = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ old_decided_after_forced = FALSE
/\ read_pending = (1 :> {} @@ 2 :> {1} @@ 3 :> {} @@ 6 :> {})
/\ read_round = (1 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 2 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 3 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 6 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}])
/\ buffered_proposals = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ read_only_entries = 1
/\ force_single = [log |-> <<>>, node |-> 1, decided |-> <<>>, history |-> <<>>]
/\ recovering = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ decided_idx = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ build_source = (6 :> 0)
/\ role = (1 :> "Follower" @@ 2 :> "Leader" @@ 3 :> "Follower" @@ 6 :> "Follower")
/\ phase = (1 :> "Recover" @@ 2 :> "Accept" @@ 3 :> "Prepare" @@ 6 :> "None")
/\ reads = <<[crashes |-> 0, rolled_back |-> {}, state |-> "pending", read_index |-> 0, node |-> 2, decided_at_start |-> <<>>, result |-> <<>>, stale |-> FALSE, contract_stale_ok |-> FALSE, ballot |-> 0, decided_new_at_start |-> <<>>, forced_decided |-> FALSE]>>
/\ old_decided_max = 0
/\ running = {2, 3, 6}
/\ crashes = 0
/\ current_seq_num = ( 1 :> [session |-> 0, counter |-> 0] @@
  2 :> [session |-> 0, counter |-> 0] @@
  3 :> [session |-> 0, counter |-> 0] @@
  6 :> [session |-> 0, counter |-> 0] )
/\ proposed = {v1, v2}
/\ forced_decided_at_force = 0
/\ chan = (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<>> @@ <<2, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "AcceptSync", log_sync |-> <<[suffix |-> <<>>, sync_idx |-> 0]>>, seq_num |-> [session |-> 1, counter |-> 1], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0], [promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "AcceptDecide", log_sync |-> <<>>, seq_num |-> [session |-> 1, counter |-> 2], entries |-> <<v2>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 1>> :> <<>> @@ <<3, 2>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 0, type |-> "ProposalForward", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<v1>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<6, 1>> :> <<>> @@ <<6, 2>> :> <<>> @@ <<6, 3>> :> <<>>)
/\ client = [session |-> 0, request_number |-> 1, sends |-> 0]
/\ past_durable = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ buffered_stopsign = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ reconfigurations = 0
/\ lease_request = <<-1, 2, -1>>
/\ decided_history = <<>>
/\ losses = 0
/\ witness_epoch = 1
/\ forced_has_decided = FALSE
/\ deactivated = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ first_log = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ wait_remaining = 0
/\ timeouts = 0
/\ duplicates = 0
/\ elections = 1
/\ leader_state = (1 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 2 :> [n_leader |-> 12, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 1, counter |-> 2] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 1 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 3], max_promise_sync |-> <<>>] @@ 3 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 6 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>])
/\ rolled_back = {}
/\ lease_reply = <<-1, -1, -1>>
/\ nonce = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ n_accepted = (1 :> 0 @@ 2 :> 12 @@ 3 :> 0 @@ 6 :> 0)
/\ halts = 0
/\ log = (1 :> <<>> @@ 2 :> <<v2>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ cached_promise_message = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ 6 :> <<>>)

State 13: <LeaseGrant line 1855, col 3 to line 1863, col 62 of module SequencePaxos>
/\ decided_ballot = <<0>>
/\ wiped = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ lease_remaining = <<0, 0, 0>>
/\ promise = (1 :> 0 @@ 2 :> 12 @@ 3 :> 12 @@ 6 :> 0)
/\ node_epoch = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 1)
/\ read_only = (1 :> TRUE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ wipes = 0
/\ accepted_idx = (1 :> 0 @@ 2 :> 1 @@ 3 :> 0 @@ 6 :> 0)
/\ stage1_ghost = <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>
/\ recovery_responses = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ old_decided_after_forced = FALSE
/\ read_pending = (1 :> {} @@ 2 :> {1} @@ 3 :> {} @@ 6 :> {})
/\ read_round = (1 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 2 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 3 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 6 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}])
/\ buffered_proposals = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ read_only_entries = 1
/\ force_single = [log |-> <<>>, node |-> 1, decided |-> <<>>, history |-> <<>>]
/\ recovering = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ decided_idx = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ build_source = (6 :> 0)
/\ role = (1 :> "Follower" @@ 2 :> "Leader" @@ 3 :> "Follower" @@ 6 :> "Follower")
/\ phase = (1 :> "Recover" @@ 2 :> "Accept" @@ 3 :> "Prepare" @@ 6 :> "None")
/\ reads = <<[crashes |-> 0, rolled_back |-> {}, state |-> "pending", read_index |-> 0, node |-> 2, decided_at_start |-> <<>>, result |-> <<>>, stale |-> FALSE, contract_stale_ok |-> FALSE, ballot |-> 0, decided_new_at_start |-> <<>>, forced_decided |-> FALSE]>>
/\ old_decided_max = 0
/\ running = {2, 3, 6}
/\ crashes = 0
/\ current_seq_num = ( 1 :> [session |-> 0, counter |-> 0] @@
  2 :> [session |-> 0, counter |-> 0] @@
  3 :> [session |-> 0, counter |-> 0] @@
  6 :> [session |-> 0, counter |-> 0] )
/\ proposed = {v1, v2}
/\ forced_decided_at_force = 0
/\ chan = (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<>> @@ <<2, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "AcceptSync", log_sync |-> <<[suffix |-> <<>>, sync_idx |-> 0]>>, seq_num |-> [session |-> 1, counter |-> 1], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0], [promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "AcceptDecide", log_sync |-> <<>>, seq_num |-> [session |-> 1, counter |-> 2], entries |-> <<v2>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 1>> :> <<>> @@ <<3, 2>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 0, type |-> "ProposalForward", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<v1>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<6, 1>> :> <<>> @@ <<6, 2>> :> <<>> @@ <<6, 3>> :> <<>>)
/\ client = [session |-> 0, request_number |-> 1, sends |-> 0]
/\ past_durable = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ buffered_stopsign = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ reconfigurations = 0
/\ lease_request = <<-1, 2, -1>>
/\ decided_history = <<>>
/\ losses = 0
/\ witness_epoch = 1
/\ forced_has_decided = FALSE
/\ deactivated = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ first_log = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ wait_remaining = 0
/\ timeouts = 0
/\ duplicates = 0
/\ elections = 1
/\ leader_state = (1 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 2 :> [n_leader |-> 12, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 1, counter |-> 2] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 1 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 3], max_promise_sync |-> <<>>] @@ 3 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 6 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>])
/\ rolled_back = {}
/\ lease_reply = <<-1, 0, -1>>
/\ nonce = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ n_accepted = (1 :> 0 @@ 2 :> 12 @@ 3 :> 0 @@ 6 :> 0)
/\ halts = 0
/\ log = (1 :> <<>> @@ 2 :> <<v2>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ cached_promise_message = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ 6 :> <<>>)

State 14: <Next line 1954, col 6 to line 1954, col 82 of module SequencePaxos>
/\ decided_ballot = <<0>>
/\ wiped = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ lease_remaining = <<0, 0, 0>>
/\ promise = (1 :> 0 @@ 2 :> 12 @@ 3 :> 12 @@ 6 :> 0)
/\ node_epoch = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 1)
/\ read_only = (1 :> TRUE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ wipes = 0
/\ accepted_idx = (1 :> 0 @@ 2 :> 1 @@ 3 :> 0 @@ 6 :> 0)
/\ stage1_ghost = <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>
/\ recovery_responses = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ old_decided_after_forced = FALSE
/\ read_pending = (1 :> {} @@ 2 :> {1} @@ 3 :> {} @@ 6 :> {})
/\ read_round = (1 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 2 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 3 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 6 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}])
/\ buffered_proposals = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ read_only_entries = 1
/\ force_single = [log |-> <<>>, node |-> 1, decided |-> <<>>, history |-> <<>>]
/\ recovering = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ decided_idx = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ build_source = (6 :> 0)
/\ role = (1 :> "Follower" @@ 2 :> "Leader" @@ 3 :> "Follower" @@ 6 :> "Follower")
/\ phase = (1 :> "Recover" @@ 2 :> "Accept" @@ 3 :> "Prepare" @@ 6 :> "None")
/\ reads = <<[crashes |-> 0, rolled_back |-> {}, state |-> "pending", read_index |-> 0, node |-> 2, decided_at_start |-> <<>>, result |-> <<>>, stale |-> FALSE, contract_stale_ok |-> FALSE, ballot |-> 0, decided_new_at_start |-> <<>>, forced_decided |-> FALSE]>>
/\ old_decided_max = 0
/\ running = {2, 3, 6}
/\ crashes = 0
/\ current_seq_num = ( 1 :> [session |-> 0, counter |-> 0] @@
  2 :> [session |-> 0, counter |-> 0] @@
  3 :> [session |-> 0, counter |-> 0] @@
  6 :> [session |-> 0, counter |-> 0] )
/\ proposed = {v1, v2}
/\ forced_decided_at_force = 0
/\ chan = (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<>> @@ <<2, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "AcceptSync", log_sync |-> <<[suffix |-> <<>>, sync_idx |-> 0]>>, seq_num |-> [session |-> 1, counter |-> 1], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0], [promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "AcceptDecide", log_sync |-> <<>>, seq_num |-> [session |-> 1, counter |-> 2], entries |-> <<v2>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 1>> :> <<>> @@ <<3, 2>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 0, type |-> "ProposalForward", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<v1>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<6, 1>> :> <<>> @@ <<6, 2>> :> <<>> @@ <<6, 3>> :> <<>>)
/\ client = [session |-> 0, request_number |-> 1, sends |-> 0]
/\ past_durable = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ buffered_stopsign = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ reconfigurations = 0
/\ lease_request = <<-1, -1, -1>>
/\ decided_history = <<>>
/\ losses = 0
/\ witness_epoch = 1
/\ forced_has_decided = FALSE
/\ deactivated = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ first_log = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ wait_remaining = 0
/\ timeouts = 0
/\ duplicates = 0
/\ elections = 1
/\ leader_state = (1 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 2 :> [n_leader |-> 12, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 1, counter |-> 2] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 1 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 3], max_promise_sync |-> <<>>] @@ 3 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 6 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>])
/\ rolled_back = {}
/\ lease_reply = <<-1, -1, -1>>
/\ nonce = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ n_accepted = (1 :> 0 @@ 2 :> 12 @@ 3 :> 0 @@ 6 :> 0)
/\ halts = 0
/\ log = (1 :> <<>> @@ 2 :> <<v2>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ cached_promise_message = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ 6 :> <<>>)

State 15: <Receive line 1138, col 3 to line 1154, col 16 of module SequencePaxos>
/\ decided_ballot = <<0>>
/\ wiped = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ lease_remaining = <<0, 0, 0>>
/\ promise = (1 :> 0 @@ 2 :> 12 @@ 3 :> 12 @@ 6 :> 0)
/\ node_epoch = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 1)
/\ read_only = (1 :> TRUE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ wipes = 0
/\ accepted_idx = (1 :> 0 @@ 2 :> 1 @@ 3 :> 0 @@ 6 :> 0)
/\ stage1_ghost = <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>
/\ recovery_responses = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ old_decided_after_forced = FALSE
/\ read_pending = (1 :> {} @@ 2 :> {1} @@ 3 :> {} @@ 6 :> {})
/\ read_round = (1 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 2 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 3 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 6 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}])
/\ buffered_proposals = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ read_only_entries = 1
/\ force_single = [log |-> <<>>, node |-> 1, decided |-> <<>>, history |-> <<>>]
/\ recovering = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ decided_idx = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ build_source = (6 :> 0)
/\ role = (1 :> "Follower" @@ 2 :> "Leader" @@ 3 :> "Follower" @@ 6 :> "Follower")
/\ phase = (1 :> "Recover" @@ 2 :> "Accept" @@ 3 :> "Accept" @@ 6 :> "None")
/\ reads = <<[crashes |-> 0, rolled_back |-> {}, state |-> "pending", read_index |-> 0, node |-> 2, decided_at_start |-> <<>>, result |-> <<>>, stale |-> FALSE, contract_stale_ok |-> FALSE, ballot |-> 0, decided_new_at_start |-> <<>>, forced_decided |-> FALSE]>>
/\ old_decided_max = 0
/\ running = {2, 3, 6}
/\ crashes = 0
/\ current_seq_num = ( 1 :> [session |-> 0, counter |-> 0] @@
  2 :> [session |-> 0, counter |-> 0] @@
  3 :> [session |-> 1, counter |-> 1] @@
  6 :> [session |-> 0, counter |-> 0] )
/\ proposed = {v1, v2}
/\ forced_decided_at_force = 0
/\ chan = (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<>> @@ <<2, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "AcceptDecide", log_sync |-> <<>>, seq_num |-> [session |-> 1, counter |-> 2], entries |-> <<v2>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 1>> :> <<>> @@ <<3, 2>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 0, type |-> "ProposalForward", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<v1>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0], [promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Accepted", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<6, 1>> :> <<>> @@ <<6, 2>> :> <<>> @@ <<6, 3>> :> <<>>)
/\ client = [session |-> 0, request_number |-> 1, sends |-> 0]
/\ past_durable = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ buffered_stopsign = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ reconfigurations = 0
/\ lease_request = <<-1, -1, -1>>
/\ decided_history = <<>>
/\ losses = 0
/\ witness_epoch = 1
/\ forced_has_decided = FALSE
/\ deactivated = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ first_log = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ wait_remaining = 0
/\ timeouts = 0
/\ duplicates = 0
/\ elections = 1
/\ leader_state = (1 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 2 :> [n_leader |-> 12, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 1, counter |-> 2] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 1 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 3], max_promise_sync |-> <<>>] @@ 3 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 6 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>])
/\ rolled_back = {}
/\ lease_reply = <<-1, -1, -1>>
/\ nonce = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ n_accepted = (1 :> 0 @@ 2 :> 12 @@ 3 :> 12 @@ 6 :> 0)
/\ halts = 0
/\ log = (1 :> <<>> @@ 2 :> <<v2>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ cached_promise_message = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)

State 16: <Next line 1954, col 6 to line 1954, col 82 of module SequencePaxos>
/\ decided_ballot = <<0>>
/\ wiped = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ lease_remaining = <<0, 0, 0>>
/\ promise = (1 :> 0 @@ 2 :> 12 @@ 3 :> 12 @@ 6 :> 0)
/\ node_epoch = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 1)
/\ read_only = (1 :> TRUE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ wipes = 0
/\ accepted_idx = (1 :> 0 @@ 2 :> 1 @@ 3 :> 0 @@ 6 :> 0)
/\ stage1_ghost = <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>
/\ recovery_responses = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ old_decided_after_forced = FALSE
/\ read_pending = (1 :> {} @@ 2 :> {1} @@ 3 :> {} @@ 6 :> {})
/\ read_round = (1 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 2 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 3 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 6 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}])
/\ buffered_proposals = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ read_only_entries = 1
/\ force_single = [log |-> <<>>, node |-> 1, decided |-> <<>>, history |-> <<>>]
/\ recovering = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ decided_idx = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ build_source = (6 :> 0)
/\ role = (1 :> "Follower" @@ 2 :> "Leader" @@ 3 :> "Follower" @@ 6 :> "Follower")
/\ phase = (1 :> "Recover" @@ 2 :> "Accept" @@ 3 :> "Accept" @@ 6 :> "None")
/\ reads = <<[crashes |-> 0, rolled_back |-> {}, state |-> "pending", read_index |-> 0, node |-> 2, decided_at_start |-> <<>>, result |-> <<>>, stale |-> FALSE, contract_stale_ok |-> FALSE, ballot |-> 0, decided_new_at_start |-> <<>>, forced_decided |-> FALSE]>>
/\ old_decided_max = 0
/\ running = {2, 3, 6}
/\ crashes = 0
/\ current_seq_num = ( 1 :> [session |-> 0, counter |-> 0] @@
  2 :> [session |-> 0, counter |-> 0] @@
  3 :> [session |-> 1, counter |-> 1] @@
  6 :> [session |-> 0, counter |-> 0] )
/\ proposed = {v1, v2}
/\ forced_decided_at_force = 0
/\ chan = (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<>> @@ <<2, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "AcceptDecide", log_sync |-> <<>>, seq_num |-> [session |-> 1, counter |-> 2], entries |-> <<v2>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 1>> :> <<>> @@ <<3, 2>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 0, type |-> "ProposalForward", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<v1>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0], [promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Accepted", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<6, 1>> :> <<>> @@ <<6, 2>> :> <<>> @@ <<6, 3>> :> <<>>)
/\ client = [session |-> 0, request_number |-> 1, sends |-> 0]
/\ past_durable = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ buffered_stopsign = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ reconfigurations = 0
/\ lease_request = <<-1, 2, -1>>
/\ decided_history = <<>>
/\ losses = 0
/\ witness_epoch = 1
/\ forced_has_decided = FALSE
/\ deactivated = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ first_log = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ wait_remaining = 0
/\ timeouts = 0
/\ duplicates = 0
/\ elections = 1
/\ leader_state = (1 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 2 :> [n_leader |-> 12, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 1, counter |-> 2] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 1 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 3], max_promise_sync |-> <<>>] @@ 3 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 6 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>])
/\ rolled_back = {}
/\ lease_reply = <<-1, -1, -1>>
/\ nonce = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ n_accepted = (1 :> 0 @@ 2 :> 12 @@ 3 :> 12 @@ 6 :> 0)
/\ halts = 0
/\ log = (1 :> <<>> @@ 2 :> <<v2>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ cached_promise_message = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)

State 17: <Receive line 1138, col 3 to line 1154, col 16 of module SequencePaxos>
/\ decided_ballot = <<0>>
/\ wiped = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ lease_remaining = <<0, 0, 0>>
/\ promise = (1 :> 0 @@ 2 :> 12 @@ 3 :> 12 @@ 6 :> 0)
/\ node_epoch = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 1)
/\ read_only = (1 :> TRUE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ wipes = 0
/\ accepted_idx = (1 :> 0 @@ 2 :> 1 @@ 3 :> 1 @@ 6 :> 0)
/\ stage1_ghost = <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>
/\ recovery_responses = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ old_decided_after_forced = FALSE
/\ read_pending = (1 :> {} @@ 2 :> {1} @@ 3 :> {} @@ 6 :> {})
/\ read_round = (1 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 2 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 3 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 6 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}])
/\ buffered_proposals = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ read_only_entries = 1
/\ force_single = [log |-> <<>>, node |-> 1, decided |-> <<>>, history |-> <<>>]
/\ recovering = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ decided_idx = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ build_source = (6 :> 0)
/\ role = (1 :> "Follower" @@ 2 :> "Leader" @@ 3 :> "Follower" @@ 6 :> "Follower")
/\ phase = (1 :> "Recover" @@ 2 :> "Accept" @@ 3 :> "Accept" @@ 6 :> "None")
/\ reads = <<[crashes |-> 0, rolled_back |-> {}, state |-> "pending", read_index |-> 0, node |-> 2, decided_at_start |-> <<>>, result |-> <<>>, stale |-> FALSE, contract_stale_ok |-> FALSE, ballot |-> 0, decided_new_at_start |-> <<>>, forced_decided |-> FALSE]>>
/\ old_decided_max = 0
/\ running = {2, 3, 6}
/\ crashes = 0
/\ current_seq_num = ( 1 :> [session |-> 0, counter |-> 0] @@
  2 :> [session |-> 0, counter |-> 0] @@
  3 :> [session |-> 1, counter |-> 2] @@
  6 :> [session |-> 0, counter |-> 0] )
/\ proposed = {v1, v2}
/\ forced_decided_at_force = 0
/\ chan = (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<>> @@ <<2, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 3>> :> <<>> @@ <<3, 1>> :> <<>> @@ <<3, 2>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 0, type |-> "ProposalForward", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<v1>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0], [promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Accepted", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<6, 1>> :> <<>> @@ <<6, 2>> :> <<>> @@ <<6, 3>> :> <<>>)
/\ client = [session |-> 0, request_number |-> 1, sends |-> 0]
/\ past_durable = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ buffered_stopsign = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ reconfigurations = 0
/\ lease_request = <<-1, 2, -1>>
/\ decided_history = <<>>
/\ losses = 0
/\ witness_epoch = 1
/\ forced_has_decided = FALSE
/\ deactivated = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ first_log = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ wait_remaining = 0
/\ timeouts = 0
/\ duplicates = 0
/\ elections = 1
/\ leader_state = (1 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 2 :> [n_leader |-> 12, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 1, counter |-> 2] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 1 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 3], max_promise_sync |-> <<>>] @@ 3 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 6 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>])
/\ rolled_back = {}
/\ lease_reply = <<-1, -1, -1>>
/\ nonce = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ n_accepted = (1 :> 0 @@ 2 :> 12 @@ 3 :> 12 @@ 6 :> 0)
/\ halts = 0
/\ log = (1 :> <<>> @@ 2 :> <<v2>> @@ 3 :> <<v2>> @@ 6 :> <<>>)
/\ cached_promise_message = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)

State 18: <Next line 1943, col 6 to line 1943, col 41 of module SequencePaxos>
/\ decided_ballot = <<0>>
/\ wiped = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ lease_remaining = <<0, 0, 0>>
/\ promise = (1 :> 0 @@ 2 :> 12 @@ 3 :> 12 @@ 6 :> 0)
/\ node_epoch = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 1)
/\ read_only = (1 :> TRUE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ wipes = 0
/\ accepted_idx = (1 :> 0 @@ 2 :> 1 @@ 3 :> 1 @@ 6 :> 0)
/\ stage1_ghost = <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>
/\ recovery_responses = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ old_decided_after_forced = FALSE
/\ read_pending = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ read_round = (1 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 2 :> [reads |-> {1}, n |-> 12, id |-> 1, read_index |-> 1, acks |-> {2}] @@ 3 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}] @@ 6 :> [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}])
/\ buffered_proposals = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ read_only_entries = 1
/\ force_single = [log |-> <<>>, node |-> 1, decided |-> <<>>, history |-> <<>>]
/\ recovering = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ decided_idx = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ build_source = (6 :> 0)
/\ role = (1 :> "Follower" @@ 2 :> "Leader" @@ 3 :> "Follower" @@ 6 :> "Follower")
/\ phase = (1 :> "Recover" @@ 2 :> "Accept" @@ 3 :> "Accept" @@ 6 :> "None")
/\ reads = <<[crashes |-> 0, rolled_back |-> {}, state |-> "pending", read_index |-> 0, node |-> 2, decided_at_start |-> <<>>, result |-> <<>>, stale |-> FALSE, contract_stale_ok |-> FALSE, ballot |-> 0, decided_new_at_start |-> <<>>, forced_decided |-> FALSE]>>
/\ old_decided_max = 0
/\ running = {2, 3, 6}
/\ crashes = 0
/\ current_seq_num = ( 1 :> [session |-> 0, counter |-> 0] @@
  2 :> [session |-> 0, counter |-> 0] @@
  3 :> [session |-> 1, counter |-> 2] @@
  6 :> [session |-> 0, counter |-> 0] )
/\ proposed = {v1, v2}
/\ forced_decided_at_force = 0
/\ chan = (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<>> @@ <<2, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0], [promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Heartbeat", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 1, read_index |-> 0]>> @@ <<2, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Heartbeat", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 1, read_index |-> 0]>> @@ <<3, 1>> :> <<>> @@ <<3, 2>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 0, type |-> "ProposalForward", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<v1>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0], [promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 12, type |-> "Accepted", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<6, 1>> :> <<>> @@ <<6, 2>> :> <<>> @@ <<6, 3>> :> <<>>)
/\ client = [session |-> 0, request_number |-> 1, sends |-> 0]
/\ past_durable = (1 :> {} @@ 2 :> {} @@ 3 :> {} @@ 6 :> {})
/\ buffered_stopsign = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ reconfigurations = 0
/\ lease_request = <<-1, 2, -1>>
/\ decided_history = <<>>
/\ losses = 0
/\ witness_epoch = 1
/\ forced_has_decided = FALSE
/\ deactivated = (1 :> FALSE @@ 2 :> FALSE @@ 3 :> FALSE @@ 6 :> FALSE)
/\ first_log = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)
/\ wait_remaining = 0
/\ timeouts = 0
/\ duplicates = 0
/\ elections = 1
/\ leader_state = (1 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 2 :> [n_leader |-> 12, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 1, counter |-> 2] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 1 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 3], max_promise_sync |-> <<>>] @@ 3 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>] @@ 6 :> [n_leader |-> 0, promises_meta |-> (1 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 2 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 3 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"] @@ 6 :> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]), follower_seq_nums |-> (1 :> [session |-> 0, counter |-> 0] @@ 2 :> [session |-> 0, counter |-> 0] @@ 3 :> [session |-> 0, counter |-> 0] @@ 6 :> [session |-> 0, counter |-> 0]), accepted_indexes |-> (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0), max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>])
/\ rolled_back = {}
/\ lease_reply = <<-1, -1, -1>>
/\ nonce = (1 :> 0 @@ 2 :> 0 @@ 3 :> 0 @@ 6 :> 0)
/\ n_accepted = (1 :> 0 @@ 2 :> 12 @@ 3 :> 12 @@ 6 :> 0)
/\ halts = 0
/\ log = (1 :> <<>> @@ 2 :> <<v2>> @@ 3 :> <<v2>> @@ 6 :> <<>>)
/\ cached_promise_message = (1 :> <<>> @@ 2 :> <<>> @@ 3 :> <<>> @@ 6 :> <<>>)

The number of states generated: 458
Simulation using seed 1 and aril 0
Progress: 500 states checked.
Finished in 00s at (2026-10-19 00:34:54)
exit 13

```

