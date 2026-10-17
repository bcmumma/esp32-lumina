# Project State

_Last updated: 2026-10-17_

## Current

The DJ and firmware tracks merged at Phase 10. Phases 10–15 are sequential (see `docs/ROADMAP.md`).

| Track | Phase | Status | Authorization |
|---|---|---|---|
| DJ (Mac + PC capture) | Complete (Phases 4–6) | Done | n/a |
| Firmware | 15: scope to be confirmed (see draft) | Not started | Not authorized |

- Authorize a phase with `/start-milestone Phase <N>` in Claude Code. Until then, everything is read-only.
- **Branch:** `main` holds v2 unchanged. `develop-v23` = `c368c96` (Phases 7–12). Phases 13–14 are on `feature/control-router`.
- **Last verified commit:** `7e3b8ff` on `feature/control-router` (Phase 14, closed 2026-10-17).
- **Boards:** both run the Phase 14 firmware. At boot: source AUTO, strobe off, brightness cap 100% (owner). Saved by the owner: strobe rate 10/s, Kick Flash 100%.

## What works

- v3 firmware: v2 parity, modular, warning-free; 41/41 on-device unit tests.
- **SoundSwitch → Art-Net → S3:** `LUMINA-16` drives a shared effect; AUTO switches to it while it sends and falls back to audio about 1 s after.
- **Audio looks:** bass-hit detection (not tempo) drives kick modes. Owner favourites: Twinkle, Kick Pump, Kick Strobe, Kick Ripple, Glitter Kick.
- **Strobe:** Strobe and Drop Strobe modes and a rig-wide Kick Flash, all behind one gate: off at boot, max rate 3/s default, 15/s ceiling.
- **Controller page:** Show/Setup screens, genre groups, source and strobe controls. Source `src/sender/web/index.html`, built by `tools/build_page.py`.
- **Phases done:** 0–14.

## Next step

1. **Owner:** confirm the Phase 15 scope below, or keep the ROADMAP's "Direct Pixel Art-Net". Also decide whether to fast-forward `develop-v23` to `feature/control-router` (Phases 13–14).
2. **Owner, before more LEDs are lit (no brightness cap now):**
   - [ ] Fusing and power injection; proposal in `docs/HARDWARE.md`.
   - [ ] One measured current at full white, to confirm the seller's 9 W/m.
   - [ ] Level converter: a reading on each side, and the part number.
3. **Owner, when convenient:** AP security mode (slow first join); SoundSwitch blackout quirks; S3 RSSI at the install position.

## Phase 15 draft (proposed re-scope, owner to confirm): one long strand, high-quality looks

Owner direction (2026-10-17): one strand (or two) of 900 LEDs total, powered by three 90 W supplies; SoundSwitch's AUTO scripting looks flat ("breathing"); interest in a hybrid where SoundSwitch and the audio decide together.

- [ ] Wiring plan for 900 LEDs and three supplies (owner wires; Claude proposes): each supply feeds its own 300 LEDs, +12 V sections kept separate, grounds common, injection at each roll, a fuse per feed. 900 × 25 mA is about 22.5 A at full white against about 18 A from three supplies, so the cap goes to about 80% *(owner)*
- [ ] One logical strand of up to 900 LEDs; measure the frame rate (about 33 fps expected on one pin) *(Claude)*
- [ ] Length-scaled motion (speed in strand lengths per second), favourites first *(Claude; owner judges)*
- [ ] Hybrid source: SoundSwitch sets colour, intensity and mode per section while the PCM1808 supplies the kicks *(Claude; owner tests with SoundSwitch)*
- [ ] Research, read-only: whether Serato or SoundSwitch can share a beat clock (for example Ableton Link) the S3 could join. UNVERIFIED *(Claude)*

## Open questions

- Mac model; dev board models (the modules are known).
- Real per-LED current; the final strand layout (one 900-LED strand, or two).
- Why SoundSwitch sometimes outputs all-zero frames, and why its rate differs wired vs Wi-Fi.

## Recent decisions

See `docs/DECISIONS.md`. Latest (2026-10-15 to 2026-10-17):
- Brightness cap removed by the owner; strobe ceiling raised to 15/s; no strobe confirm dialog (venue signposted).
- Intense strobe features added, all behind the strobe gate.
- Modes grouped by the owner's genres; mode IDs never renumber.
