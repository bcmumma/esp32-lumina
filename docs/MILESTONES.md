# Milestones

| Milestone | Date | Evidence | Commit |
|---|---|---|---|
| v14: Lumina v1.0 prototype (audio → ESP-NOW → LEDs) | before 2026-10-11 | Owner-confirmed | not recorded |
| v20.1: config saving fixed (dual-slot CRC blob) | before 2026-10-11 | Owner-confirmed | not recorded |
| Phase 0: v2 preserved on GitHub | before 2026-10-11 | Owner-confirmed | inspect the repo |
| Phase 1: Mac prepared, Serato backed up | before 2026-10-11 | Owner-confirmed | n/a |
| Phase 2: Serato + DDJ-FLX4 verified | before 2026-10-11 | Owner-confirmed | n/a |
| Phase 3: SoundSwitch connected to Serato | before 2026-10-11 | Owner-confirmed (green in Performance Mode) | n/a |
| Phase 7: PlatformIO environment + test bench | 2026-10-12 | `pio run`: sender and receiver both SUCCESS (pioarduino 55.03.312-1, Arduino 3.3.12, IDF 5.5.5). Both boards upload and log (`psram=8192KB` on the S3, `ESP32-D0WD-V3 flash=4MB` on the WROOM). Dual-port capture: receiver levels matched the S3 during a tone sweep (about 29 pkt/s, about 3% loss). PCM1808 meter at -24.8 dBFS peak for a -12 dBFS tone, 0 clips. Webcam frames show strand 1 R/G/B/white, matching the serial log; owner confirmed by eye | `948fa05` (`develop-v23`) |
| Phase 8: v2 ported to PlatformIO, parity on hardware | 2026-10-12 | `pio run` sender and receiver SUCCESS. v2 builds unchanged as C++ on Arduino 3.3.12; only the secrets move plus the receiver channel survey (owner-approved). Boot clean; saved config `cfgA seq 7` loaded; Wi-Fi channel 6; web routes 200; save/load cfgB↔cfgA round trip; full-scale tone sweep identical on both boards, `clip` 0 over 49 polls; receiver locked 5 or 6 (never 2+ off) with 0–1.5% loss; strand 1 RGB/white on camera; owner checked the web UI. Strands 2–4 waived by owner | `35449ae` (`feature/platformio-migration`) |
