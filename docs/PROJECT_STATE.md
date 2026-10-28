# Project State

_Last updated: 2026-10-27_

## Current

The DJ and firmware tracks merged at Phase 10. Phases 10–15 are done; the ROADMAP has no Phase 16 yet.

| Track | Phase | Status | Authorization |
|---|---|---|---|
| DJ (Mac + PC capture) | Complete (Phases 4–6) | Done | n/a |
| Firmware | 16: to be defined by the owner (proposal below) | Not started | Not authorized |

- Authorize a phase with `/start-milestone Phase <N>` in Claude Code. Until then, everything is read-only.
- **Branch:** `main` holds v2 unchanged. `develop-v23` = `a7f2c67` (Phases 7–14). Phase 15 is on `feature/cells` (15 commits ahead).
- **Last verified commit:** `d930bb9` on `feature/cells` (Phase 15, closed 2026-10-27).
- **Boards:**
  - **S3:** Phase 15 sender.
  - **Room 1** (`receiver`): strand 1, GPIO 33.
  - **Room 2** (`receiver_room2`): strand 3, GPIO 26.
  - **Saved:** 3 active strands, Standalone source, green look, layout L1, strobe off at boot.

## What works

- **Firmware:** v3 with v2 parity; 72/72 on-device unit tests; sender and both receiver builds compile without warnings.
- **SoundSwitch:**
  - six layouts L1–L6, each with a 5-channel Lumina FX fixture;
  - the Auto chain SoundSwitch → Hybrid → Audio;
  - the FX fixture drives a beam, kick looks, UV sparkle, and a gated FX strobe.
- **Looks:** genres House, Techno, Drum & Bass, Acid, General, Chill, Halloween, Meters; 7 kick looks; Setup can hide buttons.
- **Two rooms from one S3:** the same show in both, each with its own wiring.
- **Strobe:** off at boot, 15/s ceiling, one gate per room.
- **Phases done:** 0–15.

## Next step

1. **Owner:** decide on merging `feature/cells` into `develop-v23` (fast-forward), and pick Phase 16. Proposal: **show hardening**:
   - firmware updates over Wi-Fi for both receivers, so they no longer need USB;
   - a receiver heartbeat on the page (room 1 and room 2 online, packet loss);
   - power per room: a second supply or a brightness cap (see `docs/HARDWARE.md`);
   - a full-show soak.

   v3 is now a candidate to replace v2 on `main` once the power items are done. That merge is the owner's call.
2. **Owner, power (before white-heavy looks at full brightness):**
   - [ ] Each room's 450 LEDs can draw about 11 A at full white; one 90 W supply gives about 6 A. Add a second supply or injection per room, or set the brightness cap to about 50%.
   - [ ] Fuse each 12 V feed at 5 A.
   - [ ] One measured current at full white, to confirm the 9 W/m rating.
3. **Owner, when convenient:**
   - [ ] Bench audio: make the Modi/Magni the Windows default output (Voicemeeter may have taken over), so Claude can run tone tests again.
   - [ ] Look check of the Halloween looks and the newer kick looks.
   - [ ] AP security mode (slow first join); SoundSwitch blackout quirks.

## Open questions

- Mac model; dev board models.
- Real per-LED current.
- Why the WROOMs drop off USB now and then (power, cable, or hub).
- Why SoundSwitch sometimes outputs all-zero frames.

## Recent decisions

See `docs/DECISIONS.md`. Latest (2026-10-23 to 2026-10-27):
- Six SoundSwitch layouts with the Lumina FX fixture; the 20-layout test chart is retired.
- Two room receivers from one S3: same show, own wiring.
- Halloween genre; colour buttons follow the receiver's colour wheel (Green 86, Blue 171).
