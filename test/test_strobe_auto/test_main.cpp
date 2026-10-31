// Strobe sensitivity (lib/StrobeAuto): with Strobe on, flashes run in the loud parts of the music
// and pause in quiet ones; a drop after a breakdown opens it again quickly; Always never gates.
#include <Arduino.h>
#include <unity.h>
#include "StrobeAuto.h"

void setUp() {}
void tearDown() {}

static constexpr uint32_t kBlockMs = 32;  // about one audio block / packet
static constexpr float kLoud = 2000.0f;   // raw RMS of a loud section
static constexpr float kMinus10dB = 632.0f;
static constexpr float kMinus5dB = 1125.0f;

static strobeauto::Config config() {
  strobeauto::Config c = strobeauto::kDefaultConfig;
  c.floor = 50.0f;
  return c;
}

// Feeds `rms` for `ms` from `t`; returns the gate after the last block and advances t.
static bool feed(strobeauto::State &s, float rms, uint8_t sens, uint32_t &t, uint32_t ms) {
  bool open = false;
  const strobeauto::Config c = config();
  for (uint32_t end = t + ms; t < end; t += kBlockMs) open = strobeauto::update(s, c, rms, sens, t);
  return open;
}

// Milliseconds until the gate reads `want` (or `limit` if it never does).
static uint32_t time_until(strobeauto::State &s, float rms, uint8_t sens, uint32_t &t, bool want, uint32_t limit) {
  const strobeauto::Config c = config();
  for (uint32_t waited = 0; waited < limit; waited += kBlockMs, t += kBlockMs)
    if (strobeauto::update(s, c, rms, sens, t) == want) return waited;
  return limit;
}

static void test_threshold_mapping() {
  TEST_ASSERT_FLOAT_WITHIN(0.01f, -1.0f, strobeauto::threshold_db(0));
  TEST_ASSERT_FLOAT_WITHIN(0.05f, -6.5f, strobeauto::threshold_db(127));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, -12.0f, strobeauto::threshold_db(254));
}

static void test_always_never_gates() {
  strobeauto::State s = {};
  uint32_t t = 1000;
  TEST_ASSERT_TRUE(feed(s, 0.0f, strobeauto::kAlways, t, 2000));
  TEST_ASSERT_TRUE(feed(s, kLoud, strobeauto::kAlways, t, 2000));
}

static void test_steady_music_open() {
  strobeauto::State s = {};
  uint32_t t = 1000;
  TEST_ASSERT_TRUE(feed(s, kLoud, 128, t, 10000));
}

static void test_breakdown_closes_and_drop_opens() {
  strobeauto::State s = {};
  uint32_t t = 1000;
  TEST_ASSERT_TRUE(feed(s, kLoud, 128, t, 60000));
  const uint32_t closeMs = time_until(s, kMinus10dB, 128, t, false, 10000);
  TEST_ASSERT_TRUE(closeMs >= 1500 && closeMs <= 4000);  // the hold, plus the level falling
  TEST_ASSERT_FALSE(feed(s, kMinus10dB, 128, t, 28000));  // still quiet at the end of a 30 s breakdown
  const uint32_t openMs = time_until(s, kLoud, 128, t, true, 5000);
  TEST_ASSERT_TRUE(openMs <= 300);  // the drop
}

static void test_short_dip_keeps_it_open() {
  strobeauto::State s = {};
  uint32_t t = 1000;
  feed(s, kLoud, 128, t, 30000);
  TEST_ASSERT_TRUE(feed(s, kMinus10dB, 128, t, 1000));  // a one-second fill
  TEST_ASSERT_TRUE(feed(s, kLoud, 128, t, 2000));
}

static void test_silence_closes_and_reference_holds() {
  strobeauto::State s = {};
  uint32_t t = 1000;
  feed(s, kLoud, 128, t, 30000);
  TEST_ASSERT_FALSE(feed(s, 0.0f, 128, t, 4000));
  feed(s, 0.0f, 128, t, 60000);                         // a long pause between tracks
  TEST_ASSERT_FALSE(feed(s, kMinus10dB, 128, t, 5000));  // a quiet intro afterwards still reads quiet
}

static void test_low_sensitivity_needs_the_loudest_parts() {
  strobeauto::State s = {};
  uint32_t t = 1000;
  feed(s, kLoud, 0, t, 30000);
  TEST_ASSERT_FALSE(feed(s, kMinus5dB, 0, t, 6000));  // -5 dB is not loud enough at sensitivity 0 (-1 dB)
  TEST_ASSERT_TRUE(feed(s, kLoud, 0, t, 2000));
  strobeauto::State hi = {};
  uint32_t t2 = 1000;
  feed(hi, kLoud, 254, t2, 30000);
  TEST_ASSERT_TRUE(feed(hi, kMinus10dB, 254, t2, 6000));  // at 254 (-12 dB) a -10 dB part still flashes
}

void setup() {
  delay(2000);
  UNITY_BEGIN();
  RUN_TEST(test_threshold_mapping);
  RUN_TEST(test_always_never_gates);
  RUN_TEST(test_steady_music_open);
  RUN_TEST(test_breakdown_closes_and_drop_opens);
  RUN_TEST(test_short_dip_keeps_it_open);
  RUN_TEST(test_silence_closes_and_reference_holds);
  RUN_TEST(test_low_sensitivity_needs_the_loudest_parts);
  UNITY_END();
}

void loop() {}
