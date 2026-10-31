#include "StrobeAuto.h"

#include <math.h>

namespace strobeauto {

void reset(State &s) { s = State{}; }

float threshold_db(uint8_t sensitivity) {
  const float x = sensitivity >= 254 ? 1.0f : (float)sensitivity / 254.0f;
  return -1.0f - 11.0f * x;
}

bool update(State &s, const Config &c, float rms, uint8_t sensitivity, uint32_t nowMs) {
  const float pow = rms * rms;
  if (!s.primed) {
    s.fastPow = pow;
    s.refPow = pow;
    s.lastMs = nowMs;
    s.primed = true;
  }
  const float dt = (float)(uint32_t)(nowMs - s.lastMs);
  s.lastMs = nowMs;
  s.fastPow += (pow - s.fastPow) * (1.0f - expf(-dt / c.fastTauMs));

  const bool silent = s.fastPow <= c.floor * c.floor || s.fastPow <= 0.0f;
  if (!silent) {  // in silence the reference holds, so a quiet intro after a pause still reads quiet
    const float tau = s.fastPow > s.refPow ? c.riseTauMs : c.fallTauMs;
    s.refPow += (s.fastPow - s.refPow) * (1.0f - expf(-dt / tau));
  }
  s.levelDb = (!silent && s.refPow > 0.0f) ? 10.0f * log10f(s.fastPow / s.refPow) : -99.0f;

  if (sensitivity == kAlways) {
    s.open = true;
    s.belowSinceMs = 0;
    return true;
  }
  const float thr = threshold_db(sensitivity);
  if (silent) {
    s.open = false;
    s.belowSinceMs = 0;
  } else if (s.levelDb >= thr) {
    s.open = true;
    s.belowSinceMs = 0;
  } else if (s.levelDb < thr - c.hysteresisDb) {
    if (s.belowSinceMs == 0) s.belowSinceMs = nowMs ? nowMs : 1;
    if (nowMs - s.belowSinceMs >= c.holdMs) s.open = false;
  } else {
    s.belowSinceMs = 0;  // between the two levels: keep the current state
  }
  return s.open;
}

}  // namespace strobeauto
