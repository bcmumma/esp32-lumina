# Project State

_Last updated: 2026-10-17_

## Current

The DJ and firmware tracks merged at Phase 10. Phases 10–15 are sequential (see `docs/ROADMAP.md`).

| Track | Phase | Status | Authorization |
|---|---|---|---|
| DJ (Mac + PC capture) | Complete (Phases 4–6) | Done | n/a |
| Firmware | 15: SoundSwitch cells, fallback chain, long strand | In progress | Authorized 2026-10-17 for Phase 15 |

- Authorize a phase with `/start-milestone Phase <N>` in Claude Code. Until then, everything is read-only.
- **Branch:** `main` holds v2 unchanged. `develop-v23` = `a7f2c67` (Phases 7–14, fast-forwarded 2026-10-17, owner OK). Phase 15 is on `feature/cells`.
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

1. **Owner (now):** build the 30-cell fixture and run the two plays (Phase 15 owner checklist below).
2. **Owner, before more LEDs are lit (no brightness cap now):**
   - [ ] Fusing and power injection; proposal in `docs/HARDWARE.md`.
   - [ ] One measured current at full white, to confirm the seller's 9 W/m.
   - [ ] Level converter: a reading on each side, and the part number.
3. **Owner, when convenient:** AP security mode (slow first join); SoundSwitch blackout quirks; S3 RSSI at the install position.

## Phase 15 owner checklist (now)

- [ ] Build `Lumina 30-Cell` in Fixture Manager: mode `30 Cell`, 90 channels, cells `Cell 01`…`Cell 30` with Red/Green/Blue = 3n−2 / 3n−1 / 3n, no `MASTER`; Check Fixture passes. Also note whether Fixture Manager can **export** a fixture, and the file type.
- [ ] Venue `Lumina 30` with the fixture on Universe 1, address 1; Lumina S3 on Universe One.
- [ ] Play a moving Autoloop and tell Claude "autoloop" plus its name; then AutoScript a track in the `Lumina 30` venue, play it from Serato, and tell Claude "autoscript playing". Claude reports whether the cells chase or breathe.

## Phase 15 checklist (SoundSwitch cells, fallback chain, long strand)

Scope (owner, 2026-10-17): make SoundSwitch's auto program look good with a multi-cell fixture, Auto chain SoundSwitch → Hybrid → Audio, one long strand (900 LEDs). Direct Pixel Art-Net moves to Later.

- [x] Branch `feature/cells` off `develop-v23` (fast-forwarded to Phase 14) *(owner OK)*
- [ ] Cells packet (protocol only): second ESP-NOW packet type with up to 150 cells; receiver accepts it, nothing drawn yet *(Claude)*
- [ ] Cells on the strip: S3 decodes a 30-cell fixture (saved fixture type and cell count), receiver blends cells along the strand; Kick Flash still applies *(Claude; owner judges)*
- [ ] Chain: SoundSwitch while lit frames arrive; Hybrid (Audio look coloured by SoundSwitch's last colour, audio drives motion) after a stop or about 3 s of black; Audio after about 10 s. Each can be picked by hand; the page gains Hybrid. Router unit tests *(Claude; owner runs SoundSwitch)*
- [ ] Long strand: favourites scale motion and sparkle density to strand length (150 LEDs unchanged); frame rate measured at 900 *(Claude)*
- [ ] Wiring plan for 900 LEDs and three 90 W supplies in `docs/HARDWARE.md` *(Claude proposes; owner wires)*
- [ ] Parity at 150 LEDs; strobe gate unchanged *(Claude)*
- [ ] Owner look check: SoundSwitch cells, Hybrid, Audio *(owner)*

## Open questions

- Mac model; dev board models (the modules are known).
- Real per-LED current; the final strand layout (one 900-LED strand, or two).
- Why SoundSwitch sometimes outputs all-zero frames, and why its rate differs wired vs Wi-Fi.

## Recent decisions

See `docs/DECISIONS.md`. Latest (2026-10-15 to 2026-10-17):
- Brightness cap removed by the owner; strobe ceiling raised to 15/s; no strobe confirm dialog (venue signposted).
- Intense strobe features added, all behind the strobe gate.
- Modes grouped by the owner's genres; mode IDs never renumber.
