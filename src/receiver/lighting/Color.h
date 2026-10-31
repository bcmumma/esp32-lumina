// 8-bit color math shared by LED output and effects (FastLED-style helpers).
#pragma once

#include <Arduino.h>

struct Rgb {
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

static inline uint8_t qadd8(uint8_t a, uint8_t b) {
  uint16_t s = (uint16_t)a + (uint16_t)b;
  return s > 255 ? 255 : (uint8_t)s;
}

static inline uint8_t scale8(uint8_t v, uint8_t scale) {
  return ((uint16_t)v * (uint16_t)scale) >> 8;
}

static inline uint8_t scale8_video(uint8_t v, uint8_t scale) {
  uint16_t out = ((uint16_t)v * (uint16_t)scale) >> 8;
  if (v && scale) out++;
  return out > 255 ? 255 : (uint8_t)out;
}

static inline Rgb hsv_to_rgb(uint8_t h, uint8_t s, uint8_t v) {
  if (s == 0) return {v, v, v};

  uint8_t region = h / 43;
  uint8_t remainder = (h - region * 43) * 6;

  uint8_t p = (v * (255 - s)) >> 8;
  uint8_t q = (v * (255 - ((s * remainder) >> 8))) >> 8;
  uint8_t t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;

  switch (region) {
    case 0: return {v, t, p};
    case 1: return {q, v, p};
    case 2: return {p, v, t};
    case 3: return {p, q, v};
    case 4: return {t, p, v};
    default: return {v, p, q};
  }
}

// Inverse of hsv_to_rgb on the same 6 x 43 wheel (red 0, green 85, blue 171).
static inline void rgb_to_hsv8(Rgb c, uint8_t &h, uint8_t &s, uint8_t &v) {
  const uint8_t mx = max(c.r, max(c.g, c.b));
  const uint8_t mn = min(c.r, min(c.g, c.b));
  v = mx;
  const uint8_t d = mx - mn;
  if (mx == 0 || d == 0) { h = 0; s = 0; return; }
  s = (uint8_t)(((uint16_t)d * 255U) / mx);
  int16_t hh;
  if (mx == c.r) hh = (int16_t)(43 * ((int16_t)c.g - c.b) / d);
  else if (mx == c.g) hh = (int16_t)(85 + 43 * ((int16_t)c.b - c.r) / d);
  else hh = (int16_t)(171 + 43 * ((int16_t)c.r - c.g) / d);
  h = (uint8_t)(hh < 0 ? hh + 256 : hh);
}

static inline Rgb scale_color(Rgb c, uint8_t scale) {
  c.r = scale8_video(c.r, scale);
  c.g = scale8_video(c.g, scale);
  c.b = scale8_video(c.b, scale);
  return c;
}

static inline Rgb add_color(Rgb a, Rgb b) {
  return {qadd8(a.r, b.r), qadd8(a.g, b.g), qadd8(a.b, b.b)};
}
