# Hardware

**Don't change the proven pin maps during the PlatformIO port or the refactor.** Date every measurement you add.

## Sender: ESP32-S3 + PCM1808

| PCM1808 signal | ESP32-S3 GPIO |
|---|---|
| SCKI / MCLK | 20 |
| BCK | 21 |
| LRCK | 47 |
| DOUT | 48 |

- A common ground is required.
- MD0, MD1, and FMT are tied to ground.
- The PCM1808 needs MCLK, which is why the S3 handles audio (the classic ESP32 was awkward).
- The audio source is the DDJ-FLX4's RCA line out.
- The S3 is powered and programmed over USB.
- **Bench audio source (2026-10-12):** PC → Schiit Modi 3E (USB DAC) → Schiit Magni (amp) → the Magni's output → PCM1808 input. There are no speakers in the chain. The Magni is an amplifier, so its output can exceed the PCM1808's line-level input range. Keep its volume at a fixed position.
  - **Measured 2026-10-12, Magni knob about at the middle, Windows volume unchanged.** Phase 7 meter (`src/sender/main.cpp`), dBFS of the ADC's true 32-bit full scale:
    - a tone sweep at -12 dBFS digital (25%) read **-24.8 dBFS peak** and -28 dBFS RMS on both channels, at 100 Hz, 1 kHz, and 6 kHz alike;
    - 0 clips;
    - silence read -55 dBFS peak and -67 dBFS RMS.
  - So a full-scale 0 dBFS track would peak around -13 dBFS at the ADC: about 13 dB of headroom and no clipping. This holds only if the knob and the Windows volume stay where they are.
- **Unknown:** the exact S3 dev board model and the onboard RGB LED pin. If the onboard RGB is on GPIO48, it conflicts with PCM1808 DOUT, so the status LED must stay disabled.

## Receiver: ESP32-WROOM + Four Strands

| Strand | GPIO |
|---|---|
| 1 | 33 |
| 2 | 25 |
| 3 | 26 |
| 4 | 27 |

- Soldered and tested on a proto board. Data path: GPIO → level converter → 330 Ω → strip DIN.
- **2026-10-12 incident:** 12 V touched wires at the level converter. Afterwards:
  - The WROOM boots and logs normally.
  - GPIO 33/25/26/27 all drive high and low correctly. Their internal pull-downs read 1, which fits the pull-up resistors on a BSS138-style converter's channels (UNVERIFIED, depends on the converter type).
  - Strand 1 (first 10 LEDs, level 24/255) showed red, green, blue, and white correctly (owner-observed).
  - The converter channels for strands 2–4 haven't been tested with a strip yet.
- This is the **physical** order. The sender defaults and the receiver output must both match it.
- **Color order is RGB**, not GRB. Data rate is 800 kHz.

## Strips and Power

- The strips are 12 V addressable. WS2815 has been confirmed working. I've also mentioned having "the 11s," probably 12 V WS2811-type strips; still to be confirmed.
- They run from a separate 12 V LED supply. **That supply's ground must be tied to the ESP32 ground**, or the data signal has no reference.
- **Owner, 2026-10-14:**
  - 12 V supply rated **90 W (7.5 A)**; plan for about 80% continuous, about 6 A.
  - Level converter is a 3.3 V → 5 V type (rail measurements and part number still to come).
  - Strands 2–4 are being mounted; likely the **same effect on all four strands**.
- **Brightness budget (estimate):** 600 LEDs (4 × 150) at full white. **UNVERIFIED:** about 15 mA per WS2815 pixel at full white (from memory). If so, about 9 A, above the about 6 A budget, so cap global brightness at about 60–65%. Confirm with the strip datasheet or one measured current before Phase 12 drives LEDs from Art-Net.
- **Unknown, to inspect:**
  - strip type and LED count per strand;
  - PSU voltage and current rating;
  - fusing;
  - power-injection layout;
  - the level converter's part number. The owner confirmed one is fitted (2026-10-12), type not yet known: a 74AHCT125/74HCT245 buffer, or a BSS138-style bidirectional module;
  - 330 Ω data series resistors are fitted (owner, 2026-10-12);
  - bulk capacitors at the strips.
- **Recommendations for a permanent or product build:**
  - common ground;
  - a series resistor on each data line;
  - a level shifter;
  - power injection sized to the LED count;
  - a fuse per feed;
  - bulk capacitance at each strip.

## RF

- Keep the ESP32 antennas clear of proto boards, wiring, and enclosures.
- A blocked antenna once caused 3–347 ms ping swings and 0.24–0.46 s API calls. Unblocking it fixed the problem. Updated numbers haven't been recorded.

## Self-Check Test Bench (Phase 7)

| Check | How | Verifies |
|---|---|---|
| Boot, version, heap, errors | Structured serial lines from both boards on their COM ports | Firmware runs, no resets or panics |
| Self-test | Serial `selftest` command and an S3 diagnostics endpoint (JSON) | Config CRC, Wi-Fi, ESP-NOW, PCM1808 state |
| ESP-NOW | Capture both boards at once; match sequence numbers sent and received | Delivery and loss rate |
| Audio | PC line out → PCM1808 input (3.5 mm-to-RCA cable) playing known tones, e.g. 60 Hz, 1 kHz, 8 kHz, silence, left only, right only | Bass, mid, treble, left/right, noise floor |
| LEDs | USB webcam aimed at the strips; `pattern` command (e.g. strand 1 red, 2 green, 3 blue, 4 white, then chases); Claude inspects captured frames | Strand order, RGB vs GRB, white not red, all strands alive |
| Persistence | Change a setting over HTTP, reset the board over serial, read it back | The save/load config blob |

- **Webcam:** lock exposure and white balance if possible, run the strips at low brightness, and judge order and hue rather than exact color.
- **Audio:** the PC's line out replaces the FLX4 only during tests. Watch for hum from a ground loop.
- **What still needs me:** whether effects look good, heat and power, physical wiring, and SoundSwitch.

## Boards (to confirm in Phase 7)

The serial ports go in `CLAUDE.local.md`.

### S3 sender (read with esptool `flash_id`, 2026-10-12)

- Chip: ESP32-S3 (QFN56), revision v0.2, 40 MHz crystal.
- Flash: 16 MB, quad (4 data lines), 3.3 V.
- PSRAM: 8 MB embedded (AP_3v3).
- USB-serial: WCH CH343 (VID:PID `1A86:55D3`). Windows already has its driver.
- Onboard RGB: GPIO48, per the v20.1 boot log. This confirms the conflict with PCM1808 DOUT, so the status LED stays disabled.
- Firmware on the board: the boot banner says `Lumina v20.1`, but the committed v22 sender prints that same stale banner (`lumina_sender_v2.ino` line 3507), so the board is probably running v22. `AudioLedPacket` size is 107, and the dual-slot CRC config loads slot `cfgA`.
- Module: **ESP32-S3-WROOM-1 N16R8** (owner read the shield label, 2026-10-12). Espressif's ESP32-S3-WROOM-1 datasheet ordering table lists the N16R8 as 16 MB quad flash with 8 MB **octal** PSRAM. PlatformIO therefore needs `qio_opi` memory (flash QIO, PSRAM OPI). With octal PSRAM, GPIO35–37 are used internally and aren't free.
- **UNVERIFIED:** the dev board carrying the module. The PlatformIO board ID gets chosen in the S3 test project.

### WROOM receiver (read with esptool `flash_id`, 2026-10-12)

- Chip: ESP32-D0WD-V3 (revision v3.1), dual core, 240 MHz, 40 MHz crystal.
- Flash: 4 MB. No PSRAM reported.
- USB-serial: Silicon Labs CP210x (VID:PID `10C4:EA60`). Windows already has its driver.
- **UNVERIFIED:** the dev board model. A D0WD-V3 with 4 MB flash and a CP210x matches the common ESP32-DevKitC / ESP32-WROOM-32E boards (PlatformIO `esp32dev`). Check the module label.
- Firmware on the board: `Lumina LED Receiver V22`, which matches the committed `lumina_receiver_v2.ino`. It expects a 107-byte packet. It scans channels 1–11, then locks to the sender (it locked on channel 6).
- Saved config: 4 strands, pins 33/25/26/27, 150 LEDs each, `WS2815/WS2812 RGB 800k`, `rgbScale=[128,255,126]`.
- Baseline (15 s capture with the S3 sending): `fps=41` and `show=23566us`. Writing 4 × 150 LEDs one strand after another takes about 23.6 ms, which caps the frame rate near 42 fps. Receive loss was 8 out of 290 (about 3%), with `bad=0`.
