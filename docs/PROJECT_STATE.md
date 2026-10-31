# Project State

_Last updated: 2026-10-31_

## Current

The DJ and firmware tracks merged at Phase 10. Phases 10–16 are done; the ROADMAP has no Phase 17 yet.

| Track | Phase | Status | Authorization |
|---|---|---|---|
| DJ (Mac + PC capture) | Complete (Phases 4–6) | Done | n/a |
| Firmware | 17: not defined yet (draft below) | Not started | Not authorized |

- Authorize a phase with `/start-milestone Phase <N>` in Claude Code. Until then, everything is read-only.
- **Branch:** `main` holds v2 unchanged. `develop-v23` = `6b3d860` (Phases 7–15). Phase 16 is on `feature/show-hardening`, ready to fast-forward `develop-v23` (owner OK needed). The DJ changes (below) are on `feature/dj-feedback`, branched from Phase 16.
- **Last verified commit:** `313c620` on `feature/show-hardening` (Phase 16, closed 2026-10-28).
- **Boards:** all on Phase 16 firmware. Receivers update over Wi-Fi with `tools/ota_update.py --room N`; USB only for recovery.
  - **Room 1** (`receiver`): strand 1 GPIO 33, strand 2 GPIO 25 (on, 450 LEDs; turn it off if it isn't wired, room 1 then runs about twice as fast).
  - **Room 2** (`receiver_room2`): strand 3, GPIO 26.
  - **Saved:** Standalone source, layout L1, brightness 106, strobe off at boot.

## What works

- **Firmware:** v3 with v2 parity; 78/78 on-device unit tests; all builds warning-free.
- **SoundSwitch:** six layouts L1–L6 with the Lumina FX fixture (beam, kick looks, UV sparkle, gated FX strobe); Auto chain SoundSwitch → Hybrid → Audio.
- **Looks:** Club, General, Chill, Halloween on the page (House, Techno, Drum & Bass, Acid, Meters retired but kept); 7 kick looks; Setup can hide buttons.
- **Two rooms from one S3:** same show, own wiring. Each room reports online, fps, loss, and signal on the page (offline within about 5 s, owner-checked) and updates over Wi-Fi.
- **Reliability:** 60-minute bench soak: both rooms online throughout, loss 0.02% / 0.01%, no restarts, S3 memory steady.
- **Power:** each room's 90 W supply draws 3.7 A max at full white (owner, measured).
- **Strobe:** off at boot, 15/s ceiling, one gate per room.
- **Phases done:** 0–16.

## DJ feedback changes (owner request, 2026-10-31)

A change request, not a phase: the DJs who will run the lights asked for these. Branch `feature/dj-feedback`.

- [x] Strobe colour (all flashes, rig-wide) and colour moods (8 two-colour ranges, everything but Halloween) in spare strand bytes; packet unchanged (v12, 107 bytes) *(Claude)*
- [x] Club genre: Hyper Strobe and DnB Split plus 46 Hyper Scatter, 47 Hyper Grid, 48 Hyper Mirror, 49 Split Mirror, 50 Split Blocks, 51 Split Strobe; House, Techno, Drum & Bass, Acid, Meters retired (Setup can show them) *(Claude)*
- [x] Strobe sensitivity (`lib/StrobeAuto`): with Strobe on, flashes only in loud parts and from the drop; 255 = Always (default) *(Claude)*
- [x] Page: Live · Advanced · Setup (owner's names); Live = brightness, six chosen looks, moods, big Strobe button, strobe colour, sensitivity; Advanced hides panels the source doesn't use; sliders remembered per look (`/api/feel`) and never snapped back by polls; autosave replaces the Save bar *(Claude)*
- [x] Spaced cells for the L11 setup (owner): Advanced → SoundSwitch looks → Spaced cells, with a Gap slider *(Claude)*
- [x] Status light switch (Advanced, dark by default) *(Claude)*. **Finding:** the S3's RGB LED is on GPIO48 with the PCM1808 data, so no switch can turn it off on this wiring (GPIO38 test: no effect); the options are in `docs/HARDWARE.md` *(owner decides)*
- [x] Builds warning-free; page script parses (esprima); on-device tests 92/92 plus cells 10/10 (new gap test) *(Claude)*
- [x] Sender flashed over USB; boot log clean; new API fields; autosave and per-look sliders survive a restart; strobe gate closed in silence at sensitivity 128, open at Always *(Claude)*
- [x] Boot log: the S3 logged an NVS error at boot for a Live-page key that was never saved; both new keys are now checked first (boot log clean, re-flashed) *(Claude)*
- [ ] Receivers: update both over Wi-Fi (`tools/ota_update.py --room 1|2`); until then they run the old look code (white strobe, no moods, no gaps; Club modes show Solid) *(Claude)*. **Blocked (2026-10-31):** the S3 heard no heartbeat from either room; the owner found the cause: the S3 is at the PC (on USB for flashing), out of radio range of the rooms. The update needs no USB: the PC reaches the S3 over the home network, the S3 wakes a room over ESP-NOW, and the room joins home Wi-Fi to take the upload.
- [ ] Owner: move the S3 (with the PCM1808) to its show spot, in range of both rooms, on any USB charger, and tell Claude. Claude then checks each room's signal and loss and updates both over Wi-Fi. The S3 itself has no Wi-Fi update, so a later change to the S3 or its page needs USB again *(owner)*
- [ ] Owner/DJ look check: the six Club modes, moods, strobe colour, spaced cells, and strobe sensitivity through a real breakdown and drop *(owner)*
- [ ] Owner: the S3 RGB LED: move PCM1808 OUT to GPIO38 (then Claude switches `PIN_DIN`), open the board's RGB jumper, or cover it *(owner)*

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
