// Cell-frame tests: packet layout, split/reassemble, loss behaviour, blending.
#include <Arduino.h>
#include <string.h>
#include <unity.h>
#include "CellFrame.h"

void setUp() {}
void tearDown() {}

static uint8_t src[MAX_CELLS * 3];
static void fill_src(uint8_t total) {
  for (int i = 0; i < total * 3; i++) src[i] = (uint8_t)(i * 7 + 3);
}

static void test_layout() {
  TEST_ASSERT_EQUAL_UINT(248, sizeof(CellPacket));
  TEST_ASSERT_EQUAL_UINT(98, cell_packet_bytes(30));
  TEST_ASSERT_TRUE(cell_packet_bytes(CELLS_PER_PACKET) <= 250);
}

static void test_30_cells_one_packet() {
  fill_src(30);
  CellPacket out[2];
  uint16_t bytes[2];
  TEST_ASSERT_EQUAL_UINT8(1, cells::encode(src, 30, 7, out, bytes, 2));
  TEST_ASSERT_EQUAL_UINT16(98, bytes[0]);
  cells::Frame f;
  cells::reset(f);
  TEST_ASSERT_TRUE(cells::accept(f, (const uint8_t *)&out[0], bytes[0]));
  TEST_ASSERT_EQUAL_UINT8(30, f.total);
  TEST_ASSERT_EQUAL_MEMORY(src, f.rgb, 90);
}

static void test_150_cells_two_packets() {
  fill_src(150);
  CellPacket out[2];
  uint16_t bytes[2];
  TEST_ASSERT_EQUAL_UINT8(2, cells::encode(src, 150, 9, out, bytes, 2));
  TEST_ASSERT_EQUAL_UINT16(248, bytes[0]);
  TEST_ASSERT_EQUAL_UINT16(cell_packet_bytes(70), bytes[1]);
  cells::Frame f;
  cells::reset(f);
  TEST_ASSERT_TRUE(cells::accept(f, (const uint8_t *)&out[1], bytes[1]));  // order doesn't matter
  TEST_ASSERT_TRUE(cells::accept(f, (const uint8_t *)&out[0], bytes[0]));
  TEST_ASSERT_EQUAL_MEMORY(src, f.rgb, 450);
}

static void test_lost_part_keeps_old_cells() {
  fill_src(150);
  CellPacket out[2];
  uint16_t bytes[2];
  cells::encode(src, 150, 1, out, bytes, 2);
  cells::Frame f;
  cells::reset(f);
  cells::accept(f, (const uint8_t *)&out[0], bytes[0]);
  cells::accept(f, (const uint8_t *)&out[1], bytes[1]);
  uint8_t next[MAX_CELLS * 3];
  memset(next, 200, sizeof(next));
  cells::encode(next, 150, 2, out, bytes, 2);
  cells::accept(f, (const uint8_t *)&out[0], bytes[0]);  // second part lost
  TEST_ASSERT_EQUAL_UINT8(200, f.rgb[0]);
  TEST_ASSERT_EQUAL_UINT8(src[80 * 3], f.rgb[80 * 3]);  // previous colour held
}

static void test_rejects_bad_packets() {
  fill_src(30);
  CellPacket out[1];
  uint16_t bytes[1];
  cells::encode(src, 30, 1, out, bytes, 1);
  cells::Frame f;
  cells::reset(f);
  TEST_ASSERT_FALSE(cells::accept(f, (const uint8_t *)&out[0], bytes[0] - 1));  // wrong length
  TEST_ASSERT_FALSE(cells::accept(f, (const uint8_t *)&out[0], 107));           // an AudioLedPacket-sized blob
  out[0].magic = PACKET_MAGIC;
  TEST_ASSERT_FALSE(cells::accept(f, (const uint8_t *)&out[0], bytes[0]));      // wrong magic
  TEST_ASSERT_EQUAL_UINT8(0, f.total);
  TEST_ASSERT_EQUAL_UINT8(0, cells::encode(src, 0, 1, out, bytes, 1));
  TEST_ASSERT_EQUAL_UINT8(0, cells::encode(src, MAX_CELLS + 1, 1, out, bytes, 1));
}

static void test_sample_hard_and_smooth() {
  cells::Frame f;
  cells::reset(f);
  f.total = 2;
  f.rgb[0] = 0;   f.rgb[1] = 0; f.rgb[2] = 0;    // cell 0 black
  f.rgb[3] = 255; f.rgb[4] = 0; f.rgb[5] = 255;  // cell 1 magenta
  uint8_t r, g, b;
  // 900 LEDs, 2 cells: hard edges switch at LED 450.
  cells::sample(f, 449, 900, false, r, g, b);
  TEST_ASSERT_EQUAL_UINT8(0, r);
  cells::sample(f, 450, 900, false, r, g, b);
  TEST_ASSERT_EQUAL_UINT8(255, r);
  // Smooth: cell centres (LEDs 225 and 675) are pure; the midpoint is about half.
  cells::sample(f, 0, 900, true, r, g, b);
  TEST_ASSERT_EQUAL_UINT8(0, r);
  cells::sample(f, 899, 900, true, r, g, b);
  TEST_ASSERT_EQUAL_UINT8(255, r);
  TEST_ASSERT_EQUAL_UINT8(255, b);
  cells::sample(f, 450, 900, true, r, g, b);
  TEST_ASSERT_UINT8_WITHIN(4, 128, r);
  TEST_ASSERT_EQUAL_UINT8(0, g);
}

static void test_sample_30_cells_on_900_leds() {
  cells::Frame f;
  cells::reset(f);
  f.total = 30;
  for (int c = 0; c < 30; c++) f.rgb[c * 3] = (uint8_t)(c * 8);
  uint8_t r, g, b;
  for (uint16_t c = 0; c < 30; c++) {  // hard mode: LEDs 30c .. 30c+29 are exactly cell c
    cells::sample(f, c * 30, 900, false, r, g, b);
    TEST_ASSERT_EQUAL_UINT8(c * 8, r);
    cells::sample(f, c * 30 + 29, 900, false, r, g, b);
    TEST_ASSERT_EQUAL_UINT8(c * 8, r);
  }
}

static void test_from_dmx() {
  uint8_t dmx[512];
  for (int i = 0; i < 512; i++) dmx[i] = (uint8_t)(i + 1);
  uint8_t rgb[MAX_CELLS * 3];
  TEST_ASSERT_TRUE(cells::from_dmx(dmx, 512, 1, 30, rgb));
  TEST_ASSERT_EQUAL_UINT8(1, rgb[0]);    // cell 1 red = CH1
  TEST_ASSERT_EQUAL_UINT8(90, rgb[89]);  // cell 30 blue = CH90
  TEST_ASSERT_TRUE(cells::from_dmx(dmx, 100, 91, 5, rgb));  // runs past a short frame
  TEST_ASSERT_EQUAL_UINT8(91, rgb[0]);
  TEST_ASSERT_EQUAL_UINT8(0, rgb[14]);
  TEST_ASSERT_FALSE(cells::from_dmx(nullptr, 512, 1, 30, rgb));
  TEST_ASSERT_FALSE(cells::from_dmx(dmx, 512, 1, 0, rgb));
  TEST_ASSERT_TRUE(cells::any_lit(rgb, 5));
  uint8_t dark[90] = {0};
  TEST_ASSERT_FALSE(cells::any_lit(dark, 30));
}

void setup() {
  delay(2000);
  UNITY_BEGIN();
  RUN_TEST(test_layout);
  RUN_TEST(test_30_cells_one_packet);
  RUN_TEST(test_150_cells_two_packets);
  RUN_TEST(test_lost_part_keeps_old_cells);
  RUN_TEST(test_rejects_bad_packets);
  RUN_TEST(test_sample_hard_and_smooth);
  RUN_TEST(test_sample_30_cells_on_900_leds);
  RUN_TEST(test_from_dmx);
  UNITY_END();
}

void loop() {}
