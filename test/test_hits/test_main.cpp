// Bass-hit detector tests (pure logic, synthetic bass envelopes at the real block rate).
#include <Arduino.h>
#include <unity.h>
#include "HitDetector.h"

void setUp() {}
void tearDown() {}

static constexpr float kBlockMs = 256.0f * 1000.0f / 48000.0f;  // 5.33 ms per PCM1808 block

// Kick: jumps to `peak` and decays over about 80 ms, on top of `base`.
static float kick_env(float tSinceKickMs, float base, float peak) {
  if (tSinceKickMs < 0.0f || tSinceKickMs > 120.0f) return base;
  return base + (peak - base) * (1.0f - tSinceKickMs / 120.0f);
}

// Runs `seconds` of blocks; level(tMs) gives the raw bass RMS. Returns hits.
template <typename F>
static uint32_t run(F level, float seconds, hits::Config c = hits::kDefaultConfig) {
  hits::State s;
  hits::reset(s);
  float t = 0.0f;
  while (t < seconds * 1000.0f) {
    hits::update(s, c, level(t), 1000U + (uint32_t)t);
    t += kBlockMs;
  }
  return s.count;
}

static void test_steady_tone_never_fires() {
  TEST_ASSERT_EQUAL_UINT32(0, run([](float) { return 1000.0f; }, 5.0f));
}

static void test_house_kicks_128bpm() {
  const float period = 60000.0f / 128.0f;  // 469 ms
  // First kick at 500 ms, so the detector is primed on the base level.
  uint32_t n = run([&](float t) { return t < 500.0f ? 200.0f : kick_env(fmodf(t - 500.0f, period), 200.0f, 1500.0f); }, 5.0f);
  TEST_ASSERT_UINT32_WITHIN(1, 10, n);  // kicks at 0.5 s .. 4.72 s = 10
}

static void test_dnb_kicks_174bpm() {
  const float period = 60000.0f / 174.0f;  // 345 ms
  uint32_t n = run([&](float t) { return t < 500.0f ? 300.0f : kick_env(fmodf(t - 500.0f, period), 300.0f, 1800.0f); }, 5.0f);
  TEST_ASSERT_UINT32_WITHIN(1, 14, n);  // 4.5 s / 0.345 s = 13.04, so 14 kick onsets
}

static void test_below_floor_is_silent() {
  hits::Config c = hits::kDefaultConfig;
  c.floor = 400.0f;
  const float period = 469.0f;
  uint32_t n = run([&](float t) { return t < 500.0f ? 20.0f : kick_env(fmodf(t - 500.0f, period), 20.0f, 300.0f); }, 5.0f, c);
  TEST_ASSERT_EQUAL_UINT32(0, n);
}

static void test_min_gap_merges_close_kicks() {
  // Two kicks 80 ms apart count once (minimum gap 150 ms).
  uint32_t n = run([](float t) {
    if (t < 500.0f) return 200.0f;
    float a = kick_env(t - 500.0f, 200.0f, 1500.0f), b = kick_env(t - 580.0f, 200.0f, 1500.0f);
    return a > b ? a : b;
  }, 2.0f);
  TEST_ASSERT_EQUAL_UINT32(1, n);
}

static void test_slow_build_does_not_fire() {
  // A 2 s crescendo from 200 to 2000 is not a hit.
  TEST_ASSERT_EQUAL_UINT32(0, run([](float t) { return t < 500.0f ? 200.0f : (t > 2500.0f ? 2000.0f : 200.0f + 0.9f * (t - 500.0f)); }, 4.0f));
}

static void test_drop_fires_once() {
  // A sudden step up that holds (a drop) is one hit, then quiet.
  TEST_ASSERT_EQUAL_UINT32(1, run([](float t) { return t < 1000.0f ? 200.0f : 1500.0f; }, 4.0f));
}

void setup() {
  delay(2000);
  UNITY_BEGIN();
  RUN_TEST(test_steady_tone_never_fires);
  RUN_TEST(test_house_kicks_128bpm);
  RUN_TEST(test_dnb_kicks_174bpm);
  RUN_TEST(test_below_floor_is_silent);
  RUN_TEST(test_min_gap_merges_close_kicks);
  RUN_TEST(test_slow_build_does_not_fire);
  RUN_TEST(test_drop_fires_once);
  UNITY_END();
}

void loop() {}
