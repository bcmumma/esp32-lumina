#include "FxLayer.h"

#include <string.h>

namespace fx {

void reset(State &s) { memset(&s, 0, sizeof(s)); }

bool read(const uint8_t *dmx, uint16_t length, uint16_t address, Input &out) {
  if (!dmx || address == 0) return false;
  uint8_t v[kChannels];
  for (uint8_t k = 0; k < kChannels; k++) {
    const uint32_t i = (uint32_t)address - 1U + k;
    v[k] = i < length ? dmx[i] : 0;
  }
  out = {v[0], v[1], v[2], v[3], v[4]};
  return true;
}

static uint8_t absdiff(uint8_t a, uint8_t b) { return a > b ? (uint8_t)(a - b) : (uint8_t)(b - a); }

Output step(State &s, const Input &in, bool newFrame, const Settings &cfg, uint32_t nowMs) {
  Output o = {};
  if (nowMs == 0) nowMs = 1;
  if (newFrame) {
    if (s.primed) {
      if (absdiff(in.pan, s.last.pan) >= 2 || absdiff(in.tilt, s.last.tilt) >= 2) s.lastMoveMs = nowMs;
      if (absdiff(in.intensity, s.last.intensity) >= kBigJump) {
        s.jumpMs[0] = s.jumpMs[1];
        s.jumpMs[1] = nowMs;
      }
      if (in.intensity >= 128 && s.last.intensity < 128) {
        s.edges++;
        o.hit = true;
      }
      if (in.uv >= 128 && s.last.uv < 128) s.uvMs = nowMs;
    }
    s.last = in;
    s.primed = true;
  }

  o.moving = s.lastMoveMs != 0 && nowMs - s.lastMoveMs <= kMoveHoldMs;
  o.pulsing = s.jumpMs[0] != 0 && nowMs - s.jumpMs[0] <= kPulseWindowMs;

  // Beam: Tilt leads, Pan adds half; the small real sweep is stretched across the strip.
  const int32_t offset = ((int32_t)s.last.tilt - 127) + ((int32_t)s.last.pan - 127) / 2;
  const uint8_t spread = cfg.beamSpread < 1 ? 1 : (cfg.beamSpread > 8 ? 8 : cfg.beamSpread);
  int32_t pos = 128 + offset * spread;
  o.beamPos = (uint8_t)(pos < 0 ? 0 : (pos > 255 ? 255 : pos));
  o.beamLevel = o.moving ? (uint8_t)(((uint16_t)s.last.intensity * cfg.beamStrength + 127U) / 255U) : 0;

  if (s.uvMs != 0 && nowMs - s.uvMs < kSparkleMs) o.sparkle = (uint8_t)(255U - ((nowMs - s.uvMs) * 255U) / kSparkleMs);
  o.strobe = s.last.strobe;
  return o;
}

}  // namespace fx
