// Strobe sensitivity (DJ feedback, 2026-10-31): with Strobe on, flashes run only in the loud parts
// of the music, so the DJ can leave Strobe on through quiet sections. Compares how loud the music
// is now with how loud it has been recently: the loud reference rises quickly and forgets slowly,
// so a breakdown reads as quiet and the drop after it as loud. Pure logic, no Arduino.
#pragma once

#include <stdint.h>

namespace strobeauto {

// Sensitivity 255 = Always: no gating, flashes whenever Strobe is on (the behaviour before this).
constexpr uint8_t kAlways = 255;

struct Config {
  float fastTauMs;     // loudness now: follows sections, not single kicks
  float riseTauMs;     // the loud reference catches up with louder music
  float fallTauMs;     // ...and forgets slowly (a 30-45 s breakdown stays "quiet")
  float hysteresisDb;  // must fall this far below the threshold before it starts to close
  uint32_t holdMs;     // and stay there this long (a fill in a loud part doesn't close it)
  float floor;         // RMS at or below this is silence: closed, and the reference holds
};

constexpr Config kDefaultConfig = {400.0f, 1500.0f, 120000.0f, 2.0f, 1500, 0.0f};

struct State {
  float fastPow;          // smoothed power (RMS squared)
  float refPow;           // loud reference power
  bool primed;
  bool open;
  uint32_t lastMs;
  uint32_t belowSinceMs;  // when the level went below the closing level (0 = not below)
  float levelDb;          // now vs the loud reference (0 dB = as loud), for the page
};

void reset(State &s);

// How far below the loud reference the music may be and still flash, in dB:
// sensitivity 0 = -1 dB (only the loudest parts) ... 254 = -12 dB (almost everything).
float threshold_db(uint8_t sensitivity);

// Feed one audio block's RMS level. Returns true while flashes may run.
bool update(State &s, const Config &c, float rms, uint8_t sensitivity, uint32_t nowMs);

}  // namespace strobeauto
