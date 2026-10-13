# Lumina — Claude Code Rules

This file is a digest of `docs/PROJECT_INSTRUCTIONS.md`. That file is canonical and wins if the two differ; don't add rules here that aren't there. **This repo is public.** IPs, COM ports, and machine details go in `CLAUDE.local.md` (gitignored), never in committed files.

@docs/PROJECT_STATE.md

## What this is
A distributed smart-lighting platform:
- An ESP32-S3 sender takes PCM1808 audio input, serves a web UI on home Wi-Fi, and (in v3) receives Art-Net from SoundSwitch.
- It sends lighting state over ESP-NOW to an ESP32-WROOM receiver, which drives four LED strands.
- v2 (Arduino `.ino`) is stable on `main`. v3 is the PlatformIO rebuild on `develop-v23` and feature branches.

## Execution gate
- Requests to review, discuss, research, or plan are read-only.
- A phase starts only when I run `/start-milestone Phase <N>` or explicitly authorize it. A change request authorizes that change only.
- Batch authorization covers only steps you can verify yourself: builds, uploads, serial logs, and capture analysis. Stop at owner checks, SoundSwitch or Serato GUI work, wiring or power, network or firewall changes, merges to `main`, or spending, and leave me a checklist in PROJECT_STATE.
- Precedence: my current request > `docs/PROJECT_STATE.md` > `docs/PROJECT_INSTRUCTIONS.md` > `docs/ROADMAP.md`.

## Read when needed
- `docs/PROJECT_INSTRUCTIONS.md`: at the start of a phase, and for architecture, safety, or repo decisions.
- `docs/HARDWARE.md`: before touching pins, LED output, or audio.
- `docs/KNOWN_ISSUES.md`: before porting or refactoring (it lists regression traps).
- `docs/ROADMAP.md`: reference only; don't build ahead.

## How to work
- One phase per track at a time unless batch-authorized; stop at the gate. DJ track (Phases 4–6) and firmware track (Phases 7–9) run in parallel until Phase 10.
- Firmware order is fixed: port with no behavior change, then modularize (building after each extraction), then add Art-Net. Never change architecture and add features in the same step.
- The repo is the source of truth. Don't reconstruct old code from memory.
- All development happens on the Windows PC. The Mac is only for Serato and SoundSwitch, and I'm not Mac-fluent, so spell out exact clicks there. Explain why, not just what.
- Verify time-sensitive facts against official sources and the installed versions, and name the source. Otherwise mark the fact UNVERIFIED.
- Debugging: start with a hypothesis and evidence, and isolate layers (electrical → renderer → config → network). Check antenna and RSSI before blaming the network. Stop after two materially different failed fixes.
- Phase write-up: wiring, parts, files, steps, expected logs, troubleshooting, a pass checklist (marked Claude or owner), commit and branch, next phase.
- Usage: read only the files and line ranges you need. No subagents by default.

## Verification
- No success claims without evidence: build output, a serial-log excerpt, a capture summary, test output, or my reported observation.
- Firmware parity checks: PCM1808 levels, Wi-Fi join, web UI, save/load, ESP-NOW, strands 33/25/26/27 in order, RGB order, and white rendering as white.
- Self-check first: serial logs and commands, the diagnostics endpoint, dual-board capture, test tones, and webcam frames (`docs/HARDWARE.md`). Ask me only for what the test bench can't see.

## Commands
- PowerShell on the Windows PC.
- Build: `pio run -e <env>`. Upload: `pio run -e <env> -t upload --upload-port COM<n>`.
- Capture serial with a time-limited, non-interactive capture, not the interactive monitor. My serial monitor must be closed while you upload or capture.
- Test bench (`tools/testbench/`, run with the gitignored `.venv`): `serial_capture.py <secs> COM<a> [COM<b>]`, `tone_sweep.py COM<a> [COM<b>]`, `webcam_frame.py`. Frames and logs go in gitignored `captures/`.
- Art-Net capture: `tshark` on UDP 6454, on the PC. SoundSwitch on the Mac must broadcast or unicast to the PC. Raw captures stay outside Git.
<!-- Fill in during Phases 5–7: env names, board IDs, COM ports (in CLAUDE.local.md), tshark path and interface, capture script, test command. -->

## Rules
- Never commit credentials, IPs, MACs, captures, or personal details. Credentials live in a gitignored secrets header, with a committed example.
- Never commit directly to `main`; merging is my call.
- Keep proven pins, RGB at 800 kHz, the dual-slot CRC config, and saturation 0 as valid white.
- The recorder stays removed. No AP-only mode. PCM1808 stays. Beat detection is never a blocker. No WLED as the final firmware.
- Strobe effects get a maximum flash-rate cap and a global disable switch.
- Wiring and power are mine; you propose. Flag missing fusing, power injection, or level shifting.
- Ask before system-wide installs, Windows or macOS setting changes, network or firewall changes, deleting outside the repo, or spending.

## Phases
Start with `/start-milestone Phase <N>`, finish with `/close-milestone`. Pull before starting; push after closing.

## Compact instructions
When compacting, preserve: the current phase and step, authorization, branch, files changed, commands that worked (including serial ports), open errors, pending owner checks, and decisions made this session.
