// Colour moods (DJ feedback, 2026-10-31). A mood is a hue range on the receiver's colour wheel
// (6 x 43 steps: red 0, yellow 43, green 86, cyan 128, blue 171, magenta 213), carried per strand in
// StrandSettings.moodFrom/moodTo. The receiver folds every look colour into the range, so any look
// keeps its shapes and motion but only shows the mood's colours. Pure logic, no Arduino.
#pragma once

#include <stdint.h>

#include "LuminaProtocol.h"

namespace mood {

// One full sweep of the drift (from one end of the range to the other and back) takes about 20 s.
constexpr uint32_t kDriftStepMs = 78;  // 256 steps x 78 ms = 20 s

inline bool active(const StrandSettings &st) { return st.moodFrom != st.moodTo; }

// Width of the range going up the wheel from moodFrom (wraps past red, e.g. 230 -> 24).
inline uint8_t span(const StrandSettings &st) { return (uint8_t)(st.moodTo - st.moodFrom); }

// Wheel offset that slowly turns all colours, from the S3's clock so both rooms stay in step.
inline uint8_t drift(uint32_t senderMs) { return (uint8_t)(senderMs / kDriftStepMs); }

// Triangle fold: hue 0 (+drift) lands on moodFrom, hue 128 on moodTo, and back again, so a turning
// hue sweeps smoothly between the two ends with no jump.
inline uint8_t fold(uint8_t from, uint8_t width, uint8_t hue, uint8_t driftValue) {
  const uint8_t u = (uint8_t)(hue + driftValue);
  const uint8_t t = (u & 0x80U) ? (uint8_t)(255U - ((u & 0x7FU) << 1U)) : (uint8_t)((u & 0x7FU) << 1U);
  return (uint8_t)(from + (uint8_t)(((uint16_t)t * width + 127U) / 255U));
}

inline uint8_t fold(const StrandSettings &st, uint8_t hue, uint8_t driftValue) {
  return fold(st.moodFrom, span(st), hue, driftValue);
}

// Under a mood, colours stay vivid: a pale strand colour (low saturation) still shows the mood.
constexpr uint8_t kMinSaturation = 200;

}  // namespace mood
