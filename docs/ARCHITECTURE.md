# Architecture

Rules live in `docs/PROJECT_INSTRUCTIONS.md`. This file describes how Lumina fits together. Home-specific addresses are in `CLAUDE.local.md`, not here.

## Current Proven Path (v1.0–v2)

```text
DDJ-FLX4 RCA line out ─► PCM1808 ─I2S─► ESP32-S3 ─ESP-NOW─► ESP32-WROOM ─► Strands 1–4
                                          │
                                          └─ web UI on home Wi-Fi (station mode)
```

### Node Responsibilities

The sender doesn't stream pixel values. It sends compact feature and control packets, and each receiver renders its animations locally. This keeps wireless bandwidth low and rendering smooth.

- **Sender (S3):** capture PCM1808 audio, analyze it (energy, bands, onsets), host the web UI, store settings, and send ESP-NOW packets.
- **Receiver (WROOM):** receive ESP-NOW packets, parse lighting settings, render effects, apply color calibration, and drive the LED strands.

## Planned DJ Path (v3)

```text
DDJ-FLX4 ─USB─► Mac: Serato DJ Pro ─► SoundSwitch ─Art-Net/UDP 6454─► ESP32-S3 ─ESP-NOW─► ESP32-WROOM ─► LEDs
                                                                         ▲
                                                     PCM1808 (audio-reactive fallback)
```

- SoundSwitch owns track-aware show timing.
- Art-Net is DMX transport, not a beat API.
- Lumina never parses Serato's files.

## Operating Modes and Source Arbitration (Phase 13)

| Mode | Driven by |
|---|---|
| `STANDALONE` | Local effects; no audio needed |
| `AUDIO` | PCM1808 energy and bands (overall, bass, mid, treble, L/R, transients) |
| `ARTNET` | SoundSwitch DJ/show control |
| `AUTO` | Art-Net present → ARTNET. Art-Net timeout (~1 s) → fall back to the previous mode. |

The web UI can override the source manually. Diagnostics show the active source.

## Art-Net

- UDP port 6454. The first implementation receives and parses ArtDMX (header, opcode, universe, sequence, length) **and answers ArtPoll with ArtPollReply**. Discovery is required (see below).

**SoundSwitch baseline** (Phase 5–6, 2026-10-14):
- Setup: SoundSwitch 2.11 on macOS Tahoe, Mac on Wi-Fi, PC on Ethernet, same subnet. Measured with `tools/testbench/artnet_listen.py`; logs stay in gitignored `captures/`.

| Item | Value |
|---|---|
| Universes sent | Art-Net **0 and 1** (SoundSwitch supports two). SoundSwitch "Universe One" = Art-Net **universe 0** |
| Fixture check | `Lumina 4-Strand RGB` at address 1: the "Red" Autoloop gave CH1/4/7/10 pulsing 10–250 with G/B = 0 |
| Frame rate | About 36–37 frames/s per universe (dips to about 20–32 seen), playing or stopped |
| DMX length | 512 per frame |
| Loss | About 0.2% sequence gaps over Wi-Fi |
| Discovery | SoundSwitch polls (ArtPoll) about once a second and **sends ArtDmx only to nodes that answer**. When replies stop, it drops the node and frames stop within about 3 s. It resumes after replies return. **The S3 must answer every ArtPoll** |
| Stopped / paused show | Frames **keep coming** at full rate with the **last values frozen**. They don't drop to zero or stop |
| Idle (nothing playing) | Frames keep coming, all zero. MIDI-menu color overrides alone also give all zero; something must be playing |
| Source | The Mac's LAN address, UDP source port 6454 (ArtPoll and ArtDmx) |
| Sequence numbers | One counter shared across all nodes: with N nodes, each node sees steps of N |
| Broadcast vs unicast | **Unicast**, to exactly the IP the node advertises in its ArtPollReply. Mac `tcpdump` showed 530-byte packets (18-byte header + 512 channels): first to the address the PC monitor had advertised (a VPN adapter's), then to the PC's LAN address once the monitor advertised that instead (2026-10-14). Never broadcast |

- **Design consequences:**
  - The Phase 10 receiver needs an ArtPollReply, so it shows up as a node, and it must **advertise its own LAN IP** in that reply. SoundSwitch sends only there.
  - The Phase 13 "Art-Net timeout → fall back" only triggers when SoundSwitch quits or the network drops. A paused show looks live (frozen frames). Telling "paused" apart needs another signal (e.g. frames unchanged for N seconds), if it matters.

## Intelligent Fixture Profile `LUMINA-16` (Phase 12, draft)

| Ch | Function | Ch | Function |
|---|---|---|---|
| 1 | Master dimmer | 9–11 | Primary R/G/B |
| 2 | Effect | 12–14 | Secondary R/G/B |
| 3 | Speed | 15 | Transition |
| 4 | Intensity | 16 | Palette |
| 5 | Density | | |
| 6 | Strobe (rate-capped, see the safety rules) | | |
| 7 | Direction | | |
| 8 | Variation | | |

- The S3 sends effect parameters over ESP-NOW, not pixels. Each receiver renders locally, which keeps bandwidth low and makes long strips practical.
- Phase 4's learning fixture is a simpler 12-channel layout (4 × RGB cells); see `docs/PROJECT_STATE.md`.

## Direct Pixel Mode (Phase 15)

- RGB uses 3 channels per LED, so one 512-channel universe carries about 170 pixels.
- Prefer Art-Net direct to the WROOM. Don't relay pixel frames through the S3 over ESP-NOW unless testing proves it works.
- Receiver priority: pixel Art-Net when active, otherwise ESP-NOW intelligent mode after a timeout.

## Firmware Modules (v3, since Phase 9)

One PlatformIO project (`platformio.ini`): the `sender` env builds `src/sender/`, and the `receiver` env builds `src/receiver/`. Both include `lib/LuminaProtocol/LuminaProtocol.h`, the single definition of the 107-byte ESP-NOW packet (version 12), which `static_assert` guards. Byte 105, `senderChannel`, carries the sender's Wi-Fi channel so the receiver can lock straight to it.

| Sender (`src/sender/`) | Owns |
|---|---|
| `main.cpp` | `setup()` order and the audio → analyze → send loop |
| `config/ConfigManager` | Tuning settings, defaults, and the dual-slot CRC blob in NVS (`lumina_cfg`, slots `cfgA`/`cfgB`) |
| `lighting/LightingState` | LED/strand settings sent in every packet, and their defaults |
| `audio/PCM1808` | Pins 20/21/47/48, I2S at 48 kHz 32-bit stereo, `sample32_to_s16` |
| `audio/AudioAnalyzer` | Band filters, AGC, the (disabled) FFT beat detector, `analyze_audio_block` |
| `network/WiFiManager` | Station join and recovery, static IP from `include/secrets.h`, mDNS, the onboard status RGB |
| `network/EspNowSender` | Broadcast peer, rate-limited send, sequence numbers, send statistics |
| `web/LuminaWebServer`, `web/IndexHtml` | HTTP API (`/api/*`, `/health`), the controller page, loop/HTTP timing |
| `util/Clamp.h` | Shared clamp helpers |

| Receiver (`src/receiver/`) | Owns |
|---|---|
| `main.cpp` | `setup()`, packet hand-off, loss counting, the frame loop and its statistics |
| `network/EspNowReceiver` | Receive callback, channel scan, lock to the announced `senderChannel` (RSSI survey as fallback), Wi-Fi radio setup |
| `lighting/LedOutput` | NeoPixel strands (33/25/26/27, RGB 800 kHz), packet-driven hardware and color calibration, boot test |
| `lighting/Renderer` | Per-frame mode dispatch |
| `lighting/Effects` | The render modes |
| `lighting/Color.h`, `lighting/EffectState.h` | 8-bit color math; per-strand effect state |

The receiver has no config module because it persists nothing: its hardware and calibration settings arrive in every packet. On-device unit tests live in `test/` (`pio test -e sender_test`).

## Effects

These are v2's intended effects. They were provided but never verified, so check them in the repo.

- **Normal:** Solid Color, Aurora Drift, Starfield, Neon Scanner, Off.
- **Rave:** Laser Sweep, Rave Chase, Bass Cannons, DnB Chopper, Acid Chase, Portal Pulse, Spectrum Sparks, Hyper Strobe, Cyber Tunnel, White Lightning, DnB Split, Rave Tiles, Pulse Train, Void Bloom.
- Audio-reactive rave modes use continuous energy and band values, not a beat detector.
- VU tools (Overall, Bass, Mid, Treble) live in Audio Tune.
- The first `LUMINA-16` effects will be: Solid Color, Neon Scanner, Laser Sweep, Rave Chase, DnB Chopper, and White Lightning.

## Web UI Layout

1. Control All (default).
2. Strand 1–4 pages.
3. Audio Tune.
4. Hardware/Setup.

Keep it phone-friendly.
