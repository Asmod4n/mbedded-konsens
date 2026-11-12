# Review of plan-promise-again.md, round 1

Verdict: approve with changes. All changes are applied in the plan.

- The new condition of HandlePrepare matches the guard of
  `handle_prepare` in every role and phase. In model and code the phase
  Recover always comes with the role Follower, so the old condition is a
  part of the new one.
- The THEN branch matches the code for the new path. Two older
  differences stay: the code computes the promise again at each resend,
  the model resends the cached message; the model sends through
  `IfVotes`, the code does not check in `handle_prepare`.
- `Mutation = "NoPromiseAgain"` is the old condition exactly. No
  violation is expected under it.
- The check of section 3 did not reach the new path: MC_base,
  MC_base_monotone and MC_base_s7 have no loss, no duplicate and no
  timeout, and channels are FIFO. MC_a_F1_dup.cfg and MC_a_F1_loss.cfg
  reach it.

Changes asked for: add MC_a_F1_dup and MC_a_F1_loss; show that the path
is reached by a cover; add the invariant that Recover implies Follower;
name the open differences; say what is expected under the mutation.
