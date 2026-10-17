# Roadmap — Lumina v3

**Reference only.** Don't build anything here until `docs/PROJECT_STATE.md` says the phase is current and authorized, or I ask. Rules are in `docs/PROJECT_INSTRUCTIONS.md`. Each phase gets a full pass checklist when it starts; the gates below are the minimum.

| # | Phase | Who | Completion gate | Status |
|---|---|---|---|---|
| 0 | Preserve known-good Lumina | Owner | v2 backed up and on GitHub; pinout preserved | **Done** |
| 1 | Prepare the Mac | Owner | Mac stable for DJ and dev work; Serato data backed up | **Done** |
| 2 | Verify Serato + DDJ-FLX4 | Owner | FLX4 works; playback, mixing, Spotify, cue points, and beatgrids all work | **Done** |
| 3 | Connect SoundSwitch to Serato | Owner | Connected; green in Performance Mode | **Done (enough to proceed)** |
| 4 | Minimum SoundSwitch workflow | Owner + chat | One track runs a saved SoundSwitch show synced to Serato playback | **Current** |
| 5 | Art-Net capture setup (Windows PC) | Owner + Claude Code | Wireshark/`tshark` on the PC sees SoundSwitch's Art-Net (broadcast, or unicast to the PC); the Mac, PC, and S3 share a subnet | |
| 6 | Prove SoundSwitch emits Art-Net | Owner + Claude Code | `Serato → SoundSwitch → Art-Net → capture` works; source, destination, universe, length, rate, and broadcast-or-unicast all recorded | |
| 7 | PlatformIO environment + test bench (Windows PC) | Claude Code | Trivial projects build and upload to both boards from the PC. Dual serial capture, webcam frame capture, and test-tone playback all work. | |
| 8 | Port v2 to PlatformIO, no behavior change | Claude Code + owner checks | Every parity check passes on hardware | |
| 9 | Modularize | Claude Code + owner checks | Modular firmware behaves like Phase 8; builds after every extraction | |
| 10 | First Art-Net receiver on the S3 | Claude Code | S3-parsed DMX values equal the Wireshark capture | |
| 11 | Network validation | Claude Code + owner | Art-Net stays stable on the real network (ping, RSSI, packet consistency, VLAN and firewall path) | |
| 12 | Intelligent Fixture v1 (`LUMINA-16`) | Claude Code + owner | A SoundSwitch control changes a real Lumina effect live | |
| 13 | Control-source arbitration | Claude Code | AUTO / STANDALONE / AUDIO / ARTNET switch without a reboot; Art-Net timeout falls back after about 1 s; the web UI can override; diagnostics show the active source | |
| 14 | PCM1808 fallback | Claude Code + owner | Energy, bass, mid, treble, left, right (and transients if useful) drive effects. Beat detection is not a goal. | |
| 15 | SoundSwitch cells and fallback chain | Claude Code + owner | A multi-cell SoundSwitch fixture shows chases on the strip; Auto runs SoundSwitch → Hybrid → Audio; one long strand (900 LEDs) with length-scaled looks (re-scoped by the owner 2026-10-17; Direct Pixel Art-Net moved to Later) | |

## Two Parallel Tracks

Phases 4–6 (**DJ track**: SoundSwitch on the Mac, plus capture on the PC) and Phases 7–9 (**firmware track**: PlatformIO port on the Windows PC) don't depend on each other. Run them in parallel, one phase per track at a time. Both tracks must be complete before Phase 10 (the first Art-Net code). Phases 10–15 are sequential.

## Phase Notes

**Phase 4 (current).** SoundSwitch can't autoscript streaming tracks, so use 3–5 local files. See `docs/PROJECT_STATE.md` for the steps.

**Phase 5–6.** Install Wireshark on the PC (it includes Npcap, which capture needs). If SoundSwitch only unicasts to one IP, set it to the PC's IP for these phases. Save the capture outside Git and commit only a sanitized summary to `docs/ARCHITECTURE.md`: universe, channel count, packet rate, and broadcast or unicast.

**Phase 8 diagnostics exception.** Diagnostics-only additions are allowed in Phase 8 because they don't change lighting behavior: structured log lines, the serial command interface, the diagnostics endpoint, and a test-pattern mode. Keep them command-gated or behind a build flag.

**Phase 8 parity checks.** Run these through the test bench wherever possible. The port is done when all of these still work:

- PCM1808 levels.
- Wi-Fi station join.
- Every web UI page.
- Settings save and load (the dual-slot CRC blob).
- ESP-NOW delivery.
- Strands 1–4 on GPIO 33/25/26/27 in that order.
- RGB colors, and white rendering as white.
- The ESP-NOW channel behavior.

**Phase 9 modules.**

| Firmware | Modules |
|---|---|
| Sender | `audio/PCM1808`, `audio/AudioAnalyzer`, `network/WiFiManager`, `network/EspNowSender`, `web/WebServer`, `lighting/LightingState`, `config/ConfigManager` |
| Receiver | `network/EspNowReceiver`, `lighting/Renderer`, `lighting/Effects`, `lighting/LedOutput`, `config/ConfigManager` |

Extract one subsystem at a time and build after each extraction.

**Phase 11.** The preferred live setup is the Mac on Ethernet and the S3 on Wi-Fi, so there's only one wireless hop. Use unicast if Art-Net has to cross VLANs. Check Suricata only if the traffic actually passes through it.

## Later

- **Direct Pixel Art-Net** (was Phase 15): external software drives individual pixels, likely Art-Net straight to the WROOM, falling back to ESP-NOW on timeout. 900 LEDs need 6 universes, too much for ESP-NOW.

These are tracked in `docs/DEV_BACKLOG.md`:

- An expanded fixture, plus global and per-strand fixtures.
- A SoundSwitch AutoScript library workflow.
- A diagnostics page.
- GitHub docs and demo videos.
- Live stress testing.
- Home Assistant and MQTT.
- AI scene generation.
- Ableton Link, only if justified.
- Multiple receiver nodes.
- A custom PCB.
