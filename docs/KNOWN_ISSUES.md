# Known Issues and Regression Traps

## Open

| Issue | Impact | Plan |
|---|---|---|
| v2 was rebuilt but never confirmed to compile or run on hardware | The port baseline is uncertain | Repo audit now; build in Phase 7–8 |
| SoundSwitch can't autoscript streaming tracks | Spotify tracks can't be used for Phase 4 scripting | Use local test files (see `docs/DECISIONS.md`) |
| The SoundSwitch connection only shows green in Performance Mode, and Edit Mode doesn't mirror Serato playback | Confusing workflow | Probably expected: Performance Mode is live, Edit Mode is the editor. Confirm in Phase 4. |
| ESP-NOW channel behavior in v2 is unknown (fixed channel 1 or auto-scan) | Sender and receiver must share a channel | Check in the repo audit |
| The S3 onboard RGB is on GPIO48, the same pin as PCM1808 DOUT (confirmed from the v20.1 boot log, 2026-10-12) | The status LED would break audio | Keep the status LED disabled unless DOUT moves to another free GPIO |
| The S3 serial status line shows `pps=0`, because `packets_per_sec` is only recalculated inside the `/api/state` and `/api/live` handlers | The serial log misreports ESP-NOW as idle. On 2026-10-12, `/api/live` showed about 33 pps, `enqErr=0` and `cbFail=0` | Don't trust serial `pps`; poll `/api/live`. Fix the counter in Phase 9 or later (no behavior change in Phase 8) |
| Cosmetic log bugs: the sender banner says `v20.1` in the v22 code, and the receiver prints `LED receiver MAC: 00:00:00:00:00:00` | Misleading logs | Fix after the Phase 8 parity port |
| Receiver frame rate is capped at about 41 fps, because `show` takes 23.6 ms for 4 × 150 LEDs written one strand at a time | Limits smoothness and latency | Baseline for Phase 11. Parallel output (RMT/I2S) is an option later, not in Phase 8 |
| On boot, the S3 reports `Wi-Fi connected` with a blank SSID and 0 dBm, then drops right away. It reconnects about 10 s later and stays on at -73 to -77 dBm (40 s capture, 2026-10-12) | The boot log is misleading; the first ~10 s have no Wi-Fi; the signal is weak | No change in the Phase 8 port (parity first). Look at it in Phase 9 or later. Check antenna placement first, since -75 dBm is marginal |
| On 2026-10-12, 12 V touched the receiver's level converter. Strand 1 is proven afterwards; the converter channels for strands 2–4 are untested. GPIO 33/25/26/27 internal pull-downs read 1 (likely the converter's pull-ups, UNVERIFIED) | A damaged converter channel would look like a firmware fault in Phase 8 | Owner checks the converter rails (LV about 3.3 V, HV about 5 V) and its part number; test strands 2–4 on camera before the Phase 8 strand-order parity check |
| Strip power, fusing, and level shifting are undocumented | Safety and reliability | Inspect before calling the build product-ready |
| Post-antenna-fix latency was never measured | No baseline for Phase 11 | Measure in Phase 11 |

## Regression Traps (don't reintroduce)

- **Color order:** it's RGB. Don't default back to GRB during the migration.
- **White as red:** `if (st.saturation == 0) st.saturation = 255;` turned white into red. Saturation 0 is valid.
- **Strand order:** 33, 25, 26, 27. The old plan (25/26/27/33) is wrong.
- **Config saves:** don't go back to dozens of separate Preferences writes. The v20 error was `Configuration save FAILED: activeStrands=4 RGBscale=[128,255,126]`.
- **Arduino `.ino` auto-prototypes:** these broke the `AudioLedPacket` and `VuSource` declarations. Normal `.cpp`/`.h` fixes this; declare types before use.
- **Antenna:** a blocked antenna caused huge latency. Check RSSI before blaming the network stack.
- **LED failures:** debug in layers (electrical, then renderer, then config, then ESP-NOW). The fastest proof is a hardcoded NeoPixel test (it was GPIO25 with 10 LEDs).
