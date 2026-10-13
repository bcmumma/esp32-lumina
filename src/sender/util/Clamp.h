// Small numeric helpers shared by the sender modules.
#pragma once

#include <stdint.h>

static inline float clamp_float(float v, float lo, float hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

static inline int clamp_int(int v, int lo, int hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

static inline uint8_t clamp_u8(float v) {
  if (v < 0.0f) return 0;
  if (v > 255.0f) return 255;
  return (uint8_t)(v + 0.5f);
}
