# Project State

_Last updated: 2026-10-14_

## Current

Two tracks run in parallel until Phase 10 (see `docs/ROADMAP.md`).

| Track | Phase | Status | Authorization |
|---|---|---|---|
| DJ (Mac + PC capture) | **Complete** (Phases 4–6 closed 2026-10-14) | Done | n/a |
| Firmware (Windows PC) | 10: First Art-Net receiver on the S3 (tracks merge here) | In progress | Authorized 2026-10-14 for Phase 10 |

- Authorize a phase with `/start-milestone Phase <N>` in Claude Code. Until then, everything is read-only.
- **Branch:** `main` holds v2 unchanged. `develop-v23` was fast-forwarded to `feature/platformio-migration` (`ac75b8e`, owner OK 2026-10-14). Phase 10 works on `feature/artnet-receiver`, off `develop-v23`.
- **Last verified firmware commit:** `e605559` (cleanups, plus `senderChannel` in the packet; 8/8 unit tests pass; parity rerun). Phases 4–6 closed 2026-10-14 (docs/tools only).
- **Both boards run** the modular v3 firmware, with strand 1 set to off (a live setting; a reboot restores the saved audio mode).

## What works

- The v3 firmware has v2 parity on hardware, plus fixes:
  - the receiver locks to the announced channel in about 4 s;
  - no boot Wi-Fi drop;
  - `pps` is correct; builds are warning-free.
- **DJ path:**
  - Serato 4.0.4 → SoundSwitch 2.11 (fixture `Lumina 4-Strand RGB`, venue `Lumina Lab`, address 1) → Art-Net → PC.
  - A scripted track follows Serato play/pause (Phase 4). The Art-Net baseline is in `docs/ARCHITECTURE.md`.
- **Test bench:** serial capture, tone sweep, webcam, strand colors, and the Art-Net monitor and sender.
- **Phases done:** 0–9.

## Next step

1. **Phase 10 in progress** on `feature/artnet-receiver` (checklist below).
2. Done 2026-10-14: `feature/platformio-migration` merged (fast-forward) into `develop-v23`.
3. **Owner, when convenient:**
   - [ ] AP Wi-Fi security mode (WPA2 / mixed / WPA3), for the S3's 6 s first join.
   - [ ] Level converter: LV side about 3.3 V, HV side about 5 V, part number.
   - [ ] Strips on outputs 2–4, cycled on camera.
   - [ ] 12 V PSU amp rating, before brighter tests.
   - [ ] Retest the SoundSwitch pause/resume quirk (KNOWN_ISSUES).
4. **Optional (your network):** the PC routes its own LAN through the HomeLab VPN adapter (KNOWN_ISSUES).

## Phase 10 draft checklist (firmware track)

- [x] Branch `feature/artnet-receiver` off `develop-v23`, after the fast-forward merge *(owner OK 2026-10-14)*
- [x] Parser library `lib/ArtNet` with on-device tests (7/7 PASS) *(Claude)*
- [x] S3 Art-Net module (`network/ArtNetReceiver`, `/api/artnet`, 1 Hz serial line while active):
  - UDP 6454;
  - **answers every ArtPoll with an ArtPollReply that advertises the S3's own LAN IP** (SoundSwitch unicasts only to answering nodes, at the advertised IP, and drops them after about 3 s);
  - parses ArtDmx (universe, sequence, length, data) and reports its stats on the diagnostics endpoint.
  - Parse and report only; it doesn't drive the lights yet *(Claude)*
- [x] Parser unit tests from synthetic byte arrays shaped like the measured traffic (no raw captures) *(Claude)*
- [x] **Gate (2026-10-14, `artnet_compare.py`):**
  - Red Autoloop: 1013/1013 paired frames identical.
  - Serato-driven show: 1055 S3 frames identical to the PC copy of the same send, 0 different (817 lit, 328 distinct values).
  - `artnet_send.py --to <S3>`: 236/237 frames; every recent frame is the exact pattern.
  - SoundSwitch listed "Lumina S3" on Universe One *(Claude; owner played)*
- [x] Parity with Art-Net flowing (36 fps):
  - routes 200 (page identical), save OK;
  - tone sweep within ±4 of Phase 8–9, clip 0;
  - reboot: no crashes, config loaded, ESP-NOW 0.7% loss, SoundSwitch rediscovered the S3 *(Claude)*

## Open questions

- Mac model. Dev board models (the modules are known).
- Strip types and counts (config says 150 per strand), PSU rating, fusing, and power injection. A level converter and 330 Ω resistors are fitted.
- Channel lock and Art-Net loss at the real install distance (the Mac is on Wi-Fi: about 0.2% Art-Net gaps on the bench).

## Recent decisions

See `docs/DECISIONS.md`. Latest:
- **2026-10-14:**
  - Art-Net monitoring on the PC uses `artnet_listen.py`, which answers ArtPoll; `tshark` is optional.
  - The guide skips the fixture's `MASTER` cell (SoundSwitch 2.11).
- **2026-10-12:**
  - `senderChannel` uses the packet's spare byte (still version 12).
  - Phase 9 module layout.
  - Unit tests run on the S3.
