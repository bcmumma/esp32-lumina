---
name: close-milestone
description: Finish the current Lumina phase. Verify the gate, confirm owner checks, commit on the v3 branch, record the hash, update docs, and push. Run when a phase's pass checklist is complete.
disable-model-invocation: true
---

Close the current phase in this order. If any step fails, stop and report.

1. **Verify.** Run the applicable checks and show the commands and results:
   - `pio run` for each changed environment
   - native tests if they exist
   - a final serial capture covering the parity checks
   - a capture summary for network phases

   Owner-led phases are verified by my reported results. If anything fails, stop here.
2. **Owner checks.** Confirm I've reported a result for every owner item. If any are missing, list them with exact steps and expected results, then wait. Record my results as evidence.
3. **Definition of done.** Walk through the definition of done in `docs/PROJECT_INSTRUCTIONS.md` section 3. List anything unmet, and stop if it blocks closing.
4. **Public-repo check.** Run `git status` and `git diff --cached`. Make sure nothing private is staged: credentials, IPs, MACs, captures, `CLAUDE.local.md`, or secrets headers. Confirm you're **not on `main`**.
5. **Commit the work** as one coherent commit: `Phase <N>: <short summary>`, and note the hash. Owner-only phases with no code changes skip this step.
6. **Update docs.** Change only what changed in substance, and remove stale lines.
   - `docs/MILESTONES.md`: date, what was done, the evidence, and the commit hash (or n/a).
   - `docs/HARDWARE.md`, `docs/ARCHITECTURE.md` (including sanitized capture summaries), `docs/DECISIONS.md`, and `docs/KNOWN_ISSUES.md` as needed.
   - `README.md` and the Commands section of `CLAUDE.md`, if build, flash, or capture commands changed.
7. **Rewrite `docs/PROJECT_STATE.md`** (keep it under one page):
   - That track's next phase: Not started, not authorized. Leave the other track's status as it is.
   - Last verified commit and branch.
   - A draft checklist for the next phase.
   - Clear completed owner checks.
   - Update What works, Open questions, Recent decisions, and the date.
8. **Commit and push:** commit as `docs: close Phase <N>`, then `git push`. Don't merge to `main`; if this phase is a good merge point, say so.
9. **Report** in a few lines: what finished, the evidence, the commit hashes and branch, whether this is a merge candidate for `main`, and the proposed next phase in one sentence. Then wait.
