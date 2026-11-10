// Wire-format regression tests: the receiver depends on these exact offsets.
#include <Arduino.h>
#include <stddef.h>
#include <unity.h>
#include "LuminaProtocol.h"

void setUp() {}
void tearDown() {}

static void test_packet_size_and_identity() {
  TEST_ASSERT_EQUAL_UINT(112, sizeof(AudioLedPacket));
  TEST_ASSERT_EQUAL_UINT(18, sizeof(StrandSettings));
  TEST_ASSERT_EQUAL_HEX16(0xA17D, PACKET_MAGIC);
  TEST_ASSERT_EQUAL_UINT8(13, PACKET_VERSION);
  TEST_ASSERT_EQUAL_UINT8(12, PACKET_VERSION_V12);
  TEST_ASSERT_EQUAL_UINT8(107, PACKET_SIZE_V12);
  TEST_ASSERT_EQUAL_UINT8(4, MAX_STRANDS);
}

static void test_packet_field_offsets() {
  TEST_ASSERT_EQUAL_UINT(0, offsetof(AudioLedPacket, magic));
  TEST_ASSERT_EQUAL_UINT(2, offsetof(AudioLedPacket, version));
  TEST_ASSERT_EQUAL_UINT(3, offsetof(AudioLedPacket, size));
  TEST_ASSERT_EQUAL_UINT(4, offsetof(AudioLedPacket, sequence));
  TEST_ASSERT_EQUAL_UINT(6, offsetof(AudioLedPacket, ms));
  TEST_ASSERT_EQUAL_UINT(10, offsetof(AudioLedPacket, volume));
  TEST_ASSERT_EQUAL_UINT(23, offsetof(AudioLedPacket, maxLedsPerStrand));
  TEST_ASSERT_EQUAL_UINT(25, offsetof(AudioLedPacket, colorMap));
  TEST_ASSERT_EQUAL_UINT(32, offsetof(AudioLedPacket, beatBpm));
  TEST_ASSERT_EQUAL_UINT(33, offsetof(AudioLedPacket, strand));
  TEST_ASSERT_EQUAL_UINT(105, offsetof(AudioLedPacket, senderChannel));
  TEST_ASSERT_EQUAL_UINT(106, offsetof(AudioLedPacket, kickFlash));
  TEST_ASSERT_EQUAL_UINT(107, offsetof(AudioLedPacket, fullStrobe));  // v13 fields follow the v12 bytes
  TEST_ASSERT_EQUAL_UINT(108, offsetof(AudioLedPacket, strobeLevel));
  TEST_ASSERT_EQUAL_UINT(109, offsetof(AudioLedPacket, eq));
}

static void test_v12_defaults_and_eq() {
  AudioLedPacket p = {};
  packet_v12_defaults(p);
  TEST_ASSERT_EQUAL_UINT8(0, p.fullStrobe);
  TEST_ASSERT_EQUAL_UINT8(255, p.strobeLevel);
  TEST_ASSERT_EQUAL_UINT8(EQ_FLAT, p.eq[0]);
  TEST_ASSERT_EQUAL_UINT8(EQ_FLAT, p.eq[2]);
  TEST_ASSERT_EQUAL_UINT8(200, eq_apply(200, EQ_FLAT));  // flat leaves the band alone
  TEST_ASSERT_EQUAL_UINT8(0, eq_apply(200, 0));          // band off
  TEST_ASSERT_EQUAL_UINT8(100, eq_apply(200, 64));       // half
  TEST_ASSERT_EQUAL_UINT8(255, eq_apply(200, 255));      // boost clips at full
}

static void test_strand_field_offsets() {
  TEST_ASSERT_EQUAL_UINT(0, offsetof(StrandSettings, enabled));
  TEST_ASSERT_EQUAL_UINT(4, offsetof(StrandSettings, saturation));
  TEST_ASSERT_EQUAL_UINT(11, offsetof(StrandSettings, pin));
  TEST_ASSERT_EQUAL_UINT(13, offsetof(StrandSettings, strobeColor));
  TEST_ASSERT_EQUAL_UINT(14, offsetof(StrandSettings, ledCount));
  TEST_ASSERT_EQUAL_UINT(16, offsetof(StrandSettings, moodFrom));
  TEST_ASSERT_EQUAL_UINT(17, offsetof(StrandSettings, moodTo));
}

// Strobe colour codes: 0 is white (what older senders and saved configs hold), else hue + 1.
static void test_strobe_color_codes() {
  TEST_ASSERT_TRUE(strobe_color_is_white(0));
  TEST_ASSERT_FALSE(strobe_color_is_white(strobe_color_code(0)));
  TEST_ASSERT_EQUAL_UINT8(0, strobe_color_hue(strobe_color_code(0)));
  TEST_ASSERT_EQUAL_UINT8(171, strobe_color_hue(strobe_color_code(171)));
  TEST_ASSERT_EQUAL_UINT8(254, strobe_color_hue(strobe_color_code(255)));  // 255 can't be coded; nearest hue
}

void setup() {
  delay(2000);  // let the test runner open the serial port
  UNITY_BEGIN();
  RUN_TEST(test_packet_size_and_identity);
  RUN_TEST(test_packet_field_offsets);
  RUN_TEST(test_v12_defaults_and_eq);
  RUN_TEST(test_strand_field_offsets);
  RUN_TEST(test_strobe_color_codes);
  UNITY_END();
}

void loop() {}
