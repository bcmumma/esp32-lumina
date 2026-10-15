# Project State

_Last updated: 2026-10-14_

## Current

The DJ and firmware tracks merged at Phase 10. Phases 10–15 are sequential (see `docs/ROADMAP.md`).

| Track | Phase | Status | Authorization |
|---|---|---|---|
| DJ (Mac + PC capture) | Complete (Phases 4–6) | Done | n/a |
| Firmware | 12: Intelligent Fixture v1 (`LUMINA-16`) | In progress (checklist complete) | Authorized 2026-10-14 for Phase 12 |

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

## Phase 12 checklist (Intelligent Fixture v1, `LUMINA-16`)

Gate (ROADMAP): a SoundSwitch control changes a real Lumina effect live.

- [x] Branch `feature/intelligent-fixture` off `develop-v23` *(owner OK 2026-10-14)*
- [x] `lib/Lumina16`: CH1–11 → shared effect on all strands (CH12–16 reserved); RGB → the receiver's 258-step hue wheel; brightness capped *(Claude)*
- [x] Strobe safety, rig-wide gate on the receiver for all four flash effects; off at boot, 3/s default, 8/s ceiling. Measured with 4 strands and 10/s bursts: off 0, 3/s → 2.5/s, 8/s → 5.25/s *(Claude)*
- [x] S3 `Lumina16Control` + `/api/control` (live-only; Art-Net control off at boot; cap ≤ 25%) *(Claude)*
- [x] Unit tests 10/10 (mapping, colour round-trip, white = sat 0, bands, bounds, cap, strobe flags/gate) *(Claude)*
- [x] Deterministic: known `LUMINA-16` values → S3 decode, receiver modes/brightness, camera colours (R/G/B/white, dimmer 0, effect band); falls back after 1 s without Art-Net *(Claude)*
- [x] Fixture built in SoundSwitch 2.11 (one cell `Main`, Intensity on CH1, CH4 "Reactivity"), patched at U1/1 in venue `Lumina 16` *(owner)*
- [x] **Gate:** the Red Autoloop drove intensity live (CH1 31–220 → brightness under the cap); a colour override changed strand 1 red → greens/teals live (Art-Net, S3, camera) *(owner + Claude)*
- [x] Latency baseline: Art-Net step → first camera frame showing it, median 123 ms (106–124), an upper bound including webcam delay; internal budget about 60 ms *(Claude)*
- [x] Parity with Art-Net control off; safe defaults after reboot *(Claude)*
- Note: SoundSwitch at first sent an all-zero blackout on both universes again (KNOWN_ISSUES); playing the Autoloop from Edit Mode cleared it.

## Open questions

- Mac model; dev board models (the modules are known).
- Strip types and counts (config: 150 per strand), PSU rating, fusing, injection. A level converter and 330 Ω resistors are fitted.
- Why SoundSwitch Performance Mode sometimes outputs pure black, and why its send rate is about 21 fps wired vs about 36 on Wi-Fi.

## Recent decisions

See `docs/DECISIONS.md`. Latest (2026-10-14):
- Live setup: Mac on Ethernet in the S3's VLAN; ESP32s nearer the AP.
- Phase 11 gate judged on Art-Net loss and gaps (under 1%, no gap over 1 s), not frame rate.
- Latency baseline moved to Phase 12.
