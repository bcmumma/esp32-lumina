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
- **Both boards run** the Phase 12 firmware. Safe at boot: Art-Net control off, strobe off (3/s cap when on), brightness cap 25%. Strand 1 is set to off live; its saved mode is 12.

## What works

- v3 firmware: v2 parity, modular, warning-free; 25/25 on-device unit tests.
- **SoundSwitch → Art-Net → S3:** discovery, universe 0, loss 0.08% with the Mac wired (Phase 11).
- **`LUMINA-16` v1:** SoundSwitch drives a shared effect on all strands (intensity, colour, effect, speed, …) live, capped at 25%. Toggle with `/api/control?artnet=1`.
- **Strobe safety:** rig-wide gate, off at boot, at most 3/s by default (8/s ceiling). README warning.
- **Test bench:** serial, tones, webcam, strand colours, Art-Net monitor/sender/compare/soak, `--lumina16` sender.
- **Phases done:** 0–12.

## Next step

1. Done 2026-10-15: `feature/intelligent-fixture` merged (fast-forward) into `develop-v23`. Phase 13 in progress.
2. **Owner, before brighter than 25%:**
   - [ ] Fusing and power injection (90 W PSU, 600 LEDs).
   - [ ] Strip datasheet, or one measured current per strand at full white, to set the real cap.
   - [ ] Level converter: a reading on each side, and the part number.
3. **Owner, when convenient:**
   - [ ] Strands 2–4 mounted (same effect on all four).
   - [ ] AP security mode (S3 first join is about 6 s).
   - [ ] SoundSwitch blackout/resume quirks (KNOWN_ISSUES).
   - [ ] Re-measure S3 RSSI at the install position.

## Phase 13 draft checklist (Control-source arbitration)

Gate (ROADMAP): AUTO / STANDALONE / AUDIO / ARTNET switch without a reboot; Art-Net timeout falls back after about 1 s; the web UI can override; diagnostics show the active source.

- [x] Branch `feature/control-router` off `develop-v23` *(owner OK 2026-10-15)*
- [ ] Source router on the S3:
  - ARTNET (`LUMINA-16`);
  - AUDIO (current web-configured effects reacting to the PCM1808);
  - STANDALONE (saved look, no audio reactivity);
  - AUTO (ARTNET while frames are fresh, else AUDIO).

  It replaces the Phase 12 Art-Net toggle *(Claude)*
- [ ] Timeout ≥ about 1 s, tolerating the measured ≤480 ms stalls. Note: a paused show keeps sending frozen frames, so "paused" ≠ "timed out" *(Claude)*
- [ ] Web UI: a source selector and a strobe section (enable, rate) on the controller page, with the active source shown *(Claude; owner checks the page once)*
- [x] Persistence decided (owner): source, AUTO fallback, strobe rate, and STANDALONE level live in a small separate saved record (not the main CRC blob). Strobe enable is never saved. Boot default AUTO.
- [ ] Tests: router state machine (unit); on hardware, switch sources live, unplug Art-Net (quit SoundSwitch) → AUDIO within about 1 s, and back *(Claude; owner runs SoundSwitch)*
- [ ] Parity in each source *(Claude)*

## Open questions

- Mac model; dev board models (the modules are known).
- Strip types and counts (config: 150 per strand), fusing, injection; real per-LED current.
- Why SoundSwitch sometimes outputs all-zero frames, and why its rate is about 21 fps wired vs about 36 on Wi-Fi.

## Recent decisions

See `docs/DECISIONS.md`. Latest (2026-10-14):
- `LUMINA-16` v1 maps onto existing packet fields; CH12–16 reserved.
- Strobe safety: off at boot, 3/s default, 8/s ceiling, rig-wide gate on the receiver.
- Phase 12 toggles are live-only; brightness cap 25% until current is measured.
