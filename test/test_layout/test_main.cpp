// Fixture layout tests: the owner's six layouts, channel maths, Dimmer/Strobe per cell, mirroring.
#include <Arduino.h>
#include <string.h>
#include <unity.h>
#include "FixtureLayout.h"

void setUp() {}
void tearDown() {}

static uint8_t dmx[512], rgb[MAX_CELLS * 3], cellStrobe[MAX_CELLS];

static void test_six_layouts_fit_with_fx() {
  TEST_ASSERT_EQUAL_UINT8(6, layout::preset_count());
  const uint16_t total[] = {96, 192, 288, 102, 204, 306};
  const uint8_t cells[] = {30, 60, 90, 30, 60, 90};
  for (uint8_t id = 1; id <= 6; id++) {
    const layout::Layout *l = layout::preset(id);
    TEST_ASSERT_NOT_NULL(l);
    TEST_ASSERT_TRUE(l->fx);
    TEST_ASSERT_EQUAL_UINT16(total[id - 1], layout::total_channels(*l));
    TEST_ASSERT_TRUE(layout::total_channels(*l) + 5 <= 512);  // the FX fixture still fits
    TEST_ASSERT_EQUAL_UINT8(cells[id - 1], layout::strip_cells(*l));
  }
  TEST_ASSERT_NULL(layout::preset(0));
  TEST_ASSERT_NULL(layout::preset(7));
}

static void test_codes_and_migration() {
  TEST_ASSERT_EQUAL_UINT8(201, layout::preset_code(1));
  TEST_ASSERT_TRUE(layout::is_preset_code(206));
  TEST_ASSERT_FALSE(layout::is_preset_code(150));  // 150 plain cells
  TEST_ASSERT_EQUAL_UINT8(201, layout::migrate_code(161));  // old L11 -> new L1
  TEST_ASSERT_EQUAL_UINT8(201, layout::migrate_code(170));  // any retired layout -> L1
  TEST_ASSERT_EQUAL_UINT8(30, layout::migrate_code(30));    // plain cells unchanged
  TEST_ASSERT_EQUAL_UINT8(0, layout::migrate_code(0));      // LUMINA-16 unchanged
  TEST_ASSERT_EQUAL_UINT8(203, layout::migrate_code(203));  // new codes unchanged
}

static void test_dimmer_is_cell_1s() {
  memset(dmx, 0, sizeof(dmx));
  const layout::Layout &l = *layout::preset(1);  // 6 x (Dimmer + 5 RGB) = 16 ch
  dmx[16] = 128; dmx[17] = 200; dmx[20] = 200;   // fixture 2: dimmer 128, cells 1 and 2 red 200
  TEST_ASSERT_TRUE(layout::decode(l, dmx, 512, 1, rgb, cellStrobe));
  TEST_ASSERT_UINT8_WITHIN(1, 100, rgb[5 * 3]);  // fixture 2, Cell 1: dimmed
  TEST_ASSERT_EQUAL_UINT8(200, rgb[6 * 3]);      // fixture 2, Cell 2: not dimmed
}

static void test_strobe_is_last_cells() {
  memset(dmx, 0, sizeof(dmx));
  const layout::Layout &l = *layout::preset(4);  // 6 x (Dimmer + 5 RGB + Strobe) = 17 ch
  dmx[17 + 16] = 200;                            // fixture 2's Strobe
  TEST_ASSERT_TRUE(layout::decode(l, dmx, 512, 1, rgb, cellStrobe));
  TEST_ASSERT_EQUAL_UINT8(0, cellStrobe[5]);
  TEST_ASSERT_EQUAL_UINT8(200, cellStrobe[9]);   // fixture 2, Cell 5
  TEST_ASSERT_EQUAL_UINT8(0, cellStrobe[4]);
}

static void test_mirror_puts_first_cell_at_centre() {
  memset(dmx, 0, sizeof(dmx));
  const layout::Layout l = {"mirror", "1 Cell", 15, 1, false, false, true, false, false};
  for (int c = 0; c < 15; c++) dmx[c * 3] = (uint8_t)(c + 1);
  TEST_ASSERT_TRUE(layout::decode(l, dmx, 512, 1, rgb, cellStrobe));
  TEST_ASSERT_EQUAL_UINT8(15, rgb[0]);
  TEST_ASSERT_EQUAL_UINT8(1, rgb[14 * 3]);
  TEST_ASSERT_EQUAL_UINT8(1, rgb[15 * 3]);
  TEST_ASSERT_EQUAL_UINT8(15, rgb[29 * 3]);
}

static void test_strobe_rate_mapping() {
  TEST_ASSERT_EQUAL_UINT8(0, layout::strobe_hz(9, 15));
  TEST_ASSERT_EQUAL_UINT8(1, layout::strobe_hz(10, 15));
  TEST_ASSERT_EQUAL_UINT8(15, layout::strobe_hz(255, 15));
  TEST_ASSERT_EQUAL_UINT8(3, layout::strobe_hz(255, 3));
}

static void test_short_frame_reads_black() {
  memset(dmx, 77, sizeof(dmx));
  TEST_ASSERT_TRUE(layout::decode(*layout::preset(3), dmx, 100, 1, rgb, cellStrobe));  // needs 288 ch
  TEST_ASSERT_EQUAL_UINT8(0, rgb[89 * 3]);  // last cell, past the frame
}

void setup() {
  delay(2000);
  UNITY_BEGIN();
  RUN_TEST(test_six_layouts_fit_with_fx);
  RUN_TEST(test_codes_and_migration);
  RUN_TEST(test_dimmer_is_cell_1s);
  RUN_TEST(test_strobe_is_last_cells);
  RUN_TEST(test_mirror_puts_first_cell_at_centre);
  RUN_TEST(test_strobe_rate_mapping);
  RUN_TEST(test_short_frame_reads_black);
  UNITY_END();
}

void loop() {}
