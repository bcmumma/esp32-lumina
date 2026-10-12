---
name: start-milestone
description: Start or resume an authorized Lumina phase. Run as /start-milestone Phase <N>. Running it authorizes that phase only.
disable-model-invocation: true
---

Phase requested: $ARGUMENTS

1. **Authorization.** Running this skill authorizes only the phase named above. If no phase is named, or it doesn't match the current or next phase of either track in `docs/PROJECT_STATE.md`, stop and ask.
2. **Sync and branch.** Run `git pull`, `git status`, and `git branch --show-current`. Stop and ask if the pull conflicts or uncommitted changes could be overwritten. Work on `develop-v23` (docs and owner-led phases) or the matching feature branch (firmware phases); never on `main`. If the branch you need doesn't exist, propose creating it from `develop-v23` and wait for my OK.
3. **Read only what's needed:**
   - `docs/PROJECT_STATE.md`
   - sections 2 and 3 of `docs/PROJECT_INSTRUCTIONS.md`
   - this phase's row and notes in `docs/ROADMAP.md`
   - `docs/HARDWARE.md` and `docs/KNOWN_ISSUES.md` for firmware work
   - the files the phase will change

   Don't scan the whole repo.
4. **Plan briefly.** Put any wiring changes, parts needed, and owner checks at the top. Then give the steps and the full pass checklist, marking each item *(Claude)* or *(owner)*. For owner-led GUI phases (SoundSwitch and Serato on the Mac), write exact click-by-click steps instead of code. If the phase is **in progress**, start from what `PROJECT_STATE.md` says is done.
5. **Stop and ask** if the plan:
   - needs a decision from me (spending, a real tradeoff, network or firewall changes, a merge to `main`);
   - mixes in later-phase work;
   - would change architecture and add features in the same step;
   - or needs anything the safety rules say to ask about.

   Otherwise, continue.
6. **Update `docs/PROJECT_STATE.md`:** set Status to `In progress` and Authorization to `Authorized <date> for Phase <N>`, and record the branch.
7. **Implement** in ordered, testable steps:
   - **Firmware:** build with `pio run -e <env>`, upload, and capture a time-limited serial log after each meaningful change.
   - **Captures:** analyze with `tshark` and keep raw files out of Git.
   - **Owner steps:** write the step (with its expected result) under Owner checklist in `PROJECT_STATE.md`, tell me, and wait for my result.

   When every pass-checklist item is done, tell me to run `/close-milestone`.
