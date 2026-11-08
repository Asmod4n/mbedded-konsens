---- MODULE DataRowCrc_TTrace_1794121717 ----
EXTENDS DataRowCrc_TEConstants, DataRowCrc, Sequences, TLCExt, Toolbox, Naturals, TLC

_expression ==
    LET DataRowCrc_TEExpression == INSTANCE DataRowCrc_TEExpression
    IN DataRowCrc_TEExpression!expression
----

_trace ==
    LET DataRowCrc_TETrace == INSTANCE DataRowCrc_TETrace
    IN DataRowCrc_TETrace!trace
----

_inv ==
    ~(
        TLCGet("level") = Len(_TETrace)
        /\
        disk = ((k1 :> [v |-> v1, crc |-> <<none, v1>>] @@ k2 :> [v |-> v1, crc |-> <<none, v1>>]))
        /\
        truth = ((k1 :> v1 @@ k2 :> none))
        /\
        damages = (1)
        /\
        ended = (FALSE)
        /\
        older = ((k1 :> {} @@ k2 :> {}))
        /\
        wrong = (TRUE)
    )
----

_init ==
    /\ ended = _TETrace[1].ended
    /\ older = _TETrace[1].older
    /\ wrong = _TETrace[1].wrong
    /\ disk = _TETrace[1].disk
    /\ truth = _TETrace[1].truth
    /\ damages = _TETrace[1].damages
----

_next ==
    /\ \E i,j \in DOMAIN _TETrace:
        /\ \/ /\ j = i + 1
              /\ i = TLCGet("level")
        /\ ended  = _TETrace[i].ended
        /\ ended' = _TETrace[j].ended
        /\ older  = _TETrace[i].older
        /\ older' = _TETrace[j].older
        /\ wrong  = _TETrace[i].wrong
        /\ wrong' = _TETrace[j].wrong
        /\ disk  = _TETrace[i].disk
        /\ disk' = _TETrace[j].disk
        /\ truth  = _TETrace[i].truth
        /\ truth' = _TETrace[j].truth
        /\ damages  = _TETrace[i].damages
        /\ damages' = _TETrace[j].damages

\* Uncomment the ASSUME below to write the states of the error trace
\* to the given file in Json format. Note that you can pass any tuple
\* to `JsonSerialize`. For example, a sub-sequence of _TETrace.
    \* ASSUME
    \*     LET J == INSTANCE Json
    \*         IN J!JsonSerialize("DataRowCrc_TTrace_1794121717.json", _TETrace)

=============================================================================

 Note that you can extract this module `DataRowCrc_TEExpression`
  to a dedicated file to reuse `expression` (the module in the 
  dedicated `DataRowCrc_TEExpression.tla` file takes precedence 
  over the module `DataRowCrc_TEExpression` below).

---- MODULE DataRowCrc_TEExpression ----
EXTENDS DataRowCrc_TEConstants, DataRowCrc, Sequences, TLCExt, Toolbox, Naturals, TLC

expression == 
    [
        \* To hide variables of the `DataRowCrc` spec from the error trace,
        \* remove the variables below.  The trace will be written in the order
        \* of the fields of this record.
        ended |-> ended
        ,older |-> older
        ,wrong |-> wrong
        ,disk |-> disk
        ,truth |-> truth
        ,damages |-> damages
        
        \* Put additional constant-, state-, and action-level expressions here:
        \* ,_stateNumber |-> _TEPosition
        \* ,_endedUnchanged |-> ended = ended'
        
        \* Format the `ended` variable as Json value.
        \* ,_endedJson |->
        \*     LET J == INSTANCE Json
        \*     IN J!ToJson(ended)
        
        \* Lastly, you may build expressions over arbitrary sets of states by
        \* leveraging the _TETrace operator.  For example, this is how to
        \* count the number of times a spec variable changed up to the current
        \* state in the trace.
        \* ,_endedModCount |->
        \*     LET F[s \in DOMAIN _TETrace] ==
        \*         IF s = 1 THEN 0
        \*         ELSE IF _TETrace[s].ended # _TETrace[s-1].ended
        \*             THEN 1 + F[s-1] ELSE F[s-1]
        \*     IN F[_TEPosition - 1]
    ]

=============================================================================



Parsing and semantic processing can take forever if the trace below is long.
 In this case, it is advised to uncomment the module below to deserialize the
 trace from a generated binary file.

\*
\*---- MODULE DataRowCrc_TETrace ----
\*EXTENDS DataRowCrc_TEConstants, DataRowCrc, IOUtils, TLC
\*
\*trace == IODeserialize("DataRowCrc_TTrace_1794121717.bin", TRUE)
\*
\*=============================================================================
\*

---- MODULE DataRowCrc_TETrace ----
EXTENDS DataRowCrc_TEConstants, DataRowCrc, TLC

trace == 
    <<
    ([disk |-> (k1 :> none @@ k2 :> none),truth |-> (k1 :> none @@ k2 :> none),damages |-> 0,ended |-> FALSE,older |-> (k1 :> {} @@ k2 :> {}),wrong |-> FALSE]),
    ([disk |-> (k1 :> [v |-> v1, crc |-> <<none, v1>>] @@ k2 :> none),truth |-> (k1 :> v1 @@ k2 :> none),damages |-> 0,ended |-> FALSE,older |-> (k1 :> {} @@ k2 :> {}),wrong |-> FALSE]),
    ([disk |-> (k1 :> [v |-> v1, crc |-> <<none, v1>>] @@ k2 :> [v |-> v1, crc |-> <<none, v1>>]),truth |-> (k1 :> v1 @@ k2 :> none),damages |-> 1,ended |-> FALSE,older |-> (k1 :> {} @@ k2 :> {}),wrong |-> FALSE]),
    ([disk |-> (k1 :> [v |-> v1, crc |-> <<none, v1>>] @@ k2 :> [v |-> v1, crc |-> <<none, v1>>]),truth |-> (k1 :> v1 @@ k2 :> none),damages |-> 1,ended |-> FALSE,older |-> (k1 :> {} @@ k2 :> {}),wrong |-> TRUE])
    >>
----


=============================================================================

---- MODULE DataRowCrc_TEConstants ----
EXTENDS DataRowCrc

CONSTANTS k1, k2, v1, v2, none

=============================================================================

---- CONFIG DataRowCrc_TTrace_1794121717 ----
CONSTANTS
    Keys = { k1 , k2 }
    Values = { v1 , v2 }
    None = none
    MaxDamages = 1
    CheckOnRead = TRUE
    CrcCoversKey = FALSE
    WithLostWrite = FALSE
    v1 = v1
    k1 = k1
    none = none
    k2 = k2
    v2 = v2

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
\* Generated on Sun Nov 08 07:08:37 UTC 2026