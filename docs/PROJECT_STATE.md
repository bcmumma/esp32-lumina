# Project State

_Last updated: 2026-10-14_

## Current

The DJ and firmware tracks merged at Phase 10. Phases 10–15 are sequential (see `docs/ROADMAP.md`).

| Track | Phase | Status | Authorization |
|---|---|---|---|
| DJ (Mac + PC capture) | Complete (Phases 4–6) | Done | n/a |
| Firmware | 12: Intelligent Fixture v1 (`LUMINA-16`) | In progress | Authorized 2026-10-14 for Phase 12 |

- Authorize a phase with `/start-milestone Phase <N>` in Claude Code. Until then, everything is read-only.
- **Branch:** `main` holds v2 unchanged. `develop-v23` was fast-forwarded to `feature/artnet-receiver` (`e448077`, Phases 7–11; owner OK 2026-10-14). Phase 12 works on `feature/intelligent-fixture`.
- **Last verified commit:** `6cafb55` on `feature/artnet-receiver` (Phase 11, closed 2026-10-14).
- **Both boards run** the Phase 11 firmware. Strand 1 is set to off live; its saved mode is 12 (audio VU).

## What works

- v3 firmware: v2 parity, modular, warning-free; 15/15 on-device unit tests.
- **Art-Net into the S3 (parse/report):** SoundSwitch discovers "Lumina S3" and unicasts universe 0. The decoded values equal the PC's frame for frame.
- **Live network setup (validated):**
  - Mac on Ethernet in the S3's VLAN; Art-Net loss 0.08% over 30 min, max gap 480 ms.
  - Keep the Mac awake. A Mac on another VLAN can't discover the S3.
- **Test bench:** serial, tones, webcam, strand colors, Art-Net monitor/sender/compare, network soak.
- **Phases done:** 0–11.

## Next step

1. Done 2026-10-14: `feature/artnet-receiver` merged (fast-forward) into `develop-v23`. Phase 12 in progress.
2. **Before Phase 12 drives LEDs from Art-Net (owner):**
   - [x] 12 V PSU: **90 W (7.5 A)**, budget about 6 A. Brightness cap estimated at about 60–65% (HARDWARE.md). *(owner, 2026-10-14)*
   - [ ] Fusing and power injection; strip datasheet or one measured current per strand, to confirm the cap.
   - [~] Level converter is 3.3 V → 5 V (owner). Still to come: a multimeter reading on each side, after the 12 V touch, and the part number.
   - [ ] Strips on outputs 2–4: being mounted (owner). Likely the **same effect on all four**, so `LUMINA-16` drives one shared effect.
3. **Owner, when convenient:**
   - [ ] AP Wi-Fi security mode (WPA2 / mixed / WPA3), for the S3's 6 s first join.
   - [ ] SoundSwitch quirks (KNOWN_ISSUES): Performance Mode blackout and pause/resume.
   - [ ] Re-measure S3 RSSI and stalls at the install position.

## Phase 12 draft checklist (Intelligent Fixture v1, `LUMINA-16`)

Gate (ROADMAP): a SoundSwitch control changes a real Lumina effect live.

- [x] Branch `feature/intelligent-fixture` off `develop-v23` *(owner OK 2026-10-14)*
- [ ] SoundSwitch fixture `Lumina LUMINA-16`, 16 channels per `docs/ARCHITECTURE.md`:
  - master dimmer, effect, speed, intensity, density, strobe, direction, variation, primary/secondary RGB, transition, palette;
  - click-by-click steps in the guide *(owner, guided)*
- [ ] S3: map universe 0 CH1–16 to the existing effect parameters. The S3 sends effect parameters over ESP-NOW (not pixels); the receiver renders.
  - Art-Net control is switched on by a web toggle; automatic source arbitration is Phase 13.
- [ ] Safety:
  - strobe channel rate-capped with a global strobe disable, conservative default;
  - a brightness cap from the PSU rating *(Claude; owner sets the PSU number)*
- [ ] First effects: Solid Color, Neon Scanner, Laser Sweep, Rave Chase, DnB Chopper, White Lightning *(Claude)*
- [ ] Tests: CH1–16 mapping unit tests. On hardware, `artnet_send.py` drives known channel values, checked against the effect state and on camera *(Claude)*
- [ ] Gate: a SoundSwitch control (e.g. color or effect change) visibly changes the strips live *(owner + camera)*
- [ ] Latency baseline: Art-Net frame → LED change on camera (moved from Phase 11) *(Claude)*
- [ ] Parity: the audio mode still works when Art-Net control is off *(Claude)*

## Open questions

- Mac model; dev board models (the modules are known).
- Strip types and counts (config: 150 per strand), PSU rating, fusing, injection. A level converter and 330 Ω resistors are fitted.
- Why SoundSwitch Performance Mode sometimes outputs pure black, and why its send rate is about 21 fps wired vs about 36 on Wi-Fi.

## Recent decisions

See `docs/DECISIONS.md`. Latest (2026-10-14):
- Live setup: Mac on Ethernet in the S3's VLAN; ESP32s nearer the AP.
- Phase 11 gate judged on Art-Net loss and gaps (under 1%, no gap over 1 s), not frame rate.
- Latency baseline moved to Phase 12.
