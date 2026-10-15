// LUMINA-16 mapping and strobe-safety tests (pure logic, no LEDs).
#include <Arduino.h>
#include <string.h>
#include <unity.h>
#include "Lumina16.h"

void setUp() {}
void tearDown() {}

// The receiver's hsv_to_rgb (src/receiver/lighting/Color.h), copied for round-trip checks.
static void hsv_to_rgb(uint8_t h, uint8_t s, uint8_t v, uint8_t &r, uint8_t &g, uint8_t &b) {
  if (s == 0) { r = g = b = v; return; }
  uint8_t region = h / 43;
  uint8_t remainder = (h - region * 43) * 6;
  uint8_t p = (v * (255 - s)) >> 8;
  uint8_t q = (v * (255 - ((s * remainder) >> 8))) >> 8;
  uint8_t t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;
  switch (region) {
    case 0: r = v; g = t; b = p; break;
    case 1: r = q; g = v; b = p; break;
    case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break;
    case 4: r = t; g = p; b = v; break;
    default: r = v; g = p; b = q; break;
  }
}

static void assert_round_trip(uint8_t r, uint8_t g, uint8_t b, uint8_t tol) {
  uint8_t h, s, v, r2, g2, b2;
  lumina16::rgb_to_hsv(r, g, b, h, s, v);
  hsv_to_rgb(h, s, v, r2, g2, b2);
  char msg[64];
  snprintf(msg, sizeof(msg), "in %u,%u,%u -> h%u s%u v%u -> %u,%u,%u", r, g, b, h, s, v, r2, g2, b2);
  TEST_ASSERT_INT_WITHIN_MESSAGE(tol, r, r2, msg);
  TEST_ASSERT_INT_WITHIN_MESSAGE(tol, g, g2, msg);
  TEST_ASSERT_INT_WITHIN_MESSAGE(tol, b, b2, msg);
}

static void test_primary_colours_round_trip() {
  assert_round_trip(255, 0, 0, 6);
  assert_round_trip(0, 255, 0, 6);
  assert_round_trip(0, 0, 255, 6);
  assert_round_trip(255, 255, 0, 6);
  assert_round_trip(0, 255, 255, 6);
  assert_round_trip(255, 0, 255, 6);
  assert_round_trip(230, 0, 255, 8);   // SoundSwitch purple seen in Phase 10
  assert_round_trip(0, 85, 255, 8);    // blue from the Serato show
  assert_round_trip(245, 83, 154, 10);
}

static void test_white_is_saturation_zero() {
  uint8_t h, s, v;
  lumina16::rgb_to_hsv(200, 200, 200, h, s, v);
  TEST_ASSERT_EQUAL_UINT8(0, s);  // saturation 0 is valid white (KNOWN_ISSUES trap)
  TEST_ASSERT_EQUAL_UINT8(200, v);
  lumina16::rgb_to_hsv(0, 0, 0, h, s, v);
  TEST_ASSERT_EQUAL_UINT8(0, v);
}

static void test_effect_bands() {
  TEST_ASSERT_EQUAL_UINT8(9, lumina16::effect_mode(0));     // Solid Color
  TEST_ASSERT_EQUAL_UINT8(9, lumina16::effect_mode(42));
  TEST_ASSERT_EQUAL_UINT8(17, lumina16::effect_mode(43));   // Neon Scanner
  TEST_ASSERT_EQUAL_UINT8(18, lumina16::effect_mode(86));   // Laser Sweep
  TEST_ASSERT_EQUAL_UINT8(19, lumina16::effect_mode(128));  // Rave Chase
  TEST_ASSERT_EQUAL_UINT8(21, lumina16::effect_mode(171));  // DnB Chopper
  TEST_ASSERT_EQUAL_UINT8(27, lumina16::effect_mode(214));  // White Lightning
  TEST_ASSERT_EQUAL_UINT8(27, lumina16::effect_mode(255));
}

static void test_direction_bands() {
  TEST_ASSERT_EQUAL_UINT8(0, lumina16::direction(0));
  TEST_ASSERT_EQUAL_UINT8(0, lumina16::direction(84));
  TEST_ASSERT_EQUAL_UINT8(1, lumina16::direction(85));
  TEST_ASSERT_EQUAL_UINT8(1, lumina16::direction(169));
  TEST_ASSERT_EQUAL_UINT8(2, lumina16::direction(170));
}

static void test_decode_at_address_and_bounds() {
  uint8_t dmx[512] = {};
  // Fixture patched at address 1: dimmer, effect, speed, intensity, density, strobe, dir, var, R, G, B
  const uint8_t ch[16] = {255, 100, 10, 20, 30, 40, 200, 50, 255, 0, 0, 1, 2, 3, 4, 5};
  memcpy(dmx, ch, sizeof(ch));
  lumina16::Params p;
  TEST_ASSERT_TRUE(lumina16::decode(dmx, 512, 1, p));
  TEST_ASSERT_EQUAL_UINT8(255, p.dimmer);
  TEST_ASSERT_EQUAL_UINT8(18, p.mode);
  TEST_ASSERT_EQUAL_UINT8(10, p.speed);
  TEST_ASSERT_EQUAL_UINT8(20, p.sensitivity);
  TEST_ASSERT_EQUAL_UINT8(30, p.density);
  TEST_ASSERT_EQUAL_UINT8(40, p.strobe);
  TEST_ASSERT_EQUAL_UINT8(2, p.direction);
  TEST_ASSERT_EQUAL_UINT8(50, p.variation);
  TEST_ASSERT_EQUAL_UINT8(0, p.hue);
  TEST_ASSERT_EQUAL_UINT8(255, p.saturation);
  TEST_ASSERT_EQUAL_UINT8(255, p.value);
  // Same fixture at address 497 (last that fits): channels 497-512
  memcpy(dmx + 496, ch, sizeof(ch));
  TEST_ASSERT_TRUE(lumina16::decode(dmx, 512, 497, p));
  TEST_ASSERT_FALSE(lumina16::decode(dmx, 512, 498, p));  // would run past 512
  TEST_ASSERT_FALSE(lumina16::decode(dmx, 12, 1, p));     // short frame
  TEST_ASSERT_FALSE(lumina16::decode(dmx, 512, 0, p));    // addresses are 1-based
}

static void test_brightness_cap() {
  lumina16::Params p = {};
  p.dimmer = 255;
  p.value = 255;
  TEST_ASSERT_EQUAL_UINT8(64, lumina16::brightness(p, 64));  // 25% cap holds at full
  p.dimmer = 128;
  TEST_ASSERT_EQUAL_UINT8(32, lumina16::brightness(p, 64));
  p.value = 0;
  TEST_ASSERT_EQUAL_UINT8(0, lumina16::brightness(p, 64));  // black RGB = dark
}

static void test_apply_keeps_hardware_fields() {
  AudioLedPacket pkt = {};
  for (uint8_t s = 0; s < MAX_STRANDS; s++) {
    pkt.strand[s].pin = 33 + s;
    pkt.strand[s].ledCount = 150;
    pkt.strand[s].enabled = 1;
    pkt.strand[s].ledType = 0;
  }
  pkt.globalHue = 77;
  lumina16::Params p = {};
  p.dimmer = 255; p.value = 255; p.mode = 17; p.hue = 170; p.saturation = 255; p.speed = 9; p.direction = 1;
  lumina16::apply(pkt, p, 64);
  TEST_ASSERT_EQUAL_UINT8(0, pkt.globalHue);
  for (uint8_t s = 0; s < MAX_STRANDS; s++) {
    TEST_ASSERT_EQUAL_UINT8(17, pkt.strand[s].mode);
    TEST_ASSERT_EQUAL_UINT8(64, pkt.strand[s].brightness);
    TEST_ASSERT_EQUAL_UINT8(170, pkt.strand[s].hue);
    TEST_ASSERT_EQUAL_UINT8(1, pkt.strand[s].direction);
    TEST_ASSERT_EQUAL_UINT8(33 + s, pkt.strand[s].pin);
    TEST_ASSERT_EQUAL_UINT16(150, pkt.strand[s].ledCount);
    TEST_ASSERT_EQUAL_UINT8(1, pkt.strand[s].enabled);
  }
}

static void test_strobe_flags() {
  uint8_t f = strobe_encode_flags(0x03, false, STROBE_DEFAULT_HZ);
  TEST_ASSERT_EQUAL_HEX8(0x03, f & 0x03);  // beat bits untouched
  TEST_ASSERT_FALSE(strobe_enabled(f));
  f = strobe_encode_flags(f, true, 3);
  TEST_ASSERT_TRUE(strobe_enabled(f));
  TEST_ASSERT_EQUAL_UINT8(3, strobe_max_hz(f));
  TEST_ASSERT_EQUAL_UINT32(333, strobe_min_interval_ms(f));
  TEST_ASSERT_EQUAL_UINT8(8, strobe_max_hz(strobe_encode_flags(0, true, 50)));  // hard ceiling
  TEST_ASSERT_EQUAL_UINT8(1, strobe_max_hz(strobe_encode_flags(0, true, 0)));
  TEST_ASSERT_FALSE(strobe_enabled(0x00));  // old senders: bits clear = disabled (safe)
}

static void test_strobe_allow_flash() {
  uint8_t on3 = strobe_encode_flags(0, true, 3);
  TEST_ASSERT_TRUE(strobe_allow_flash(on3, 1000, 0));     // first flash
  TEST_ASSERT_FALSE(strobe_allow_flash(on3, 1300, 1000)); // 300 ms < 333 ms
  TEST_ASSERT_TRUE(strobe_allow_flash(on3, 1334, 1000));
  uint8_t off = strobe_encode_flags(0, false, 3);
  TEST_ASSERT_FALSE(strobe_allow_flash(off, 5000, 0));    // disabled: never
  uint8_t on8 = strobe_encode_flags(0, true, 8);
  TEST_ASSERT_FALSE(strobe_allow_flash(on8, 1100, 1000)); // 100 ms < 125 ms
  TEST_ASSERT_TRUE(strobe_allow_flash(on8, 1125, 1000));
}

static void test_strobe_gate_is_rig_wide() {
  StrobeGate g = {0, 0};
  uint8_t on3 = strobe_encode_flags(0, true, 3);
  // Four strands triggering in the same render frame are one event.
  TEST_ASSERT_TRUE(strobe_gate_allow(g, on3, 1000));
  TEST_ASSERT_TRUE(strobe_gate_allow(g, on3, 1001));
  TEST_ASSERT_TRUE(strobe_gate_allow(g, on3, 1005));
  TEST_ASSERT_TRUE(strobe_gate_allow(g, on3, 1019));
  TEST_ASSERT_EQUAL_UINT32(1, g.events);
  // Out-of-step strands can't add events inside the cap interval.
  TEST_ASSERT_FALSE(strobe_gate_allow(g, on3, 1100));
  TEST_ASSERT_FALSE(strobe_gate_allow(g, on3, 1250));
  TEST_ASSERT_TRUE(strobe_gate_allow(g, on3, 1334));
  TEST_ASSERT_EQUAL_UINT32(2, g.events);
  // Simulate 4 strands each trying every 25 ms, out of phase, for 10 s: events <= 3/s.
  StrobeGate h = {0, 0};
  for (uint32_t t = 1; t < 10000; t++) {
    if (t % 25 == 0 || t % 25 == 6 || t % 25 == 12 || t % 25 == 18) strobe_gate_allow(h, on3, t);
  }
  TEST_ASSERT_LESS_OR_EQUAL_UINT32(30, h.events);
  StrobeGate off = {0, 0};
  TEST_ASSERT_FALSE(strobe_gate_allow(off, strobe_encode_flags(0, false, 8), 1000));
  TEST_ASSERT_EQUAL_UINT32(0, off.events);
}

void setup() {
  delay(2000);
  UNITY_BEGIN();
  RUN_TEST(test_primary_colours_round_trip);
  RUN_TEST(test_white_is_saturation_zero);
  RUN_TEST(test_effect_bands);
  RUN_TEST(test_direction_bands);
  RUN_TEST(test_decode_at_address_and_bounds);
  RUN_TEST(test_brightness_cap);
  RUN_TEST(test_apply_keeps_hardware_fields);
  RUN_TEST(test_strobe_flags);
  RUN_TEST(test_strobe_allow_flash);
  RUN_TEST(test_strobe_gate_is_rig_wide);
  UNITY_END();
}

void loop() {}
