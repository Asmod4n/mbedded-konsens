---------------------------- MODULE DataRowCrc ----------------------------
(* A row of the table data holds a value and a CRC32C.                    *)
(* The model asks one question: does a read ever give a client a value    *)
(* that differs from the value that the node wrote last?                  *)
(* The CRC is an abstraction with no collision: it is the tuple of what   *)
(* it covers. A real CRC32C misses a random damage with a chance of       *)
(* 2^-32. CRC32C is linear, so two keys that a client chooses can give    *)
(* the same CRC; a misdirected write is a random fault, not a choice.     *)
(* With MaxDamages = 1 a misdirected write cannot act as a lost write;    *)
(* MC_drc_limit_TwoMisdirects shows the case with two.                    *)
EXTENDS Naturals, FiniteSets

CONSTANTS Keys, Values, None,
          CheckOnRead,     \* the read compares the stored CRC
          CrcCoversKey,    \* the CRC covers the key of the row
          WithLostWrite,   \* the disk can give back an older row
          MaxDamages

VARIABLES truth,    \* key -> the value the node wrote last, or None
          disk,     \* key -> [v, crc] as the disk gives it, or None
          older,    \* key -> the rows the disk held before, None for no row
          ended,    \* the process ended at a damage
          wrong,    \* a reply held a value that the node did not write last
          damages

vars == <<truth, disk, older, ended, wrong, damages>>

Crc(k, v) == IF CrcCoversKey THEN <<k, v>> ELSE <<None, v>>
Row(k, v) == [v |-> v, crc |-> Crc(k, v)]

Init == /\ truth = [k \in Keys |-> None]
        /\ disk = [k \in Keys |-> None]
        /\ older = [k \in Keys |-> {}]
        /\ ended = FALSE
        /\ wrong = FALSE
        /\ damages = 0

Write(k, v) ==
    /\ ~ended
    /\ truth' = [truth EXCEPT ![k] = v]
    /\ disk' = [disk EXCEPT ![k] = Row(k, v)]
    /\ older' = [older EXCEPT ![k] = @ \cup {disk[k]}]
    /\ UNCHANGED <<ended, wrong, damages>>

Erase(k) ==
    /\ ~ended
    /\ truth[k] # None
    /\ truth' = [truth EXCEPT ![k] = None]
    /\ disk' = [disk EXCEPT ![k] = None]
    /\ older' = [older EXCEPT ![k] = @ \cup {disk[k]}]
    /\ UNCHANGED <<ended, wrong, damages>>

\* A bit of the value flips. The stored CRC stays.
Flip(k, v) ==
    /\ damages < MaxDamages
    /\ disk[k] # None
    /\ disk[k].v # v
    /\ disk' = [disk EXCEPT ![k] = [v |-> v, crc |-> disk[k].crc]]
    /\ damages' = damages + 1
    /\ UNCHANGED <<truth, older, ended, wrong>>

\* A bit of the stored CRC flips. The value stays.
FlipCrc(k) ==
    /\ damages < MaxDamages
    /\ disk[k] # None
    /\ disk' = [disk EXCEPT ![k] = [v |-> disk[k].v, crc |-> <<None, None>>]]
    /\ damages' = damages + 1
    /\ UNCHANGED <<truth, older, ended, wrong>>

\* The bytes of the row of k1 stand at the place of the row of k2.
Misdirect(k1, k2) ==
    /\ damages < MaxDamages
    /\ k1 # k2
    /\ disk[k1] # None
    /\ disk[k2] # disk[k1]
    /\ disk' = [disk EXCEPT ![k2] = disk[k1]]
    /\ damages' = damages + 1
    /\ UNCHANGED <<truth, older, ended, wrong>>

\* The disk drops the last write of a page and gives an older row back:
\* an older value, a row that an erase removed, or no row where the
\* first write of the key stood.
LostWrite(k, r) ==
    /\ WithLostWrite
    /\ damages < MaxDamages
    /\ r \in older[k]
    /\ disk[k] # r
    /\ disk' = [disk EXCEPT ![k] = r]
    /\ damages' = damages + 1
    /\ UNCHANGED <<truth, older, ended, wrong>>

Read(k) ==
    /\ ~ended
    /\ IF disk[k] # None /\ CheckOnRead /\ disk[k].crc # Crc(k, disk[k].v)
       THEN /\ ended' = TRUE
            /\ UNCHANGED wrong
       ELSE /\ wrong' = (wrong \/ (IF disk[k] = None THEN None ELSE disk[k].v) # truth[k])
            /\ UNCHANGED ended
    /\ UNCHANGED <<truth, disk, older, damages>>

Next == \/ \E k \in Keys, v \in Values : Write(k, v) \/ Flip(k, v)
        \/ \E k \in Keys : Erase(k) \/ Read(k) \/ FlipCrc(k)
        \/ \E k1, k2 \in Keys : Misdirect(k1, k2)
        \/ \E k \in Keys : \E r \in older[k] : LostWrite(k, r)

Spec == Init /\ [][Next]_vars

ReadGivesTheLastWrite == ~wrong

\* Cover: a read finds a damage and the process ends in some run.
ProcessNeverEnds == ~ended
=============================================================================
