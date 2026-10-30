# Review 3 of the plan "the storage is the only source"

Reviewer: a second agent, 2026-10-25, on branch lmdb-only-source at
86fb1df. The text below is the report of the reviewer. A note after its findings names what was removed.

Task: I review the plan doc/plan-storage-only-source.md and the TLA+ changes since 2f27efa on branch lmdb-only-source, against RULES.md, the results of rounds 9-12 and two bench files. I changed, committed and pushed nothing. I did not question the owner decisions or report the known wrong assumptions again. My one TLC run: S5 plus EntryRunsOnOnePrefix on MC_pipe_net, MC_pipe_crash and MC_pipe_fault, 300 s each, seed 7, `-deadlock`, about 1.7M states each, no violation.

**Verdict: approve after the listed fixes.** Findings 1, 2, 7, 8 and 15 must be fixed first.

**Model against plan**
1. plan:455-460 says `StateInStorage` and `MaxInFlight` exist and that "result in decided entry = result of run after decide" is checked. Neither name is in SequencePaxos.tla or in any cfg (grep). The model has no state, no applied index, no result. Proposal: model them, or rewrite 3.1 to what round 12 checks.
2. tla:2619-2623 `EntryRunsOnOnePrefix` follows from S5 (tla:2650-2652) restricted to voters. S5 held in all three pipe configs in my run, so round 12 checks log matching, not the state transaction. Proposal: add per node `ran[i]` (the prefix the open state txn holds) and `applied[i]`, with actions run, partial decide (abort, rerun) and cut (abort). Invariants: `ran[i]` is a prefix of `log[i]`; applied <= decided; the result of entry k as a function of its prefix is the same at accept and at apply.
3. tla:2626 `NoPipeline` is violated by one AcceptDecide that carries two entries. It does not show two batches in flight. All MC_pipe_* cfgs have `ClientSessions=FALSE` and `MaxTimeout=0`, so the dedupe that 2.6 item 7 and the round-12 form 1 rely on is not checked. Proposal: a cover over chan (two AcceptDecide beyond decided), and a pipe config with ClientSessions TRUE and MaxTimeout 1.
4. plan:376-383 names only `accept_sync` as a cut. These cut too: the leader's own sync in handle_majority_promises (tla:852), the repair cut of 2.4 item 5, the recovery, and the whole-file copy. Proposal: one rule — every write that cuts or replaces the log below the end of the open state txn aborts it.
5. tla:342-349 `CheckedDecided`/`PrefixAt` compare the whole prefix. The code compares the stored chain CRC at d (plan:214-235). The model finds a changed entry below d whose stored CRC at d stays (the old-leaf-page case); the code does not. Proposal: 2.3 item 4 says that VerifyCrc results hold only for faults that keep the stored chain consistent.
6. plan:416-419 says `accepted_idx` goes after round 9. It is still in the model (Mismatch, tla:~1385), and round 12 still checks `AcceptedIdxIsLogLen`. plan:490 (Values {v1,v2}, MaxLog 2) differs from round 12 (v3, MaxLog 3). 3.2 has no questions for rounds 10-12.

**Repair**
7. plan:279-295 (repair by entries) does not touch the state. If the cut index is at or below the applied index, the state holds effects of entries that were replaced. If the peer's decided index is below the own applied index, applied > decided after the repair. Proposal: when cut <= applied, also replace the state by the copy of item 9; keep the maximum decided index where the prefixes agree.
8. plan:305-321 (whole copy):
   - (a) Removed (see the note after finding 16).
   - (b) There are two renames, not atomic, and no fsync of the directory. A crash between them leaves a mixed pair. Proposal: one directory per pair, fsync of the files and the directory, `renameat2(RENAME_EXCHANGE)` or a durable marker that the start checks read.
   - (c) "applied <= decided" is a weak check of consistency. Proposal: the state stores the chain CRC at its applied index; the requester compares it with the log copy.
   - (d) Where the durable repair count lives is not stated. If it is in a replaced file, the copy brings the peer's count and a rollback restores an old count. The model keeps `repairs` outside the durable tuple (tla:244, 253; DamageFound copies only DurableOf, tla:478-481). Proposal: the plan says the count stands outside both replaced files.
9. plan:303-304 leaves "does a damage found by the start checks count?" open, but section 7 lists only decided items. The model already takes "no" (StorageFault, tla:1665ff). Proposal: put it in section 7 as an open question with options.
10. tla:2604 `NoRepairLimitReached` can also be violated by the "no source" ELSE branch of DamageFound (tla:1726-1734), so it does not show that the limit was reached. Proposal: a ghost flag set only when a source existed. A node in `refused` never comes back (tla:2298-2300). The operator's re-admission of a waiting node (plan:300) is not modelled; mark it so.

**Evidence that cannot tell the forms apart**
11. plan:581-585 says "none changes safety", but the comparison does not test it. With HighWater on, no mismatch was reached (round 10). With HighWater off, S1 fails before `MismatchFound` acts (round 11). The "damage" path never ran up to a repair. Proposal: a config where a mismatch is reachable and safety must hold, or write in the plan that the choice has no model evidence.
12. tla:426-436 and 860-861: no cover shows `MaxDecidedVerified` ≠ `MaxDecided`; round 9 says "changed no result". It also compares with lg1 after the sync, so a damaged promiser whose sync the leader adopts passes by construction. Proposal: a cover, and a run with VerifyPromise FALSE that must fail; else call it a defence with no model evidence.

**Pipeline and Execute-Verify**
13. plan:360-372: at each partial decide the bench reruns every batch that is not decided (bench 231534Z, code `for (auto& b : executed) execute(n, b)`). Replays were close to commits, so most decides were partial. Runs per request were not recorded, and they can reach the in-flight depth (64 in the bench). The bound has no name in the plan (rule: each limit one name). The numbers do not say: 64 B log entries, decide simulated as send + rtt, no followers. Proposal: record runs per request and CPU time, with a costlier apply; name the bound; state what was measured.
14. plan:373-375 gives no check for the case where the run after the decide gives another result than the entry holds (apply not deterministic, or damage). Proposal: compare the result CRC at apply; the owner decides the error path. The model has no result CRC.

**Claims about the store**
15. Removed.
16. Removed.

Findings 8 (a), 15 and 16, the list of cited source lines and the check of the copy named only the former storage. Their former form is in the git history at ebd9f42.

**Not checked:** the MC_hw_* and mutation cfgs one by one; the bench of section 1 (194139Z); the round-9 traces.
