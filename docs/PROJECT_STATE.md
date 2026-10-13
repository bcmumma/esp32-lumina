# Project State

_Last updated: 2026-10-12_

## Current

Two tracks run in parallel until Phase 10 (see `docs/ROADMAP.md`).

| Track | Phase | Status | Authorization |
|---|---|---|---|
| DJ (Mac + PC capture) | 4 — Minimum SoundSwitch workflow (owner-led, guided from the Lumina Project chat) | In progress | Not authorized in this workflow yet |
| Firmware (Windows PC) | 10 — First Art-Net receiver on the S3 | Not started; **blocked until DJ Phases 4–6 are done** (ROADMAP: both tracks finish before Phase 10) | Not authorized |

- Authorize a phase with `/start-milestone Phase <N>` in Claude Code. Until then, everything is read-only.
- **Branch:** `main` holds v2 unchanged. `feature/platformio-migration` (off `develop-v23`) holds Phases 8–9. Merging it into `develop-v23` is your call.
- **Last verified commit:** `7ab46e6` on `feature/platformio-migration` (Phase 9, closed 2026-10-12).
- **Both boards run the Phase 9 modular firmware** (v2 behavior plus the receiver channel survey).

## What works

- The v3 firmware is modular (`docs/ARCHITECTURE.md`) and has parity with v2 on hardware:
  - boot, Wi-Fi, the web UI and API, and dual-slot save/load;
  - PCM1808 levels (clip 0) and ESP-NOW (about 1–3% loss once locked);
  - strand 1 RGB and white on camera. Strands 2–4 are waived (expected to pass).
- One shared packet header; on-device unit tests (`pio test -e sender_test`) for the packet layout and config CRC.
- Test bench (`tools/testbench/`): dual serial capture, tone sweep (`--amplitude 1.0`), webcam frames, and `strand_colors.py`.
- Phases 0–3 and 7–9 are done.

## Next step

1. **DJ track:** the Phase 4 owner steps below, then Phases 5–6 (Art-Net capture on the PC). Phase 10 waits on these.
2. **Your call:** merge `feature/platformio-migration` into `develop-v23`.
3. **Optional firmware work while Phase 10 is blocked** (a separate change request; each changes behavior slightly): the `pps` serial counter, the `v20.1` banner, the `volatile` warnings, the boot Wi-Fi drop, and the sender announcing its channel in the packet.
4. **Owner hardware checks, when convenient:**
   - [ ] Level converter: LV side about 3.3 V, HV side about 5 V, no damage; report its part number.
   - [ ] With power off, connect strips to outputs 2–4. Claude then cycles all four strands on camera.
5. **Remaining audit questions (read-only, small):** v2 tags, and the S3 status RGB code path.

## Phase 4 checklist (DJ track, all owner steps)

Use local files: SoundSwitch's article "Music Streaming with SoundSwitch" (undated; check it against your installed version) says streaming tracks can't be autoscripted, and Spotify isn't listed for Serato. Autoscripts are stored as a tag inside the audio file. Your Spotify cue points stay as they are.

- [ ] Get 3–5 local test tracks (purchased or owned files).
- [ ] In Serato, create the crate `Lumina Test`, add the tracks, analyze them, and check BPM and beatgrids.
- [ ] Open SoundSwitch Edit Mode and make the `Lumina Test` crate visible.
- [ ] In Fixture Manager, create the local fixture `Lumina 4-Strand RGB` with 12 channels: CH1–3 Strand 1 R/G/B, CH4–6 Strand 2, CH7–9 Strand 3, CH10–12 Strand 4.
- [ ] Validate the fixture and save it.
- [ ] Create the venue `Lumina Lab` and patch the fixture at DMX address 1.
- [ ] Load one test track in Edit Mode, confirm its beatgrid, and run AutoScript.
- [ ] Inspect one phrase and edit it, then save the lightshow and the project.
- [ ] Switch to Performance Mode, load the same track in Serato, and confirm the saved show follows playback.

**Gate:** one track runs its saved show in sync with Serato.

## Phase 10 draft checklist (firmware track; starts only after Phases 4–6)

- [ ] Prerequisites: a Phase 5–6 capture summary in `docs/ARCHITECTURE.md` (universe, channel count, packet rate, broadcast or unicast), and the network path from the Mac to the S3 confirmed *(owner + Claude)*
- [ ] Branch `feature/artnet-receiver` off the merged v3 branch *(owner OK)*
- [ ] Art-Net module on the S3: UDP 6454 listener, ArtDmx parse (universe, sequence, length, DMX data), and stats on the diagnostics endpoint. Parse and report only; it doesn't drive the lights yet *(Claude)*
- [ ] Parser unit tests built from captured frames (sanitized byte arrays, not raw captures) *(Claude)*
- [ ] Gate: values the S3 parses equal the `tshark` capture for the same frames *(Claude)*
- [ ] Parity unchanged: audio, ESP-NOW, web, and save/load still pass *(Claude)*

## Open questions

- Mac model, macOS version, Serato version, and SoundSwitch version (needed to check features).
- Are the Mac, the Windows PC, and the S3 on the same VLAN/subnet? (The PC reaches the S3's web UI; whether they share a subnet isn't confirmed.) Phases 5 and 10 need this.
- Dev board models (the modules are known: S3-WROOM-1 N16R8 and ESP32-D0WD-V3).
- Strip types and LED counts per strand (saved config says 150 per strand), PSU rating, fusing, and power injection. A level converter and 330 Ω resistors are fitted.
- Channel lock and loss at the real install distance.

## Recent decisions

See `docs/DECISIONS.md`. Latest (2026-10-12):
- The Phase 9 module layout, with its deviations: `LuminaWebServer`, and no receiver config module.
- Unit tests run on the S3 instead of a native env (no host compiler installed).
- Receiver channel survey: an owner-approved Phase 8 exception.
- Secrets live in gitignored `include/secrets.h`.
