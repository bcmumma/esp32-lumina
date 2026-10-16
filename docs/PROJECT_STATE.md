# Project State

_Last updated: 2026-10-15_

## Current

The DJ and firmware tracks merged at Phase 10. Phases 10–15 are sequential (see `docs/ROADMAP.md`).

| Track | Phase | Status | Authorization |
|---|---|---|---|
| DJ (Mac + PC capture) | Complete (Phases 4–6) | Done | n/a |
| Firmware | 14: PCM1808 fallback | Not started | Not authorized |

- Authorize a phase with `/start-milestone Phase <N>` in Claude Code. Until then, everything is read-only.
- **Branch:** `main` holds v2 unchanged. `develop-v23` = `c368c96` (Phases 7–12). Phase 13 is on `feature/control-router`.
- **Last verified commit:** `98a73e6` on `feature/control-router` (Phase 13, closed 2026-10-15).
- **Boards:** the S3 runs the Phase 13 sender and the WROOM runs the Phase 12 receiver. Safe at boot: source AUTO, strobe off (3/s cap when on), Art-Net brightness cap 25%.

## What works

- v3 firmware: v2 parity, modular, warning-free; 34/34 on-device unit tests.
- **SoundSwitch → Art-Net → S3:** discovery, universe 0, loss 0.08% with the Mac wired (Phase 11).
- **`LUMINA-16` v1:** SoundSwitch drives a shared effect on all strands, capped at 25%.
- **Control sources:** AUTO (the default) uses SoundSwitch while it sends and falls back to audio about 1 s after it stops. AUDIO, STANDALONE, and ARTNET can be picked on the controller page or through `/api/control`.
- **Strobe safety:** rig-wide gate, off at boot, at most 3/s by default (8/s ceiling). No confirm dialog, since the venue is signposted.
- **Test bench:** serial, tones, webcam, strand colours, and the Art-Net tools (monitor, sender, compare, soak, `--lumina16`).
- **Phases done:** 0–13.

## Next step

1. **Owner:** decide whether to fast-forward `develop-v23` to `feature/control-router` (Phase 13). Then authorize Phase 14 with `/start-milestone Phase 14`.
2. **Owner, before brighter than 25% or before connecting strands 2–4:**
   - [ ] Fusing and power injection. The strips are rated 9 W/m, so 4 × 150 LEDs is about 15 A at full white, against about 6 A continuous from the 90 W supply. Proposal in `docs/HARDWARE.md`.
   - [ ] One measured current per strand at full white, to confirm the seller's 9 W/m and set the real cap (about 40% if the rating holds).
   - [ ] Decide on AUDIO/STANDALONE brightness. The 25% cap only covers Art-Net, and the saved web brightness is 255. Either lower the sliders, or authorize a cap that covers every source (this could join Phase 14).
   - [ ] Level converter: a reading on each side, and the part number.
3. **Owner, when convenient:**
   - [ ] Strands 2–4 mounted (same effect on all four).
   - [ ] AP security mode (the S3's first join is sometimes about 6 s).
   - [ ] SoundSwitch blackout/resume quirks. Under AUTO a blackout keeps the rig dark (KNOWN_ISSUES).
   - [ ] Re-measure S3 RSSI at the install position.

## Phase 14 draft checklist (PCM1808 fallback)

Gate (ROADMAP): energy, bass, mid, treble, left, and right (plus transients if useful) drive effects. Beat detection is not a goal.

- [ ] Branch: owner's choice. Proposal: `feature/audio-fallback` off `develop-v23` after the Phase 13 merge *(owner)*
- [ ] Audit which effects use which audio fields today, and which fields (left/right) reach the receiver but go unused *(Claude)*
- [ ] Make the fallback look good unattended: one AUDIO preset that uses energy, bands, and stereo, without beat detection *(Claude; owner judges the look)*
- [ ] Transients (onset strength) as a field, only if the audit shows a use for it *(Claude)*
- [ ] Tests: tone sweep per band and left/right separately; music playback; webcam frames *(Claude)*
- [ ] Parity, plus AUTO fallback still within about 1 s *(Claude)*

## Open questions

- Mac model; dev board models (the modules are known).
- Fusing and injection; the real per-LED current. The strips are known: WS2815 12 V, 30 LEDs/m, 9 W/m, IP30, 6 × 5 m rolls.
- Why SoundSwitch sometimes outputs all-zero frames, and why its rate is about 21 fps wired vs about 36 on Wi-Fi.

## Recent decisions

See `docs/DECISIONS.md`. Latest (2026-10-15):
- Control sources: boot default AUTO; settings in a separate NVS record; strobe enable never saved.
- No strobe confirm dialog (the venue is signposted); every other strobe safety stays.
- Brightness budget revised to about 40% from the strips' 9 W/m rating (to be confirmed by measurement).
