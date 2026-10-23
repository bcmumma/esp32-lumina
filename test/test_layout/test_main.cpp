// Fixture layout tests: channel maths, decoding with dimmer/strobe, mirroring, preset table.
#include <Arduino.h>
#include <string.h>
#include <unity.h>
#include "FixtureLayout.h"

void setUp() {}
void tearDown() {}

static uint8_t dmx[512], rgb[MAX_CELLS * 3], strobeVal[MAX_CELLS], cellFix[MAX_CELLS];

static void test_preset_table_fits() {
  TEST_ASSERT_EQUAL_UINT8(20, layout::preset_count());
  for (uint8_t id = 1; id <= layout::preset_count(); id++) {
    const layout::Layout *l = layout::preset(id);
    TEST_ASSERT_NOT_NULL(l);
    TEST_ASSERT_TRUE(layout::total_channels(*l) <= 512);
    TEST_ASSERT_TRUE(layout::strip_cells(*l) <= MAX_CELLS);
    TEST_ASSERT_TRUE(layout::is_preset_code(layout::preset_code(id)));
    TEST_ASSERT_EQUAL_UINT8(id, layout::preset_id(layout::preset_code(id)));
  }
  TEST_ASSERT_NULL(layout::preset(0));
  TEST_ASSERT_NULL(layout::preset(21));
  TEST_ASSERT_FALSE(layout::is_preset_code(30));  // 30 = 30 plain cells
}

static void test_channel_maths() {
  const layout::Layout &a = *layout::preset(3);   // 10x3 RGB
  TEST_ASSERT_EQUAL_UINT16(9, layout::channels_per_fixture(a));
  TEST_ASSERT_EQUAL_UINT16(90, layout::total_channels(a));
  const layout::Layout &b = *layout::preset(13);  // 10x3 Dim Strobe
  TEST_ASSERT_EQUAL_UINT16(11, layout::channels_per_fixture(b));
  TEST_ASSERT_EQUAL_UINT16(110, layout::total_channels(b));
  const layout::Layout &m = *layout::preset(15);  // Mirror 15x1
  TEST_ASSERT_EQUAL_UINT8(15, layout::source_cells(m));
  TEST_ASSERT_EQUAL_UINT8(30, layout::strip_cells(m));
}

static void test_plain_layout_equals_contiguous_cells() {
  for (int i = 0; i < 512; i++) dmx[i] = (uint8_t)(i * 3 + 1);
  TEST_ASSERT_TRUE(layout::decode(*layout::preset(4), dmx, 512, 1, rgb, strobeVal, cellFix));  // 6x5 RGB
  TEST_ASSERT_EQUAL_MEMORY(dmx, rgb, 90);
  TEST_ASSERT_EQUAL_UINT8(0, cellFix[0]);
  TEST_ASSERT_EQUAL_UINT8(1, cellFix[5]);
  TEST_ASSERT_EQUAL_UINT8(5, cellFix[29]);
}

static void test_dimmer_and_strobe_channels() {
  memset(dmx, 0, sizeof(dmx));
  const layout::Layout &l = *layout::preset(13);  // 10x3 Dim Strobe: D, 9 x RGB, S = 11 ch
  // fixture 0: dim 255, cell 0 red 200, strobe 0; fixture 1 (ch 12..22): dim 128, cell 0 red 200, strobe 200
  dmx[0] = 255; dmx[1] = 200; dmx[10] = 0;
  dmx[11] = 128; dmx[12] = 200; dmx[21] = 200;
  TEST_ASSERT_TRUE(layout::decode(l, dmx, 512, 1, rgb, strobeVal, cellFix));
  TEST_ASSERT_EQUAL_UINT8(200, rgb[0]);
  TEST_ASSERT_UINT8_WITHIN(1, 100, rgb[3 * 3]);  // fixture 1, first cell: 200 x 128/255
  TEST_ASSERT_EQUAL_UINT8(0, strobeVal[0]);
  TEST_ASSERT_EQUAL_UINT8(200, strobeVal[1]);
  TEST_ASSERT_EQUAL_UINT8(1, cellFix[3]);
}

static void test_mirror_puts_first_cell_at_centre() {
  memset(dmx, 0, sizeof(dmx));
  const layout::Layout &l = *layout::preset(15);  // Mirror 15x1 RGB
  for (int c = 0; c < 15; c++) dmx[c * 3] = (uint8_t)(c + 1);  // red = cell number
  TEST_ASSERT_TRUE(layout::decode(l, dmx, 512, 1, rgb, strobeVal, cellFix));
  TEST_ASSERT_EQUAL_UINT8(15, rgb[0]);        // strip start: SoundSwitch's last cell
  TEST_ASSERT_EQUAL_UINT8(1, rgb[14 * 3]);    // just left of centre: first cell
  TEST_ASSERT_EQUAL_UINT8(1, rgb[15 * 3]);    // just right of centre: first cell
  TEST_ASSERT_EQUAL_UINT8(15, rgb[29 * 3]);   // strip end: last cell
  TEST_ASSERT_EQUAL_UINT8(14, cellFix[0]);
  TEST_ASSERT_EQUAL_UINT8(0, cellFix[15]);
}

static void test_strobe_rate_mapping() {
  TEST_ASSERT_EQUAL_UINT8(0, layout::strobe_hz(0, 15));
  TEST_ASSERT_EQUAL_UINT8(0, layout::strobe_hz(9, 15));
  TEST_ASSERT_EQUAL_UINT8(1, layout::strobe_hz(10, 15));
  TEST_ASSERT_EQUAL_UINT8(15, layout::strobe_hz(255, 15));
  TEST_ASSERT_EQUAL_UINT8(3, layout::strobe_hz(255, 3));  // never above the rig's max rate
}

static void test_short_frame_reads_black() {
  memset(dmx, 77, sizeof(dmx));
  TEST_ASSERT_TRUE(layout::decode(*layout::preset(18), dmx, 100, 1, rgb, strobeVal, cellFix));  // 180 ch needed
  TEST_ASSERT_EQUAL_UINT8(77, rgb[99]);
  TEST_ASSERT_EQUAL_UINT8(0, rgb[100]);
}

void setup() {
  delay(2000);
  UNITY_BEGIN();
  RUN_TEST(test_preset_table_fits);
  RUN_TEST(test_channel_maths);
  RUN_TEST(test_plain_layout_equals_contiguous_cells);
  RUN_TEST(test_dimmer_and_strobe_channels);
  RUN_TEST(test_mirror_puts_first_cell_at_centre);
  RUN_TEST(test_strobe_rate_mapping);
  RUN_TEST(test_short_frame_reads_black);
  UNITY_END();
}

void loop() {}
