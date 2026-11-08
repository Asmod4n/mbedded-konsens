---------------------------- MODULE DataRowCrc ----------------------------
(* A row of the table data holds a value and a CRC32C.                    *)
(* The model asks one question: does a read ever give a client a value    *)
(* that differs from the value that the node wrote last?                  *)
(* The CRC is an abstraction with no collision: it is the tuple of what   *)
(* it covers. A real CRC32C misses a random damage with a chance of       *)
(* 2^-32. CRC32C is linear, so two keys that a client chooses can give    *)
(* the same CRC; a misdirected write is a random fault, not a choice.     *)
(* truth is the state that the decided log gives. The node writes the     *)
(* log before the state, so the log holds every write. A repair builds    *)
(* the state again from the log: every row, not only the row that the     *)
(* read found.                                                            *)
EXTENDS Naturals, FiniteSets

CONSTANTS Keys, Values, None,
          CheckOnRead,     \* the read compares the stored CRC
          CrcCoversKey,    \* the CRC covers the key of the row
          RepairFromLog,   \* the repair writes the rows that the log gives
          AutoRepairs,     \* repairs that start without the operator
          MaxDamages

VARIABLES truth,    \* key -> the value of the decided log, or None
          disk,     \* key -> [v, crc] as the disk gives it, or None
          damaged,  \* a read found a damage; the node does no work
          waits,    \* the node waits for the operator
          repairs,  \* the durable count of repairs
          wrong,    \* a reply held a value that the node did not write last
          damages

vars == <<truth, disk, damaged, waits, repairs, wrong, damages>>

Crc(k, v) == IF CrcCoversKey THEN <<k, v>> ELSE <<None, v>>
Row(k, v) == [v |-> v, crc |-> Crc(k, v)]
RowOf(k) == IF truth[k] = None THEN None ELSE Row(k, truth[k])

Init == /\ truth = [k \in Keys |-> None]
        /\ disk = [k \in Keys |-> None]
        /\ damaged = FALSE
        /\ waits = FALSE
        /\ repairs = 0
        /\ wrong = FALSE
        /\ damages = 0

Write(k, v) ==
    /\ ~damaged
    /\ truth' = [truth EXCEPT ![k] = v]
    /\ disk' = [disk EXCEPT ![k] = Row(k, v)]
    /\ UNCHANGED <<damaged, waits, repairs, wrong, damages>>

Erase(k) ==
    /\ ~damaged
    /\ truth[k] # None
    /\ truth' = [truth EXCEPT ![k] = None]
    /\ disk' = [disk EXCEPT ![k] = None]
    /\ UNCHANGED <<damaged, waits, repairs, wrong, damages>>

\* A bit of the value flips. The stored CRC stays.
Flip(k, v) ==
    /\ damages < MaxDamages
    /\ disk[k] # None
    /\ disk[k].v # v
    /\ disk' = [disk EXCEPT ![k] = [v |-> v, crc |-> disk[k].crc]]
    /\ damages' = damages + 1
    /\ UNCHANGED <<truth, damaged, waits, repairs, wrong>>

\* A bit of the stored CRC flips. The value stays.
FlipCrc(k) ==
    /\ damages < MaxDamages
    /\ disk[k] # None
    /\ disk' = [disk EXCEPT ![k] = [v |-> disk[k].v, crc |-> <<None, None>>]]
    /\ damages' = damages + 1
    /\ UNCHANGED <<truth, damaged, waits, repairs, wrong>>

\* The bytes of the row of k1 stand at the place of the row of k2.
\* A second misdirect can carry an older row of k2 back to k2. That is a
\* lost write, and a lost write does not occur: the node writes the log
\* before the state (owner, 2026-11-08). The guard keeps it out.
Misdirect(k1, k2) ==
    /\ damages < MaxDamages
    /\ k1 # k2
    /\ disk[k1] # None
    /\ disk[k1].crc[1] # k2
    /\ disk[k2] # disk[k1]
    /\ disk' = [disk EXCEPT ![k2] = disk[k1]]
    /\ damages' = damages + 1
    /\ UNCHANGED <<truth, damaged, waits, repairs, wrong>>

\* A read that finds a damage gives no reply and stops the work. The
\* first AutoRepairs repairs start at once; after them the node waits.
Read(k) ==
    /\ ~damaged
    /\ IF disk[k] # None /\ CheckOnRead /\ disk[k].crc # Crc(k, disk[k].v)
       THEN /\ damaged' = TRUE
            /\ waits' = (repairs >= AutoRepairs)
            /\ UNCHANGED wrong
       ELSE /\ wrong' = (wrong \/ (IF disk[k] = None THEN None ELSE disk[k].v) # truth[k])
            /\ UNCHANGED <<damaged, waits>>
    /\ UNCHANGED <<truth, disk, repairs, damages>>

\* The repair writes every row again in one transaction. The mutation
\* RepairFromLog = FALSE computes a new CRC over the stored value.
Repair ==
    /\ damaged
    /\ ~waits
    /\ disk' = IF RepairFromLog
               THEN [k \in Keys |-> RowOf(k)]
               ELSE [k \in Keys |-> IF disk[k] = None THEN None
                                    ELSE Row(k, disk[k].v)]
    /\ damaged' = FALSE
    /\ repairs' = repairs + 1
    /\ UNCHANGED <<truth, waits, wrong, damages>>

\* The operator allows one more repair.
OperatorAllowsRepair ==
    /\ waits
    /\ waits' = FALSE
    /\ UNCHANGED <<truth, disk, damaged, repairs, wrong, damages>>

Next == \/ \E k \in Keys, v \in Values : Write(k, v) \/ Flip(k, v)
        \/ \E k \in Keys : Erase(k) \/ Read(k) \/ FlipCrc(k)
        \/ \E k1, k2 \in Keys : Misdirect(k1, k2)
        \/ Repair
        \/ OperatorAllowsRepair

Spec == Init /\ [][Next]_vars

ReadGivesTheLastWrite == ~wrong

\* Covers: each is violated in some run.
NoRepair == repairs = 0
NeverWaits == ~waits
NoSecondRepair == repairs < 2
=============================================================================
