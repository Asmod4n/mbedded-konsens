---- MODULE SequencePaxos_TTrace_1794523212 ----
EXTENDS Sequences, TLCExt, Toolbox, SequencePaxos, Naturals, TLC, SequencePaxos_TEConstants

_expression ==
    LET SequencePaxos_TEExpression == INSTANCE SequencePaxos_TEExpression
    IN SequencePaxos_TEExpression!expression
----

_trace ==
    LET SequencePaxos_TETrace == INSTANCE SequencePaxos_TETrace
    IN SequencePaxos_TETrace!trace
----

_inv ==
    ~(
        TLCGet("level") = Len(_TETrace)
        /\
        open_mark = (<<0, 0, 0>>)
        /\
        counts = (<<[ans |-> <<[ptr |-> -1, map |-> <<>>], [ptr |-> -1, map |-> <<>>], [ptr |-> -1, map |-> <<>>]>>, tried |-> {}, again |-> {}, refusals |-> {}, appended |-> {}], [ans |-> <<[ptr |-> -1, map |-> <<>>], [ptr |-> -1, map |-> <<>>], [ptr |-> -1, map |-> <<>>]>>, tried |-> {}, again |-> {}, refusals |-> {}, appended |-> {}], [ans |-> <<[ptr |-> -1, map |-> <<>>], [ptr |-> -1, map |-> <<>>], [ptr |-> -1, map |-> <<>>]>>, tried |-> {}, again |-> {}, refusals |-> {}, appended |-> {}]>>)
        /\
        force_single = ([log |-> <<>>, decided |-> <<>>, node |-> 0, history |-> <<>>])
        /\
        own = (<<<<>>, <<>>, <<>>>>)
        /\
        timeouts = (0)
        /\
        lease_remaining = (<<0, 0, 0>>)
        /\
        promise = (<<11, 11, 0>>)
        /\
        crashes = (0)
        /\
        losses = (0)
        /\
        deactivated = (<<FALSE, FALSE, FALSE>>)
        /\
        read_only = (<<FALSE, FALSE, FALSE>>)
        /\
        rolled_back = ({})
        /\
        proof = (<<{}, {}, {}>>)
        /\
        selected = (<<<<>>, <<>>, <<>>>>)
        /\
        accepted_idx = (<<0, 0, 0>>)
        /\
        phase = (<<"Prepare", "Prepare", "None">>)
        /\
        duplicates = (1)
        /\
        read_round = (<<[reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}]>>)
        /\
        read_pending = (<<{}, {}, {}>>)
        /\
        elections = (1)
        /\
        peer_seen = (<<<<[n |-> 0, idx |-> 0], [n |-> 0, idx |-> 0], [n |-> 0, idx |-> 0]>>, <<[n |-> 0, idx |-> 0], [n |-> 0, idx |-> 0], [n |-> 0, idx |-> 0]>>, <<[n |-> 0, idx |-> 0], [n |-> 0, idx |-> 0], [n |-> 0, idx |-> 0]>>>>)
        /\
        nonce = (<<0, 0, 0>>)
        /\
        old_decided_after_forced = (FALSE)
        /\
        repairs = (<<0, 0, 0>>)
        /\
        wipes = (0)
        /\
        proposed = ({})
        /\
        halts = (0)
        /\
        in_txn = (<<<<>>, <<>>, <<>>>>)
        /\
        lease_reply = (<<-1, -1, -1>>)
        /\
        cached_promise_message = (<<<<>>, <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, recovering |-> FALSE, nonce |-> 0, reads |-> {}, n |-> 11, last |-> <<>>, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0, ptr |-> 0, base |-> <<>>, results |-> <<>>, idx |-> 0]>>, <<>>>>)
        /\
        recovery_responses = (<<{}, {}, {}>>)
        /\
        buffered_stopsign = (<<<<>>, <<>>, <<>>>>)
        /\
        recovering = (<<FALSE, FALSE, FALSE>>)
        /\
        role = (<<"Leader", "Follower", "Follower">>)
        /\
        wiped = (<<FALSE, FALSE, FALSE>>)
        /\
        applied = (<<<<>>, <<>>, <<>>>>)
        /\
        log = (<<<<>>, <<>>, <<>>>>)
        /\
        first_log = (<<<<>>, <<>>, <<>>>>)
        /\
        decided_history = (<<>>)
        /\
        lease_request = (<<-1, -1, -1>>)
        /\
        storage_faults = (0)
        /\
        running = ({1, 2, 3})
        /\
        current_seq_num = (<<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>)
        /\
        past_durable = (<<{}, {}, {}>>)
        /\
        verified_idx = (<<0, 0, 0>>)
        /\
        reported = ({})
        /\
        power_loss_completed = ({})
        /\
        forced_has_decided = (FALSE)
        /\
        client = ([session |-> 0, request_number |-> 1, sends |-> 0])
        /\
        n_accepted = (<<0, 0, 0>>)
        /\
        result_throws = (<<>>)
        /\
        decided_idx = (<<0, 0, 0>>)
        /\
        answered = (<<[n |-> -1, ptr |-> -1], [n |-> -1, ptr |-> -1], [n |-> -1, ptr |-> -1]>>)
        /\
        leader_state = (<<[n_leader |-> 11, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised", last |-> <<>>], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised", last |-> <<>>], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised", last |-> <<>>]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, last |-> <<>>, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised", last |-> <<>>], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised", last |-> <<>>], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised", last |-> <<>>]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, last |-> <<>>, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised", last |-> <<>>], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised", last |-> <<>>], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised", last |-> <<>>]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, last |-> <<>>, pid |-> 0], max_promise_sync |-> <<>>]>>)
        /\
        refused = ({})
        /\
        high_water = (<<[promise |-> 0, n_accepted |-> 0, acc |-> 0, decided |-> 0], [promise |-> 0, n_accepted |-> 0, acc |-> 0, decided |-> 0], [promise |-> 0, n_accepted |-> 0, acc |-> 0, decided |-> 0]>>)
        /\
        buffered_proposals = (<<<<>>, <<>>, <<>>>>)
        /\
        reads = (<<>>)
        /\
        stage1_ghost = (<<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>)
        /\
        refusal = (<<[n |-> -1, idx |-> 0], [n |-> -1, idx |-> 0], [n |-> -1, idx |-> 0]>>)
        /\
        decided_ballot = (<<0>>)
        /\
        node_epoch = (<<0, 0, 0>>)
        /\
        witness_epoch = (0)
        /\
        forced_decided_at_force = (0)
        /\
        old_decided_max = (0)
        /\
        mark_covered = (<<FALSE, FALSE, FALSE>>)
        /\
        read_only_entries = (0)
        /\
        wrong = ({})
        /\
        reconfigurations = (0)
        /\
        flips = ({})
        /\
        build_source = (<<>>)
        /\
        wait_remaining = (-1)
        /\
        chan = ((<<1, 2>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, recovering |-> FALSE, nonce |-> 0, reads |-> {}, n |-> 11, last |-> <<>>, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0, ptr |-> 0, base |-> <<>>, results |-> <<>>, idx |-> 0]>> @@ <<1, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, recovering |-> FALSE, nonce |-> 0, reads |-> {}, n |-> 11, last |-> <<>>, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0, ptr |-> 0, base |-> <<>>, results |-> <<>>, idx |-> 0]>> @@ <<2, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, recovering |-> FALSE, nonce |-> 0, reads |-> {}, n |-> 11, last |-> <<>>, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0, ptr |-> 0, base |-> <<>>, results |-> <<>>, idx |-> 0]>> @@ <<2, 3>> :> <<>> @@ <<3, 1>> :> <<>> @@ <<3, 2>> :> <<>>))
        /\
        runs = ({})
    )
----

_init ==
    /\ decided_ballot = _TETrace[1].decided_ballot
    /\ accepted_idx = _TETrace[1].accepted_idx
    /\ runs = _TETrace[1].runs
    /\ buffered_stopsign = _TETrace[1].buffered_stopsign
    /\ force_single = _TETrace[1].force_single
    /\ old_decided_after_forced = _TETrace[1].old_decided_after_forced
    /\ lease_request = _TETrace[1].lease_request
    /\ deactivated = _TETrace[1].deactivated
    /\ cached_promise_message = _TETrace[1].cached_promise_message
    /\ repairs = _TETrace[1].repairs
    /\ in_txn = _TETrace[1].in_txn
    /\ old_decided_max = _TETrace[1].old_decided_max
    /\ role = _TETrace[1].role
    /\ proof = _TETrace[1].proof
    /\ reads = _TETrace[1].reads
    /\ n_accepted = _TETrace[1].n_accepted
    /\ lease_remaining = _TETrace[1].lease_remaining
    /\ node_epoch = _TETrace[1].node_epoch
    /\ forced_decided_at_force = _TETrace[1].forced_decided_at_force
    /\ timeouts = _TETrace[1].timeouts
    /\ rolled_back = _TETrace[1].rolled_back
    /\ crashes = _TETrace[1].crashes
    /\ open_mark = _TETrace[1].open_mark
    /\ past_durable = _TETrace[1].past_durable
    /\ halts = _TETrace[1].halts
    /\ stage1_ghost = _TETrace[1].stage1_ghost
    /\ answered = _TETrace[1].answered
    /\ mark_covered = _TETrace[1].mark_covered
    /\ applied = _TETrace[1].applied
    /\ power_loss_completed = _TETrace[1].power_loss_completed
    /\ elections = _TETrace[1].elections
    /\ phase = _TETrace[1].phase
    /\ wiped = _TETrace[1].wiped
    /\ duplicates = _TETrace[1].duplicates
    /\ selected = _TETrace[1].selected
    /\ wipes = _TETrace[1].wipes
    /\ reported = _TETrace[1].reported
    /\ read_round = _TETrace[1].read_round
    /\ counts = _TETrace[1].counts
    /\ wait_remaining = _TETrace[1].wait_remaining
    /\ chan = _TETrace[1].chan
    /\ read_pending = _TETrace[1].read_pending
    /\ decided_idx = _TETrace[1].decided_idx
    /\ verified_idx = _TETrace[1].verified_idx
    /\ storage_faults = _TETrace[1].storage_faults
    /\ forced_has_decided = _TETrace[1].forced_has_decided
    /\ flips = _TETrace[1].flips
    /\ build_source = _TETrace[1].build_source
    /\ recovery_responses = _TETrace[1].recovery_responses
    /\ running = _TETrace[1].running
    /\ read_only_entries = _TETrace[1].read_only_entries
    /\ peer_seen = _TETrace[1].peer_seen
    /\ recovering = _TETrace[1].recovering
    /\ lease_reply = _TETrace[1].lease_reply
    /\ wrong = _TETrace[1].wrong
    /\ witness_epoch = _TETrace[1].witness_epoch
    /\ losses = _TETrace[1].losses
    /\ decided_history = _TETrace[1].decided_history
    /\ first_log = _TETrace[1].first_log
    /\ current_seq_num = _TETrace[1].current_seq_num
    /\ refusal = _TETrace[1].refusal
    /\ leader_state = _TETrace[1].leader_state
    /\ buffered_proposals = _TETrace[1].buffered_proposals
    /\ result_throws = _TETrace[1].result_throws
    /\ promise = _TETrace[1].promise
    /\ nonce = _TETrace[1].nonce
    /\ reconfigurations = _TETrace[1].reconfigurations
    /\ refused = _TETrace[1].refused
    /\ log = _TETrace[1].log
    /\ client = _TETrace[1].client
    /\ proposed = _TETrace[1].proposed
    /\ high_water = _TETrace[1].high_water
    /\ read_only = _TETrace[1].read_only
    /\ own = _TETrace[1].own
----

_next ==
    /\ \E i,j \in DOMAIN _TETrace:
        /\ \/ /\ j = i + 1
              /\ i = TLCGet("level")
        /\ decided_ballot  = _TETrace[i].decided_ballot
        /\ decided_ballot' = _TETrace[j].decided_ballot
        /\ accepted_idx  = _TETrace[i].accepted_idx
        /\ accepted_idx' = _TETrace[j].accepted_idx
        /\ runs  = _TETrace[i].runs
        /\ runs' = _TETrace[j].runs
        /\ buffered_stopsign  = _TETrace[i].buffered_stopsign
        /\ buffered_stopsign' = _TETrace[j].buffered_stopsign
        /\ force_single  = _TETrace[i].force_single
        /\ force_single' = _TETrace[j].force_single
        /\ old_decided_after_forced  = _TETrace[i].old_decided_after_forced
        /\ old_decided_after_forced' = _TETrace[j].old_decided_after_forced
        /\ lease_request  = _TETrace[i].lease_request
        /\ lease_request' = _TETrace[j].lease_request
        /\ deactivated  = _TETrace[i].deactivated
        /\ deactivated' = _TETrace[j].deactivated
        /\ cached_promise_message  = _TETrace[i].cached_promise_message
        /\ cached_promise_message' = _TETrace[j].cached_promise_message
        /\ repairs  = _TETrace[i].repairs
        /\ repairs' = _TETrace[j].repairs
        /\ in_txn  = _TETrace[i].in_txn
        /\ in_txn' = _TETrace[j].in_txn
        /\ old_decided_max  = _TETrace[i].old_decided_max
        /\ old_decided_max' = _TETrace[j].old_decided_max
        /\ role  = _TETrace[i].role
        /\ role' = _TETrace[j].role
        /\ proof  = _TETrace[i].proof
        /\ proof' = _TETrace[j].proof
        /\ reads  = _TETrace[i].reads
        /\ reads' = _TETrace[j].reads
        /\ n_accepted  = _TETrace[i].n_accepted
        /\ n_accepted' = _TETrace[j].n_accepted
        /\ lease_remaining  = _TETrace[i].lease_remaining
        /\ lease_remaining' = _TETrace[j].lease_remaining
        /\ node_epoch  = _TETrace[i].node_epoch
        /\ node_epoch' = _TETrace[j].node_epoch
        /\ forced_decided_at_force  = _TETrace[i].forced_decided_at_force
        /\ forced_decided_at_force' = _TETrace[j].forced_decided_at_force
        /\ timeouts  = _TETrace[i].timeouts
        /\ timeouts' = _TETrace[j].timeouts
        /\ rolled_back  = _TETrace[i].rolled_back
        /\ rolled_back' = _TETrace[j].rolled_back
        /\ crashes  = _TETrace[i].crashes
        /\ crashes' = _TETrace[j].crashes
        /\ open_mark  = _TETrace[i].open_mark
        /\ open_mark' = _TETrace[j].open_mark
        /\ past_durable  = _TETrace[i].past_durable
        /\ past_durable' = _TETrace[j].past_durable
        /\ halts  = _TETrace[i].halts
        /\ halts' = _TETrace[j].halts
        /\ stage1_ghost  = _TETrace[i].stage1_ghost
        /\ stage1_ghost' = _TETrace[j].stage1_ghost
        /\ answered  = _TETrace[i].answered
        /\ answered' = _TETrace[j].answered
        /\ mark_covered  = _TETrace[i].mark_covered
        /\ mark_covered' = _TETrace[j].mark_covered
        /\ applied  = _TETrace[i].applied
        /\ applied' = _TETrace[j].applied
        /\ power_loss_completed  = _TETrace[i].power_loss_completed
        /\ power_loss_completed' = _TETrace[j].power_loss_completed
        /\ elections  = _TETrace[i].elections
        /\ elections' = _TETrace[j].elections
        /\ phase  = _TETrace[i].phase
        /\ phase' = _TETrace[j].phase
        /\ wiped  = _TETrace[i].wiped
        /\ wiped' = _TETrace[j].wiped
        /\ duplicates  = _TETrace[i].duplicates
        /\ duplicates' = _TETrace[j].duplicates
        /\ selected  = _TETrace[i].selected
        /\ selected' = _TETrace[j].selected
        /\ wipes  = _TETrace[i].wipes
        /\ wipes' = _TETrace[j].wipes
        /\ reported  = _TETrace[i].reported
        /\ reported' = _TETrace[j].reported
        /\ read_round  = _TETrace[i].read_round
        /\ read_round' = _TETrace[j].read_round
        /\ counts  = _TETrace[i].counts
        /\ counts' = _TETrace[j].counts
        /\ wait_remaining  = _TETrace[i].wait_remaining
        /\ wait_remaining' = _TETrace[j].wait_remaining
        /\ chan  = _TETrace[i].chan
        /\ chan' = _TETrace[j].chan
        /\ read_pending  = _TETrace[i].read_pending
        /\ read_pending' = _TETrace[j].read_pending
        /\ decided_idx  = _TETrace[i].decided_idx
        /\ decided_idx' = _TETrace[j].decided_idx
        /\ verified_idx  = _TETrace[i].verified_idx
        /\ verified_idx' = _TETrace[j].verified_idx
        /\ storage_faults  = _TETrace[i].storage_faults
        /\ storage_faults' = _TETrace[j].storage_faults
        /\ forced_has_decided  = _TETrace[i].forced_has_decided
        /\ forced_has_decided' = _TETrace[j].forced_has_decided
        /\ flips  = _TETrace[i].flips
        /\ flips' = _TETrace[j].flips
        /\ build_source  = _TETrace[i].build_source
        /\ build_source' = _TETrace[j].build_source
        /\ recovery_responses  = _TETrace[i].recovery_responses
        /\ recovery_responses' = _TETrace[j].recovery_responses
        /\ running  = _TETrace[i].running
        /\ running' = _TETrace[j].running
        /\ read_only_entries  = _TETrace[i].read_only_entries
        /\ read_only_entries' = _TETrace[j].read_only_entries
        /\ peer_seen  = _TETrace[i].peer_seen
        /\ peer_seen' = _TETrace[j].peer_seen
        /\ recovering  = _TETrace[i].recovering
        /\ recovering' = _TETrace[j].recovering
        /\ lease_reply  = _TETrace[i].lease_reply
        /\ lease_reply' = _TETrace[j].lease_reply
        /\ wrong  = _TETrace[i].wrong
        /\ wrong' = _TETrace[j].wrong
        /\ witness_epoch  = _TETrace[i].witness_epoch
        /\ witness_epoch' = _TETrace[j].witness_epoch
        /\ losses  = _TETrace[i].losses
        /\ losses' = _TETrace[j].losses
        /\ decided_history  = _TETrace[i].decided_history
        /\ decided_history' = _TETrace[j].decided_history
        /\ first_log  = _TETrace[i].first_log
        /\ first_log' = _TETrace[j].first_log
        /\ current_seq_num  = _TETrace[i].current_seq_num
        /\ current_seq_num' = _TETrace[j].current_seq_num
        /\ refusal  = _TETrace[i].refusal
        /\ refusal' = _TETrace[j].refusal
        /\ leader_state  = _TETrace[i].leader_state
        /\ leader_state' = _TETrace[j].leader_state
        /\ buffered_proposals  = _TETrace[i].buffered_proposals
        /\ buffered_proposals' = _TETrace[j].buffered_proposals
        /\ result_throws  = _TETrace[i].result_throws
        /\ result_throws' = _TETrace[j].result_throws
        /\ promise  = _TETrace[i].promise
        /\ promise' = _TETrace[j].promise
        /\ nonce  = _TETrace[i].nonce
        /\ nonce' = _TETrace[j].nonce
        /\ reconfigurations  = _TETrace[i].reconfigurations
        /\ reconfigurations' = _TETrace[j].reconfigurations
        /\ refused  = _TETrace[i].refused
        /\ refused' = _TETrace[j].refused
        /\ log  = _TETrace[i].log
        /\ log' = _TETrace[j].log
        /\ client  = _TETrace[i].client
        /\ client' = _TETrace[j].client
        /\ proposed  = _TETrace[i].proposed
        /\ proposed' = _TETrace[j].proposed
        /\ high_water  = _TETrace[i].high_water
        /\ high_water' = _TETrace[j].high_water
        /\ read_only  = _TETrace[i].read_only
        /\ read_only' = _TETrace[j].read_only
        /\ own  = _TETrace[i].own
        /\ own' = _TETrace[j].own

\* Uncomment the ASSUME below to write the states of the error trace
\* to the given file in Json format. Note that you can pass any tuple
\* to `JsonSerialize`. For example, a sub-sequence of _TETrace.
    \* ASSUME
    \*     LET J == INSTANCE Json
    \*         IN J!JsonSerialize("SequencePaxos_TTrace_1794523212.json", _TETrace)

=============================================================================

 Note that you can extract this module `SequencePaxos_TEExpression`
  to a dedicated file to reuse `expression` (the module in the 
  dedicated `SequencePaxos_TEExpression.tla` file takes precedence 
  over the module `SequencePaxos_TEExpression` below).

---- MODULE SequencePaxos_TEExpression ----
EXTENDS Sequences, TLCExt, Toolbox, SequencePaxos, Naturals, TLC, SequencePaxos_TEConstants

expression == 
    [
        \* To hide variables of the `SequencePaxos` spec from the error trace,
        \* remove the variables below.  The trace will be written in the order
        \* of the fields of this record.
        decided_ballot |-> decided_ballot
        ,accepted_idx |-> accepted_idx
        ,runs |-> runs
        ,buffered_stopsign |-> buffered_stopsign
        ,force_single |-> force_single
        ,old_decided_after_forced |-> old_decided_after_forced
        ,lease_request |-> lease_request
        ,deactivated |-> deactivated
        ,cached_promise_message |-> cached_promise_message
        ,repairs |-> repairs
        ,in_txn |-> in_txn
        ,old_decided_max |-> old_decided_max
        ,role |-> role
        ,proof |-> proof
        ,reads |-> reads
        ,n_accepted |-> n_accepted
        ,lease_remaining |-> lease_remaining
        ,node_epoch |-> node_epoch
        ,forced_decided_at_force |-> forced_decided_at_force
        ,timeouts |-> timeouts
        ,rolled_back |-> rolled_back
        ,crashes |-> crashes
        ,open_mark |-> open_mark
        ,past_durable |-> past_durable
        ,halts |-> halts
        ,stage1_ghost |-> stage1_ghost
        ,answered |-> answered
        ,mark_covered |-> mark_covered
        ,applied |-> applied
        ,power_loss_completed |-> power_loss_completed
        ,elections |-> elections
        ,phase |-> phase
        ,wiped |-> wiped
        ,duplicates |-> duplicates
        ,selected |-> selected
        ,wipes |-> wipes
        ,reported |-> reported
        ,read_round |-> read_round
        ,counts |-> counts
        ,wait_remaining |-> wait_remaining
        ,chan |-> chan
        ,read_pending |-> read_pending
        ,decided_idx |-> decided_idx
        ,verified_idx |-> verified_idx
        ,storage_faults |-> storage_faults
        ,forced_has_decided |-> forced_has_decided
        ,flips |-> flips
        ,build_source |-> build_source
        ,recovery_responses |-> recovery_responses
        ,running |-> running
        ,read_only_entries |-> read_only_entries
        ,peer_seen |-> peer_seen
        ,recovering |-> recovering
        ,lease_reply |-> lease_reply
        ,wrong |-> wrong
        ,witness_epoch |-> witness_epoch
        ,losses |-> losses
        ,decided_history |-> decided_history
        ,first_log |-> first_log
        ,current_seq_num |-> current_seq_num
        ,refusal |-> refusal
        ,leader_state |-> leader_state
        ,buffered_proposals |-> buffered_proposals
        ,result_throws |-> result_throws
        ,promise |-> promise
        ,nonce |-> nonce
        ,reconfigurations |-> reconfigurations
        ,refused |-> refused
        ,log |-> log
        ,client |-> client
        ,proposed |-> proposed
        ,high_water |-> high_water
        ,read_only |-> read_only
        ,own |-> own
        
        \* Put additional constant-, state-, and action-level expressions here:
        \* ,_stateNumber |-> _TEPosition
        \* ,_decided_ballotUnchanged |-> decided_ballot = decided_ballot'
        
        \* Format the `decided_ballot` variable as Json value.
        \* ,_decided_ballotJson |->
        \*     LET J == INSTANCE Json
        \*     IN J!ToJson(decided_ballot)
        
        \* Lastly, you may build expressions over arbitrary sets of states by
        \* leveraging the _TETrace operator.  For example, this is how to
        \* count the number of times a spec variable changed up to the current
        \* state in the trace.
        \* ,_decided_ballotModCount |->
        \*     LET F[s \in DOMAIN _TETrace] ==
        \*         IF s = 1 THEN 0
        \*         ELSE IF _TETrace[s].decided_ballot # _TETrace[s-1].decided_ballot
        \*             THEN 1 + F[s-1] ELSE F[s-1]
        \*     IN F[_TEPosition - 1]
    ]

=============================================================================



Parsing and semantic processing can take forever if the trace below is long.
 In this case, it is advised to uncomment the module below to deserialize the
 trace from a generated binary file.

\*
\*---- MODULE SequencePaxos_TETrace ----
\*EXTENDS IOUtils, SequencePaxos, TLC, SequencePaxos_TEConstants
\*
\*trace == IODeserialize("SequencePaxos_TTrace_1794523212.bin", TRUE)
\*
\*=============================================================================
\*

---- MODULE SequencePaxos_TETrace ----
EXTENDS SequencePaxos, TLC, SequencePaxos_TEConstants

trace == 
    <<
    ([open_mark |-> <<0, 0, 0>>,counts |-> <<[ans |-> <<[ptr |-> -1, map |-> <<>>], [ptr |-> -1, map |-> <<>>], [ptr |-> -1, map |-> <<>>]>>, tried |-> {}, again |-> {}, refusals |-> {}, appended |-> {}], [ans |-> <<[ptr |-> -1, map |-> <<>>], [ptr |-> -1, map |-> <<>>], [ptr |-> -1, map |-> <<>>]>>, tried |-> {}, again |-> {}, refusals |-> {}, appended |-> {}], [ans |-> <<[ptr |-> -1, map |-> <<>>], [ptr |-> -1, map |-> <<>>], [ptr |-> -1, map |-> <<>>]>>, tried |-> {}, again |-> {}, refusals |-> {}, appended |-> {}]>>,force_single |-> [log |-> <<>>, decided |-> <<>>, node |-> 0, history |-> <<>>],own |-> <<<<>>, <<>>, <<>>>>,timeouts |-> 0,lease_remaining |-> <<0, 0, 0>>,promise |-> <<0, 0, 0>>,crashes |-> 0,losses |-> 0,deactivated |-> <<FALSE, FALSE, FALSE>>,read_only |-> <<FALSE, FALSE, FALSE>>,rolled_back |-> {},proof |-> <<{}, {}, {}>>,selected |-> <<<<>>, <<>>, <<>>>>,accepted_idx |-> <<0, 0, 0>>,phase |-> <<"None", "None", "None">>,duplicates |-> 0,read_round |-> <<[reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}]>>,read_pending |-> <<{}, {}, {}>>,elections |-> 0,peer_seen |-> <<<<[n |-> 0, idx |-> 0], [n |-> 0, idx |-> 0], [n |-> 0, idx |-> 0]>>, <<[n |-> 0, idx |-> 0], [n |-> 0, idx |-> 0], [n |-> 0, idx |-> 0]>>, <<[n |-> 0, idx |-> 0], [n |-> 0, idx |-> 0], [n |-> 0, idx |-> 0]>>>>,nonce |-> <<0, 0, 0>>,old_decided_after_forced |-> FALSE,repairs |-> <<0, 0, 0>>,wipes |-> 0,proposed |-> {},halts |-> 0,in_txn |-> <<<<>>, <<>>, <<>>>>,lease_reply |-> <<-1, -1, -1>>,cached_promise_message |-> <<<<>>, <<>>, <<>>>>,recovery_responses |-> <<{}, {}, {}>>,buffered_stopsign |-> <<<<>>, <<>>, <<>>>>,recovering |-> <<FALSE, FALSE, FALSE>>,role |-> <<"Follower", "Follower", "Follower">>,wiped |-> <<FALSE, FALSE, FALSE>>,applied |-> <<<<>>, <<>>, <<>>>>,log |-> <<<<>>, <<>>, <<>>>>,first_log |-> <<<<>>, <<>>, <<>>>>,decided_history |-> <<>>,lease_request |-> <<-1, -1, -1>>,storage_faults |-> 0,running |-> {1, 2, 3},current_seq_num |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>,past_durable |-> <<{}, {}, {}>>,verified_idx |-> <<0, 0, 0>>,reported |-> {},power_loss_completed |-> {},forced_has_decided |-> FALSE,client |-> [session |-> 0, request_number |-> 1, sends |-> 0],n_accepted |-> <<0, 0, 0>>,result_throws |-> <<>>,decided_idx |-> <<0, 0, 0>>,answered |-> <<[n |-> -1, ptr |-> -1], [n |-> -1, ptr |-> -1], [n |-> -1, ptr |-> -1]>>,leader_state |-> <<[n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised", last |-> <<>>], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised", last |-> <<>>], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised", last |-> <<>>]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, last |-> <<>>, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised", last |-> <<>>], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised", last |-> <<>>], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised", last |-> <<>>]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, last |-> <<>>, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised", last |-> <<>>], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised", last |-> <<>>], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised", last |-> <<>>]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, last |-> <<>>, pid |-> 0], max_promise_sync |-> <<>>]>>,refused |-> {},high_water |-> <<[promise |-> 0, n_accepted |-> 0, acc |-> 0, decided |-> 0], [promise |-> 0, n_accepted |-> 0, acc |-> 0, decided |-> 0], [promise |-> 0, n_accepted |-> 0, acc |-> 0, decided |-> 0]>>,buffered_proposals |-> <<<<>>, <<>>, <<>>>>,reads |-> <<>>,stage1_ghost |-> <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>,refusal |-> <<[n |-> -1, idx |-> 0], [n |-> -1, idx |-> 0], [n |-> -1, idx |-> 0]>>,decided_ballot |-> <<0>>,node_epoch |-> <<0, 0, 0>>,witness_epoch |-> 0,forced_decided_at_force |-> 0,old_decided_max |-> 0,mark_covered |-> <<FALSE, FALSE, FALSE>>,read_only_entries |-> 0,wrong |-> {},reconfigurations |-> 0,flips |-> {},build_source |-> <<>>,wait_remaining |-> -1,chan |-> (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<>> @@ <<2, 1>> :> <<>> @@ <<2, 3>> :> <<>> @@ <<3, 1>> :> <<>> @@ <<3, 2>> :> <<>>),runs |-> {}]),
    ([open_mark |-> <<0, 0, 0>>,counts |-> <<[ans |-> <<[ptr |-> -1, map |-> <<>>], [ptr |-> -1, map |-> <<>>], [ptr |-> -1, map |-> <<>>]>>, tried |-> {}, again |-> {}, refusals |-> {}, appended |-> {}], [ans |-> <<[ptr |-> -1, map |-> <<>>], [ptr |-> -1, map |-> <<>>], [ptr |-> -1, map |-> <<>>]>>, tried |-> {}, again |-> {}, refusals |-> {}, appended |-> {}], [ans |-> <<[ptr |-> -1, map |-> <<>>], [ptr |-> -1, map |-> <<>>], [ptr |-> -1, map |-> <<>>]>>, tried |-> {}, again |-> {}, refusals |-> {}, appended |-> {}]>>,force_single |-> [log |-> <<>>, decided |-> <<>>, node |-> 0, history |-> <<>>],own |-> <<<<>>, <<>>, <<>>>>,timeouts |-> 0,lease_remaining |-> <<0, 0, 0>>,promise |-> <<11, 0, 0>>,crashes |-> 0,losses |-> 0,deactivated |-> <<FALSE, FALSE, FALSE>>,read_only |-> <<FALSE, FALSE, FALSE>>,rolled_back |-> {},proof |-> <<{}, {}, {}>>,selected |-> <<<<>>, <<>>, <<>>>>,accepted_idx |-> <<0, 0, 0>>,phase |-> <<"Prepare", "None", "None">>,duplicates |-> 0,read_round |-> <<[reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}]>>,read_pending |-> <<{}, {}, {}>>,elections |-> 1,peer_seen |-> <<<<[n |-> 0, idx |-> 0], [n |-> 0, idx |-> 0], [n |-> 0, idx |-> 0]>>, <<[n |-> 0, idx |-> 0], [n |-> 0, idx |-> 0], [n |-> 0, idx |-> 0]>>, <<[n |-> 0, idx |-> 0], [n |-> 0, idx |-> 0], [n |-> 0, idx |-> 0]>>>>,nonce |-> <<0, 0, 0>>,old_decided_after_forced |-> FALSE,repairs |-> <<0, 0, 0>>,wipes |-> 0,proposed |-> {},halts |-> 0,in_txn |-> <<<<>>, <<>>, <<>>>>,lease_reply |-> <<-1, -1, -1>>,cached_promise_message |-> <<<<>>, <<>>, <<>>>>,recovery_responses |-> <<{}, {}, {}>>,buffered_stopsign |-> <<<<>>, <<>>, <<>>>>,recovering |-> <<FALSE, FALSE, FALSE>>,role |-> <<"Leader", "Follower", "Follower">>,wiped |-> <<FALSE, FALSE, FALSE>>,applied |-> <<<<>>, <<>>, <<>>>>,log |-> <<<<>>, <<>>, <<>>>>,first_log |-> <<<<>>, <<>>, <<>>>>,decided_history |-> <<>>,lease_request |-> <<-1, -1, -1>>,storage_faults |-> 0,running |-> {1, 2, 3},current_seq_num |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>,past_durable |-> <<{}, {}, {}>>,verified_idx |-> <<0, 0, 0>>,reported |-> {},power_loss_completed |-> {},forced_has_decided |-> FALSE,client |-> [session |-> 0, request_number |-> 1, sends |-> 0],n_accepted |-> <<0, 0, 0>>,result_throws |-> <<>>,decided_idx |-> <<0, 0, 0>>,answered |-> <<[n |-> -1, ptr |-> -1], [n |-> -1, ptr |-> -1], [n |-> -1, ptr |-> -1]>>,leader_state |-> <<[n_leader |-> 11, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised", last |-> <<>>], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised", last |-> <<>>], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised", last |-> <<>>]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, last |-> <<>>, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised", last |-> <<>>], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised", last |-> <<>>], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised", last |-> <<>>]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, last |-> <<>>, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised", last |-> <<>>], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised", last |-> <<>>], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised", last |-> <<>>]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, last |-> <<>>, pid |-> 0], max_promise_sync |-> <<>>]>>,refused |-> {},high_water |-> <<[promise |-> 0, n_accepted |-> 0, acc |-> 0, decided |-> 0], [promise |-> 0, n_accepted |-> 0, acc |-> 0, decided |-> 0], [promise |-> 0, n_accepted |-> 0, acc |-> 0, decided |-> 0]>>,buffered_proposals |-> <<<<>>, <<>>, <<>>>>,reads |-> <<>>,stage1_ghost |-> <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>,refusal |-> <<[n |-> -1, idx |-> 0], [n |-> -1, idx |-> 0], [n |-> -1, idx |-> 0]>>,decided_ballot |-> <<0>>,node_epoch |-> <<0, 0, 0>>,witness_epoch |-> 0,forced_decided_at_force |-> 0,old_decided_max |-> 0,mark_covered |-> <<FALSE, FALSE, FALSE>>,read_only_entries |-> 0,wrong |-> {},reconfigurations |-> 0,flips |-> {},build_source |-> <<>>,wait_remaining |-> -1,chan |-> (<<1, 2>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, recovering |-> FALSE, nonce |-> 0, reads |-> {}, n |-> 11, last |-> <<>>, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0, ptr |-> 0, base |-> <<>>, results |-> <<>>, idx |-> 0]>> @@ <<1, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, recovering |-> FALSE, nonce |-> 0, reads |-> {}, n |-> 11, last |-> <<>>, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0, ptr |-> 0, base |-> <<>>, results |-> <<>>, idx |-> 0]>> @@ <<2, 1>> :> <<>> @@ <<2, 3>> :> <<>> @@ <<3, 1>> :> <<>> @@ <<3, 2>> :> <<>>),runs |-> {}]),
    ([open_mark |-> <<0, 0, 0>>,counts |-> <<[ans |-> <<[ptr |-> -1, map |-> <<>>], [ptr |-> -1, map |-> <<>>], [ptr |-> -1, map |-> <<>>]>>, tried |-> {}, again |-> {}, refusals |-> {}, appended |-> {}], [ans |-> <<[ptr |-> -1, map |-> <<>>], [ptr |-> -1, map |-> <<>>], [ptr |-> -1, map |-> <<>>]>>, tried |-> {}, again |-> {}, refusals |-> {}, appended |-> {}], [ans |-> <<[ptr |-> -1, map |-> <<>>], [ptr |-> -1, map |-> <<>>], [ptr |-> -1, map |-> <<>>]>>, tried |-> {}, again |-> {}, refusals |-> {}, appended |-> {}]>>,force_single |-> [log |-> <<>>, decided |-> <<>>, node |-> 0, history |-> <<>>],own |-> <<<<>>, <<>>, <<>>>>,timeouts |-> 0,lease_remaining |-> <<0, 0, 0>>,promise |-> <<11, 0, 0>>,crashes |-> 0,losses |-> 0,deactivated |-> <<FALSE, FALSE, FALSE>>,read_only |-> <<FALSE, FALSE, FALSE>>,rolled_back |-> {},proof |-> <<{}, {}, {}>>,selected |-> <<<<>>, <<>>, <<>>>>,accepted_idx |-> <<0, 0, 0>>,phase |-> <<"Prepare", "None", "None">>,duplicates |-> 1,read_round |-> <<[reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}]>>,read_pending |-> <<{}, {}, {}>>,elections |-> 1,peer_seen |-> <<<<[n |-> 0, idx |-> 0], [n |-> 0, idx |-> 0], [n |-> 0, idx |-> 0]>>, <<[n |-> 0, idx |-> 0], [n |-> 0, idx |-> 0], [n |-> 0, idx |-> 0]>>, <<[n |-> 0, idx |-> 0], [n |-> 0, idx |-> 0], [n |-> 0, idx |-> 0]>>>>,nonce |-> <<0, 0, 0>>,old_decided_after_forced |-> FALSE,repairs |-> <<0, 0, 0>>,wipes |-> 0,proposed |-> {},halts |-> 0,in_txn |-> <<<<>>, <<>>, <<>>>>,lease_reply |-> <<-1, -1, -1>>,cached_promise_message |-> <<<<>>, <<>>, <<>>>>,recovery_responses |-> <<{}, {}, {}>>,buffered_stopsign |-> <<<<>>, <<>>, <<>>>>,recovering |-> <<FALSE, FALSE, FALSE>>,role |-> <<"Leader", "Follower", "Follower">>,wiped |-> <<FALSE, FALSE, FALSE>>,applied |-> <<<<>>, <<>>, <<>>>>,log |-> <<<<>>, <<>>, <<>>>>,first_log |-> <<<<>>, <<>>, <<>>>>,decided_history |-> <<>>,lease_request |-> <<-1, -1, -1>>,storage_faults |-> 0,running |-> {1, 2, 3},current_seq_num |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>,past_durable |-> <<{}, {}, {}>>,verified_idx |-> <<0, 0, 0>>,reported |-> {},power_loss_completed |-> {},forced_has_decided |-> FALSE,client |-> [session |-> 0, request_number |-> 1, sends |-> 0],n_accepted |-> <<0, 0, 0>>,result_throws |-> <<>>,decided_idx |-> <<0, 0, 0>>,answered |-> <<[n |-> -1, ptr |-> -1], [n |-> -1, ptr |-> -1], [n |-> -1, ptr |-> -1]>>,leader_state |-> <<[n_leader |-> 11, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised", last |-> <<>>], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised", last |-> <<>>], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised", last |-> <<>>]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, last |-> <<>>, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised", last |-> <<>>], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised", last |-> <<>>], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised", last |-> <<>>]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, last |-> <<>>, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised", last |-> <<>>], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised", last |-> <<>>], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised", last |-> <<>>]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, last |-> <<>>, pid |-> 0], max_promise_sync |-> <<>>]>>,refused |-> {},high_water |-> <<[promise |-> 0, n_accepted |-> 0, acc |-> 0, decided |-> 0], [promise |-> 0, n_accepted |-> 0, acc |-> 0, decided |-> 0], [promise |-> 0, n_accepted |-> 0, acc |-> 0, decided |-> 0]>>,buffered_proposals |-> <<<<>>, <<>>, <<>>>>,reads |-> <<>>,stage1_ghost |-> <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>,refusal |-> <<[n |-> -1, idx |-> 0], [n |-> -1, idx |-> 0], [n |-> -1, idx |-> 0]>>,decided_ballot |-> <<0>>,node_epoch |-> <<0, 0, 0>>,witness_epoch |-> 0,forced_decided_at_force |-> 0,old_decided_max |-> 0,mark_covered |-> <<FALSE, FALSE, FALSE>>,read_only_entries |-> 0,wrong |-> {},reconfigurations |-> 0,flips |-> {},build_source |-> <<>>,wait_remaining |-> -1,chan |-> (<<1, 2>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, recovering |-> FALSE, nonce |-> 0, reads |-> {}, n |-> 11, last |-> <<>>, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0, ptr |-> 0, base |-> <<>>, results |-> <<>>, idx |-> 0], [promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, recovering |-> FALSE, nonce |-> 0, reads |-> {}, n |-> 11, last |-> <<>>, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0, ptr |-> 0, base |-> <<>>, results |-> <<>>, idx |-> 0]>> @@ <<1, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, recovering |-> FALSE, nonce |-> 0, reads |-> {}, n |-> 11, last |-> <<>>, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0, ptr |-> 0, base |-> <<>>, results |-> <<>>, idx |-> 0]>> @@ <<2, 1>> :> <<>> @@ <<2, 3>> :> <<>> @@ <<3, 1>> :> <<>> @@ <<3, 2>> :> <<>>),runs |-> {}]),
    ([open_mark |-> <<0, 0, 0>>,counts |-> <<[ans |-> <<[ptr |-> -1, map |-> <<>>], [ptr |-> -1, map |-> <<>>], [ptr |-> -1, map |-> <<>>]>>, tried |-> {}, again |-> {}, refusals |-> {}, appended |-> {}], [ans |-> <<[ptr |-> -1, map |-> <<>>], [ptr |-> -1, map |-> <<>>], [ptr |-> -1, map |-> <<>>]>>, tried |-> {}, again |-> {}, refusals |-> {}, appended |-> {}], [ans |-> <<[ptr |-> -1, map |-> <<>>], [ptr |-> -1, map |-> <<>>], [ptr |-> -1, map |-> <<>>]>>, tried |-> {}, again |-> {}, refusals |-> {}, appended |-> {}]>>,force_single |-> [log |-> <<>>, decided |-> <<>>, node |-> 0, history |-> <<>>],own |-> <<<<>>, <<>>, <<>>>>,timeouts |-> 0,lease_remaining |-> <<0, 0, 0>>,promise |-> <<11, 11, 0>>,crashes |-> 0,losses |-> 0,deactivated |-> <<FALSE, FALSE, FALSE>>,read_only |-> <<FALSE, FALSE, FALSE>>,rolled_back |-> {},proof |-> <<{}, {}, {}>>,selected |-> <<<<>>, <<>>, <<>>>>,accepted_idx |-> <<0, 0, 0>>,phase |-> <<"Prepare", "Prepare", "None">>,duplicates |-> 1,read_round |-> <<[reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}]>>,read_pending |-> <<{}, {}, {}>>,elections |-> 1,peer_seen |-> <<<<[n |-> 0, idx |-> 0], [n |-> 0, idx |-> 0], [n |-> 0, idx |-> 0]>>, <<[n |-> 0, idx |-> 0], [n |-> 0, idx |-> 0], [n |-> 0, idx |-> 0]>>, <<[n |-> 0, idx |-> 0], [n |-> 0, idx |-> 0], [n |-> 0, idx |-> 0]>>>>,nonce |-> <<0, 0, 0>>,old_decided_after_forced |-> FALSE,repairs |-> <<0, 0, 0>>,wipes |-> 0,proposed |-> {},halts |-> 0,in_txn |-> <<<<>>, <<>>, <<>>>>,lease_reply |-> <<-1, -1, -1>>,cached_promise_message |-> <<<<>>, <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, recovering |-> FALSE, nonce |-> 0, reads |-> {}, n |-> 11, last |-> <<>>, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0, ptr |-> 0, base |-> <<>>, results |-> <<>>, idx |-> 0]>>, <<>>>>,recovery_responses |-> <<{}, {}, {}>>,buffered_stopsign |-> <<<<>>, <<>>, <<>>>>,recovering |-> <<FALSE, FALSE, FALSE>>,role |-> <<"Leader", "Follower", "Follower">>,wiped |-> <<FALSE, FALSE, FALSE>>,applied |-> <<<<>>, <<>>, <<>>>>,log |-> <<<<>>, <<>>, <<>>>>,first_log |-> <<<<>>, <<>>, <<>>>>,decided_history |-> <<>>,lease_request |-> <<-1, -1, -1>>,storage_faults |-> 0,running |-> {1, 2, 3},current_seq_num |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>,past_durable |-> <<{}, {}, {}>>,verified_idx |-> <<0, 0, 0>>,reported |-> {},power_loss_completed |-> {},forced_has_decided |-> FALSE,client |-> [session |-> 0, request_number |-> 1, sends |-> 0],n_accepted |-> <<0, 0, 0>>,result_throws |-> <<>>,decided_idx |-> <<0, 0, 0>>,answered |-> <<[n |-> -1, ptr |-> -1], [n |-> -1, ptr |-> -1], [n |-> -1, ptr |-> -1]>>,leader_state |-> <<[n_leader |-> 11, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "Promised", last |-> <<>>], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised", last |-> <<>>], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised", last |-> <<>>]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, last |-> <<>>, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised", last |-> <<>>], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised", last |-> <<>>], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised", last |-> <<>>]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, last |-> <<>>, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised", last |-> <<>>], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised", last |-> <<>>], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised", last |-> <<>>]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, last |-> <<>>, pid |-> 0], max_promise_sync |-> <<>>]>>,refused |-> {},high_water |-> <<[promise |-> 0, n_accepted |-> 0, acc |-> 0, decided |-> 0], [promise |-> 0, n_accepted |-> 0, acc |-> 0, decided |-> 0], [promise |-> 0, n_accepted |-> 0, acc |-> 0, decided |-> 0]>>,buffered_proposals |-> <<<<>>, <<>>, <<>>>>,reads |-> <<>>,stage1_ghost |-> <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>,refusal |-> <<[n |-> -1, idx |-> 0], [n |-> -1, idx |-> 0], [n |-> -1, idx |-> 0]>>,decided_ballot |-> <<0>>,node_epoch |-> <<0, 0, 0>>,witness_epoch |-> 0,forced_decided_at_force |-> 0,old_decided_max |-> 0,mark_covered |-> <<FALSE, FALSE, FALSE>>,read_only_entries |-> 0,wrong |-> {},reconfigurations |-> 0,flips |-> {},build_source |-> <<>>,wait_remaining |-> -1,chan |-> (<<1, 2>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, recovering |-> FALSE, nonce |-> 0, reads |-> {}, n |-> 11, last |-> <<>>, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0, ptr |-> 0, base |-> <<>>, results |-> <<>>, idx |-> 0]>> @@ <<1, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, recovering |-> FALSE, nonce |-> 0, reads |-> {}, n |-> 11, last |-> <<>>, type |-> "Prepare", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0, ptr |-> 0, base |-> <<>>, results |-> <<>>, idx |-> 0]>> @@ <<2, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, recovering |-> FALSE, nonce |-> 0, reads |-> {}, n |-> 11, last |-> <<>>, type |-> "Promise", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0, ptr |-> 0, base |-> <<>>, results |-> <<>>, idx |-> 0]>> @@ <<2, 3>> :> <<>> @@ <<3, 1>> :> <<>> @@ <<3, 2>> :> <<>>),runs |-> {}])
    >>
----


=============================================================================

---- MODULE SequencePaxos_TEConstants ----
EXTENDS SequencePaxos

CONSTANTS v1, v2

=============================================================================

---- CONFIG SequencePaxos_TTrace_1794523212 ----
CONSTANTS
    Nodes = { 1 , 2 , 3 }
    Values = { v1 , v2 }
    MaxBallot = 1
    MaxLog = 1
    MaxCounter = 3
    MaxSession = 2
    Cap = 2
    MaxCrash = 0
    MaxWipe = 0
    MaxLoss = 0
    MaxDuplicate = 1
    MaxTimeout = 0
    MaxElection = 2
    RuleR = TRUE
    DecidedIdxMonotone = FALSE
    MaxRollback = 0
    Recovery = FALSE
    PowerLoss = FALSE
    Mutation = "none"
    MaxStorageFault = 0
    StartChecks = TRUE
    RunningFaults = FALSE
    StepChecks = TRUE
    VerifyCrc = FALSE
    ChainCheck = FALSE
    MetaRecord = FALSE
    PeerCheck = FALSE
    HighWater = FALSE
    VerifyPromise = FALSE
    MismatchAction = "keep"
    StateInStorage = FALSE
    MaxInFlight = 0
    StateRepair = "copy"
    StartDamageCounts = FALSE
    RepairFromPeer = FALSE
    StorageFaultKind = { }
    ClientSessions = FALSE
    MaxClientSend = 0
    MaxRequest = 0
    ReadIndex = FALSE
    MaxRead = 0
    NextConfigNodes = { }
    MaxReconfigure = 0
    SingleNode = FALSE
    MaxForceSingle = 0
    MaxReadOnly = 0
    MaxHalt = 0
    StaleReadNode = FALSE
    Witness = FALSE
    LeaseLength = 0
    ExecuteVerify = FALSE
    MaxWrong = 0
    Spread = FALSE
    Flip = FALSE
    MaxFlip = 0
    MaxResultThrow = 0
    SpreadAction = "throw"
    v2 = v2
    v1 = v1

INVARIANT
    _inv

CHECK_DEADLOCK
    \* CHECK_DEADLOCK off because of PROPERTY or INVARIANT above.
    FALSE

INIT
    _init

NEXT
    _next

CONSTANT
    _TETrace <- _trace

ALIAS
    _expression
=============================================================================
\* Generated on Thu Nov 12 22:40:14 UTC 2026