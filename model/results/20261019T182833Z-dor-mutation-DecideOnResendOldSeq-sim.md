# TLC run dor-mutation-DecideOnResendOldSeq-sim, 20261019T182833Z

Counter-check of doc/plan-decide-on-resend.md section 6 (F3, F4): the Decide at a resend timeout carries the old sequence number. Model 20406a3, 3 workers. Random simulation (-simulate -deadlock -depth 60, seed 1, limit 15 min). Expected: a violation of S5.

## Configuration

Spec: `model/tla/SequencePaxos.tla`. Config file `model/tla/MC_mut_DecideOnResendOldSeq.cfg`:

```
CONSTANTS
  Nodes = {1, 2, 3}
  Values = {v1, v2}
  MaxBallot = 1
  MaxLog = 2
  MaxCounter = 5
  MaxSession = 2
  Cap = 2
  MaxCrash = 0
  MaxWipe = 0
  MaxLoss = 1
  MaxDuplicate = 0
  MaxTimeout = 1
  MaxElection = 2
  RuleR = TRUE
  DecidedIdxMonotone = TRUE
  MaxRollback = 0
  Recovery = FALSE
  Mutation = "DecideOnResendOldSeq"
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
INVARIANTS S5
```

## Tools

- TLC2 Version 2.19 of 08 August 2024 (rev: 5a47802)
- Running Random Simulation with seed 1 with 3 workers on 4 cores with 3040MB heap and 64MB offheap memory [pid: 10046] (Linux 6.18.44-fc-v139 amd64, Ubuntu 21.0.12.1 x86_64).
- Java:

```
Picked up JAVA_TOOL_OPTIONS: -Djavax.net.ssl.trustStore=/etc/ssl/certs/java/cacerts -Djavax.net.ssl.trustStoreType=JKS -Dhttps.proxyHost=127.0.0.1 -Dhttps.proxyPort=46401 -Dhttp.nonProxyHosts=localhost|127.0.0.1|::1|127.*|0.*|::|169.254.*|api.anthropic.com|api-staging.anthropic.com|api-pr-preview.anthropic.com|mcp-proxy.anthropic.com|mcp-proxy-staging.anthropic.com|registry.npmjs.org|jsr.io|npm.jsr.io|pypi.org|files.pythonhosted.org|index.crates.io|proxy.golang.org|host.docker.internal|10.*|172.16.*|172.17.*|172.18.*|172.19.*|172.20.*|172.21.*|172.22.*|172.23.*|172.24.*|172.25.*|172.26.*|172.27.*|172.28.*|172.29.*|172.30.*|172.31.*|192.168.*|100.64.0.0/10|*.svc.cluster.local|*.svc.cluster.local -Djdk.http.auth.tunneling.disabledSchemes= -Djdk.http.auth.proxying.disabledSchemes=
openjdk version "21.0.12.1" 2026-08-18
OpenJDK Runtime Environment (build 21.0.12.1+1-1-24.04.4-Ubuntu)
OpenJDK 64-Bit Server VM (build 21.0.12.1+1-1-24.04.4-Ubuntu, mixed mode, sharing)
```

## Numbers

- Last progress line: n/a
- Wall time: start 20261019T182833Z, end 20261019T182836Z, UTC. Exit code 255.

## Result

TLC found the violation of S5 after 3 s. It printed the trace up to state 12 and then stopped with an ArrayIndexOutOfBoundsException, the known fault of TLC 2.19 in the print of a trace. State 8 loses a message, state 9 is ResendMessageTimeout, and states 10 to 12 deliver messages. The state that violates S5 comes after state 12 and was not printed.

```
Error: Invariant S5 is violated.
Error: The behavior up to this point is:
State 1: <Initial predicate>
/\ decided_ballot = <<0, 0>>
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

State 2: <Next line 1940, col 6 to line 1940, col 62 of module SequencePaxos>
/\ decided_ballot = <<0, 0>>
/\ wiped = <<FALSE, FALSE, FALSE>>
/\ lease_remaining = <<0, 0, 0>>
/\ promise = <<0, 0, 13>>
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
/\ role = <<"Follower", "Follower", "Leader">>
/\ phase = <<"None", "None", "Prepare">>
/\ reads = << >>
/\ old_decided_max = 0
/\ running = {1, 2, 3}
/\ crashes = 0
/\ current_seq_num = << [session |-> 0, counter |-> 0],
   [session |-> 0, counter |-> 0],
   [session |-> 0, counter |-> 0] >>
/\ proposed = {}
/\ forced_decided_at_force = 0
/\ chan = (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<>> @@ <<2, 1>> :> <<>> @@ <<2, 3>> :> <<>> @@ <<3, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 2>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>>)
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
/\ leader_state = <<[n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 13, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 3], max_promise_sync |-> <<>>]>>
/\ rolled_back = {}
/\ lease_reply = <<-1, -1, -1>>
/\ nonce = <<0, 0, 0>>
/\ n_accepted = <<0, 0, 0>>
/\ halts = 0
/\ log = <<<<>>, <<>>, <<>>>>
/\ cached_promise_message = <<<<>>, <<>>, <<>>>>

State 3: <Next line 1941, col 6 to line 1941, col 51 of module SequencePaxos>
/\ decided_ballot = <<0, 0>>
/\ wiped = <<FALSE, FALSE, FALSE>>
/\ lease_remaining = <<0, 0, 0>>
/\ promise = <<0, 0, 13>>
/\ node_epoch = <<0, 0, 0>>
/\ read_only = <<FALSE, FALSE, FALSE>>
/\ wipes = 0
/\ accepted_idx = <<0, 0, 0>>
/\ stage1_ghost = <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>
/\ recovery_responses = <<{}, {}, {}>>
/\ old_decided_after_forced = FALSE
/\ read_pending = <<{}, {}, {}>>
/\ read_round = <<[reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}]>>
/\ buffered_proposals = <<<<v1>>, <<>>, <<>>>>
/\ read_only_entries = 0
/\ force_single = [log |-> <<>>, node |-> 0, decided |-> <<>>, history |-> <<>>]
/\ recovering = <<FALSE, FALSE, FALSE>>
/\ decided_idx = <<0, 0, 0>>
/\ build_source = << >>
/\ role = <<"Follower", "Follower", "Leader">>
/\ phase = <<"None", "None", "Prepare">>
/\ reads = << >>
/\ old_decided_max = 0
/\ running = {1, 2, 3}
/\ crashes = 0
/\ current_seq_num = << [session |-> 0, counter |-> 0],
   [session |-> 0, counter |-> 0],
   [session |-> 0, counter |-> 0] >>
/\ proposed = {v1}
/\ forced_decided_at_force = 0
/\ chan = (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<>> @@ <<2, 1>> :> <<>> @@ <<2, 3>> :> <<>> @@ <<3, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 2>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>>)
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
/\ leader_state = <<[n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 13, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 3], max_promise_sync |-> <<>>]>>
/\ rolled_back = {}
/\ lease_reply = <<-1, -1, -1>>
/\ nonce = <<0, 0, 0>>
/\ n_accepted = <<0, 0, 0>>
/\ halts = 0
/\ log = <<<<>>, <<>>, <<>>>>
/\ cached_promise_message = <<<<>>, <<>>, <<>>>>

State 4: <Receive line 1138, col 3 to line 1154, col 16 of module SequencePaxos>
/\ decided_ballot = <<0, 0>>
/\ wiped = <<FALSE, FALSE, FALSE>>
/\ lease_remaining = <<0, 0, 0>>
/\ promise = <<13, 0, 13>>
/\ node_epoch = <<0, 0, 0>>
/\ read_only = <<FALSE, FALSE, FALSE>>
/\ wipes = 0
/\ accepted_idx = <<0, 0, 0>>
/\ stage1_ghost = <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>
/\ recovery_responses = <<{}, {}, {}>>
/\ old_decided_after_forced = FALSE
/\ read_pending = <<{}, {}, {}>>
/\ read_round = <<[reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}]>>
/\ buffered_proposals = <<<<v1>>, <<>>, <<>>>>
/\ read_only_entries = 0
/\ force_single = [log |-> <<>>, node |-> 0, decided |-> <<>>, history |-> <<>>]
/\ recovering = <<FALSE, FALSE, FALSE>>
/\ decided_idx = <<0, 0, 0>>
/\ build_source = << >>
/\ role = <<"Follower", "Follower", "Leader">>
/\ phase = <<"Prepare", "None", "Prepare">>
/\ reads = << >>
/\ old_decided_max = 0
/\ running = {1, 2, 3}
/\ crashes = 0
/\ current_seq_num = << [session |-> 0, counter |-> 0],
   [session |-> 0, counter |-> 0],
   [session |-> 0, counter |-> 0] >>
/\ proposed = {v1}
/\ forced_decided_at_force = 0
/\ chan = (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 1>> :> <<>> @@ <<2, 3>> :> <<>> @@ <<3, 1>> :> <<>> @@ <<3, 2>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>>)
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
/\ leader_state = <<[n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 13, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 3], max_promise_sync |-> <<>>]>>
/\ rolled_back = {}
/\ lease_reply = <<-1, -1, -1>>
/\ nonce = <<0, 0, 0>>
/\ n_accepted = <<0, 0, 0>>
/\ halts = 0
/\ log = <<<<>>, <<>>, <<>>>>
/\ cached_promise_message = <<<<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>>, <<>>, <<>>>>

State 5: <Receive line 1138, col 3 to line 1154, col 16 of module SequencePaxos>
/\ decided_ballot = <<0, 0>>
/\ wiped = <<FALSE, FALSE, FALSE>>
/\ lease_remaining = <<0, 0, 0>>
/\ promise = <<13, 0, 13>>
/\ node_epoch = <<0, 0, 0>>
/\ read_only = <<FALSE, FALSE, FALSE>>
/\ wipes = 0
/\ accepted_idx = <<0, 0, 0>>
/\ stage1_ghost = <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>
/\ recovery_responses = <<{}, {}, {}>>
/\ old_decided_after_forced = FALSE
/\ read_pending = <<{}, {}, {}>>
/\ read_round = <<[reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}]>>
/\ buffered_proposals = <<<<v1>>, <<>>, <<>>>>
/\ read_only_entries = 0
/\ force_single = [log |-> <<>>, node |-> 0, decided |-> <<>>, history |-> <<>>]
/\ recovering = <<FALSE, FALSE, FALSE>>
/\ decided_idx = <<0, 0, 0>>
/\ build_source = << >>
/\ role = <<"Follower", "Follower", "Leader">>
/\ phase = <<"Prepare", "None", "Accept">>
/\ reads = << >>
/\ old_decided_max = 0
/\ running = {1, 2, 3}
/\ crashes = 0
/\ current_seq_num = << [session |-> 0, counter |-> 0],
   [session |-> 0, counter |-> 0],
   [session |-> 0, counter |-> 0] >>
/\ proposed = {v1}
/\ forced_decided_at_force = 0
/\ chan = (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<>> @@ <<2, 1>> :> <<>> @@ <<2, 3>> :> <<>> @@ <<3, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "AcceptSync", log_sync |-> <<[suffix |-> <<>>, sync_idx |-> 0]>>, seq_num |-> [session |-> 1, counter |-> 1], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 2>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>>)
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
/\ leader_state = <<[n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 13, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"]>>, follower_seq_nums |-> <<[session |-> 1, counter |-> 1], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 1], max_promise_sync |-> <<>>]>>
/\ rolled_back = {}
/\ lease_reply = <<-1, -1, -1>>
/\ nonce = <<0, 0, 0>>
/\ n_accepted = <<0, 0, 13>>
/\ halts = 0
/\ log = <<<<>>, <<>>, <<>>>>
/\ cached_promise_message = <<<<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>>, <<>>, <<>>>>

State 6: <Receive line 1138, col 3 to line 1154, col 16 of module SequencePaxos>
/\ decided_ballot = <<0, 0>>
/\ wiped = <<FALSE, FALSE, FALSE>>
/\ lease_remaining = <<0, 0, 0>>
/\ promise = <<13, 0, 13>>
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
/\ role = <<"Follower", "Follower", "Leader">>
/\ phase = <<"Accept", "None", "Accept">>
/\ reads = << >>
/\ old_decided_max = 0
/\ running = {1, 2, 3}
/\ crashes = 0
/\ current_seq_num = << [session |-> 1, counter |-> 1],
   [session |-> 0, counter |-> 0],
   [session |-> 0, counter |-> 0] >>
/\ proposed = {v1}
/\ forced_decided_at_force = 0
/\ chan = (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 0, type |-> "ProposalForward", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<v1>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0], [promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "Accepted", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 1>> :> <<>> @@ <<2, 3>> :> <<>> @@ <<3, 1>> :> <<>> @@ <<3, 2>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>>)
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
/\ leader_state = <<[n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 13, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"]>>, follower_seq_nums |-> <<[session |-> 1, counter |-> 1], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 1], max_promise_sync |-> <<>>]>>
/\ rolled_back = {}
/\ lease_reply = <<-1, -1, -1>>
/\ nonce = <<0, 0, 0>>
/\ n_accepted = <<13, 0, 13>>
/\ halts = 0
/\ log = <<<<>>, <<>>, <<>>>>
/\ cached_promise_message = <<<<>>, <<>>, <<>>>>

State 7: <Next line 1941, col 6 to line 1941, col 51 of module SequencePaxos>
/\ decided_ballot = <<0, 0>>
/\ wiped = <<FALSE, FALSE, FALSE>>
/\ lease_remaining = <<0, 0, 0>>
/\ promise = <<13, 0, 13>>
/\ node_epoch = <<0, 0, 0>>
/\ read_only = <<FALSE, FALSE, FALSE>>
/\ wipes = 0
/\ accepted_idx = <<0, 0, 1>>
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
/\ role = <<"Follower", "Follower", "Leader">>
/\ phase = <<"Accept", "None", "Accept">>
/\ reads = << >>
/\ old_decided_max = 0
/\ running = {1, 2, 3}
/\ crashes = 0
/\ current_seq_num = << [session |-> 1, counter |-> 1],
   [session |-> 0, counter |-> 0],
   [session |-> 0, counter |-> 0] >>
/\ proposed = {v1, v2}
/\ forced_decided_at_force = 0
/\ chan = (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 0, type |-> "ProposalForward", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<v1>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0], [promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "Accepted", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 1>> :> <<>> @@ <<2, 3>> :> <<>> @@ <<3, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "AcceptDecide", log_sync |-> <<>>, seq_num |-> [session |-> 1, counter |-> 2], entries |-> <<v2>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 2>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>>)
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
/\ leader_state = <<[n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 13, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"]>>, follower_seq_nums |-> <<[session |-> 1, counter |-> 2], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 1>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 1], max_promise_sync |-> <<>>]>>
/\ rolled_back = {}
/\ lease_reply = <<-1, -1, -1>>
/\ nonce = <<0, 0, 0>>
/\ n_accepted = <<13, 0, 13>>
/\ halts = 0
/\ log = <<<<>>, <<>>, <<v2>>>>
/\ cached_promise_message = <<<<>>, <<>>, <<>>>>

State 8: <Next line 1946, col 37 to line 1946, col 67 of module SequencePaxos>
/\ decided_ballot = <<0, 0>>
/\ wiped = <<FALSE, FALSE, FALSE>>
/\ lease_remaining = <<0, 0, 0>>
/\ promise = <<13, 0, 13>>
/\ node_epoch = <<0, 0, 0>>
/\ read_only = <<FALSE, FALSE, FALSE>>
/\ wipes = 0
/\ accepted_idx = <<0, 0, 1>>
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
/\ role = <<"Follower", "Follower", "Leader">>
/\ phase = <<"Accept", "None", "Accept">>
/\ reads = << >>
/\ old_decided_max = 0
/\ running = {1, 2, 3}
/\ crashes = 0
/\ current_seq_num = << [session |-> 1, counter |-> 1],
   [session |-> 0, counter |-> 0],
   [session |-> 0, counter |-> 0] >>
/\ proposed = {v1, v2}
/\ forced_decided_at_force = 0
/\ chan = (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 0, type |-> "ProposalForward", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<v1>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0], [promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "Accepted", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 1>> :> <<>> @@ <<2, 3>> :> <<>> @@ <<3, 1>> :> <<>> @@ <<3, 2>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>>)
/\ client = [session |-> 0, request_number |-> 1, sends |-> 0]
/\ past_durable = <<{}, {}, {}>>
/\ buffered_stopsign = <<<<>>, <<>>, <<>>>>
/\ reconfigurations = 0
/\ lease_request = <<-1, -1, -1>>
/\ decided_history = <<>>
/\ losses = 1
/\ witness_epoch = 0
/\ forced_has_decided = FALSE
/\ deactivated = <<FALSE, FALSE, FALSE>>
/\ first_log = <<<<>>, <<>>, <<>>>>
/\ wait_remaining = -1
/\ timeouts = 0
/\ duplicates = 0
/\ elections = 1
/\ leader_state = <<[n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 13, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"]>>, follower_seq_nums |-> <<[session |-> 1, counter |-> 2], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 1>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 1], max_promise_sync |-> <<>>]>>
/\ rolled_back = {}
/\ lease_reply = <<-1, -1, -1>>
/\ nonce = <<0, 0, 0>>
/\ n_accepted = <<13, 0, 13>>
/\ halts = 0
/\ log = <<<<>>, <<>>, <<v2>>>>
/\ cached_promise_message = <<<<>>, <<>>, <<>>>>

State 9: <Next line 1945, col 6 to line 1945, col 47 of module SequencePaxos>
/\ decided_ballot = <<0, 0>>
/\ wiped = <<FALSE, FALSE, FALSE>>
/\ lease_remaining = <<0, 0, 0>>
/\ promise = <<13, 0, 13>>
/\ node_epoch = <<0, 0, 0>>
/\ read_only = <<FALSE, FALSE, FALSE>>
/\ wipes = 0
/\ accepted_idx = <<0, 0, 1>>
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
/\ role = <<"Follower", "Follower", "Leader">>
/\ phase = <<"Accept", "None", "Accept">>
/\ reads = << >>
/\ old_decided_max = 0
/\ running = {1, 2, 3}
/\ crashes = 0
/\ current_seq_num = << [session |-> 1, counter |-> 1],
   [session |-> 0, counter |-> 0],
   [session |-> 0, counter |-> 0] >>
/\ proposed = {v1, v2}
/\ forced_decided_at_force = 0
/\ chan = (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 0, type |-> "ProposalForward", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<v1>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0], [promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "Accepted", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 1>> :> <<>> @@ <<2, 3>> :> <<>> @@ <<3, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "Decide", log_sync |-> <<>>, seq_num |-> [session |-> 1, counter |-> 2], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 2>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0], [promise |-> 0, n_accepted |-> 13, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 1, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>>)
/\ client = [session |-> 0, request_number |-> 1, sends |-> 0]
/\ past_durable = <<{}, {}, {}>>
/\ buffered_stopsign = <<<<>>, <<>>, <<>>>>
/\ reconfigurations = 0
/\ lease_request = <<-1, -1, -1>>
/\ decided_history = <<>>
/\ losses = 1
/\ witness_epoch = 0
/\ forced_has_decided = FALSE
/\ deactivated = <<FALSE, FALSE, FALSE>>
/\ first_log = <<<<>>, <<>>, <<>>>>
/\ wait_remaining = -1
/\ timeouts = 1
/\ duplicates = 0
/\ elections = 1
/\ leader_state = <<[n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 13, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"]>>, follower_seq_nums |-> <<[session |-> 1, counter |-> 2], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 1>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 1], max_promise_sync |-> <<>>]>>
/\ rolled_back = {}
/\ lease_reply = <<-1, -1, -1>>
/\ nonce = <<0, 0, 0>>
/\ n_accepted = <<13, 0, 13>>
/\ halts = 0
/\ log = <<<<>>, <<>>, <<v2>>>>
/\ cached_promise_message = <<<<>>, <<>>, <<>>>>

State 10: <Receive line 1138, col 3 to line 1154, col 16 of module SequencePaxos>
/\ decided_ballot = <<0, 0>>
/\ wiped = <<FALSE, FALSE, FALSE>>
/\ lease_remaining = <<0, 0, 0>>
/\ promise = <<13, 13, 13>>
/\ node_epoch = <<0, 0, 0>>
/\ read_only = <<FALSE, FALSE, FALSE>>
/\ wipes = 0
/\ accepted_idx = <<0, 0, 1>>
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
/\ role = <<"Follower", "Follower", "Leader">>
/\ phase = <<"Accept", "Prepare", "Accept">>
/\ reads = << >>
/\ old_decided_max = 0
/\ running = {1, 2, 3}
/\ crashes = 0
/\ current_seq_num = << [session |-> 1, counter |-> 1],
   [session |-> 0, counter |-> 0],
   [session |-> 0, counter |-> 0] >>
/\ proposed = {v1, v2}
/\ forced_decided_at_force = 0
/\ chan = (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 0, type |-> "ProposalForward", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<v1>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0], [promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "Accepted", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 1>> :> <<>> @@ <<2, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "Decide", log_sync |-> <<>>, seq_num |-> [session |-> 1, counter |-> 2], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 2>> :> <<[promise |-> 0, n_accepted |-> 13, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 1, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>>)
/\ client = [session |-> 0, request_number |-> 1, sends |-> 0]
/\ past_durable = <<{}, {}, {}>>
/\ buffered_stopsign = <<<<>>, <<>>, <<>>>>
/\ reconfigurations = 0
/\ lease_request = <<-1, -1, -1>>
/\ decided_history = <<>>
/\ losses = 1
/\ witness_epoch = 0
/\ forced_has_decided = FALSE
/\ deactivated = <<FALSE, FALSE, FALSE>>
/\ first_log = <<<<>>, <<>>, <<>>>>
/\ wait_remaining = -1
/\ timeouts = 1
/\ duplicates = 0
/\ elections = 1
/\ leader_state = <<[n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 13, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"]>>, follower_seq_nums |-> <<[session |-> 1, counter |-> 2], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 1>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 1], max_promise_sync |-> <<>>]>>
/\ rolled_back = {}
/\ lease_reply = <<-1, -1, -1>>
/\ nonce = <<0, 0, 0>>
/\ n_accepted = <<13, 0, 13>>
/\ halts = 0
/\ log = <<<<>>, <<>>, <<v2>>>>
/\ cached_promise_message = <<<<>>, <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>>, <<>>>>

State 11: <Receive line 1138, col 3 to line 1154, col 16 of module SequencePaxos>
/\ decided_ballot = <<0, 0>>
/\ wiped = <<FALSE, FALSE, FALSE>>
/\ lease_remaining = <<0, 0, 0>>
/\ promise = <<13, 13, 13>>
/\ node_epoch = <<0, 0, 0>>
/\ read_only = <<FALSE, FALSE, FALSE>>
/\ wipes = 0
/\ accepted_idx = <<0, 0, 1>>
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
/\ role = <<"Follower", "Follower", "Leader">>
/\ phase = <<"Accept", "Prepare", "Accept">>
/\ reads = << >>
/\ old_decided_max = 0
/\ running = {1, 2, 3}
/\ crashes = 0
/\ current_seq_num = << [session |-> 1, counter |-> 2],
   [session |-> 0, counter |-> 0],
   [session |-> 0, counter |-> 0] >>
/\ proposed = {v1, v2}
/\ forced_decided_at_force = 0
/\ chan = (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 0, type |-> "ProposalForward", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<v1>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0], [promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "Accepted", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 1>> :> <<>> @@ <<2, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 1>> :> <<>> @@ <<3, 2>> :> <<[promise |-> 0, n_accepted |-> 13, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 1, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>>)
/\ client = [session |-> 0, request_number |-> 1, sends |-> 0]
/\ past_durable = <<{}, {}, {}>>
/\ buffered_stopsign = <<<<>>, <<>>, <<>>>>
/\ reconfigurations = 0
/\ lease_request = <<-1, -1, -1>>
/\ decided_history = <<>>
/\ losses = 1
/\ witness_epoch = 0
/\ forced_has_decided = FALSE
/\ deactivated = <<FALSE, FALSE, FALSE>>
/\ first_log = <<<<>>, <<>>, <<>>>>
/\ wait_remaining = -1
/\ timeouts = 1
/\ duplicates = 0
/\ elections = 1
/\ leader_state = <<[n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 13, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"]>>, follower_seq_nums |-> <<[session |-> 1, counter |-> 2], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 1>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 1], max_promise_sync |-> <<>>]>>
/\ rolled_back = {}
/\ lease_reply = <<-1, -1, -1>>
/\ nonce = <<0, 0, 0>>
/\ n_accepted = <<13, 0, 13>>
/\ halts = 0
/\ log = <<<<>>, <<>>, <<v2>>>>
/\ cached_promise_message = <<<<>>, <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>>, <<>>>>

State 12: <Receive line 1138, col 3 to line 1154, col 16 of module SequencePaxos>
/\ decided_ballot = <<0, 0>>
/\ wiped = <<FALSE, FALSE, FALSE>>
/\ lease_remaining = <<0, 0, 0>>
/\ promise = <<13, 13, 13>>
/\ node_epoch = <<0, 0, 0>>
/\ read_only = <<FALSE, FALSE, FALSE>>
/\ wipes = 0
/\ accepted_idx = <<0, 0, 1>>
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
/\ role = <<"Follower", "Follower", "Leader">>
/\ phase = <<"Accept", "Prepare", "Accept">>
/\ reads = << >>
/\ old_decided_max = 0
/\ running = {1, 2, 3}
/\ crashes = 0
/\ current_seq_num = << [session |-> 1, counter |-> 2],
   [session |-> 0, counter |-> 0],
   [session |-> 0, counter |-> 0] >>
/\ proposed = {v1, v2}
/\ forced_decided_at_force = 0
/\ chan = (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 0, type |-> "ProposalForward", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<v1>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0], [promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "Accepted", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 1>> :> <<>> @@ <<2, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 1>> :> <<>> @@ <<3, 2>> :> <<>>)
/\ client = [session |-> 0, request_number |-> 1, sends |-> 0]
/\ past_durable = <<{}, {}, {}>>
/\ buffered_stopsign = <<<<>>, <<>>, <<>>>>
/\ reconfigurations = 0
/\ lease_request = <<-1, -1, -1>>
/\ decided_history = <<>>
/\ losses = 1
/\ witness_epoch = 0
/\ forced_has_decided = FALSE
/\ deactivated = <<FALSE, FALSE, FALSE>>
/\ first_log = <<<<>>, <<>>, <<>>>>
/\ wait_remaining = -1
/\ timeouts = 1
/\ duplicates = 0
/\ elections = 1
/\ leader_state = <<[n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 13, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised"]>>, follower_seq_nums |-> <<[session |-> 1, counter |-> 2], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 1>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 1], max_promise_sync |-> <<>>]>>
/\ rolled_back = {}
/\ lease_reply = <<-1, -1, -1>>
/\ nonce = <<0, 0, 0>>
/\ n_accepted = <<13, 0, 13>>
/\ halts = 0
/\ log = <<<<>>, <<>>, <<v2>>>>
/\ cached_promise_message = <<<<>>, <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, nonce |-> 0, reads |-> {}, n |-> 13, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>>, <<>>>>

Error: TLC threw an unexpected exception.
This was probably caused by an error in the spec or model.
See the User Output or TLC Console for clues to what happened.
The exception was a java.lang.ArrayIndexOutOfBoundsException
: Index 1 out of bounds for length 1
Finished in 03s at (2026-10-19 18:28:36)
```
