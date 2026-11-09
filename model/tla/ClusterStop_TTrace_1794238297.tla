---- MODULE ClusterStop_TTrace_1794238297 ----
EXTENDS Sequences, ClusterStop, TLCExt, Toolbox, Naturals, TLC

_expression ==
    LET ClusterStop_TEExpression == INSTANCE ClusterStop_TEExpression
    IN ClusterStop_TEExpression!expression
----

_trace ==
    LET ClusterStop_TETrace == INSTANCE ClusterStop_TETrace
    IN ClusterStop_TETrace!trace
----

_inv ==
    ~(
        TLCGet("level") = Len(_TETrace)
        /\
        msgs = ({[b |-> 1, type |-> "prepare"], [b |-> 2, type |-> "prepare"], [b |-> 1, log |-> <<>>, type |-> "accept"], [b |-> 2, log |-> <<>>, type |-> "accept"], [b |-> 2, log |-> <<[b |-> 2, v |-> "w"]>>, type |-> "accept"], [b |-> 2, type |-> "decide", idx |-> 1], [b |-> 2, from |-> 3, type |-> "ack", idx |-> 1], [b |-> 1, acc |-> 0, log |-> <<>>, from |-> 3, type |-> "promise"], [b |-> 2, acc |-> 0, log |-> <<>>, from |-> 3, type |-> "promise"]})
        /\
        st = (<<[prom |-> 1, acc |-> 1, log |-> <<>>, dec |-> 0], [prom |-> 2, acc |-> 2, log |-> <<[b |-> 2, v |-> "w"]>>, dec |-> 1], [prom |-> 2, acc |-> 2, log |-> <<[b |-> 2, v |-> "w"]>>, dec |-> 0]>>)
        /\
        ver = (<<0, 1, 1>>)
        /\
        damages = (0)
        /\
        count = (<<0, 0, 0>>)
        /\
        crashes = (0)
        /\
        lead = (<<[b |-> 1, log |-> <<>>, phase |-> "accept", promises |-> {[acc |-> 0, log |-> <<>>, from |-> 1], [acc |-> 0, log |-> <<>>, from |-> 3]}, acks |-> <<0, 0, 0>>, sent_stop |-> FALSE], [b |-> 2, log |-> <<[b |-> 2, v |-> "w"]>>, phase |-> "accept", promises |-> {[acc |-> 0, log |-> <<>>, from |-> 2], [acc |-> 0, log |-> <<>>, from |-> 3]}, acks |-> <<0, 1, 1>>, sent_stop |-> FALSE], [b |-> 0, log |-> <<>>, phase |-> "none", promises |-> {}, acks |-> <<0, 0, 0>>, sent_stop |-> FALSE]>>)
        /\
        chosen = (<<[b |-> 2, v |-> "w"]>>)
        /\
        stopped_by = ({})
        /\
        damaged = (<<0, 0, 0>>)
        /\
        stops = (0)
        /\
        mark = (<<-1, -1, -1>>)
        /\
        status = (<<"running", "running", "running">>)
    )
----

_init ==
    /\ lead = _TETrace[1].lead
    /\ stops = _TETrace[1].stops
    /\ mark = _TETrace[1].mark
    /\ stopped_by = _TETrace[1].stopped_by
    /\ ver = _TETrace[1].ver
    /\ msgs = _TETrace[1].msgs
    /\ st = _TETrace[1].st
    /\ count = _TETrace[1].count
    /\ crashes = _TETrace[1].crashes
    /\ chosen = _TETrace[1].chosen
    /\ damaged = _TETrace[1].damaged
    /\ damages = _TETrace[1].damages
    /\ status = _TETrace[1].status
----

_next ==
    /\ \E i,j \in DOMAIN _TETrace:
        /\ \/ /\ j = i + 1
              /\ i = TLCGet("level")
        /\ lead  = _TETrace[i].lead
        /\ lead' = _TETrace[j].lead
        /\ stops  = _TETrace[i].stops
        /\ stops' = _TETrace[j].stops
        /\ mark  = _TETrace[i].mark
        /\ mark' = _TETrace[j].mark
        /\ stopped_by  = _TETrace[i].stopped_by
        /\ stopped_by' = _TETrace[j].stopped_by
        /\ ver  = _TETrace[i].ver
        /\ ver' = _TETrace[j].ver
        /\ msgs  = _TETrace[i].msgs
        /\ msgs' = _TETrace[j].msgs
        /\ st  = _TETrace[i].st
        /\ st' = _TETrace[j].st
        /\ count  = _TETrace[i].count
        /\ count' = _TETrace[j].count
        /\ crashes  = _TETrace[i].crashes
        /\ crashes' = _TETrace[j].crashes
        /\ chosen  = _TETrace[i].chosen
        /\ chosen' = _TETrace[j].chosen
        /\ damaged  = _TETrace[i].damaged
        /\ damaged' = _TETrace[j].damaged
        /\ damages  = _TETrace[i].damages
        /\ damages' = _TETrace[j].damages
        /\ status  = _TETrace[i].status
        /\ status' = _TETrace[j].status

\* Uncomment the ASSUME below to write the states of the error trace
\* to the given file in Json format. Note that you can pass any tuple
\* to `JsonSerialize`. For example, a sub-sequence of _TETrace.
    \* ASSUME
    \*     LET J == INSTANCE Json
    \*         IN J!JsonSerialize("ClusterStop_TTrace_1794238297.json", _TETrace)

=============================================================================

 Note that you can extract this module `ClusterStop_TEExpression`
  to a dedicated file to reuse `expression` (the module in the 
  dedicated `ClusterStop_TEExpression.tla` file takes precedence 
  over the module `ClusterStop_TEExpression` below).

---- MODULE ClusterStop_TEExpression ----
EXTENDS Sequences, ClusterStop, TLCExt, Toolbox, Naturals, TLC

expression == 
    [
        \* To hide variables of the `ClusterStop` spec from the error trace,
        \* remove the variables below.  The trace will be written in the order
        \* of the fields of this record.
        lead |-> lead
        ,stops |-> stops
        ,mark |-> mark
        ,stopped_by |-> stopped_by
        ,ver |-> ver
        ,msgs |-> msgs
        ,st |-> st
        ,count |-> count
        ,crashes |-> crashes
        ,chosen |-> chosen
        ,damaged |-> damaged
        ,damages |-> damages
        ,status |-> status
        
        \* Put additional constant-, state-, and action-level expressions here:
        \* ,_stateNumber |-> _TEPosition
        \* ,_leadUnchanged |-> lead = lead'
        
        \* Format the `lead` variable as Json value.
        \* ,_leadJson |->
        \*     LET J == INSTANCE Json
        \*     IN J!ToJson(lead)
        
        \* Lastly, you may build expressions over arbitrary sets of states by
        \* leveraging the _TETrace operator.  For example, this is how to
        \* count the number of times a spec variable changed up to the current
        \* state in the trace.
        \* ,_leadModCount |->
        \*     LET F[s \in DOMAIN _TETrace] ==
        \*         IF s = 1 THEN 0
        \*         ELSE IF _TETrace[s].lead # _TETrace[s-1].lead
        \*             THEN 1 + F[s-1] ELSE F[s-1]
        \*     IN F[_TEPosition - 1]
    ]

=============================================================================



Parsing and semantic processing can take forever if the trace below is long.
 In this case, it is advised to uncomment the module below to deserialize the
 trace from a generated binary file.

\*
\*---- MODULE ClusterStop_TETrace ----
\*EXTENDS IOUtils, ClusterStop, TLC
\*
\*trace == IODeserialize("ClusterStop_TTrace_1794238297.bin", TRUE)
\*
\*=============================================================================
\*

---- MODULE ClusterStop_TETrace ----
EXTENDS ClusterStop, TLC

trace == 
    <<
    ([msgs |-> {},st |-> <<[prom |-> 0, acc |-> 0, log |-> <<>>, dec |-> 0], [prom |-> 0, acc |-> 0, log |-> <<>>, dec |-> 0], [prom |-> 0, acc |-> 0, log |-> <<>>, dec |-> 0]>>,ver |-> <<0, 0, 0>>,damages |-> 0,count |-> <<0, 0, 0>>,crashes |-> 0,lead |-> <<[b |-> 0, log |-> <<>>, phase |-> "none", promises |-> {}, acks |-> <<0, 0, 0>>, sent_stop |-> FALSE], [b |-> 0, log |-> <<>>, phase |-> "none", promises |-> {}, acks |-> <<0, 0, 0>>, sent_stop |-> FALSE], [b |-> 0, log |-> <<>>, phase |-> "none", promises |-> {}, acks |-> <<0, 0, 0>>, sent_stop |-> FALSE]>>,chosen |-> <<>>,stopped_by |-> {},damaged |-> <<0, 0, 0>>,stops |-> 0,mark |-> <<-1, -1, -1>>,status |-> <<"running", "running", "running">>]),
    ([msgs |-> {[b |-> 1, type |-> "prepare"]},st |-> <<[prom |-> 1, acc |-> 0, log |-> <<>>, dec |-> 0], [prom |-> 0, acc |-> 0, log |-> <<>>, dec |-> 0], [prom |-> 0, acc |-> 0, log |-> <<>>, dec |-> 0]>>,ver |-> <<0, 0, 0>>,damages |-> 0,count |-> <<0, 0, 0>>,crashes |-> 0,lead |-> <<[b |-> 1, log |-> <<>>, phase |-> "prepare", promises |-> {[acc |-> 0, log |-> <<>>, from |-> 1]}, acks |-> <<0, 0, 0>>, sent_stop |-> FALSE], [b |-> 0, log |-> <<>>, phase |-> "none", promises |-> {}, acks |-> <<0, 0, 0>>, sent_stop |-> FALSE], [b |-> 0, log |-> <<>>, phase |-> "none", promises |-> {}, acks |-> <<0, 0, 0>>, sent_stop |-> FALSE]>>,chosen |-> <<>>,stopped_by |-> {},damaged |-> <<0, 0, 0>>,stops |-> 0,mark |-> <<-1, -1, -1>>,status |-> <<"running", "running", "running">>]),
    ([msgs |-> {[b |-> 1, type |-> "prepare"], [b |-> 2, type |-> "prepare"]},st |-> <<[prom |-> 1, acc |-> 0, log |-> <<>>, dec |-> 0], [prom |-> 2, acc |-> 0, log |-> <<>>, dec |-> 0], [prom |-> 0, acc |-> 0, log |-> <<>>, dec |-> 0]>>,ver |-> <<0, 0, 0>>,damages |-> 0,count |-> <<0, 0, 0>>,crashes |-> 0,lead |-> <<[b |-> 1, log |-> <<>>, phase |-> "prepare", promises |-> {[acc |-> 0, log |-> <<>>, from |-> 1]}, acks |-> <<0, 0, 0>>, sent_stop |-> FALSE], [b |-> 2, log |-> <<>>, phase |-> "prepare", promises |-> {[acc |-> 0, log |-> <<>>, from |-> 2]}, acks |-> <<0, 0, 0>>, sent_stop |-> FALSE], [b |-> 0, log |-> <<>>, phase |-> "none", promises |-> {}, acks |-> <<0, 0, 0>>, sent_stop |-> FALSE]>>,chosen |-> <<>>,stopped_by |-> {},damaged |-> <<0, 0, 0>>,stops |-> 0,mark |-> <<-1, -1, -1>>,status |-> <<"running", "running", "running">>]),
    ([msgs |-> {[b |-> 1, type |-> "prepare"], [b |-> 2, type |-> "prepare"], [b |-> 1, acc |-> 0, log |-> <<>>, from |-> 3, type |-> "promise"]},st |-> <<[prom |-> 1, acc |-> 0, log |-> <<>>, dec |-> 0], [prom |-> 2, acc |-> 0, log |-> <<>>, dec |-> 0], [prom |-> 1, acc |-> 0, log |-> <<>>, dec |-> 0]>>,ver |-> <<0, 0, 0>>,damages |-> 0,count |-> <<0, 0, 0>>,crashes |-> 0,lead |-> <<[b |-> 1, log |-> <<>>, phase |-> "prepare", promises |-> {[acc |-> 0, log |-> <<>>, from |-> 1]}, acks |-> <<0, 0, 0>>, sent_stop |-> FALSE], [b |-> 2, log |-> <<>>, phase |-> "prepare", promises |-> {[acc |-> 0, log |-> <<>>, from |-> 2]}, acks |-> <<0, 0, 0>>, sent_stop |-> FALSE], [b |-> 0, log |-> <<>>, phase |-> "none", promises |-> {}, acks |-> <<0, 0, 0>>, sent_stop |-> FALSE]>>,chosen |-> <<>>,stopped_by |-> {},damaged |-> <<0, 0, 0>>,stops |-> 0,mark |-> <<-1, -1, -1>>,status |-> <<"running", "running", "running">>]),
    ([msgs |-> {[b |-> 1, type |-> "prepare"], [b |-> 2, type |-> "prepare"], [b |-> 1, acc |-> 0, log |-> <<>>, from |-> 3, type |-> "promise"], [b |-> 2, acc |-> 0, log |-> <<>>, from |-> 3, type |-> "promise"]},st |-> <<[prom |-> 1, acc |-> 0, log |-> <<>>, dec |-> 0], [prom |-> 2, acc |-> 0, log |-> <<>>, dec |-> 0], [prom |-> 2, acc |-> 0, log |-> <<>>, dec |-> 0]>>,ver |-> <<0, 0, 0>>,damages |-> 0,count |-> <<0, 0, 0>>,crashes |-> 0,lead |-> <<[b |-> 1, log |-> <<>>, phase |-> "prepare", promises |-> {[acc |-> 0, log |-> <<>>, from |-> 1]}, acks |-> <<0, 0, 0>>, sent_stop |-> FALSE], [b |-> 2, log |-> <<>>, phase |-> "prepare", promises |-> {[acc |-> 0, log |-> <<>>, from |-> 2]}, acks |-> <<0, 0, 0>>, sent_stop |-> FALSE], [b |-> 0, log |-> <<>>, phase |-> "none", promises |-> {}, acks |-> <<0, 0, 0>>, sent_stop |-> FALSE]>>,chosen |-> <<>>,stopped_by |-> {},damaged |-> <<0, 0, 0>>,stops |-> 0,mark |-> <<-1, -1, -1>>,status |-> <<"running", "running", "running">>]),
    ([msgs |-> {[b |-> 1, type |-> "prepare"], [b |-> 2, type |-> "prepare"], [b |-> 1, log |-> <<>>, type |-> "accept"], [b |-> 1, acc |-> 0, log |-> <<>>, from |-> 3, type |-> "promise"], [b |-> 2, acc |-> 0, log |-> <<>>, from |-> 3, type |-> "promise"]},st |-> <<[prom |-> 1, acc |-> 1, log |-> <<>>, dec |-> 0], [prom |-> 2, acc |-> 0, log |-> <<>>, dec |-> 0], [prom |-> 2, acc |-> 0, log |-> <<>>, dec |-> 0]>>,ver |-> <<0, 0, 0>>,damages |-> 0,count |-> <<0, 0, 0>>,crashes |-> 0,lead |-> <<[b |-> 1, log |-> <<>>, phase |-> "accept", promises |-> {[acc |-> 0, log |-> <<>>, from |-> 1], [acc |-> 0, log |-> <<>>, from |-> 3]}, acks |-> <<0, 0, 0>>, sent_stop |-> FALSE], [b |-> 2, log |-> <<>>, phase |-> "prepare", promises |-> {[acc |-> 0, log |-> <<>>, from |-> 2]}, acks |-> <<0, 0, 0>>, sent_stop |-> FALSE], [b |-> 0, log |-> <<>>, phase |-> "none", promises |-> {}, acks |-> <<0, 0, 0>>, sent_stop |-> FALSE]>>,chosen |-> <<>>,stopped_by |-> {},damaged |-> <<0, 0, 0>>,stops |-> 0,mark |-> <<-1, -1, -1>>,status |-> <<"running", "running", "running">>]),
    ([msgs |-> {[b |-> 1, type |-> "prepare"], [b |-> 2, type |-> "prepare"], [b |-> 1, log |-> <<>>, type |-> "accept"], [b |-> 2, log |-> <<>>, type |-> "accept"], [b |-> 1, acc |-> 0, log |-> <<>>, from |-> 3, type |-> "promise"], [b |-> 2, acc |-> 0, log |-> <<>>, from |-> 3, type |-> "promise"]},st |-> <<[prom |-> 1, acc |-> 1, log |-> <<>>, dec |-> 0], [prom |-> 2, acc |-> 2, log |-> <<>>, dec |-> 0], [prom |-> 2, acc |-> 0, log |-> <<>>, dec |-> 0]>>,ver |-> <<0, 0, 0>>,damages |-> 0,count |-> <<0, 0, 0>>,crashes |-> 0,lead |-> <<[b |-> 1, log |-> <<>>, phase |-> "accept", promises |-> {[acc |-> 0, log |-> <<>>, from |-> 1], [acc |-> 0, log |-> <<>>, from |-> 3]}, acks |-> <<0, 0, 0>>, sent_stop |-> FALSE], [b |-> 2, log |-> <<>>, phase |-> "accept", promises |-> {[acc |-> 0, log |-> <<>>, from |-> 2], [acc |-> 0, log |-> <<>>, from |-> 3]}, acks |-> <<0, 0, 0>>, sent_stop |-> FALSE], [b |-> 0, log |-> <<>>, phase |-> "none", promises |-> {}, acks |-> <<0, 0, 0>>, sent_stop |-> FALSE]>>,chosen |-> <<>>,stopped_by |-> {},damaged |-> <<0, 0, 0>>,stops |-> 0,mark |-> <<-1, -1, -1>>,status |-> <<"running", "running", "running">>]),
    ([msgs |-> {[b |-> 1, type |-> "prepare"], [b |-> 2, type |-> "prepare"], [b |-> 1, log |-> <<>>, type |-> "accept"], [b |-> 2, log |-> <<>>, type |-> "accept"], [b |-> 2, log |-> <<[b |-> 2, v |-> "w"]>>, type |-> "accept"], [b |-> 1, acc |-> 0, log |-> <<>>, from |-> 3, type |-> "promise"], [b |-> 2, acc |-> 0, log |-> <<>>, from |-> 3, type |-> "promise"]},st |-> <<[prom |-> 1, acc |-> 1, log |-> <<>>, dec |-> 0], [prom |-> 2, acc |-> 2, log |-> <<[b |-> 2, v |-> "w"]>>, dec |-> 0], [prom |-> 2, acc |-> 0, log |-> <<>>, dec |-> 0]>>,ver |-> <<0, 0, 0>>,damages |-> 0,count |-> <<0, 0, 0>>,crashes |-> 0,lead |-> <<[b |-> 1, log |-> <<>>, phase |-> "accept", promises |-> {[acc |-> 0, log |-> <<>>, from |-> 1], [acc |-> 0, log |-> <<>>, from |-> 3]}, acks |-> <<0, 0, 0>>, sent_stop |-> FALSE], [b |-> 2, log |-> <<[b |-> 2, v |-> "w"]>>, phase |-> "accept", promises |-> {[acc |-> 0, log |-> <<>>, from |-> 2], [acc |-> 0, log |-> <<>>, from |-> 3]}, acks |-> <<0, 0, 0>>, sent_stop |-> FALSE], [b |-> 0, log |-> <<>>, phase |-> "none", promises |-> {}, acks |-> <<0, 0, 0>>, sent_stop |-> FALSE]>>,chosen |-> <<>>,stopped_by |-> {},damaged |-> <<0, 0, 0>>,stops |-> 0,mark |-> <<-1, -1, -1>>,status |-> <<"running", "running", "running">>]),
    ([msgs |-> {[b |-> 1, type |-> "prepare"], [b |-> 2, type |-> "prepare"], [b |-> 1, log |-> <<>>, type |-> "accept"], [b |-> 2, log |-> <<>>, type |-> "accept"], [b |-> 2, log |-> <<[b |-> 2, v |-> "w"]>>, type |-> "accept"], [b |-> 1, acc |-> 0, log |-> <<>>, from |-> 3, type |-> "promise"], [b |-> 2, acc |-> 0, log |-> <<>>, from |-> 3, type |-> "promise"]},st |-> <<[prom |-> 1, acc |-> 1, log |-> <<>>, dec |-> 0], [prom |-> 2, acc |-> 2, log |-> <<[b |-> 2, v |-> "w"]>>, dec |-> 0], [prom |-> 2, acc |-> 2, log |-> <<[b |-> 2, v |-> "w"]>>, dec |-> 0]>>,ver |-> <<0, 0, 0>>,damages |-> 0,count |-> <<0, 0, 0>>,crashes |-> 0,lead |-> <<[b |-> 1, log |-> <<>>, phase |-> "accept", promises |-> {[acc |-> 0, log |-> <<>>, from |-> 1], [acc |-> 0, log |-> <<>>, from |-> 3]}, acks |-> <<0, 0, 0>>, sent_stop |-> FALSE], [b |-> 2, log |-> <<[b |-> 2, v |-> "w"]>>, phase |-> "accept", promises |-> {[acc |-> 0, log |-> <<>>, from |-> 2], [acc |-> 0, log |-> <<>>, from |-> 3]}, acks |-> <<0, 0, 0>>, sent_stop |-> FALSE], [b |-> 0, log |-> <<>>, phase |-> "none", promises |-> {}, acks |-> <<0, 0, 0>>, sent_stop |-> FALSE]>>,chosen |-> <<>>,stopped_by |-> {},damaged |-> <<0, 0, 0>>,stops |-> 0,mark |-> <<-1, -1, -1>>,status |-> <<"running", "running", "running">>]),
    ([msgs |-> {[b |-> 1, type |-> "prepare"], [b |-> 2, type |-> "prepare"], [b |-> 1, log |-> <<>>, type |-> "accept"], [b |-> 2, log |-> <<>>, type |-> "accept"], [b |-> 2, log |-> <<[b |-> 2, v |-> "w"]>>, type |-> "accept"], [b |-> 1, acc |-> 0, log |-> <<>>, from |-> 3, type |-> "promise"], [b |-> 2, acc |-> 0, log |-> <<>>, from |-> 3, type |-> "promise"]},st |-> <<[prom |-> 1, acc |-> 1, log |-> <<>>, dec |-> 0], [prom |-> 2, acc |-> 2, log |-> <<[b |-> 2, v |-> "w"]>>, dec |-> 0], [prom |-> 2, acc |-> 2, log |-> <<[b |-> 2, v |-> "w"]>>, dec |-> 0]>>,ver |-> <<0, 1, 0>>,damages |-> 0,count |-> <<0, 0, 0>>,crashes |-> 0,lead |-> <<[b |-> 1, log |-> <<>>, phase |-> "accept", promises |-> {[acc |-> 0, log |-> <<>>, from |-> 1], [acc |-> 0, log |-> <<>>, from |-> 3]}, acks |-> <<0, 0, 0>>, sent_stop |-> FALSE], [b |-> 2, log |-> <<[b |-> 2, v |-> "w"]>>, phase |-> "accept", promises |-> {[acc |-> 0, log |-> <<>>, from |-> 2], [acc |-> 0, log |-> <<>>, from |-> 3]}, acks |-> <<0, 0, 0>>, sent_stop |-> FALSE], [b |-> 0, log |-> <<>>, phase |-> "none", promises |-> {}, acks |-> <<0, 0, 0>>, sent_stop |-> FALSE]>>,chosen |-> <<>>,stopped_by |-> {},damaged |-> <<0, 0, 0>>,stops |-> 0,mark |-> <<-1, -1, -1>>,status |-> <<"running", "running", "running">>]),
    ([msgs |-> {[b |-> 1, type |-> "prepare"], [b |-> 2, type |-> "prepare"], [b |-> 1, log |-> <<>>, type |-> "accept"], [b |-> 2, log |-> <<>>, type |-> "accept"], [b |-> 2, log |-> <<[b |-> 2, v |-> "w"]>>, type |-> "accept"], [b |-> 1, acc |-> 0, log |-> <<>>, from |-> 3, type |-> "promise"], [b |-> 2, acc |-> 0, log |-> <<>>, from |-> 3, type |-> "promise"]},st |-> <<[prom |-> 1, acc |-> 1, log |-> <<>>, dec |-> 0], [prom |-> 2, acc |-> 2, log |-> <<[b |-> 2, v |-> "w"]>>, dec |-> 0], [prom |-> 2, acc |-> 2, log |-> <<[b |-> 2, v |-> "w"]>>, dec |-> 0]>>,ver |-> <<0, 1, 1>>,damages |-> 0,count |-> <<0, 0, 0>>,crashes |-> 0,lead |-> <<[b |-> 1, log |-> <<>>, phase |-> "accept", promises |-> {[acc |-> 0, log |-> <<>>, from |-> 1], [acc |-> 0, log |-> <<>>, from |-> 3]}, acks |-> <<0, 0, 0>>, sent_stop |-> FALSE], [b |-> 2, log |-> <<[b |-> 2, v |-> "w"]>>, phase |-> "accept", promises |-> {[acc |-> 0, log |-> <<>>, from |-> 2], [acc |-> 0, log |-> <<>>, from |-> 3]}, acks |-> <<0, 0, 0>>, sent_stop |-> FALSE], [b |-> 0, log |-> <<>>, phase |-> "none", promises |-> {}, acks |-> <<0, 0, 0>>, sent_stop |-> FALSE]>>,chosen |-> <<>>,stopped_by |-> {},damaged |-> <<0, 0, 0>>,stops |-> 0,mark |-> <<-1, -1, -1>>,status |-> <<"running", "running", "running">>]),
    ([msgs |-> {[b |-> 1, type |-> "prepare"], [b |-> 2, type |-> "prepare"], [b |-> 1, log |-> <<>>, type |-> "accept"], [b |-> 2, log |-> <<>>, type |-> "accept"], [b |-> 2, log |-> <<[b |-> 2, v |-> "w"]>>, type |-> "accept"], [b |-> 2, from |-> 3, type |-> "ack", idx |-> 1], [b |-> 1, acc |-> 0, log |-> <<>>, from |-> 3, type |-> "promise"], [b |-> 2, acc |-> 0, log |-> <<>>, from |-> 3, type |-> "promise"]},st |-> <<[prom |-> 1, acc |-> 1, log |-> <<>>, dec |-> 0], [prom |-> 2, acc |-> 2, log |-> <<[b |-> 2, v |-> "w"]>>, dec |-> 0], [prom |-> 2, acc |-> 2, log |-> <<[b |-> 2, v |-> "w"]>>, dec |-> 0]>>,ver |-> <<0, 1, 1>>,damages |-> 0,count |-> <<0, 0, 0>>,crashes |-> 0,lead |-> <<[b |-> 1, log |-> <<>>, phase |-> "accept", promises |-> {[acc |-> 0, log |-> <<>>, from |-> 1], [acc |-> 0, log |-> <<>>, from |-> 3]}, acks |-> <<0, 0, 0>>, sent_stop |-> FALSE], [b |-> 2, log |-> <<[b |-> 2, v |-> "w"]>>, phase |-> "accept", promises |-> {[acc |-> 0, log |-> <<>>, from |-> 2], [acc |-> 0, log |-> <<>>, from |-> 3]}, acks |-> <<0, 0, 0>>, sent_stop |-> FALSE], [b |-> 0, log |-> <<>>, phase |-> "none", promises |-> {}, acks |-> <<0, 0, 0>>, sent_stop |-> FALSE]>>,chosen |-> <<>>,stopped_by |-> {},damaged |-> <<0, 0, 0>>,stops |-> 0,mark |-> <<-1, -1, -1>>,status |-> <<"running", "running", "running">>]),
    ([msgs |-> {[b |-> 1, type |-> "prepare"], [b |-> 2, type |-> "prepare"], [b |-> 1, log |-> <<>>, type |-> "accept"], [b |-> 2, log |-> <<>>, type |-> "accept"], [b |-> 2, log |-> <<[b |-> 2, v |-> "w"]>>, type |-> "accept"], [b |-> 2, type |-> "decide", idx |-> 1], [b |-> 2, from |-> 3, type |-> "ack", idx |-> 1], [b |-> 1, acc |-> 0, log |-> <<>>, from |-> 3, type |-> "promise"], [b |-> 2, acc |-> 0, log |-> <<>>, from |-> 3, type |-> "promise"]},st |-> <<[prom |-> 1, acc |-> 1, log |-> <<>>, dec |-> 0], [prom |-> 2, acc |-> 2, log |-> <<[b |-> 2, v |-> "w"]>>, dec |-> 1], [prom |-> 2, acc |-> 2, log |-> <<[b |-> 2, v |-> "w"]>>, dec |-> 0]>>,ver |-> <<0, 1, 1>>,damages |-> 0,count |-> <<0, 0, 0>>,crashes |-> 0,lead |-> <<[b |-> 1, log |-> <<>>, phase |-> "accept", promises |-> {[acc |-> 0, log |-> <<>>, from |-> 1], [acc |-> 0, log |-> <<>>, from |-> 3]}, acks |-> <<0, 0, 0>>, sent_stop |-> FALSE], [b |-> 2, log |-> <<[b |-> 2, v |-> "w"]>>, phase |-> "accept", promises |-> {[acc |-> 0, log |-> <<>>, from |-> 2], [acc |-> 0, log |-> <<>>, from |-> 3]}, acks |-> <<0, 1, 1>>, sent_stop |-> FALSE], [b |-> 0, log |-> <<>>, phase |-> "none", promises |-> {}, acks |-> <<0, 0, 0>>, sent_stop |-> FALSE]>>,chosen |-> <<[b |-> 2, v |-> "w"]>>,stopped_by |-> {},damaged |-> <<0, 0, 0>>,stops |-> 0,mark |-> <<-1, -1, -1>>,status |-> <<"running", "running", "running">>])
    >>
----


=============================================================================

---- CONFIG ClusterStop_TTrace_1794238297 ----
CONSTANTS
    Nodes = { 1 , 2 , 3 }
    MaxBallot = 3
    MaxLog = 2
    MaxCrash = 1
    MaxDamage = 1
    MaxStop = 1
    AutoRepairs = 1
    Mutation = "MarkBelowDamage"

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
\* Generated on Mon Nov 09 15:32:18 UTC 2026