// FX layer tests: beam from Pan/Tilt, busy-section detection, UV sparkle, hit edges.
#include <Arduino.h>
#include <unity.h>
#include "FxLayer.h"

void setUp() {}
void tearDown() {}

static const fx::Settings kCfg = {255, 3};

static void test_read_channels() {
  uint8_t dmx[512] = {0};
  dmx[96] = 200; dmx[97] = 0; dmx[98] = 127; dmx[99] = 150; dmx[100] = 253;  // L11 + FX at 97
  fx::Input in;
  TEST_ASSERT_TRUE(fx::read(dmx, 512, 97, in));
  TEST_ASSERT_EQUAL_UINT8(200, in.intensity);
  TEST_ASSERT_EQUAL_UINT8(127, in.pan);
  TEST_ASSERT_EQUAL_UINT8(150, in.tilt);
  TEST_ASSERT_EQUAL_UINT8(253, in.uv);
  TEST_ASSERT_FALSE(fx::read(nullptr, 512, 97, in));
}

static void test_beam_only_while_moving() {
  fx::State s;
  fx::reset(s);
  fx::Output o = fx::step(s, {255, 0, 127, 127, 0}, true, kCfg, 1000);
  TEST_ASSERT_FALSE(o.moving);
  TEST_ASSERT_EQUAL_UINT8(0, o.beamLevel);
  o = fx::step(s, {255, 0, 127, 140, 0}, true, kCfg, 1030);  // Tilt moves
  TEST_ASSERT_TRUE(o.moving);
  TEST_ASSERT_EQUAL_UINT8(255, o.beamLevel);
  TEST_ASSERT_EQUAL_UINT8(128 + 13 * 3, o.beamPos);         // stretched by the spread
  o = fx::step(s, {255, 0, 127, 140, 0}, true, kCfg, 1030 + fx::kMoveHoldMs + 1);  // still for too long
  TEST_ASSERT_FALSE(o.moving);
  TEST_ASSERT_EQUAL_UINT8(0, o.beamLevel);
}

static void test_beam_position_follows_sweep() {
  fx::State s;
  fx::reset(s);
  fx::step(s, {255, 0, 127, 127, 0}, true, kCfg, 10);
  fx::Output lo = fx::step(s, {255, 0, 127, 99, 0}, true, kCfg, 40);   // Tilt low end of the real sweep
  fx::Output hi = fx::step(s, {255, 0, 127, 156, 0}, true, kCfg, 70);  // high end
  TEST_ASSERT_TRUE(lo.beamPos < 64);
  TEST_ASSERT_TRUE(hi.beamPos > 192);
  fx::Output half = fx::step(s, {128, 0, 127, 157, 0}, true, kCfg, 100);
  TEST_ASSERT_UINT8_WITHIN(1, 128, half.beamLevel);  // beam brightness follows Intensity
}

static void test_pulsing_means_busy_section() {
  fx::State s;
  fx::reset(s);
  fx::step(s, {255, 0, 127, 127, 0}, true, kCfg, 100);
  fx::Output o = fx::step(s, {255, 0, 127, 127, 0}, true, kCfg, 130);
  TEST_ASSERT_FALSE(o.pulsing);  // held full = not busy
  fx::step(s, {0, 0, 127, 127, 0}, true, kCfg, 300);
  o = fx::step(s, {255, 0, 127, 127, 0}, true, kCfg, 470);  // two big jumps within 1 s
  TEST_ASSERT_TRUE(o.pulsing);
  TEST_ASSERT_TRUE(o.hit);  // rising edge through 128
  o = fx::step(s, {255, 0, 127, 127, 0}, false, kCfg, 470 + 1100);
  TEST_ASSERT_FALSE(o.pulsing);  // pulses stopped
}

static void test_hit_edges_counted_once_per_frame() {
  fx::State s;
  fx::reset(s);
  fx::step(s, {0, 0, 127, 127, 0}, true, kCfg, 10);
  TEST_ASSERT_TRUE(fx::step(s, {200, 0, 127, 127, 0}, true, kCfg, 40).hit);
  TEST_ASSERT_FALSE(fx::step(s, {200, 0, 127, 127, 0}, false, kCfg, 45).hit);  // same frame again
  TEST_ASSERT_FALSE(fx::step(s, {210, 0, 127, 127, 0}, true, kCfg, 70).hit);   // still high
  TEST_ASSERT_EQUAL_UINT32(1, s.edges);
}

static void test_uv_burst_sparkle_fades() {
  fx::State s;
  fx::reset(s);
  fx::step(s, {255, 0, 127, 127, 0}, true, kCfg, 1000);
  fx::Output o = fx::step(s, {255, 0, 127, 127, 253}, true, kCfg, 1030);
  TEST_ASSERT_EQUAL_UINT8(255, o.sparkle);
  o = fx::step(s, {255, 0, 127, 127, 253}, false, kCfg, 1030 + fx::kSparkleMs / 2);
  TEST_ASSERT_UINT8_WITHIN(3, 128, o.sparkle);
  o = fx::step(s, {255, 0, 127, 127, 0}, true, kCfg, 1030 + fx::kSparkleMs);
  TEST_ASSERT_EQUAL_UINT8(0, o.sparkle);
}

void setup() {
  delay(2000);
  UNITY_BEGIN();
  RUN_TEST(test_read_channels);
  RUN_TEST(test_beam_only_while_moving);
  RUN_TEST(test_beam_position_follows_sweep);
  RUN_TEST(test_pulsing_means_busy_section);
  RUN_TEST(test_hit_edges_counted_once_per_frame);
  RUN_TEST(test_uv_burst_sparkle_fades);
  UNITY_END();
}

void loop() {}
