# Project State

_Last updated: 2026-10-27_

## Current

The DJ and firmware tracks merged at Phase 10. Phases 10–15 are done; the ROADMAP has no Phase 16 yet.

| Track | Phase | Status | Authorization |
|---|---|---|---|
| DJ (Mac + PC capture) | Complete (Phases 4–6) | Done | n/a |
| Firmware | 16: Show hardening (receiver heartbeat, soak, per-room power) | In progress | Authorized 2026-10-27 for Phase 16 |

- Authorize a phase with `/start-milestone Phase <N>` in Claude Code. Until then, everything is read-only.
- **Branch:** `main` holds v2 unchanged. `develop-v23` = `6b3d860` (Phases 7–15, fast-forwarded 2026-10-27, owner OK). Phase 16 is on `feature/show-hardening`.
- **Last verified commit:** `d930bb9` on `feature/cells` (Phase 15, closed 2026-10-27).
- **Boards:**
  - **S3:** Phase 15 sender.
  - **Room 1** (`receiver`): strand 1, GPIO 33.
  - **Room 2** (`receiver_room2`): strand 3, GPIO 26.
  - **Saved:** 3 active strands, Standalone source, green look, layout L1, strobe off at boot.

## What works

- **Firmware:** v3 with v2 parity; 72/72 on-device unit tests; sender and both receiver builds compile without warnings.
- **SoundSwitch:**
  - six layouts L1–L6, each with a 5-channel Lumina FX fixture;
  - the Auto chain SoundSwitch → Hybrid → Audio;
  - the FX fixture drives a beam, kick looks, UV sparkle, and a gated FX strobe.
- **Looks:** genres House, Techno, Drum & Bass, Acid, General, Chill, Halloween, Meters; 7 kick looks; Setup can hide buttons.
- **Two rooms from one S3:** the same show in both, each with its own wiring.
- **Strobe:** off at boot, 15/s ceiling, one gate per room.
- **Phases done:** 0–15.

## Phase 16 checklist (show hardening)

Scope (owner, 2026-10-27): receiver heartbeat, receiver firmware updates over Wi-Fi (added by the owner the same day), a full-show soak, and per-room power.

- [x] Branch `feature/show-hardening` off `develop-v23` (fast-forwarded to Phase 15) *(owner OK)*
- [x] Heartbeat packet (protocol only, `c8ce9eb`): receivers send a small status packet back to the S3 about once a second (room, packets received and lost, frame rate, LED counts, uptime); unit tests *(Claude)*
- [x] S3 tracks each room and the page shows "Room 1/2 online · loss %", offline after about 5 s of silence (`e56d473`, heartbeat tests 6/6) *(Claude)*
- [x] Wireless updates: Setup > Receiver updates (or `/api/control?update=N`) puts a room on the home Wi-Fi for 10 min with an HTTP update server (password in the gitignored secrets.h); `tools/ota_update.py --room N` builds, uploads and confirms (`e56d473`) *(Claude)*
- [ ] Owner: plug both receivers in over USB **one last time** so Claude can flash the Phase 16 firmware; after that, updates go over Wi-Fi *(owner)*
- [ ] Wireless update test: update each room with `tools/ota_update.py` and see it check in again *(Claude)*
- [ ] Owner: with both rooms on, the page shows both online; unplug one receiver's power and it shows offline within about 5 s *(owner)*
- [ ] Soak: 60 minutes of the full show (SoundSwitch + FX, Auto), both rooms. Pass: both online throughout, loss under about 2%, no restarts, S3 memory steady *(Claude; owner runs SoundSwitch)*
- [ ] Power per room: a second supply with injection, or the brightness cap at about 50%, and a 5 A fuse on each 12 V feed (`docs/HARDWARE.md`) *(owner)*
- [ ] Parity checks and docs *(Claude)*

## Owner, when convenient

- [ ] Bench audio: make the Modi/Magni the Windows default output (Voicemeeter may have taken over), so Claude can run tone tests again.
- [ ] One measured current at full white, to confirm the 9 W/m rating.
- [ ] Look check of the Halloween looks and the newer kick looks.

## Open questions

- Mac model; dev board models.
- Real per-LED current.
- Why the WROOMs drop off USB now and then (power, cable, or hub).
- Why SoundSwitch sometimes outputs all-zero frames.

## Recent decisions

See `docs/DECISIONS.md`. Latest (2026-10-23 to 2026-10-27):
- Six SoundSwitch layouts with the Lumina FX fixture; the 20-layout test chart is retired.
- Two room receivers from one S3: same show, own wiring.
- Halloween genre; colour buttons follow the receiver's colour wheel (Green 86, Blue 171).
