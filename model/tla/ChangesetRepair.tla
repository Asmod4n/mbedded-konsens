---------------------------- MODULE ChangesetRepair ----------------------------
(***************************************************************************)
(* The repair of a state by a changeset of the SQLite session extension    *)
(* (plan-sqlite-storage.md 7, owner 2026-10-26 18:22 "ausprobieren und     *)
(* modellieren").                                                          *)
(*                                                                         *)
(* A state is a function from keys to values. The peer keeps the state at  *)
(* a snapshot index and the state at its applied index. The changeset is   *)
(* sqlite3session_diff of the two: for each key that differs, the old and  *)
(* the new value. sqlite3changeset_apply checks the old value of each row  *)
(* it changes; a mismatch is a conflict, and the handler aborts. A damage  *)
(* changes one value of the node and nothing else.                         *)
(***************************************************************************)
EXTENDS Naturals, FiniteSets, TLC

CONSTANTS Keys, Values, MaxIdx, MaxDamage,
          DigestCheck      \* the node compares the digest of its state with the one of the peer

VARIABLES node, nodeIdx, phase, damages, writes

vars == <<node, nodeIdx, phase, damages, writes>>

\* The decided writes: the write of index i sets one key to one value.
\* TLC checks every sequence of writes of length MaxIdx.

V0 == CHOOSE v \in Values : TRUE
Zero == [k \in Keys |-> V0]

RECURSIVE StateAt(_)
StateAt(i) == IF i = 0 THEN Zero
              ELSE [StateAt(i - 1) EXCEPT ![writes[i][1]] = writes[i][2]]

\* The peer is a voting node whose state is whole.
PeerIdx == MaxIdx

Changeset(s) == {<<k, StateAt(s)[k], StateAt(PeerIdx)[k]>> :
                    k \in {x \in Keys : StateAt(s)[x] # StateAt(PeerIdx)[x]}}

Init ==
    /\ writes \in [1..MaxIdx -> Keys \X Values]
    /\ nodeIdx \in 0..MaxIdx
    /\ node = StateAt(nodeIdx)
    /\ phase = "behind"
    /\ damages = 0

\* A bit flips in a value of the node.
Damage ==
    /\ phase = "behind"
    /\ damages < MaxDamage
    /\ \E k \in Keys, v \in Values :
          /\ v # node[k]
          /\ node' = [node EXCEPT ![k] = v]
    /\ damages' = damages + 1
    /\ UNCHANGED <<nodeIdx, phase, writes>>

\* The peer sends the changeset from the snapshot that the node names.
ApplyChangeset ==
    /\ phase = "behind"
    /\ LET C == Changeset(nodeIdx)
       IN IF \A c \in C : node[c[1]] = c[2]
          THEN /\ node' = [k \in Keys |-> IF \E c \in C : c[1] = k
                                          THEN (CHOOSE c \in C : c[1] = k)[3]
                                          ELSE node[k]]
               /\ phase' = "applied"
          ELSE /\ phase' = "copy"
               /\ UNCHANGED node
    /\ UNCHANGED <<nodeIdx, damages, writes>>

\* With DigestCheck the node compares the digest of its whole state with
\* the digest of the peer at the same index; a difference asks for the
\* full copy.
Verify ==
    /\ phase = "applied"
    /\ IF DigestCheck /\ node # StateAt(PeerIdx)
       THEN phase' = "copy" /\ UNCHANGED node
       ELSE phase' = "done" /\ UNCHANGED node
    /\ UNCHANGED <<nodeIdx, damages, writes>>

\* The full copy (VACUUM INTO at the peer).
FullCopy ==
    /\ phase = "copy"
    /\ node' = StateAt(PeerIdx)
    /\ phase' = "done"
    /\ UNCHANGED <<nodeIdx, damages, writes>>

Next == Damage \/ ApplyChangeset \/ Verify \/ FullCopy

Spec == Init /\ [][Next]_vars

\* A repaired node holds the state of the peer.
RepairIsWhole == phase = "done" => node = StateAt(PeerIdx)

\* Cover: a changeset repair without a full copy ends. TLC must find this violated.
CoverChangesetDone == ~(phase = "done" /\ damages = 0 /\ nodeIdx < PeerIdx)
=============================================================================
