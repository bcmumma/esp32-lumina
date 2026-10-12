# Project State

_Last updated: 2026-10-11_

## Current

Two tracks run in parallel until Phase 10 (see `docs/ROADMAP.md`).

| Track | Phase | Status | Authorization |
|---|---|---|---|
| DJ (Mac + PC capture) | 4 — Minimum SoundSwitch workflow (owner-led, guided from the Lumina Project chat) | In progress | Not authorized in this workflow yet |
| Firmware (Windows PC) | 7 — PlatformIO environment | Not started | Not authorized |

- Authorize a phase with `/start-milestone Phase <N>` in Claude Code. Until then, everything is read-only.
- **Branch:** `main` holds v22 unchanged. All v23 work, docs included, lives on `develop-v23` (created during setup). Firmware phases use feature branches off it.
- **Last verified commit:** none recorded in this workflow.

## What works

- The v14 prototype (Lumina v1.0) was proven.
- Confirmed working: the PCM1808 input, ESP-NOW, the four strands (GPIO 33/25/26/27, RGB, 800 kHz), the v20.1 config saving, and home Wi-Fi control.
- v22 is backed up on GitHub, but it was never confirmed to compile and run after the rebuild.
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

- [ ] VS Code + PlatformIO work on the PC; `pio --version` runs in Claude Code's terminal *(Claude)*
- [ ] Identify both boards (chip, flash, PSRAM, USB-serial chip) and their COM ports, and record them in `docs/HARDWARE.md` and `CLAUDE.local.md` *(Claude)*
- [ ] Choose the repo layout for the two firmware projects or environments, and record it in `docs/DECISIONS.md` *(Claude)*
- [ ] A trivial S3 project builds, uploads, and logs over serial *(Claude)*
- [ ] A trivial WROOM project builds, uploads, and logs over serial *(Claude)*
- [ ] Both boards blink or log as expected *(owner)*

## Before Phase 7 (read-only)

Ask Claude Code for a **repo audit** that changes nothing. It should answer:

- Which v22 sender and receiver files are committed, with which tags and branches.
- The ESP-NOW packet version.
- Whether the receiver uses a fixed or auto-scanned Wi-Fi channel.
- Whether the dual-slot CRC config and the white/saturation fix are intact.
- Whether the S3 status RGB feature is present.
- Whether a license file exists.
- **Whether any committed file, in any branch or in history, contains a hardcoded Wi-Fi SSID or password, a home IP, or other personal details.** The repo is public. If a password was ever pushed, tell me to change it on the router; rotating it matters more than rewriting history.
- Whether the v22 sketches follow the Arduino folder rule (each `.ino` in a folder with the same name). If not, propose the `legacy/v22/` layout from `docs/DECISIONS.md`.

Record the answers here and in `docs/KNOWN_ISSUES.md`.

## Owner checklist

The Phase 4 steps above.

## Open questions

- Mac model, macOS version, Serato version, and SoundSwitch version (needed to check features).
- Is the Windows PC on the same VLAN and subnet as the Mac and the S3? (Phase 5 needs it for Art-Net capture.)
- Does v22 compile and run as committed? Answered in Phase 7–8, or partly by the audit.
- Board models for the S3 and the WROOM, and the S3 onboard RGB pin. This matters because GPIO48 is the PCM1808 DOUT.
- LED count per strand, strip types, PSU rating, fusing and injection, and level shifter or resistors.
- Are the Mac, the Windows PC, and the S3 on the same VLAN? Which IP does the S3 use today?

## Recent decisions

See `docs/DECISIONS.md`. Latest: all development happens on the Windows PC, and the Mac is DJ-only. Use local files for SoundSwitch scripting, because streaming tracks can't be autoscripted.
