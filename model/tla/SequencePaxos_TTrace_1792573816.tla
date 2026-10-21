---- MODULE SequencePaxos_TTrace_1792573816 ----
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
        force_single = ([log |-> <<>>, node |-> 0, decided |-> <<>>, history |-> <<>>])
        /\
        timeouts = (0)
        /\
        lease_remaining = (<<0, 0, 0>>)
        /\
        promise = (<<0, 0, 0>>)
        /\
        crashes = (3)
        /\
        losses = (0)
        /\
        deactivated = (<<FALSE, FALSE, FALSE>>)
        /\
        read_only = (<<FALSE, FALSE, FALSE>>)
        /\
        rolled_back = ({})
        /\
        accepted_idx = (<<0, 0, 0>>)
        /\
        phase = (<<"Recover", "Recover", "Recover">>)
        /\
        duplicates = (0)
        /\
        read_round = (<<[reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}]>>)
        /\
        read_pending = (<<{}, {}, {}>>)
        /\
        elections = (0)
        /\
        nonce = (<<1, 1, 1>>)
        /\
        old_decided_after_forced = (FALSE)
        /\
        wipes = (0)
        /\
        proposed = ({})
        /\
        halts = (0)
        /\
        lease_reply = (<<-1, -1, -1>>)
        /\
        cached_promise_message = (<<<<>>, <<>>, <<>>>>)
        /\
        recovery_responses = (<<{}, {}, {}>>)
        /\
        recovering = (<<FALSE, TRUE, TRUE>>)
        /\
        buffered_stopsign = (<<<<>>, <<>>, <<>>>>)
        /\
        wiped = (<<FALSE, FALSE, FALSE>>)
        /\
        role = (<<"Follower", "Follower", "Follower">>)
        /\
        log = (<<<<>>, <<>>, <<>>>>)
        /\
        first_log = (<<<<>>, <<>>, <<>>>>)
        /\
        decided_history = (<<>>)
        /\
        lease_request = (<<-1, -1, -1>>)
        /\
        running = ({1, 2, 3})
        /\
        current_seq_num = (<<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>)
        /\
        past_durable = (<<{}, {}, {}>>)
        /\
        power_loss_completed = ({1})
        /\
        client = ([session |-> 0, request_number |-> 1, sends |-> 0])
        /\
        forced_has_decided = (FALSE)
        /\
        n_accepted = (<<0, 0, 0>>)
        /\
        decided_idx = (<<0, 0, 0>>)
        /\
        leader_state = (<<[n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>]>>)
        /\
        buffered_proposals = (<<<<>>, <<>>, <<>>>>)
        /\
        stage1_ghost = (<<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>)
        /\
        reads = (<<>>)
        /\
        decided_ballot = (<<0>>)
        /\
        node_epoch = (<<0, 0, 0>>)
        /\
        witness_epoch = (0)
        /\
        forced_decided_at_force = (0)
        /\
        read_only_entries = (0)
        /\
        old_decided_max = (0)
        /\
        reconfigurations = (0)
        /\
        build_source = (<<>>)
        /\
        wait_remaining = (-1)
        /\
        chan = ((<<1, 2>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, recovering |-> FALSE, nonce |-> 0, reads |-> {}, n |-> 0, type |-> "PrepareReq", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<1, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, recovering |-> FALSE, nonce |-> 0, reads |-> {}, n |-> 0, type |-> "PrepareReq", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, recovering |-> FALSE, nonce |-> 1, reads |-> {}, n |-> 0, type |-> "Recovery", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, recovering |-> FALSE, nonce |-> 1, reads |-> {}, n |-> 0, type |-> "Recovery", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, recovering |-> FALSE, nonce |-> 1, reads |-> {}, n |-> 0, type |-> "Recovery", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 2>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, recovering |-> FALSE, nonce |-> 1, reads |-> {}, n |-> 0, type |-> "Recovery", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>>))
    )
----

_init ==
    /\ decided_ballot = _TETrace[1].decided_ballot
    /\ wiped = _TETrace[1].wiped
    /\ lease_remaining = _TETrace[1].lease_remaining
    /\ promise = _TETrace[1].promise
    /\ node_epoch = _TETrace[1].node_epoch
    /\ read_only = _TETrace[1].read_only
    /\ wipes = _TETrace[1].wipes
    /\ accepted_idx = _TETrace[1].accepted_idx
    /\ stage1_ghost = _TETrace[1].stage1_ghost
    /\ recovery_responses = _TETrace[1].recovery_responses
    /\ old_decided_after_forced = _TETrace[1].old_decided_after_forced
    /\ read_pending = _TETrace[1].read_pending
    /\ read_round = _TETrace[1].read_round
    /\ buffered_proposals = _TETrace[1].buffered_proposals
    /\ read_only_entries = _TETrace[1].read_only_entries
    /\ force_single = _TETrace[1].force_single
    /\ recovering = _TETrace[1].recovering
    /\ decided_idx = _TETrace[1].decided_idx
    /\ build_source = _TETrace[1].build_source
    /\ role = _TETrace[1].role
    /\ power_loss_completed = _TETrace[1].power_loss_completed
    /\ phase = _TETrace[1].phase
    /\ reads = _TETrace[1].reads
    /\ old_decided_max = _TETrace[1].old_decided_max
    /\ running = _TETrace[1].running
    /\ crashes = _TETrace[1].crashes
    /\ current_seq_num = _TETrace[1].current_seq_num
    /\ proposed = _TETrace[1].proposed
    /\ forced_decided_at_force = _TETrace[1].forced_decided_at_force
    /\ chan = _TETrace[1].chan
    /\ client = _TETrace[1].client
    /\ past_durable = _TETrace[1].past_durable
    /\ buffered_stopsign = _TETrace[1].buffered_stopsign
    /\ reconfigurations = _TETrace[1].reconfigurations
    /\ lease_request = _TETrace[1].lease_request
    /\ decided_history = _TETrace[1].decided_history
    /\ losses = _TETrace[1].losses
    /\ witness_epoch = _TETrace[1].witness_epoch
    /\ forced_has_decided = _TETrace[1].forced_has_decided
    /\ deactivated = _TETrace[1].deactivated
    /\ first_log = _TETrace[1].first_log
    /\ wait_remaining = _TETrace[1].wait_remaining
    /\ timeouts = _TETrace[1].timeouts
    /\ duplicates = _TETrace[1].duplicates
    /\ elections = _TETrace[1].elections
    /\ leader_state = _TETrace[1].leader_state
    /\ rolled_back = _TETrace[1].rolled_back
    /\ lease_reply = _TETrace[1].lease_reply
    /\ nonce = _TETrace[1].nonce
    /\ n_accepted = _TETrace[1].n_accepted
    /\ halts = _TETrace[1].halts
    /\ log = _TETrace[1].log
    /\ cached_promise_message = _TETrace[1].cached_promise_message
----

_next ==
    /\ \E i,j \in DOMAIN _TETrace:
        /\ \/ /\ j = i + 1
              /\ i = TLCGet("level")
        /\ decided_ballot  = _TETrace[i].decided_ballot
        /\ decided_ballot' = _TETrace[j].decided_ballot
        /\ wiped  = _TETrace[i].wiped
        /\ wiped' = _TETrace[j].wiped
        /\ lease_remaining  = _TETrace[i].lease_remaining
        /\ lease_remaining' = _TETrace[j].lease_remaining
        /\ promise  = _TETrace[i].promise
        /\ promise' = _TETrace[j].promise
        /\ node_epoch  = _TETrace[i].node_epoch
        /\ node_epoch' = _TETrace[j].node_epoch
        /\ read_only  = _TETrace[i].read_only
        /\ read_only' = _TETrace[j].read_only
        /\ wipes  = _TETrace[i].wipes
        /\ wipes' = _TETrace[j].wipes
        /\ accepted_idx  = _TETrace[i].accepted_idx
        /\ accepted_idx' = _TETrace[j].accepted_idx
        /\ stage1_ghost  = _TETrace[i].stage1_ghost
        /\ stage1_ghost' = _TETrace[j].stage1_ghost
        /\ recovery_responses  = _TETrace[i].recovery_responses
        /\ recovery_responses' = _TETrace[j].recovery_responses
        /\ old_decided_after_forced  = _TETrace[i].old_decided_after_forced
        /\ old_decided_after_forced' = _TETrace[j].old_decided_after_forced
        /\ read_pending  = _TETrace[i].read_pending
        /\ read_pending' = _TETrace[j].read_pending
        /\ read_round  = _TETrace[i].read_round
        /\ read_round' = _TETrace[j].read_round
        /\ buffered_proposals  = _TETrace[i].buffered_proposals
        /\ buffered_proposals' = _TETrace[j].buffered_proposals
        /\ read_only_entries  = _TETrace[i].read_only_entries
        /\ read_only_entries' = _TETrace[j].read_only_entries
        /\ force_single  = _TETrace[i].force_single
        /\ force_single' = _TETrace[j].force_single
        /\ recovering  = _TETrace[i].recovering
        /\ recovering' = _TETrace[j].recovering
        /\ decided_idx  = _TETrace[i].decided_idx
        /\ decided_idx' = _TETrace[j].decided_idx
        /\ build_source  = _TETrace[i].build_source
        /\ build_source' = _TETrace[j].build_source
        /\ role  = _TETrace[i].role
        /\ role' = _TETrace[j].role
        /\ power_loss_completed  = _TETrace[i].power_loss_completed
        /\ power_loss_completed' = _TETrace[j].power_loss_completed
        /\ phase  = _TETrace[i].phase
        /\ phase' = _TETrace[j].phase
        /\ reads  = _TETrace[i].reads
        /\ reads' = _TETrace[j].reads
        /\ old_decided_max  = _TETrace[i].old_decided_max
        /\ old_decided_max' = _TETrace[j].old_decided_max
        /\ running  = _TETrace[i].running
        /\ running' = _TETrace[j].running
        /\ crashes  = _TETrace[i].crashes
        /\ crashes' = _TETrace[j].crashes
        /\ current_seq_num  = _TETrace[i].current_seq_num
        /\ current_seq_num' = _TETrace[j].current_seq_num
        /\ proposed  = _TETrace[i].proposed
        /\ proposed' = _TETrace[j].proposed
        /\ forced_decided_at_force  = _TETrace[i].forced_decided_at_force
        /\ forced_decided_at_force' = _TETrace[j].forced_decided_at_force
        /\ chan  = _TETrace[i].chan
        /\ chan' = _TETrace[j].chan
        /\ client  = _TETrace[i].client
        /\ client' = _TETrace[j].client
        /\ past_durable  = _TETrace[i].past_durable
        /\ past_durable' = _TETrace[j].past_durable
        /\ buffered_stopsign  = _TETrace[i].buffered_stopsign
        /\ buffered_stopsign' = _TETrace[j].buffered_stopsign
        /\ reconfigurations  = _TETrace[i].reconfigurations
        /\ reconfigurations' = _TETrace[j].reconfigurations
        /\ lease_request  = _TETrace[i].lease_request
        /\ lease_request' = _TETrace[j].lease_request
        /\ decided_history  = _TETrace[i].decided_history
        /\ decided_history' = _TETrace[j].decided_history
        /\ losses  = _TETrace[i].losses
        /\ losses' = _TETrace[j].losses
        /\ witness_epoch  = _TETrace[i].witness_epoch
        /\ witness_epoch' = _TETrace[j].witness_epoch
        /\ forced_has_decided  = _TETrace[i].forced_has_decided
        /\ forced_has_decided' = _TETrace[j].forced_has_decided
        /\ deactivated  = _TETrace[i].deactivated
        /\ deactivated' = _TETrace[j].deactivated
        /\ first_log  = _TETrace[i].first_log
        /\ first_log' = _TETrace[j].first_log
        /\ wait_remaining  = _TETrace[i].wait_remaining
        /\ wait_remaining' = _TETrace[j].wait_remaining
        /\ timeouts  = _TETrace[i].timeouts
        /\ timeouts' = _TETrace[j].timeouts
        /\ duplicates  = _TETrace[i].duplicates
        /\ duplicates' = _TETrace[j].duplicates
        /\ elections  = _TETrace[i].elections
        /\ elections' = _TETrace[j].elections
        /\ leader_state  = _TETrace[i].leader_state
        /\ leader_state' = _TETrace[j].leader_state
        /\ rolled_back  = _TETrace[i].rolled_back
        /\ rolled_back' = _TETrace[j].rolled_back
        /\ lease_reply  = _TETrace[i].lease_reply
        /\ lease_reply' = _TETrace[j].lease_reply
        /\ nonce  = _TETrace[i].nonce
        /\ nonce' = _TETrace[j].nonce
        /\ n_accepted  = _TETrace[i].n_accepted
        /\ n_accepted' = _TETrace[j].n_accepted
        /\ halts  = _TETrace[i].halts
        /\ halts' = _TETrace[j].halts
        /\ log  = _TETrace[i].log
        /\ log' = _TETrace[j].log
        /\ cached_promise_message  = _TETrace[i].cached_promise_message
        /\ cached_promise_message' = _TETrace[j].cached_promise_message

\* Uncomment the ASSUME below to write the states of the error trace
\* to the given file in Json format. Note that you can pass any tuple
\* to `JsonSerialize`. For example, a sub-sequence of _TETrace.
    \* ASSUME
    \*     LET J == INSTANCE Json
    \*         IN J!JsonSerialize("SequencePaxos_TTrace_1792573816.json", _TETrace)

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
        ,wiped |-> wiped
        ,lease_remaining |-> lease_remaining
        ,promise |-> promise
        ,node_epoch |-> node_epoch
        ,read_only |-> read_only
        ,wipes |-> wipes
        ,accepted_idx |-> accepted_idx
        ,stage1_ghost |-> stage1_ghost
        ,recovery_responses |-> recovery_responses
        ,old_decided_after_forced |-> old_decided_after_forced
        ,read_pending |-> read_pending
        ,read_round |-> read_round
        ,buffered_proposals |-> buffered_proposals
        ,read_only_entries |-> read_only_entries
        ,force_single |-> force_single
        ,recovering |-> recovering
        ,decided_idx |-> decided_idx
        ,build_source |-> build_source
        ,role |-> role
        ,power_loss_completed |-> power_loss_completed
        ,phase |-> phase
        ,reads |-> reads
        ,old_decided_max |-> old_decided_max
        ,running |-> running
        ,crashes |-> crashes
        ,current_seq_num |-> current_seq_num
        ,proposed |-> proposed
        ,forced_decided_at_force |-> forced_decided_at_force
        ,chan |-> chan
        ,client |-> client
        ,past_durable |-> past_durable
        ,buffered_stopsign |-> buffered_stopsign
        ,reconfigurations |-> reconfigurations
        ,lease_request |-> lease_request
        ,decided_history |-> decided_history
        ,losses |-> losses
        ,witness_epoch |-> witness_epoch
        ,forced_has_decided |-> forced_has_decided
        ,deactivated |-> deactivated
        ,first_log |-> first_log
        ,wait_remaining |-> wait_remaining
        ,timeouts |-> timeouts
        ,duplicates |-> duplicates
        ,elections |-> elections
        ,leader_state |-> leader_state
        ,rolled_back |-> rolled_back
        ,lease_reply |-> lease_reply
        ,nonce |-> nonce
        ,n_accepted |-> n_accepted
        ,halts |-> halts
        ,log |-> log
        ,cached_promise_message |-> cached_promise_message
        
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
\*trace == IODeserialize("SequencePaxos_TTrace_1792573816.bin", TRUE)
\*
\*=============================================================================
\*

---- MODULE SequencePaxos_TETrace ----
EXTENDS SequencePaxos, TLC, SequencePaxos_TEConstants

trace == 
    <<
    ([force_single |-> [log |-> <<>>, node |-> 0, decided |-> <<>>, history |-> <<>>],timeouts |-> 0,lease_remaining |-> <<0, 0, 0>>,promise |-> <<0, 0, 0>>,crashes |-> 0,losses |-> 0,deactivated |-> <<FALSE, FALSE, FALSE>>,read_only |-> <<FALSE, FALSE, FALSE>>,rolled_back |-> {},accepted_idx |-> <<0, 0, 0>>,phase |-> <<"None", "None", "None">>,duplicates |-> 0,read_round |-> <<[reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}]>>,read_pending |-> <<{}, {}, {}>>,elections |-> 0,nonce |-> <<0, 0, 0>>,old_decided_after_forced |-> FALSE,wipes |-> 0,proposed |-> {},halts |-> 0,lease_reply |-> <<-1, -1, -1>>,cached_promise_message |-> <<<<>>, <<>>, <<>>>>,recovery_responses |-> <<{}, {}, {}>>,recovering |-> <<FALSE, FALSE, FALSE>>,buffered_stopsign |-> <<<<>>, <<>>, <<>>>>,wiped |-> <<FALSE, FALSE, FALSE>>,role |-> <<"Follower", "Follower", "Follower">>,log |-> <<<<>>, <<>>, <<>>>>,first_log |-> <<<<>>, <<>>, <<>>>>,decided_history |-> <<>>,lease_request |-> <<-1, -1, -1>>,running |-> {1, 2, 3},current_seq_num |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>,past_durable |-> <<{}, {}, {}>>,power_loss_completed |-> {},client |-> [session |-> 0, request_number |-> 1, sends |-> 0],forced_has_decided |-> FALSE,n_accepted |-> <<0, 0, 0>>,decided_idx |-> <<0, 0, 0>>,leader_state |-> <<[n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>]>>,buffered_proposals |-> <<<<>>, <<>>, <<>>>>,stage1_ghost |-> <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>,reads |-> <<>>,decided_ballot |-> <<0>>,node_epoch |-> <<0, 0, 0>>,witness_epoch |-> 0,forced_decided_at_force |-> 0,read_only_entries |-> 0,old_decided_max |-> 0,reconfigurations |-> 0,build_source |-> <<>>,wait_remaining |-> -1,chan |-> (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<>> @@ <<2, 1>> :> <<>> @@ <<2, 3>> :> <<>> @@ <<3, 1>> :> <<>> @@ <<3, 2>> :> <<>>)]),
    ([force_single |-> [log |-> <<>>, node |-> 0, decided |-> <<>>, history |-> <<>>],timeouts |-> 0,lease_remaining |-> <<0, 0, 0>>,promise |-> <<0, 0, 0>>,crashes |-> 1,losses |-> 0,deactivated |-> <<FALSE, FALSE, FALSE>>,read_only |-> <<FALSE, FALSE, FALSE>>,rolled_back |-> {},accepted_idx |-> <<0, 0, 0>>,phase |-> <<"Recover", "None", "None">>,duplicates |-> 0,read_round |-> <<[reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}]>>,read_pending |-> <<{}, {}, {}>>,elections |-> 0,nonce |-> <<1, 0, 0>>,old_decided_after_forced |-> FALSE,wipes |-> 0,proposed |-> {},halts |-> 0,lease_reply |-> <<-1, -1, -1>>,cached_promise_message |-> <<<<>>, <<>>, <<>>>>,recovery_responses |-> <<{}, {}, {}>>,recovering |-> <<TRUE, FALSE, FALSE>>,buffered_stopsign |-> <<<<>>, <<>>, <<>>>>,wiped |-> <<FALSE, FALSE, FALSE>>,role |-> <<"Follower", "Follower", "Follower">>,log |-> <<<<>>, <<>>, <<>>>>,first_log |-> <<<<>>, <<>>, <<>>>>,decided_history |-> <<>>,lease_request |-> <<-1, -1, -1>>,running |-> {1, 2, 3},current_seq_num |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>,past_durable |-> <<{}, {}, {}>>,power_loss_completed |-> {},client |-> [session |-> 0, request_number |-> 1, sends |-> 0],forced_has_decided |-> FALSE,n_accepted |-> <<0, 0, 0>>,decided_idx |-> <<0, 0, 0>>,leader_state |-> <<[n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>]>>,buffered_proposals |-> <<<<>>, <<>>, <<>>>>,stage1_ghost |-> <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>,reads |-> <<>>,decided_ballot |-> <<0>>,node_epoch |-> <<0, 0, 0>>,witness_epoch |-> 0,forced_decided_at_force |-> 0,read_only_entries |-> 0,old_decided_max |-> 0,reconfigurations |-> 0,build_source |-> <<>>,wait_remaining |-> -1,chan |-> (<<1, 2>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, recovering |-> FALSE, nonce |-> 1, reads |-> {}, n |-> 0, type |-> "Recovery", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<1, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, recovering |-> FALSE, nonce |-> 1, reads |-> {}, n |-> 0, type |-> "Recovery", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 1>> :> <<>> @@ <<2, 3>> :> <<>> @@ <<3, 1>> :> <<>> @@ <<3, 2>> :> <<>>)]),
    ([force_single |-> [log |-> <<>>, node |-> 0, decided |-> <<>>, history |-> <<>>],timeouts |-> 0,lease_remaining |-> <<0, 0, 0>>,promise |-> <<0, 0, 0>>,crashes |-> 1,losses |-> 0,deactivated |-> <<FALSE, FALSE, FALSE>>,read_only |-> <<FALSE, FALSE, FALSE>>,rolled_back |-> {},accepted_idx |-> <<0, 0, 0>>,phase |-> <<"Recover", "None", "None">>,duplicates |-> 0,read_round |-> <<[reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}]>>,read_pending |-> <<{}, {}, {}>>,elections |-> 0,nonce |-> <<1, 0, 0>>,old_decided_after_forced |-> FALSE,wipes |-> 0,proposed |-> {},halts |-> 0,lease_reply |-> <<-1, -1, -1>>,cached_promise_message |-> <<<<>>, <<>>, <<>>>>,recovery_responses |-> <<{}, {}, {}>>,recovering |-> <<TRUE, FALSE, FALSE>>,buffered_stopsign |-> <<<<>>, <<>>, <<>>>>,wiped |-> <<FALSE, FALSE, FALSE>>,role |-> <<"Follower", "Follower", "Follower">>,log |-> <<<<>>, <<>>, <<>>>>,first_log |-> <<<<>>, <<>>, <<>>>>,decided_history |-> <<>>,lease_request |-> <<-1, -1, -1>>,running |-> {1, 2, 3},current_seq_num |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>,past_durable |-> <<{}, {}, {}>>,power_loss_completed |-> {},client |-> [session |-> 0, request_number |-> 1, sends |-> 0],forced_has_decided |-> FALSE,n_accepted |-> <<0, 0, 0>>,decided_idx |-> <<0, 0, 0>>,leader_state |-> <<[n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>]>>,buffered_proposals |-> <<<<>>, <<>>, <<>>>>,stage1_ghost |-> <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>,reads |-> <<>>,decided_ballot |-> <<0>>,node_epoch |-> <<0, 0, 0>>,witness_epoch |-> 0,forced_decided_at_force |-> 0,read_only_entries |-> 0,old_decided_max |-> 0,reconfigurations |-> 0,build_source |-> <<>>,wait_remaining |-> -1,chan |-> (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, recovering |-> FALSE, nonce |-> 1, reads |-> {}, n |-> 0, type |-> "Recovery", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, recovering |-> FALSE, nonce |-> 1, reads |-> {}, n |-> 0, type |-> "RecoveryResponse", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 3>> :> <<>> @@ <<3, 1>> :> <<>> @@ <<3, 2>> :> <<>>)]),
    ([force_single |-> [log |-> <<>>, node |-> 0, decided |-> <<>>, history |-> <<>>],timeouts |-> 0,lease_remaining |-> <<0, 0, 0>>,promise |-> <<0, 0, 0>>,crashes |-> 1,losses |-> 0,deactivated |-> <<FALSE, FALSE, FALSE>>,read_only |-> <<FALSE, FALSE, FALSE>>,rolled_back |-> {},accepted_idx |-> <<0, 0, 0>>,phase |-> <<"Recover", "None", "None">>,duplicates |-> 0,read_round |-> <<[reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}]>>,read_pending |-> <<{}, {}, {}>>,elections |-> 0,nonce |-> <<1, 0, 0>>,old_decided_after_forced |-> FALSE,wipes |-> 0,proposed |-> {},halts |-> 0,lease_reply |-> <<-1, -1, -1>>,cached_promise_message |-> <<<<>>, <<>>, <<>>>>,recovery_responses |-> <<{}, {}, {}>>,recovering |-> <<TRUE, FALSE, FALSE>>,buffered_stopsign |-> <<<<>>, <<>>, <<>>>>,wiped |-> <<FALSE, FALSE, FALSE>>,role |-> <<"Follower", "Follower", "Follower">>,log |-> <<<<>>, <<>>, <<>>>>,first_log |-> <<<<>>, <<>>, <<>>>>,decided_history |-> <<>>,lease_request |-> <<-1, -1, -1>>,running |-> {1, 2, 3},current_seq_num |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>,past_durable |-> <<{}, {}, {}>>,power_loss_completed |-> {},client |-> [session |-> 0, request_number |-> 1, sends |-> 0],forced_has_decided |-> FALSE,n_accepted |-> <<0, 0, 0>>,decided_idx |-> <<0, 0, 0>>,leader_state |-> <<[n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>]>>,buffered_proposals |-> <<<<>>, <<>>, <<>>>>,stage1_ghost |-> <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>,reads |-> <<>>,decided_ballot |-> <<0>>,node_epoch |-> <<0, 0, 0>>,witness_epoch |-> 0,forced_decided_at_force |-> 0,read_only_entries |-> 0,old_decided_max |-> 0,reconfigurations |-> 0,build_source |-> <<>>,wait_remaining |-> -1,chan |-> (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<>> @@ <<2, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, recovering |-> FALSE, nonce |-> 1, reads |-> {}, n |-> 0, type |-> "RecoveryResponse", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 3>> :> <<>> @@ <<3, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, recovering |-> FALSE, nonce |-> 1, reads |-> {}, n |-> 0, type |-> "RecoveryResponse", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 2>> :> <<>>)]),
    ([force_single |-> [log |-> <<>>, node |-> 0, decided |-> <<>>, history |-> <<>>],timeouts |-> 0,lease_remaining |-> <<0, 0, 0>>,promise |-> <<0, 0, 0>>,crashes |-> 1,losses |-> 0,deactivated |-> <<FALSE, FALSE, FALSE>>,read_only |-> <<FALSE, FALSE, FALSE>>,rolled_back |-> {},accepted_idx |-> <<0, 0, 0>>,phase |-> <<"Recover", "None", "None">>,duplicates |-> 0,read_round |-> <<[reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}]>>,read_pending |-> <<{}, {}, {}>>,elections |-> 0,nonce |-> <<1, 0, 0>>,old_decided_after_forced |-> FALSE,wipes |-> 0,proposed |-> {},halts |-> 0,lease_reply |-> <<-1, -1, -1>>,cached_promise_message |-> <<<<>>, <<>>, <<>>>>,recovery_responses |-> <<{[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, recovering |-> FALSE, primary |-> FALSE, from |-> 2]}, {}, {}>>,recovering |-> <<TRUE, FALSE, FALSE>>,buffered_stopsign |-> <<<<>>, <<>>, <<>>>>,wiped |-> <<FALSE, FALSE, FALSE>>,role |-> <<"Follower", "Follower", "Follower">>,log |-> <<<<>>, <<>>, <<>>>>,first_log |-> <<<<>>, <<>>, <<>>>>,decided_history |-> <<>>,lease_request |-> <<-1, -1, -1>>,running |-> {1, 2, 3},current_seq_num |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>,past_durable |-> <<{}, {}, {}>>,power_loss_completed |-> {},client |-> [session |-> 0, request_number |-> 1, sends |-> 0],forced_has_decided |-> FALSE,n_accepted |-> <<0, 0, 0>>,decided_idx |-> <<0, 0, 0>>,leader_state |-> <<[n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>]>>,buffered_proposals |-> <<<<>>, <<>>, <<>>>>,stage1_ghost |-> <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>,reads |-> <<>>,decided_ballot |-> <<0>>,node_epoch |-> <<0, 0, 0>>,witness_epoch |-> 0,forced_decided_at_force |-> 0,read_only_entries |-> 0,old_decided_max |-> 0,reconfigurations |-> 0,build_source |-> <<>>,wait_remaining |-> -1,chan |-> (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<>> @@ <<2, 1>> :> <<>> @@ <<2, 3>> :> <<>> @@ <<3, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, recovering |-> FALSE, nonce |-> 1, reads |-> {}, n |-> 0, type |-> "RecoveryResponse", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 2>> :> <<>>)]),
    ([force_single |-> [log |-> <<>>, node |-> 0, decided |-> <<>>, history |-> <<>>],timeouts |-> 0,lease_remaining |-> <<0, 0, 0>>,promise |-> <<0, 0, 0>>,crashes |-> 2,losses |-> 0,deactivated |-> <<FALSE, FALSE, FALSE>>,read_only |-> <<FALSE, FALSE, FALSE>>,rolled_back |-> {},accepted_idx |-> <<0, 0, 0>>,phase |-> <<"Recover", "Recover", "None">>,duplicates |-> 0,read_round |-> <<[reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}]>>,read_pending |-> <<{}, {}, {}>>,elections |-> 0,nonce |-> <<1, 1, 0>>,old_decided_after_forced |-> FALSE,wipes |-> 0,proposed |-> {},halts |-> 0,lease_reply |-> <<-1, -1, -1>>,cached_promise_message |-> <<<<>>, <<>>, <<>>>>,recovery_responses |-> <<{[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, recovering |-> FALSE, primary |-> FALSE, from |-> 2]}, {}, {}>>,recovering |-> <<TRUE, TRUE, FALSE>>,buffered_stopsign |-> <<<<>>, <<>>, <<>>>>,wiped |-> <<FALSE, FALSE, FALSE>>,role |-> <<"Follower", "Follower", "Follower">>,log |-> <<<<>>, <<>>, <<>>>>,first_log |-> <<<<>>, <<>>, <<>>>>,decided_history |-> <<>>,lease_request |-> <<-1, -1, -1>>,running |-> {1, 2, 3},current_seq_num |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>,past_durable |-> <<{}, {}, {}>>,power_loss_completed |-> {},client |-> [session |-> 0, request_number |-> 1, sends |-> 0],forced_has_decided |-> FALSE,n_accepted |-> <<0, 0, 0>>,decided_idx |-> <<0, 0, 0>>,leader_state |-> <<[n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>]>>,buffered_proposals |-> <<<<>>, <<>>, <<>>>>,stage1_ghost |-> <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>,reads |-> <<>>,decided_ballot |-> <<0>>,node_epoch |-> <<0, 0, 0>>,witness_epoch |-> 0,forced_decided_at_force |-> 0,read_only_entries |-> 0,old_decided_max |-> 0,reconfigurations |-> 0,build_source |-> <<>>,wait_remaining |-> -1,chan |-> (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<>> @@ <<2, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, recovering |-> FALSE, nonce |-> 1, reads |-> {}, n |-> 0, type |-> "Recovery", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, recovering |-> FALSE, nonce |-> 1, reads |-> {}, n |-> 0, type |-> "Recovery", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, recovering |-> FALSE, nonce |-> 1, reads |-> {}, n |-> 0, type |-> "RecoveryResponse", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 2>> :> <<>>)]),
    ([force_single |-> [log |-> <<>>, node |-> 0, decided |-> <<>>, history |-> <<>>],timeouts |-> 0,lease_remaining |-> <<0, 0, 0>>,promise |-> <<0, 0, 0>>,crashes |-> 3,losses |-> 0,deactivated |-> <<FALSE, FALSE, FALSE>>,read_only |-> <<FALSE, FALSE, FALSE>>,rolled_back |-> {},accepted_idx |-> <<0, 0, 0>>,phase |-> <<"Recover", "Recover", "Recover">>,duplicates |-> 0,read_round |-> <<[reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}]>>,read_pending |-> <<{}, {}, {}>>,elections |-> 0,nonce |-> <<1, 1, 1>>,old_decided_after_forced |-> FALSE,wipes |-> 0,proposed |-> {},halts |-> 0,lease_reply |-> <<-1, -1, -1>>,cached_promise_message |-> <<<<>>, <<>>, <<>>>>,recovery_responses |-> <<{[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, recovering |-> FALSE, primary |-> FALSE, from |-> 2]}, {}, {}>>,recovering |-> <<TRUE, TRUE, TRUE>>,buffered_stopsign |-> <<<<>>, <<>>, <<>>>>,wiped |-> <<FALSE, FALSE, FALSE>>,role |-> <<"Follower", "Follower", "Follower">>,log |-> <<<<>>, <<>>, <<>>>>,first_log |-> <<<<>>, <<>>, <<>>>>,decided_history |-> <<>>,lease_request |-> <<-1, -1, -1>>,running |-> {1, 2, 3},current_seq_num |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>,past_durable |-> <<{}, {}, {}>>,power_loss_completed |-> {},client |-> [session |-> 0, request_number |-> 1, sends |-> 0],forced_has_decided |-> FALSE,n_accepted |-> <<0, 0, 0>>,decided_idx |-> <<0, 0, 0>>,leader_state |-> <<[n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>]>>,buffered_proposals |-> <<<<>>, <<>>, <<>>>>,stage1_ghost |-> <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>,reads |-> <<>>,decided_ballot |-> <<0>>,node_epoch |-> <<0, 0, 0>>,witness_epoch |-> 0,forced_decided_at_force |-> 0,read_only_entries |-> 0,old_decided_max |-> 0,reconfigurations |-> 0,build_source |-> <<>>,wait_remaining |-> -1,chan |-> (<<1, 2>> :> <<>> @@ <<1, 3>> :> <<>> @@ <<2, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, recovering |-> FALSE, nonce |-> 1, reads |-> {}, n |-> 0, type |-> "Recovery", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, recovering |-> FALSE, nonce |-> 1, reads |-> {}, n |-> 0, type |-> "Recovery", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, recovering |-> FALSE, nonce |-> 1, reads |-> {}, n |-> 0, type |-> "RecoveryResponse", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0], [promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, recovering |-> FALSE, nonce |-> 1, reads |-> {}, n |-> 0, type |-> "Recovery", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 2>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, recovering |-> FALSE, nonce |-> 1, reads |-> {}, n |-> 0, type |-> "Recovery", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>>)]),
    ([force_single |-> [log |-> <<>>, node |-> 0, decided |-> <<>>, history |-> <<>>],timeouts |-> 0,lease_remaining |-> <<0, 0, 0>>,promise |-> <<0, 0, 0>>,crashes |-> 3,losses |-> 0,deactivated |-> <<FALSE, FALSE, FALSE>>,read_only |-> <<FALSE, FALSE, FALSE>>,rolled_back |-> {},accepted_idx |-> <<0, 0, 0>>,phase |-> <<"Recover", "Recover", "Recover">>,duplicates |-> 0,read_round |-> <<[reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}], [reads |-> {}, n |-> 0, id |-> 0, read_index |-> 0, acks |-> {}]>>,read_pending |-> <<{}, {}, {}>>,elections |-> 0,nonce |-> <<1, 1, 1>>,old_decided_after_forced |-> FALSE,wipes |-> 0,proposed |-> {},halts |-> 0,lease_reply |-> <<-1, -1, -1>>,cached_promise_message |-> <<<<>>, <<>>, <<>>>>,recovery_responses |-> <<{}, {}, {}>>,recovering |-> <<FALSE, TRUE, TRUE>>,buffered_stopsign |-> <<<<>>, <<>>, <<>>>>,wiped |-> <<FALSE, FALSE, FALSE>>,role |-> <<"Follower", "Follower", "Follower">>,log |-> <<<<>>, <<>>, <<>>>>,first_log |-> <<<<>>, <<>>, <<>>>>,decided_history |-> <<>>,lease_request |-> <<-1, -1, -1>>,running |-> {1, 2, 3},current_seq_num |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>,past_durable |-> <<{}, {}, {}>>,power_loss_completed |-> {1},client |-> [session |-> 0, request_number |-> 1, sends |-> 0],forced_has_decided |-> FALSE,n_accepted |-> <<0, 0, 0>>,decided_idx |-> <<0, 0, 0>>,leader_state |-> <<[n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>], [n_leader |-> 0, promises_meta |-> <<[n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"], [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, state |-> "NotPromised"]>>, follower_seq_nums |-> <<[session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0], [session |-> 0, counter |-> 0]>>, accepted_indexes |-> <<0, 0, 0>>, max_promise_meta |-> [n_accepted |-> 0, decided_idx |-> 0, accepted_idx |-> 0, pid |-> 0], max_promise_sync |-> <<>>]>>,buffered_proposals |-> <<<<>>, <<>>, <<>>>>,stage1_ghost |-> <<[down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1], [down |-> {}, leave_decided |-> -1]>>,reads |-> <<>>,decided_ballot |-> <<0>>,node_epoch |-> <<0, 0, 0>>,witness_epoch |-> 0,forced_decided_at_force |-> 0,read_only_entries |-> 0,old_decided_max |-> 0,reconfigurations |-> 0,build_source |-> <<>>,wait_remaining |-> -1,chan |-> (<<1, 2>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, recovering |-> FALSE, nonce |-> 0, reads |-> {}, n |-> 0, type |-> "PrepareReq", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<1, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, recovering |-> FALSE, nonce |-> 0, reads |-> {}, n |-> 0, type |-> "PrepareReq", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, recovering |-> FALSE, nonce |-> 1, reads |-> {}, n |-> 0, type |-> "Recovery", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<2, 3>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, recovering |-> FALSE, nonce |-> 1, reads |-> {}, n |-> 0, type |-> "Recovery", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 1>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, recovering |-> FALSE, nonce |-> 1, reads |-> {}, n |-> 0, type |-> "Recovery", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>> @@ <<3, 2>> :> <<[promise |-> 0, n_accepted |-> 0, log |-> <<>>, decided_idx |-> 0, accepted_idx |-> 0, recovering |-> FALSE, nonce |-> 1, reads |-> {}, n |-> 0, type |-> "Recovery", log_sync |-> <<>>, seq_num |-> [session |-> 0, counter |-> 0], entries |-> <<>>, primary |-> FALSE, read |-> 0, id |-> 0, read_index |-> 0]>>)])
    >>
----


=============================================================================

---- MODULE SequencePaxos_TEConstants ----
EXTENDS SequencePaxos

CONSTANTS v1, v2

=============================================================================

---- CONFIG SequencePaxos_TTrace_1792573816 ----
CONSTANTS
    Nodes = { 1 , 2 , 3 }
    Values = { v1 , v2 }
    MaxBallot = 2
    MaxLog = 1
    MaxCounter = 4
    MaxSession = 2
    Cap = 2
    MaxCrash = 3
    MaxWipe = 0
    MaxLoss = 0
    MaxDuplicate = 0
    MaxTimeout = 1
    MaxElection = 2
    RuleR = TRUE
    DecidedIdxMonotone = TRUE
    MaxRollback = 0
    Recovery = TRUE
    PowerLoss = TRUE
    Mutation = "none"
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
\* Generated on Wed Oct 21 09:14:04 UTC 2026