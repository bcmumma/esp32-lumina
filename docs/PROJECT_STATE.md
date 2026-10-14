# Project State

_Last updated: 2026-10-14_

## Current

Two tracks run in parallel until Phase 10 (see `docs/ROADMAP.md`).

| Track | Phase | Status | Authorization |
|---|---|---|---|
| DJ (Mac + PC capture) | 6: Prove SoundSwitch emits Art-Net (Serato → SoundSwitch → Art-Net → capture) | In progress; one owner step left (destination address) | Not authorized in this workflow (done at the owner's request) |
| Firmware (Windows PC) | 10: First Art-Net receiver on the S3 | Not started. Blocked only until Phase 6 is closed | Not authorized |

- Authorize a phase with `/start-milestone Phase <N>` in Claude Code. Until then, everything is read-only.
- **Branch:** `main` holds v2 unchanged. `feature/platformio-migration` (off `develop-v23`) holds Phases 8–9, the follow-up firmware fixes, and the DJ-track docs. Merging into `develop-v23` is your call.
- **Last verified firmware commit:** `e605559` (cleanups, plus `senderChannel` in the packet; 8/8 unit tests pass; parity rerun). Phases 4–5 closed 2026-10-14.
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
- **Phases done:** 0–5 and 7–9.

## Next step

1. **Finish Phase 6:** the gate also needs the Art-Net **destination address** (broadcast or unicast); the rest is in `docs/ARCHITECTURE.md` → Art-Net.
   - Claude starts `artnet_listen.py`, which now prints the source address. Play the "Red" Autoloop.
   - On the Mac: **Finder → Applications → Utilities → Terminal**, type `sudo tcpdump -n -c 10 udp port 6454`, press Return, and enter your login password (it doesn't show while you type).
   - Send Claude the lines. The address after `>` is the destination: your PC's address means unicast; one ending in `.255` means broadcast.
   - Then `/close-milestone`, and Phase 10 can be authorized.
2. **Your call:** merge `feature/platformio-migration` into `develop-v23`.
3. **Owner, when convenient:**
   - [ ] AP Wi-Fi security mode (WPA2 / mixed / WPA3), for the S3's 6 s first join.
   - [ ] Level converter: LV side about 3.3 V, HV side about 5 V, part number.
   - [ ] Strips on outputs 2–4, cycled on camera.
   - [ ] 12 V PSU amp rating, before brighter tests.
   - [ ] Retest the SoundSwitch pause/resume quirk (KNOWN_ISSUES).
4. **Optional:** the Art-Net destination address via `tshark`. Wireshark is a system-wide install, so ask first.

## Phase 10 draft checklist (firmware track)

- [ ] Branch `feature/artnet-receiver` off the merged v3 branch *(owner OK)*
- [ ] S3 Art-Net module:
  - UDP 6454;
  - **answers every ArtPoll with an ArtPollReply** (SoundSwitch only sends to nodes that answer, and drops them after about 3 s);
  - parses ArtDmx (universe, sequence, length, data) and reports its stats on the diagnostics endpoint.
  - Parse and report only; it doesn't drive the lights yet *(Claude)*
- [ ] Parser unit tests from sanitized byte arrays (no raw captures) *(Claude)*
- [ ] **Gate:** S3-parsed values for universe 0 equal what the PC monitor sees for the same SoundSwitch output; also check with `artnet_send.py --to <S3 IP>` *(Claude; owner plays SoundSwitch)*
- [ ] Parity unchanged: audio, ESP-NOW, web, and save/load *(Claude)*

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
