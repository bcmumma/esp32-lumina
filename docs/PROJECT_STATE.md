# Project State

_Last updated: 2026-10-14_

## Current

The DJ and firmware tracks merged at Phase 10. Phases 10–15 are sequential (see `docs/ROADMAP.md`).

| Track | Phase | Status | Authorization |
|---|---|---|---|
| DJ (Mac + PC capture) | Complete (Phases 4–6) | Done | n/a |
| Firmware | 11: Network validation | Not started | Not authorized |

- Authorize a phase with `/start-milestone Phase <N>` in Claude Code. Until then, everything is read-only.
- **Branch:** `main` holds v2 unchanged. `develop-v23` = Phases 7–9 + DJ track (`ac75b8e`). Phase 10 is on `feature/artnet-receiver`; merging it into `develop-v23` is your call (a good merge point).
- **Last verified commit:** `62995bc` on `feature/artnet-receiver` (Phase 10, closed 2026-10-14).
- **Both boards run** the Phase 10 firmware. Strand 1 is set to off live; a reboot restores its saved audio mode (mode 12).

## What works

- v3 firmware: v2 parity, modular, warning-free; 15/15 on-device unit tests.
- **Art-Net into the S3 (parse/report only):**
  - SoundSwitch discovers "Lumina S3" and unicasts universe 0 at about 36 fps.
  - The S3's decoded values equal the PC's frame for frame, with a Red Autoloop and a Serato-driven show.
  - `/api/artnet` reports it.
- DJ path: Serato 4.0.4 → SoundSwitch 2.11 → Art-Net. A scripted show follows Serato play/pause (see KNOWN_ISSUES for the blackout and resume quirks).
- **Test bench:** serial, tones, webcam, strand colors, Art-Net monitor/sender/compare.
- **Phases done:** 0–10.

## Next step

1. **Your call:** merge `feature/artnet-receiver` into `develop-v23`.
2. **Phase 11** when authorized (draft below).
3. **Owner, when convenient:**
   - [ ] AP Wi-Fi security mode (WPA2 / mixed / WPA3), for the S3's 6 s first join.
   - [ ] Level converter: LV side about 3.3 V, HV side about 5 V, part number.
   - [ ] Strips on outputs 2–4, cycled on camera.
   - [ ] 12 V PSU amp rating, before brighter tests.
   - [ ] SoundSwitch quirks (KNOWN_ISSUES): Performance Mode blackout (blue-highlighted tracks? fader position?) and pause/resume.
4. **Optional (your network):** the PC routes its own LAN through the HomeLab VPN adapter, so `.100` traffic takes an extra hop. Switching the Ethernet port or the VPN routes is your call.

## Phase 11 draft checklist (Network validation)

Gate (ROADMAP): Art-Net stays stable on the real network (ping, RSSI, packet consistency, VLAN and firewall path).

- [ ] Place the S3 and the receiver at their install positions; antennas clear *(owner)*
- [ ] Preferred live setup: Mac on Ethernet, S3 on Wi-Fi (one wireless hop). Compare with the Mac on Wi-Fi *(owner switches; Claude measures)*
- [ ] 30-minute soak with SoundSwitch playing:
  - Art-Net fps and loss on the S3, measured as frames vs. the expected rate (gap counts aren't valid with several nodes);
  - S3 RSSI log;
  - ESP-NOW loss;
  - Wi-Fi reconnects;
  - heap trend *(Claude)*
- [ ] Ping/jitter: PC→S3, plus Mac→S3 if possible. Record the path (same VLAN; no firewall hop) *(Claude, owner for the Mac)*
- [ ] Latency baseline: Art-Net arrival vs. LED output (camera), and the post-antenna ESP-NOW latency that was never measured *(Claude)*
- [ ] Check Suricata/pfSense only if the traffic crosses them (it doesn't today: same subnet) *(owner)*
- [ ] Parity still holds *(Claude)*

## Open questions

- Mac model; dev board models (the modules are known).
- Strip types and counts (config: 150 per strand), PSU rating, fusing, injection. A level converter and 330 Ω resistors are fitted.
- Why SoundSwitch Performance Mode sometimes outputs pure black.

## Recent decisions

See `docs/DECISIONS.md`. Latest (2026-10-14):
- Phase 10 gate method: `artnet_compare.py` pairs frames by neighbour sequence and aligned time, because SoundSwitch shares one sequence counter across nodes.
- The S3's ArtPollReply advertises its own IP; SoundSwitch unicasts only there.
- `develop-v23` fast-forwarded to `feature/platformio-migration` (owner OK).
