# Project State

_Last updated: 2026-10-15_

## Current

The DJ and firmware tracks merged at Phase 10. Phases 10–15 are sequential (see `docs/ROADMAP.md`).

| Track | Phase | Status | Authorization |
|---|---|---|---|
| DJ (Mac + PC capture) | Complete (Phases 4–6) | Done | n/a |
| Firmware | 13: Control-source arbitration | In progress | Authorized 2026-10-15 for Phase 13 |

- Authorize a phase with `/start-milestone Phase <N>` in Claude Code. Until then, everything is read-only.
- **Branch:** `main` holds v2 unchanged. `develop-v23` was fast-forwarded to `feature/intelligent-fixture` (`c368c96`, Phases 7–12; owner OK 2026-10-15). Phase 13 works on `feature/control-router`.
- **Last verified commit:** `ac7ddba` on `feature/intelligent-fixture` (Phase 12, closed 2026-10-15).
- **Boards:** the S3 runs the Phase 13 sender and the WROOM runs the Phase 12 receiver. Safe at boot: source AUTO, strobe off (3/s cap when on), Art-Net brightness cap 25%. Strand 1 is set to off live; its saved mode is 12.

## What works

- v3 firmware: v2 parity, modular, warning-free; 25/25 on-device unit tests.
- **SoundSwitch → Art-Net → S3:** discovery, universe 0, loss 0.08% with the Mac wired (Phase 11).
- **`LUMINA-16` v1:** SoundSwitch drives a shared effect on all strands (intensity, colour, effect, speed, …) live, capped at 25%. Toggle with `/api/control?artnet=1`.
- **Strobe safety:** rig-wide gate, off at boot, at most 3/s by default (8/s ceiling). README warning.
- **Test bench:** serial, tones, webcam, strand colours, Art-Net monitor/sender/compare/soak, `--lumina16` sender.
- **Phases done:** 0–12.

## Next step

1. Done 2026-10-15: `feature/intelligent-fixture` merged (fast-forward) into `develop-v23`. Phase 13 in progress.
2. **Owner, before brighter than 25% or before connecting strands 2–4:**
   - [ ] Fusing and power injection. The strips are rated 9 W/m, so 4 × 150 LEDs is about 15 A at full white, against about 6 A continuous from the 90 W supply. Proposal in `docs/HARDWARE.md`.
   - [ ] One measured current per strand at full white, to confirm the seller's 9 W/m and set the real cap (about 40% if the rating holds).
   - [ ] Decide on the AUDIO/STANDALONE brightness: the 25% cap only covers Art-Net, and the saved web brightness is 255. Either lower the sliders, or authorize a cap that covers every source.
   - [ ] Level converter: a reading on each side, and the part number.
3. **Owner, when convenient:**
   - [ ] Strands 2–4 mounted (same effect on all four).
   - [ ] AP security mode (S3 first join is about 6 s).
   - [ ] SoundSwitch blackout/resume quirks (KNOWN_ISSUES).
   - [ ] Re-measure S3 RSSI at the install position.

## Phase 13 owner checklist

- [x] Controller page card works *(owner 2026-10-15)*. The strobe confirm dialog was removed at the owner's request; the venue is signposted.
- [x] Real SoundSwitch AUTO test: switches seen on connect and disconnect *(owner 2026-10-15)*. Confirmed on the S3: `switches=7`, about 9,600 frames from the Mac, and back to AUDIO after the quit.

## Phase 13 draft checklist (Control-source arbitration)

Gate (ROADMAP): AUTO / STANDALONE / AUDIO / ARTNET switch without a reboot; Art-Net timeout falls back after about 1 s; the web UI can override; diagnostics show the active source.

- [x] Branch `feature/control-router` off `develop-v23` *(owner OK 2026-10-15)*
- [x] Source router on the S3:
  - ARTNET (`LUMINA-16`);
  - AUDIO (current web-configured effects reacting to the PCM1808);
  - STANDALONE (saved look, no audio reactivity);
  - AUTO (ARTNET while frames are fresh, else AUDIO).

  It replaces the Phase 12 Art-Net toggle *(Claude)*
- [x] Timeout ≥ about 1 s, tolerating the measured ≤480 ms stalls. Note: a paused show keeps sending frozen frames, so "paused" ≠ "timed out" *(Claude)*
- [x] Web UI: a source selector and a strobe section (enable, rate) on the controller page, with the active source shown *(Claude; owner checked the page)*
- [x] Persistence decided (owner): source, AUTO fallback, strobe rate, and STANDALONE level live in a small separate saved record (not the main CRC blob). Strobe enable is never saved. Boot default AUTO.
- [x] Tests: router state machine (unit, 9/9); on hardware, switch sources live, unplug Art-Net (quit SoundSwitch) → AUDIO within about 1 s, and back *(Claude: AUTO → ARTNET in 78 ms, → AUDIO 1045 ms after the stop; owner: real SoundSwitch)*
- [x] Parity in each source *(Claude)*

## Open questions

- Mac model; dev board models (the modules are known).
- Fusing and injection; real per-LED current. The strips are now known: WS2815 12 V, 30 LEDs/m, 9 W/m, IP30, 6 × 5 m rolls.
- Why SoundSwitch sometimes outputs all-zero frames, and why its rate is about 21 fps wired vs about 36 on Wi-Fi.

## Recent decisions

See `docs/DECISIONS.md`. Latest (2026-10-14):
- `LUMINA-16` v1 maps onto existing packet fields; CH12–16 reserved.
- Strobe safety: off at boot, 3/s default, 8/s ceiling, rig-wide gate on the receiver.
- Phase 12 toggles are live-only; brightness cap 25% until current is measured.
