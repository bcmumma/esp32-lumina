# Known Issues and Regression Traps

## Open

| Issue | Impact | Plan |
|---|---|---|
| SoundSwitch can't autoscript streaming tracks | Spotify tracks can't be used for Phase 4 scripting | Use local test files (see `docs/DECISIONS.md`) |
| The SoundSwitch connection only shows green in Performance Mode, and Edit Mode doesn't mirror Serato playback | Confusing workflow | Probably expected: Performance Mode is live, Edit Mode is the editor. Confirm in Phase 4. |
| The receiver auto-scans channels 1–11; the sender uses the router's channel (6 on 2026-10-12). Since Phase 8 the receiver surveys a full sweep and locks to the channel with the strongest average RSSI. At centimetre range, channels ±1 from the sender arrive within 1–3 dB, so it may lock one channel off (seen: 5 instead of 6). Loss there measured about 0–1.5%, at or below the v2 baseline | One-channel-off lock at short range; not measured at install distance | Phase 9 or later: the sender writes its channel into the packet and the receiver jumps to it (a protocol change for both boards). Check lock and loss at install distance |
| The S3 onboard RGB is on GPIO48, the same pin as PCM1808 DOUT (confirmed from the v20.1 boot log, 2026-10-12) | The status LED would break audio | Keep the status LED disabled unless DOUT moves to another free GPIO |
| The S3 serial status line shows `pps=0`, because `packets_per_sec` is only recalculated inside the `/api/state` and `/api/live` handlers | The serial log misreports ESP-NOW as idle. On 2026-10-12, `/api/live` showed about 33 pps, `enqErr=0` and `cbFail=0` | Don't trust serial `pps`; poll `/api/live`. Fix the counter in Phase 9 or later (no behavior change in Phase 8) |
| Cosmetic: the sender banner still says `v20.1` in the v22 code. (Since the Arduino 3.3.12 build, the receiver MAC prints correctly; it no longer prints zeros. The receiver also logs a harmless `[E][STA.cpp] disconnect(): STA not started!` at boot) | Misleading logs | Fix in Phase 9 or later |
| Receiver frame rate is capped at about 41 fps, because `show` takes 23.6 ms for 4 × 150 LEDs written one strand at a time | Limits smoothness and latency | Baseline for Phase 11. Parallel output (RMT/I2S) is an option later, not in Phase 8 |
| On boot, the S3 reports `Wi-Fi connected` with a blank SSID and 0 dBm, then drops right away. It reconnects about 10 s later and stays on at -73 to -77 dBm (40 s capture, 2026-10-12) | The boot log is misleading; the first ~10 s have no Wi-Fi; the signal is weak | No change in the Phase 8 port (parity first). Look at it in Phase 9 or later. Check antenna placement first, since -75 dBm is marginal |
| On 2026-10-12, 12 V touched the receiver's level converter. Strand 1 is proven afterwards (RGB and white on camera in Phase 8). The converter channels for strands 2–4 are untested: the owner waived them in Phase 8 and expects easy passes. GPIO 33/25/26/27 internal pull-downs read 1 (likely the converter's pull-ups, UNVERIFIED) | A damaged converter channel would look like a firmware fault | Owner checks the converter rails (LV about 3.3 V, HV about 5 V) and its part number; test strands 2–4 on camera when they're connected |
| Strip power, fusing, and level shifting are undocumented | Safety and reliability | Inspect before calling the build product-ready |
| Post-antenna-fix latency was never measured | No baseline for Phase 11 | Measure in Phase 11 |
| Build warnings: 2 in the sender and 4 in the receiver, all `'++' expression of 'volatile'-qualified type is deprecated` (C++20) on v2 counters | None (warnings only) | Clean up during Phase 9 |
| Receiver app uses 71% of its 1.25 MB partition (`esp32dev` default 4 MB layout) | Room for growth is limited | Consider a larger app partition before Phase 12 features |

## Regression Traps (don't reintroduce)

- **Color order:** it's RGB. Don't default back to GRB during the migration.
- **White as red:** `if (st.saturation == 0) st.saturation = 255;` turned white into red. Saturation 0 is valid.
- **Strand order:** 33, 25, 26, 27. The old plan (25/26/27/33) is wrong.
- **Config saves:** don't go back to dozens of separate Preferences writes. The v20 error was `Configuration save FAILED: activeStrands=4 RGBscale=[128,255,126]`.
- **Arduino `.ino` auto-prototypes:** these broke the `AudioLedPacket` and `VuSource` declarations. Normal `.cpp`/`.h` fixes this; declare types before use.
- **Antenna:** a blocked antenna caused huge latency. Check RSSI before blaming the network stack.
- **LED failures:** debug in layers (electrical, then renderer, then config, then ESP-NOW). The fastest proof is a hardcoded NeoPixel test (it was GPIO25 with 10 LEDs).
