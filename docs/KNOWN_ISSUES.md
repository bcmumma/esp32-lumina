# Known Issues and Regression Traps

## Open

| Issue | Impact | Plan |
|---|---|---|
| v2 was rebuilt but never confirmed to compile or run on hardware | The port baseline is uncertain | Repo audit now; build in Phase 7–8 |
| SoundSwitch can't autoscript streaming tracks | Spotify tracks can't be used for Phase 4 scripting | Use local test files (see `docs/DECISIONS.md`) |
| The SoundSwitch connection only shows green in Performance Mode, and Edit Mode doesn't mirror Serato playback | Confusing workflow | Probably expected: Performance Mode is live, Edit Mode is the editor. Confirm in Phase 4. |
| ESP-NOW channel behavior in v2 is unknown (fixed channel 1 or auto-scan) | Sender and receiver must share a channel | Check in the repo audit |
| The S3 onboard RGB may share GPIO48 with PCM1808 DOUT | The status LED could break audio | Confirm the board's RGB pin; keep the status LED disabled if it conflicts |
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
