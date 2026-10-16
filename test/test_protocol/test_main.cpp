// Wire-format regression tests: the receiver depends on these exact offsets.
#include <Arduino.h>
#include <stddef.h>
#include <unity.h>
#include "LuminaProtocol.h"

void setUp() {}
void tearDown() {}

static void test_packet_size_and_identity() {
  TEST_ASSERT_EQUAL_UINT(107, sizeof(AudioLedPacket));
  TEST_ASSERT_EQUAL_UINT(18, sizeof(StrandSettings));
  TEST_ASSERT_EQUAL_HEX16(0xA17D, PACKET_MAGIC);
  TEST_ASSERT_EQUAL_UINT8(12, PACKET_VERSION);
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
}

static void test_strand_field_offsets() {
  TEST_ASSERT_EQUAL_UINT(0, offsetof(StrandSettings, enabled));
  TEST_ASSERT_EQUAL_UINT(4, offsetof(StrandSettings, saturation));
  TEST_ASSERT_EQUAL_UINT(11, offsetof(StrandSettings, pin));
  TEST_ASSERT_EQUAL_UINT(14, offsetof(StrandSettings, ledCount));
}

void setup() {
  delay(2000);  // let the test runner open the serial port
  UNITY_BEGIN();
  RUN_TEST(test_packet_size_and_identity);
  RUN_TEST(test_packet_field_offsets);
  RUN_TEST(test_strand_field_offsets);
  UNITY_END();
}

void loop() {}
