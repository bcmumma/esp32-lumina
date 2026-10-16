#include "HitDetector.h"

namespace hits {

void reset(State &s) { s = State{0.0f, 0.0f, true, false, 0, 0, 0}; }

static float ema_alpha(float dtMs, float tauMs) { return dtMs <= 0.0f ? 0.0f : dtMs / (tauMs + dtMs); }

bool update(State &s, const Config &c, float bassRms, uint32_t nowMs) {
  if (!s.primed) {
    s.fast = bassRms;
    s.slow = bassRms;
    s.lastMs = nowMs;
    s.primed = true;
    return false;
  }
  const float dt = (float)(nowMs - s.lastMs);
  s.lastMs = nowMs;
  s.fast += ema_alpha(dt, c.fastTauMs) * (bassRms - s.fast);
  // The long envelope is updated after the test so a hit is judged against the level before it.
  const float slow = s.slow;
  s.slow += ema_alpha(dt, c.slowTauMs) * (bassRms - s.slow);

  if (!s.armed) {
    if (s.fast < slow * c.rearmRatio) s.armed = true;
    return false;
  }
  if (s.fast <= c.floor || s.fast < slow * c.ratio) return false;
  if (s.lastHitMs != 0 && nowMs - s.lastHitMs < c.minGapMs) return false;

  s.armed = false;
  s.lastHitMs = nowMs == 0 ? 1 : nowMs;
  s.count++;
  return true;
}

}  // namespace hits
