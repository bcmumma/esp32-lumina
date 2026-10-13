# Project State

_Last updated: 2026-10-12_

## Current

Two tracks run in parallel until Phase 10 (see `docs/ROADMAP.md`).

| Track | Phase | Status | Authorization |
|---|---|---|---|
| DJ (Mac + PC capture) | 4 — Minimum SoundSwitch workflow (owner-led, guided from the Lumina Project chat) | In progress | Not authorized in this workflow yet |
| Firmware (Windows PC) | 7 — PlatformIO environment | In progress | Authorized 2026-10-12 for Phase 7 |

- Authorize a phase with `/start-milestone Phase <N>` in Claude Code. Until then, everything is read-only.
- **Branch:** `main` holds v2 unchanged. All v3 work, docs included, lives on `develop-v23` (created and pushed 2026-10-12). Firmware phases use feature branches off it. Phase 7 is toolchain-only (no Lumina code changes), so it works on `develop-v23`; Phase 8 starts `feature/platformio-migration`.
- **Last verified commit:** none recorded in this workflow.

## What works

- The v14 prototype (Lumina v1.0) was proven.
- Confirmed working: the PCM1808 input, ESP-NOW, the four strands (GPIO 33/25/26/27, RGB, 800 kHz), the v20.1 config saving, and home Wi-Fi control.
- v2 is backed up on GitHub (`firmware/sender/lumina_sender_v2.ino`, `firmware/receiver/lumina_receiver_v2.ino`), but it was never confirmed to compile and run after the rebuild.
- Phases 0–3 are done. Serato + FLX4 + Spotify work, and SoundSwitch connects (green in Performance Mode).

## Research finding (2026-10-11)

- SoundSwitch's support article "Music Streaming with SoundSwitch" says autoscripts **can't** be created for streaming tracks; streamed tracks get Autoloops only.
- For Serato, the supported services listed are Apple Music, Beatsource, Beatport, SoundCloud, and TIDAL. Spotify isn't listed.
- Autoscript data is stored as a metadata tag inside the audio file, so scripting needs local files.
- The article is undated, so check it against your installed SoundSwitch version.
- **Implication:** use 3–5 local test tracks for Phase 4. Your Spotify cue points stay as they are; there's no need to migrate your library.

## Next step (Phase 4 checklist, all owner steps)

- [ ] Get 3–5 local test tracks (purchased or owned files).
- [ ] In Serato, create the crate `Lumina Test`, add the tracks, analyze them, and check BPM and beatgrids.
- [ ] Open SoundSwitch Edit Mode and make the `Lumina Test` crate visible.
- [ ] Open Fixture Manager and create the local fixture `Lumina 4-Strand RGB` with 12 channels:
  - CH1–3: Strand 1 R/G/B
  - CH4–6: Strand 2 R/G/B
  - CH7–9: Strand 3 R/G/B
  - CH10–12: Strand 4 R/G/B
- [ ] Validate the fixture and save it.
- [ ] Create the venue `Lumina Lab` and patch the fixture at DMX address 1.
- [ ] Load one test track in Edit Mode, confirm its beatgrid, and run AutoScript.
- [ ] Inspect one phrase and edit it, then save the lightshow and the project.
- [ ] Switch to Performance Mode, load the same track in Serato, and confirm the saved show follows playback.

**Gate:** one track runs its saved show in sync with Serato.

## Phase 7 draft checklist (firmware track)

- [x] VS Code + PlatformIO work on the PC; `pio --version` runs in Claude Code's terminal *(Claude)*. PlatformIO Core 6.2.0. `pio` isn't on PATH, so it runs by its full path (see `CLAUDE.local.md`).
- [x] Identify both boards (chip, flash, PSRAM, USB-serial chip) and their COM ports, and record them in `docs/HARDWARE.md` and `CLAUDE.local.md` *(Claude)*. Done 2026-10-12: S3 = ESP32-S3-WROOM-1 N16R8 on a CH343; WROOM = ESP32-D0WD-V3 with 4 MB flash on a CP210x. The dev board models are still UNVERIFIED.
- [x] Choose the repo layout for the two firmware projects or environments, and record it in `docs/DECISIONS.md` *(Claude)*. Root `platformio.ini` with `sender` and `receiver` environments, on pioarduino 55.03.312-1 (Arduino 3.3.12 / IDF 5.5.5).
- [x] A trivial S3 project builds, uploads, and logs over serial *(Claude)*. 2026-10-12: `LUMINA-P7 flash=16MB psram=8192KB arduino=3.3.12 idf=v5.5.5`.
- [x] A trivial WROOM project builds, uploads, and logs over serial *(Claude)*. 2026-10-12: `LUMINA-P7 chip=ESP32-D0WD-V3 flash=4MB arduino=3.3.12 idf=v5.5.5`.
- [x] Test bench: both COM ports captured at once *(Claude)*. Passed 2026-10-12 on the boards' current v22 firmware, with a tone sweep. The receiver's vol/bass/mid/treb matched the S3 within one status line. About 29 packets/s arrived, with roughly 3% loss and `bad=0`, at 40–41 fps.
- [x] Test bench: a webcam frame of the strips captured and inspected *(Claude; owner aims the camera)*. Passed 2026-10-12 with an HD264 USB webcam and OpenCV at exposure -7. Frames synced to the serial log showed strand 1's 10 LEDs as red, green, and blue, each strongest in its own channel, and white raising all three. Frames go to `captures/webcam/` (gitignored).
- [x] Test bench: tones play from the PC's line out (owner connects the cable to the PCM1808) *(Claude)*. Passed 2026-10-12 on the S3's v22 firmware. Audio chain: PC → Schiit Modi 3E (USB DAC, the Windows default output) → Magni (amp) → its output → PCM1808. There are no speakers. Readings depend on the Magni volume setting. Tones at 25% amplitude read 100 Hz: bass 220 / mid 120 / treble 0; 1 kHz: bass 0 / mid 254 / treble 166; 6 kHz: bass 0 / mid 179 / treble 255; silence: all 0.
- [x] Both boards blink or log as expected *(owner, once, to confirm the camera view is trustworthy)*. 2026-10-12: the owner saw strand 1 cycle red, green, blue, white, off on all 10 LEDs, and the camera frames agree.
- Test-bench scripts: `tools/testbench/serial_capture.py`, `tone_sweep.py`, and `webcam_frame.py`. Run them with the repo venv `.venv\Scripts\python.exe` (gitignored; has opencv-python-headless and pyserial). PlatformIO's Python also runs the two serial scripts.
- **Both boards now run Phase 7 test programs, not v2. The receiver cycles the first 10 LEDs of strand 1. The LEDs won't react to music until the Phase 8 port is uploaded.**
- Open (owner): after 12 V touched the receiver's level converter, check the converter (LV side about 3.3 V, HV side about 5 V, no damage) and test strands 2–4. See `docs/HARDWARE.md`.
- Parts: a USB webcam and a 3.5 mm-to-RCA cable. Installing ffmpeg (for camera capture) is a system-wide install, so ask first.

## Before Phase 7 (read-only)

Ask Claude Code for a **repo audit** that changes nothing. It should answer:

- Which v2 sender and receiver files are committed, with which tags and branches.
- The ESP-NOW packet version.
- Whether the receiver uses a fixed or auto-scanned Wi-Fi channel.
- Whether the dual-slot CRC config and the white/saturation fix are intact.
- Whether the S3 status RGB feature is present.
- Whether a license file exists.
- **Whether any committed file, in any branch or in history, contains a hardcoded Wi-Fi SSID or password, a home IP, or other personal details.** The repo is public. If a password was ever pushed, tell me to change it on the router; rotating it matters more than rewriting history.
- ~~Whether the v2 sketches follow the Arduino folder rule.~~ Settled 2026-10-12: they don't, and they stay in place (see `docs/DECISIONS.md`).

Record the answers here and in `docs/KNOWN_ISSUES.md`.

## Owner checklist

The Phase 4 steps above.

## Open questions

- Mac model, macOS version, Serato version, and SoundSwitch version (needed to check features).
- Is the Windows PC on the same VLAN and subnet as the Mac and the S3? (Phase 5 needs it for Art-Net capture.)
- Does v2 compile and run as committed? Answered in Phase 7–8, or partly by the audit.
- Board models for the S3 and the WROOM, and the S3 onboard RGB pin. This matters because GPIO48 is the PCM1808 DOUT.
- LED count per strand, strip types, PSU rating, fusing and injection, and level shifter or resistors.
- Are the Mac, the Windows PC, and the S3 on the same VLAN? Which IP does the S3 use today?

## Recent decisions

See `docs/DECISIONS.md`. Latest: all development happens on the Windows PC, and the Mac is DJ-only. Use local files for SoundSwitch scripting, because streaming tracks can't be autoscripted.
