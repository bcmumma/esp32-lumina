// Room receiver tests: which strand slots each room drives.
#include <Arduino.h>
#include <unity.h>
#include "RoomMap.h"

void setUp() {}
void tearDown() {}

static AudioLedPacket packet(uint8_t active) {
  AudioLedPacket p = {};
  p.activeStrands = active;
  const uint8_t pins[4] = {33, 25, 26, 27};
  for (uint8_t s = 0; s < 4; s++) {
    p.strand[s].enabled = 1;
    p.strand[s].pin = pins[s];
    p.strand[s].ledCount = (uint16_t)(100 * (s + 1));  // 100, 200, 300, 400: tells the slots apart
    p.strand[s].mode = (uint8_t)(10 + s);
  }
  return p;
}

static void test_room1_keeps_slots_1_and_2() {
  AudioLedPacket p = packet(4);
  room::view(p, 1);
  TEST_ASSERT_EQUAL_UINT8(2, p.activeStrands);
  TEST_ASSERT_EQUAL_UINT16(100, p.strand[0].ledCount);
  TEST_ASSERT_EQUAL_UINT16(200, p.strand[1].ledCount);
  TEST_ASSERT_EQUAL_UINT8(33, p.strand[0].pin);
  TEST_ASSERT_EQUAL_UINT8(0, p.strand[2].enabled);
  TEST_ASSERT_EQUAL_UINT16(0, p.strand[3].ledCount);
}

static void test_room2_moves_slots_3_and_4_to_its_outputs_1_and_2() {
  AudioLedPacket p = packet(4);
  p.strand[2].pin = 33;  // room 2 wired like room 1
  p.strand[3].pin = 25;
  room::view(p, 2);
  TEST_ASSERT_EQUAL_UINT8(2, p.activeStrands);
  TEST_ASSERT_EQUAL_UINT16(300, p.strand[0].ledCount);
  TEST_ASSERT_EQUAL_UINT16(400, p.strand[1].ledCount);
  TEST_ASSERT_EQUAL_UINT8(12, p.strand[0].mode);
  TEST_ASSERT_EQUAL_UINT8(33, p.strand[0].pin);
  TEST_ASSERT_EQUAL_UINT8(25, p.strand[1].pin);
  TEST_ASSERT_EQUAL_UINT8(26, p.strand[2].pin);  // spare outputs stay off on the spare pins
  TEST_ASSERT_EQUAL_UINT8(0, p.strand[2].enabled);
}

static void test_partial_counts() {
  AudioLedPacket p = packet(3);  // room 1 has 2 strands, room 2 has 1
  room::view(p, 2);
  TEST_ASSERT_EQUAL_UINT8(1, p.activeStrands);
  TEST_ASSERT_EQUAL_UINT16(300, p.strand[0].ledCount);
  AudioLedPacket q = packet(1);
  room::view(q, 1);
  TEST_ASSERT_EQUAL_UINT8(1, q.activeStrands);
}

static void test_room_without_strands_is_dark() {
  AudioLedPacket p = packet(2);  // only room 1's strands are active
  room::view(p, 2);
  TEST_ASSERT_EQUAL_UINT8(1, p.activeStrands);
  TEST_ASSERT_EQUAL_UINT8(0, p.strand[0].enabled);
  TEST_ASSERT_EQUAL_UINT16(0, p.strand[0].ledCount);
}

void setup() {
  delay(2000);
  UNITY_BEGIN();
  RUN_TEST(test_room1_keeps_slots_1_and_2);
  RUN_TEST(test_room2_moves_slots_3_and_4_to_its_outputs_1_and_2);
  RUN_TEST(test_partial_counts);
  RUN_TEST(test_room_without_strands_is_dark);
  UNITY_END();
}

void loop() {}
