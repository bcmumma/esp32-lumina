# Project State

_Last updated: 2026-10-28_

## Current

The DJ and firmware tracks merged at Phase 10. Phases 10–16 are done; the ROADMAP has no Phase 17 yet.

| Track | Phase | Status | Authorization |
|---|---|---|---|
| DJ (Mac + PC capture) | Complete (Phases 4–6) | Done | n/a |
| Firmware | 17: not defined yet (draft below) | Not started | Not authorized |

- Authorize a phase with `/start-milestone Phase <N>` in Claude Code. Until then, everything is read-only.
- **Branch:** `main` holds v2 unchanged. `develop-v23` = `6b3d860` (Phases 7–15). Phase 16 is on `feature/show-hardening`, ready to fast-forward `develop-v23` (owner OK needed).
- **Last verified commit:** `313c620` on `feature/show-hardening` (Phase 16, closed 2026-10-28).
- **Boards:** all on Phase 16 firmware. Receivers update over Wi-Fi with `tools/ota_update.py --room N`; USB only for recovery.
  - **Room 1** (`receiver`): strand 1 GPIO 33, strand 2 GPIO 25 (on, 450 LEDs; turn it off if it isn't wired, room 1 then runs about twice as fast).
  - **Room 2** (`receiver_room2`): strand 3, GPIO 26.
  - **Saved:** Standalone source, layout L1, brightness 106, strobe off at boot.

## What works

- **Firmware:** v3 with v2 parity; 78/78 on-device unit tests; all builds warning-free.
- **SoundSwitch:** six layouts L1–L6 with the Lumina FX fixture (beam, kick looks, UV sparkle, gated FX strobe); Auto chain SoundSwitch → Hybrid → Audio.
- **Looks:** House, Techno, Drum & Bass, Acid, General, Chill, Halloween, Meters; 7 kick looks; Setup can hide buttons.
- **Two rooms from one S3:** same show, own wiring. Each room reports online, fps, loss, and signal on the page (offline within about 5 s, owner-checked) and updates over Wi-Fi.
- **Reliability:** 60-minute bench soak: both rooms online throughout, loss 0.02% / 0.01%, no restarts, S3 memory steady.
- **Power:** each room's 90 W supply draws 3.7 A max at full white (owner, measured).
- **Strobe:** off at boot, 15/s ceiling, one gate per room.
- **Phases done:** 0–16.

## Next phase (draft, not authorized)

Phase 17 isn't on the ROADMAP. A proposal, for the owner to change or replace:

**Phase 17: first live show run**
- [ ] One hour with real SoundSwitch in Auto, both rooms; Claude watches heartbeats and S3 memory (pass as in Phase 16) *(owner runs SoundSwitch; Claude)*
- [ ] Owner look check: SoundSwitch cells, Hybrid, Audio, the Halloween looks, the newer kick looks *(owner)*
- [ ] Fix whatever the run turns up, one change at a time, updated over Wi-Fi *(Claude)*

## Owner, when convenient

- [ ] A 5 A fuse on each room's 12 V feed, at the supply (`docs/HARDWARE.md`).
- [ ] Confirm the 3.7 A was measured with Brightness at 255 (the saved brightness is 106). If it was at 106, full brightness can draw about 2.5 times more; measure once at 255.
- [ ] Bench audio: make the Modi/Magni the Windows default output (Voicemeeter may have taken over), so Claude can run tone tests again.

## Open questions

- Mac model; dev board models.
- Why the WROOMs drop off USB now and then (less important now that updates go over Wi-Fi).
- Why SoundSwitch sometimes outputs all-zero frames.

## Recent decisions

See `docs/DECISIONS.md`. Latest (2026-10-25 to 2026-10-28):
- Receiver updates over Wi-Fi, started from the S3 over ESP-NOW.
- Receiver loss counts only packets that never arrived.
- One 90 W supply per room, no brightness cap (measured 3.7 A per room).
- Two room receivers from one S3; Halloween genre; colour buttons on the receiver's colour wheel.
