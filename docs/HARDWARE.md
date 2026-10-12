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
- **Unknown:** the exact S3 dev board model and the onboard RGB LED pin. If the onboard RGB is on GPIO48, it conflicts with PCM1808 DOUT, so the status LED must stay disabled.

## Receiver: ESP32-WROOM + Four Strands

| Strand | GPIO |
|---|---|
| 1 | 33 |
| 2 | 25 |
| 3 | 26 |
| 4 | 27 |

- Soldered and tested on a proto board.
- This is the **physical** order. The sender defaults and the receiver output must both match it.
- **Color order is RGB**, not GRB. Data rate is 800 kHz.

## Strips and Power

- The strips are 12 V addressable. WS2815 has been confirmed working. I've also mentioned having "the 11s," probably 12 V WS2811-type strips; still to be confirmed.
- They run from a separate 12 V LED supply. **That supply's ground must be tied to the ESP32 ground**, or the data signal has no reference.
- **Unknown, to inspect:**
  - strip type and LED count per strand;
  - PSU voltage and current rating;
  - fusing;
  - power-injection layout;
  - whether there's a level shifter (74AHCT125 or 74HCT245, 3.3 V → 5 V);
  - data series resistors;
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

The exact S3 and WROOM dev board models, their USB-serial chips, and their PlatformIO board IDs are all unknown. Record them here once known. The serial ports go in `CLAUDE.local.md`.
