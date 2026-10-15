# Known Issues and Regression Traps

## Open

| Issue | Impact | Plan |
|---|---|---|
| SoundSwitch can't autoscript streaming tracks | Spotify tracks can't be used for Phase 4 scripting | Use local test files (see `docs/DECISIONS.md`) |
| The SoundSwitch connection only shows green in Performance Mode, and Edit Mode doesn't mirror Serato playback | Confusing workflow | Probably expected: Performance Mode is live, Edit Mode is the editor. Confirm in Phase 4. |
| The receiver auto-scans channels 1–11 until the first packet. Since 2026-10-12 every packet carries `senderChannel`, so the receiver locks straight to it (6 in all tests, about 4 s after a receiver reset) and retunes if it ever differs. The RSSI survey (Phase 8) remains only as a fallback for a sender that announces 0 | None seen at the bench; not yet measured at install distance | Check lock and loss at install distance |
| The S3 onboard RGB is on GPIO48, the same pin as PCM1808 DOUT (confirmed from the v20.1 boot log, 2026-10-12) | The status LED would break audio | Keep the status LED disabled unless DOUT moves to another free GPIO |
| Receiver frame rate is capped at about 41 fps, because `show` takes 23.6 ms for 4 × 150 LEDs written one strand at a time | Limits smoothness and latency | Baseline for Phase 11. Parallel output (RMT/I2S) is an option later, not in Phase 8 |
| The S3's first Wi-Fi join takes about 6 s: on every boot, the first attempt fails with `Reason 202 AUTH_FAIL` (+0.9 s), the next with `Reason 2 AUTH_EXPIRE` (+5.2 s), then it joins (debug log, 2026-10-12, with the correct password). Deterministic, so probably not signal. Suspect a WPA3/PMF negotiation with the AP | Sender is offline for about 6 s after boot | Owner: report the AP's security mode (WPA2 / WPA2-WPA3 mixed / WPA3). Then try pinning the auth mode (e.g. `WiFi.setMinSecurity`) |
| On 2026-10-12, 12 V touched the receiver's level converter. Strand 1 is proven afterwards (RGB and white on camera in Phase 8). The converter channels for strands 2–4 are untested: the owner waived them in Phase 8 and expects easy passes. GPIO 33/25/26/27 internal pull-downs read 1 (likely the converter's pull-ups, UNVERIFIED) | A damaged converter channel would look like a firmware fault | Owner checks the converter rails (LV about 3.3 V, HV about 5 V) and its part number; test strands 2–4 on camera when they're connected |
| SoundSwitch 2.11 + Serato 4.0.4: about 13 s into a Serato pause, SoundSwitch fades the Art-Net output to black, and after pressing play it **stayed black for about 2 minutes** until the owner re-cued (jump or pause/play) in Serato (Art-Net monitor, 2026-10-14). Cause unverified: either SoundSwitch loses the playback position after a pause, or the track resumed in a long dark section | A show may not come back after a pause during a set | Retest: pause, play, and watch the monitor without re-cueing; try a different track. If it reproduces, check SoundSwitch support for Serato 4 notes |
| The PC routes its own LAN subnet through a VPN adapter ("HomeLab") instead of its Ethernet port on that LAN (found 2026-10-14). A tool that advertises "the routed address" picks the VPN address, and LAN traffic takes an extra hop through the router | Extra latency and router load for PC↔LAN traffic. `artnet_listen.py` now advertises the same-subnet address (with an `--advertise` override) | Owner: check the VPN's allowed IPs/routes if direct LAN routing is wanted (a network change, owner's call) |
| SoundSwitch numbers Art-Net frames from **one counter shared by all nodes**. With two nodes, each node sees steps of 2 (PC even, S3 odd), so the per-node `gaps` counters (S3 serial and `/api/artnet`, `artnet_listen.py`) count every frame as a gap; with one node, steps are 1 and gaps are 0 (2026-10-14). The Art-Net spec expects per-universe +1 steps | Gap counts aren't a loss measure when several nodes are registered | For Phase 11 loss, use frames vs expected rate, or run with only the S3 registered. `artnet_compare.py` handles it (pairs by neighbour sequence and time) |
| SoundSwitch 2.11 Performance Mode sometimes outputs **pure black on every channel of both universes** while Serato plays a scripted track (seen several times 2026-10-14, on both nodes alike, so not the S3). Light came back after reloading the track, and immediately after a fresh start (quit both, open Serato, load the track, then SoundSwitch Performance Mode). Related to the pause/resume item above? | The show can go dark mid-set | Owner to test the suspects: Serato's **blue-highlighted (already played) tracks**, and the **channel fader / crossfader position** (middle vs full). Also the MIDI-menu Blackout and master dimmer. Workaround: reload the track |
| Short Art-Net stalls on the **S3's Wi-Fi hop**: about 22 gaps over 250 ms per 30 min, max 480 ms, even with the Mac wired. Ping to the S3 p95 22 ms / max 307 ms, while the wired Mac is p95 1 ms. Bench RSSI median -72 dBm (min -83) (Phase 11, 2026-10-14) | Brief freezes possible when Art-Net drives LEDs | Re-measure at the install position, nearer the AP. Phase 13's Art-Net timeout must tolerate about 0.5 s gaps: keep it at about 1 s or more |
| SoundSwitch's Art-Net send rate depends on the Mac's setup: about 36 fps with the Mac on Wi-Fi, about 21 fps on Ethernet (2026-10-14). Cause unknown | Effect smoothness under Art-Net control | Note in Phase 12; the receiver interpolates or renders locally anyway |
| A sleeping Mac stops all Art-Net (frames and ArtPoll). Run 1 had a 160 s outage (owner confirmed sleep) | Total loss of SoundSwitch control | Live: Mac awake (Lock Screen → display off Never; prevent sleep). Phase 13 falls back to audio on timeout |
| Strip power, fusing, and level shifting are undocumented | Safety and reliability | Inspect before calling the build product-ready |
| Post-antenna-fix latency was never measured | No baseline for Phase 11 | Measure in Phase 11 |
| Receiver app uses 71% of its 1.25 MB partition (`esp32dev` default 4 MB layout) | Room for growth is limited | Consider a larger app partition before Phase 12 features |

## Resolved

- Wrong-channel lock: fixed 2026-10-12 by `senderChannel` in the packet (receiver locked to 6 on every boot; it had locked 4, then 5 or 6 by survey)
- Serial `pps=0` / API pps spikes: fixed 2026-10-12. `update_packet_rate()` runs once a second from `loop()`; serial and API both read about 28
- Stale `v20.1` / `V22` banners: now `Lumina v3 sender` / `Lumina v3 receiver` (2026-10-12)
- Boot Wi-Fi drop (blank SSID, 0 dBm, rejoin about 8 s later): fixed 2026-10-12. `esp_wifi_set_bandwidth(HT20)` after the join made the station leave (`Reason 8 ASSOC_LEAVE`); it now runs before `WiFi.begin()`
- `volatile` `++` deprecation warnings: fixed 2026-10-12 (`x = x + 1`); both builds are warning-free

## Regression Traps (don't reintroduce)

- **Color order:** it's RGB. Don't default back to GRB during the migration.
- **White as red:** `if (st.saturation == 0) st.saturation = 255;` turned white into red. Saturation 0 is valid.
- **Strand order:** 33, 25, 26, 27. The old plan (25/26/27/33) is wrong.
- **Config saves:** don't go back to dozens of separate Preferences writes. The v20 error was `Configuration save FAILED: activeStrands=4 RGBscale=[128,255,126]`.
- **Arduino `.ino` auto-prototypes:** these broke the `AudioLedPacket` and `VuSource` declarations. Normal `.cpp`/`.h` fixes this; declare types before use.
- **Antenna:** a blocked antenna caused huge latency. Check RSSI before blaming the network stack.
- **LED failures:** debug in layers (electrical, then renderer, then config, then ESP-NOW). The fastest proof is a hardcoded NeoPixel test (it was GPIO25 with 10 LEDs).
