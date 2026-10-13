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

static inline Rgb scale_color(Rgb c, uint8_t scale) {
  c.r = scale8_video(c.r, scale);
  c.g = scale8_video(c.g, scale);
  c.b = scale8_video(c.b, scale);
  return c;
}

static inline Rgb add_color(Rgb a, Rgb b) {
  return {qadd8(a.r, b.r), qadd8(a.g, b.g), qadd8(a.b, b.b)};
}
