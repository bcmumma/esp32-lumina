# Project State

_Last updated: 2026-10-12_

## Current

Two tracks run in parallel until Phase 10 (see `docs/ROADMAP.md`).

| Track | Phase | Status | Authorization |
|---|---|---|---|
| DJ (Mac + PC capture) | 4 — Minimum SoundSwitch workflow (owner-led, guided from the Lumina Project chat) | In progress | Not authorized in this workflow yet |
| Firmware (Windows PC) | 9 — Modularize | Not started | Not authorized |

- Authorize a phase with `/start-milestone Phase <N>` in Claude Code. Until then, everything is read-only.
- **Branch:** `main` holds v2 unchanged. `feature/platformio-migration` (off `develop-v23`) holds the Phase 8 port. Merging it into `develop-v23` is your call; Phase 9 continues from it.
- **Last verified commit:** `35449ae` on `feature/platformio-migration` (Phase 8, closed 2026-10-12).
- **Both boards run the Phase 8 port** (v2 behavior, plus the receiver channel survey). The lights react to music again.

## What works

- The v3 PlatformIO build of v2 runs on both boards with parity:
  - boot, Wi-Fi (channel 6), web UI and API, and dual-slot save/load;
  - PCM1808 levels and ESP-NOW (0–1.5% loss once locked);
  - strand 1 RGB and white on camera.
- Strands 2–4 were waived by the owner (expected to pass).
- Credentials and LAN addresses live in gitignored `include/secrets.h`. History has no real Wi-Fi credentials.
- Test bench (`tools/testbench/`): dual serial capture, tone sweep (`--amplitude 1.0` for music level with the Magni at the middle), and webcam frames.
- Phases 0–3, 7, and 8 are done.

## Next step

1. **Your call:** merge `feature/platformio-migration` into `develop-v23`. `main` stays v2 until you decide v3 replaces it.
2. **Owner hardware checks, when convenient:**
   - [ ] Level converter: LV side about 3.3 V, HV side about 5 V, no damage; report its part number.
   - [ ] With power off, connect strips to outputs 2–4. Claude then cycles all four strands on camera.
3. **Remaining audit questions (read-only, small):** v2 tags, whether the CRC config and white fix are intact in code, and the S3 status RGB code path. Already answered: packet version and size (107), channel scan, history credentials, and license.
4. **Phase 9**, once authorized.

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

## Phase 9 draft checklist (firmware track)

Goal: same behavior as Phase 8, split into modules (see `docs/ROADMAP.md`). One subsystem per step; build, upload, and run the Phase 8 parity checks after each.

- [ ] Branch for Phase 9 (proposed: continue on `feature/platformio-migration`, or a new `feature/modularize`) *(owner decides)*
- [ ] Shared `lib/` header for `AudioLedPacket` (sender and receiver must stay byte-identical, 107 bytes) *(Claude)*
- [ ] Sender modules: `audio/PCM1808`, `audio/AudioAnalyzer`, `network/WiFiManager`, `network/EspNowSender`, `web/WebServer`, `lighting/LightingState`, `config/ConfigManager` *(Claude)*
- [ ] Receiver modules: `network/EspNowReceiver`, `lighting/Renderer`, `lighting/Effects`, `lighting/LedOutput`, `config/ConfigManager` *(Claude)*
- [ ] Native tests: config blob with CRC, and packet encoding *(Claude)*
- [ ] Parity rerun after the last extraction: tone sweep, Wi-Fi, web routes, save/load, ESP-NOW loss and lock, strand 1 on camera *(Claude)*
- Not in Phase 9 unless you approve separately (each changes behavior): the `pps` serial counter, the `v20.1` banner, the boot Wi-Fi drop, and the channel number in the packet.

## Open questions

- Mac model, macOS version, Serato version, and SoundSwitch version (needed to check features).
- Are the Mac, the Windows PC, and the S3 on the same VLAN/subnet? (The PC reaches the S3's web UI; whether they share a subnet isn't confirmed.) Phase 5 needs this.
- Dev board models (the modules are known: S3-WROOM-1 N16R8 and ESP32-D0WD-V3).
- Strip types and LED counts per strand (saved config says 150 per strand), PSU rating, fusing, and power injection. A level converter and 330 Ω resistors are fitted.
- Channel lock and loss at the real install distance (the survey may lock one channel off at close range).

## Recent decisions

See `docs/DECISIONS.md`. Latest (2026-10-12):
- Secrets live in gitignored `include/secrets.h`.
- Receiver channel survey: an owner-approved exception to Phase 8's "no behavior change".
- The sender announcing its channel in the packet is deferred to Phase 9+ as a protocol change.
- PlatformIO: one root project, pioarduino 55.03.312-1 (Arduino 3.3.12).
