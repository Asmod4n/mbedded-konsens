# Review 1 of the plan "the storage is the only source"

Reviewer: a second agent, 2026-10-24, on branch lmdb-only-source at
fe79246. The text below is the report of the reviewer. A note after its findings names what was removed.

My task: I reviewed doc/plan-storage-only-source.md and model/tla/SequencePaxos.tla at fe79246 before any code exists. I checked them against the code at the base, the four run files, the three bench files and RULES.md. I edited, committed, built and ran nothing, and I did not question the owner's decisions. Where I looked: `git diff 06a52bf HEAD -- include src` is empty, so the line numbers in section 1 apply to HEAD.

Section 1 facts. I checked all of them except the ones listed as not checked:
- About 75 cited lines in sequence_paxos.hpp, node.hpp:127/221/374/466, persistent_storage.cpp:31-33/192-200/403/454/499/515/538 and storage.hpp:121-129. All hold. Small points: `:953` is `if (source == pid_)`, and its use of the fields is at `:954`. In plan line 63, `:276-301` stands under a node.hpp bullet but means sequence_paxos.hpp.
- Not checked: the page-cache tests (fdatasync, fadvise of a mapped file, "0 of 64" pages) and "360 of 527" (plan :85-98). No result file holds them, and commit 421e72a changed only the doc. This breaks the rule that each run of a bench is one file.

Findings:
1. Removed (see the note after the list).
2. Removed (see the note after the list).
3. Plan :181-183 (2.3.3). This is wrong. A lost entry in the middle leaves the last entry and the meta record equal, so the chain check cannot find it. Only the continuity check of 2.1 item 4 finds it. The model's `k < Len(log[i])` branch is the continuity check, so the model is right and the text is wrong. Fix: say this.
4. Model :1532-1546, MetaRecord. This detection is too strong. DamagedStates (:1444-1455) only sets a value to its default, and MetaRecord detects every such damage. The disk can also give back an older meta record whose CRC is valid. Example: a promise-only step whose leaf write is lost. The record then has a valid CRC, the same last index, the same chain CRC and an older promise. No check finds it, and that is the round-3 S1 trace. With MetaRecord on, every injected fault is found at once, so round 4 only shows that a node which stops is safe. That result proves nothing about faults the checks miss. Fix: add a kind `stale_meta` (an earlier value from past_durable with the log unchanged) and a kind "rollback of entries plus meta record while running" (past_durable without Restart; Rollback :1380 exists only as a restart). Run both. Then the owner decides what finds them, for example peers that compare promises.
5. Model :1541-1546 detects in the same action as the fault, so in the model a node never votes on a fault that its checks can find. That matches the code only if every path that sends runs the check first. In the code it is not so:
   - node.hpp:127 (hb_timeout and ballot_above_promise :374), :221 and :466 read get_promise between steps. Plan 2.1.3 gives these getters no check.
   - A page that the cache drops and reads again from disk in the middle of a step comes after that step's check.

   Fix: (a) the getters of 2.1.3 call the same check function, or the node reads them inside the step transaction. (b) In the model, the fault sets a pending mark, and an action of node i is enabled only if the check ran first. Or state that the model allows one step on damaged state.
6. Removed (see the note after the list).
7. Plan :99-104 and 2.4. One page stayed in the cache in 19 of 84 runs, and the plan says nothing about what the node does then. The kernel also skips a page that another process maps. Fix: check mincore after the page-out. A page that stays is not a read-back. The node then pages it out again or does not report. This is an error path, so the owner decides.
8. Removed (see the note after the list).
9. Plan :162-163 and :167-169. The entry CRC covers the state, but the state differs between nodes (in work or committed, depending on each node's decided index). Today, recovery compares last_crc across nodes (sequence_paxos.hpp:899, :936, :994). That comparison would fail on equal logs. The result of 2.5.1 is stored in the entry, but no CRC covers it. Fix: every comparison across nodes uses the chain CRC. Name the CRC that covers the result.
10. Plan :157-161 (2.2.2-3). Two problems:
    - Every path that writes entries or the decided index must set the states. That is recovery (model :1048), build (:1944), force (:2017), and the majority of promises (:777). If it does not, 2.2.3 finds a false damage.
    - A check of "every entry up to d" reads the whole log at each step.

    Plan 3.1 :270-273 says there is nothing to model in the state field, but the field can fail in both directions. Fix: name every writer. Check only the entries at d and d+1. Add a test for each path.
11. Plan :221-227 (2.5). A follower runs the client work in a nested transaction before its accepted, so before the decide, and the step commits that work durably. accept_sync can then truncate that entry (:804-810), and its effects stay. Fix: say how the effects of an entry that is never decided are removed. Options are an undo kept in the storage, or a nested transaction that is aborted and run again at commit. This is a new design, so the owner decides.
12. Plan :137-140 and :200-202. The plan says "stop voting and start recovery", but the model takes the node out of running for good (refused). The recovery of a damaged node is not modelled. The recovery may pick the node's own state as its source (:953-954, which puts its own pid first). Fix: model the recovery from a peer's copy (as StorageFault :1507), or say that the node stays stopped.
13. Rules:
    - 2.1.3 "the pool": no pool exists, and the name is made here.
    - 2.1.7: the name "transaction" hides konsens::transaction (node.hpp:70).
    - 2.4.2: "reports the proof" (to whom, in what form) is an error path that is not marked provisional.
    - 2.8: the migration of an old store when the format version goes up is not described.
    - Item 10 of the start-checks plan ("no change to the file format") becomes out of date.
14. Model :1048 (recovery) and :777 raise decided_idx with no CheckedDecided. Plan 2.4.5 names only decide, accept_decide and accept_sync. Fix: also name the recovery and new_state paths, or state why they are safe.
15. The configs set MaxTimeout = 0, MaxLoss = 0 and MaxCrash = 0, so no resend is ever modelled together with a running fault. Fix: run one config with MaxTimeout = 1 after finding 5.

Findings 1, 2, 6 and 8 named only the former storage. Their former form is in the git history at ebd9f42.

Verdict: reject in this form. Findings 4, 5, 6, 9 and 11 change the code or show that the model is too strong. Section 3 needs one more model round with the new fault kinds of finding 4, and the owner decides findings 7, 8, 11 and 12. The other findings are fixes to the text.

