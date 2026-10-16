// Bass-hit (transient) detector for the PCM1808 fallback (Phase 14).
// Not a beat tracker: it fires when bass energy jumps well above its recent
// average, with a minimum gap between hits. No tempo, no prediction.
#pragma once

#include <stdint.h>

namespace hits {

struct Config {
  float fastTauMs;   // short envelope (follows each kick)
  float slowTauMs;   // long envelope (the running bass level)
  float ratio;       // fast must exceed slow * ratio to fire
  float rearmRatio;  // fast must fall below slow * rearmRatio before the next hit
  float floor;       // raw bass RMS below this never fires (silence / noise)
  uint32_t minGapMs; // at most one hit per gap
};

// Tuned on the bench (docs/DECISIONS.md): fires on kicks, ignores a steady bass tone.
constexpr Config kDefaultConfig = {12.0f, 300.0f, 1.6f, 1.2f, 0.0f, 150};

struct State {
  float fast;
  float slow;
  bool armed;
  bool primed;
  uint32_t lastHitMs;  // 0 = never
  uint32_t lastMs;
  uint32_t count;
};

void reset(State &s);

// Feed one block's raw bass RMS. Returns true when this block is a hit.
bool update(State &s, const Config &c, float bassRms, uint32_t nowMs);

}  // namespace hits
