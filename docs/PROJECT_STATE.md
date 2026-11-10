# Project State

_Last updated: 2026-11-09_

## Current

The DJ and firmware tracks merged at Phase 10. Phases 10–16 are done; the ROADMAP has no Phase 17 yet.

| Track | Phase | Status | Authorization |
|---|---|---|---|
| DJ (Mac + PC capture) | Complete (Phases 4–6) | Done | n/a |
| Firmware | 17: not defined yet (draft below) | Not started | Not authorized |

- Authorize a phase with `/start-milestone Phase <N>` in Claude Code. Until then, everything is read-only.
- **Branch:** `main` holds v2 unchanged. `develop-v23` = `6b3d860` (Phases 7–15). Phase 16 is on `feature/show-hardening`, ready to fast-forward `develop-v23` (owner OK needed). The DJ changes (below) are on `feature/dj-feedback`, branched from Phase 16.
- **Last verified commit:** `313c620` on `feature/show-hardening` (Phase 16, closed 2026-10-28).
- **Boards:** all three run the DJ-feedback firmware (`feature/dj-feedback`, 2026-10-31). The S3 now sits at its show spot on a USB charger (owner, 2026-10-31); it has no Wi-Fi update, so a change to the S3 or its page needs it back on USB. Receivers update over Wi-Fi with `tools/ota_update.py --room N`; USB only for recovery.
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
- [x] Owner moved the S3 from the PC (where the rooms couldn't hear it) to its show spot on a USB charger; both rooms checked in within seconds *(owner, 2026-10-31)*
- [x] Receivers updated over Wi-Fi (2026-10-31): both uploads HTTP 200, both back online within seconds. Then the six Club modes, two moods (one wrapping past red) and spaced cells (Art-Net test pattern, gap 6 and 12, with a mood) ran on both rooms: no restarts, frame rates steady (room 1 about 30 fps, room 2 about 62), settings restored. Not exercised: the Club bursts, which need Strobe on and music *(Claude)*
- [ ] **Room 2's radio link is weak from the S3's show spot** (2026-10-31, about 7 minutes after the update): room 1 −66 dBm, 0.5% loss; room 2 −77 to −85 dBm, 6.3% loss (Phase 16 bench: 0.02% / 0.01%). A crowd weakens 2.4 GHz further. Owner: move the S3 toward room 2 or higher up (room 1 has about 15 dB to spare), and keep both boards' antenna ends clear of metal and the LED supply; then Claude re-measures (aim: room 2 under 1% loss) *(owner; Claude)*
- [x] DnB Split Original (owner, 2026-11-09): look 52 in Club, mode 28's drawing with no colour mood and a white kick flash; picking it always applies the original sliders. Builds warning-free; not flashed yet *(Claude)*
- [x] Owner changes (2026-11-09): 3-band EQ (Bass, Mid, High) for DnB Split, DnB Split Original, Split Mirror and Split Strobe, shown only on those looks (Live and Advanced); flash looks run without the Strobe button, which is now the full-strip strobe over any look; Strobe brightness slider; Setup → Power & safety → Flashing looks is the global flash kill switch (on by default); Laser Sweep back in Club and on the Live page (eight looks now). Packet v13 (112 bytes); receivers also accept v12. Builds warning-free, unit tests compile; not flashed *(Claude)*
- [x] Both rooms updated over Wi-Fi to `278ffc4` (2026-11-09): HTTP 200, back online, running on the old S3's v12 packets; one minute after: room 1 −65 dBm, 1.1% loss; room 2 −81 dBm, 5.2% loss *(Claude)*
- [x] S3 flashed over USB with `278ffc4` (2026-11-09): on-device tests 94/94; boot clean, 112-byte packets, saved settings loaded (Flashing looks on, strobe brightness 100%, EQ flat); page has the EQ panel and the full-strip strobe button; Live page lists eight looks *(Claude)*
- [x] Strobe fix (owner report, 2026-11-09: the whole strip flashed on every look with Strobe off, and Strobe on was far too flashy). Cause: Kick Flash (saved at 95%) went through the flash gate that now follows Flashing looks, and the button added a steady full-strip strobe at the max rate. Now whole-strip flashes need the Strobe button: on, the strip flashes on every kick (strobe colour and brightness, sensitivity, max rate); off, nothing flashes the whole strip and flash looks keep only their small flashes. Strobe, Drop Strobe, Beat Strobe and SoundSwitch's FX Strobe also need the button; the steady overlay and the Kick flash slider are gone. Rooms updated over Wi-Fi (both back, drawing at 31 / 61 fps) *(Claude)*
- [x] S3 flashed with `a653ab7` (2026-11-09): page shows STROBE ON/OFF, no Kick flash slider; FX Strobe waits for the button. Its native USB socket can't flash it (GPIO20 is the PCM1808 MCLK); use the UART socket *(Claude)*
- [ ] Owner: check on the lights: Strobe off = no whole-strip flashes on any look; Strobe on = a flash on each kick; then DnB Split Original, the EQ, strobe brightness *(owner)*
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
