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

## Boards (to confirm in Phase 7)

The exact S3 and WROOM dev board models, their USB-serial chips, and their PlatformIO board IDs are all unknown. Record them here once known. The serial ports go in `CLAUDE.local.md`.
