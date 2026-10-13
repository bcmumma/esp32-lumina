# Project State

_Last updated: 2026-10-12_

## Current

Two tracks run in parallel until Phase 10 (see `docs/ROADMAP.md`).

| Track | Phase | Status | Authorization |
|---|---|---|---|
| DJ (Mac + PC capture) | 4 — Minimum SoundSwitch workflow (owner-led, guided from the Lumina Project chat) | In progress | Not authorized in this workflow yet |
| Firmware (Windows PC) | 8 — Port v2 to PlatformIO, no behavior change | Not started | Not authorized |

- Authorize a phase with `/start-milestone Phase <N>` in Claude Code. Until then, everything is read-only.
- **Branch:** `main` holds v2 unchanged. v3 work lives on `develop-v23`; Phase 8 starts `feature/platformio-migration` off it.
- **Last verified commit:** `948fa05` on `develop-v23` (Phase 7, closed 2026-10-12).
- **The boards run Phase 7 bench programs, not v2.** The S3 runs a PCM1808 level meter; the WROOM runs a pin check and cycles strand 1 (10 LEDs). The lights won't react to music until Phase 8 uploads the port.

## What works

- v2 (v22 code) ran on both boards up to 2026-10-12: tone sweep → PCM1808 → ESP-NOW (channel 6, auto-scan lock) → receiver levels matched. See `docs/HARDWARE.md` for the baseline numbers.
- PlatformIO builds and uploads both boards from the PC (`platformio.ini`, two environments).
- Test bench (`tools/testbench/`): dual-port serial capture, tone sweep through Modi 3E → Magni → PCM1808 (Magni knob at the middle: about 13 dB headroom), and webcam frames of the strips (`captures/webcam/`, gitignored).
- Strand 1 (GPIO33) renders RGB and white correctly after the 12 V incident at the level converter.
- Phases 0–3 and 7 are done.

## Next step

1. **Repo audit (read-only, still owed from before Phase 7).** Ask Claude Code to run it. It answers:
   - which v2 files, tags, and branches exist;
   - the packet version, and the channel scan behavior;
   - whether the CRC config and the white fix are intact;
   - whether the S3 status RGB feature is present;
   - the license;
   - **whether any branch or any point in history contains a Wi-Fi SSID or password, a home IP, or personal details.** If a password was ever pushed, change it on the router.
2. **Owner hardware checks** (before the Phase 8 strand-order check):
   - [ ] Level converter: measure the LV side (about 3.3 V) and HV side (about 5 V), look for damage, and report the part number.
   - [ ] With power off, connect strips to outputs 2–4. Claude then cycles all four strands on camera.
3. **Phase 8**, once authorized.

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

## Phase 8 draft checklist (firmware track)

- [ ] Branch `feature/platformio-migration` off `develop-v23` *(Claude)*
- [ ] Gitignored `include/secrets.h` with a committed `secrets.example.h`; the **owner fills in the SSID and password locally** *(owner)*
- [ ] Port the v2 sender and receiver into `src/sender/` and `src/receiver/` as `.cpp`/`.h`, with no behavior change. Declare types before use (see KNOWN_ISSUES on `.ino` prototypes). Build after each step *(Claude)*
- [ ] Parity, checked on the test bench *(Claude unless marked)*:
  - [ ] PCM1808 levels: tone sweep; `clip` in `/api/live` stays 0
  - [ ] Wi-Fi station join; S3 RSSI logged
  - [ ] Every web UI page loads *(Claude over HTTP; owner glance once)*
  - [ ] Settings save and load: change over HTTP, reset over serial, read back
  - [ ] ESP-NOW delivery: dual capture, sequence numbers, loss compared with the v2 baseline (about 3%)
  - [ ] ESP-NOW channel behavior: receiver scans, then locks to the sender's channel
  - [ ] Strands 1–4 on GPIO 33/25/26/27 in order *(camera; needs owner check 2 above)*
  - [ ] RGB colors and white rendering as white *(camera)*
- [ ] Diagnostics-only additions, if any, are command-gated or behind a build flag *(Claude)*

## Open questions

- Mac model, macOS version, Serato version, and SoundSwitch version (needed to check features).
- Are the Mac, the Windows PC, and the S3 on the same VLAN/subnet? (The PC reached the S3's web UI over HTTP on 2026-10-12; whether they share a subnet isn't confirmed.) Phase 5 needs this.
- Does the committed v2 source build unchanged on Arduino 3.3.12? Phase 8 answers this.
- Dev board models (the modules are known: S3-WROOM-1 N16R8 and ESP32-D0WD-V3).
- Strip types and LED counts per strand (saved config says 150 per strand), PSU rating, fusing, and power injection. A level converter and 330 Ω resistors are fitted.

## Recent decisions

See `docs/DECISIONS.md`. Latest (2026-10-12):
- One root PlatformIO project with `sender` and `receiver` environments.
- pioarduino 55.03.312-1 (Arduino 3.3.12), because the official platform only ships Arduino 2.0.17.
- Board IDs `esp32-s3-devkitc-1` (`qio_opi`, 16 MB) and `esp32dev`.
