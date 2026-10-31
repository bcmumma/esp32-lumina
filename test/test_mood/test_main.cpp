// Colour moods (lib/Mood): every look colour lands inside the mood's hue range, the two ends are
// reachable, and a turning hue sweeps smoothly (no jump from one end to the other).
#include <Arduino.h>
#include <unity.h>
#include "Mood.h"

void setUp() {}
void tearDown() {}

static StrandSettings strand_with(uint8_t from, uint8_t to) {
  StrandSettings st = {};
  st.moodFrom = from;
  st.moodTo = to;
  return st;
}

static void test_active_only_with_a_range() {
  TEST_ASSERT_FALSE(mood::active(strand_with(0, 0)));  // zero-filled = no mood (older senders, saved configs)
  TEST_ASSERT_FALSE(mood::active(strand_with(171, 171)));
  TEST_ASSERT_TRUE(mood::active(strand_with(165, 200)));
}

static void test_ends_reachable() {
  const StrandSettings st = strand_with(165, 200);  // Blue-Purple
  TEST_ASSERT_EQUAL_UINT8(165, mood::fold(st, 0, 0));
  TEST_ASSERT_EQUAL_UINT8(200, mood::fold(st, 128, 0));
  TEST_ASSERT_UINT8_WITHIN(1, 183, mood::fold(st, 64, 0));
  TEST_ASSERT_EQUAL_UINT8(200, mood::fold(st, 0, 128));  // drift turns the wheel
}

static void test_range_wrapping_past_red() {
  const StrandSettings st = strand_with(220, 24);  // Sunset: pink through red to orange
  TEST_ASSERT_EQUAL_UINT8(60, mood::span(st));
  TEST_ASSERT_EQUAL_UINT8(220, mood::fold(st, 0, 0));
  TEST_ASSERT_EQUAL_UINT8(24, mood::fold(st, 128, 0));
}

static void test_every_colour_inside_the_range() {
  const uint8_t ranges[][2] = {{0, 24}, {165, 200}, {220, 24}, {40, 90}};
  for (const auto &r : ranges) {
    const StrandSettings st = strand_with(r[0], r[1]);
    for (int drift = 0; drift < 256; drift += 37) {
      for (int h = 0; h < 256; h++) {
        const uint8_t out = mood::fold(st, (uint8_t)h, (uint8_t)drift);
        TEST_ASSERT_TRUE((uint8_t)(out - r[0]) <= mood::span(st));
      }
    }
  }
}

static void test_turning_hue_never_jumps() {
  const StrandSettings st = strand_with(0, 60);
  uint8_t prev = mood::fold(st, 0, 0);
  for (int h = 1; h < 512; h++) {
    const uint8_t out = mood::fold(st, (uint8_t)h, 0);
    const int step = abs((int)out - (int)prev);
    TEST_ASSERT_TRUE(step <= 2);  // at most about 2 x 60 / 255 per hue step
    prev = out;
  }
}

static void test_drift_from_sender_clock() {
  TEST_ASSERT_EQUAL_UINT8(0, mood::drift(0));
  TEST_ASSERT_EQUAL_UINT8(1, mood::drift(mood::kDriftStepMs));
  TEST_ASSERT_EQUAL_UINT8(0, mood::drift(256 * mood::kDriftStepMs));  // one turn of the wheel = about 20 s
}

void setup() {
  delay(2000);
  UNITY_BEGIN();
  RUN_TEST(test_active_only_with_a_range);
  RUN_TEST(test_ends_reachable);
  RUN_TEST(test_range_wrapping_past_red);
  RUN_TEST(test_every_colour_inside_the_range);
  RUN_TEST(test_turning_hue_never_jumps);
  RUN_TEST(test_drift_from_sender_clock);
  UNITY_END();
}

void loop() {}
