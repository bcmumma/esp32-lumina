#include "FixtureLayout.h"

namespace layout {

// The test chart (docs/ARTNET_SOUNDSWITCH_GUIDE.md, Part 6). Order and ids are fixed: the S3
// saves the id, so append new layouts at the end and never reorder.
//                  name                     SoundSwitch mode        F   C   dim    strobe mirror hard
static const Layout kPresets[] = {
    {"30x1 RGB",              "1 Cell",               30,  1, false, false, false, false},  // L01
    {"15x2 RGB",              "2 Cell",               15,  2, false, false, false, false},  // L02
    {"10x3 RGB",              "3 Cell",               10,  3, false, false, false, false},  // L03
    {"6x5 RGB",               "5 Cell",                6,  5, false, false, false, false},  // L04
    {"3x10 RGB",              "10 Cell",               3, 10, false, false, false, false},  // L05
    {"1x30 RGB",              "30 Cell",               1, 30, false, false, false, false},  // L06
    {"10x3 RGB Blocks",       "3 Cell",               10,  3, false, false, false, true},   // L07
    {"6x5 RGB Blocks",        "5 Cell",                6,  5, false, false, false, true},   // L08
    {"30x1 Dim",              "1 Cell Dim",           30,  1, true,  false, false, false},  // L09
    {"10x3 Dim",              "3 Cell Dim",           10,  3, true,  false, false, false},  // L10
    {"6x5 Dim",               "5 Cell Dim",            6,  5, true,  false, false, false},  // L11
    {"30x1 Dim Strobe",       "1 Cell Dim Strobe",    30,  1, true,  true,  false, false},  // L12
    {"10x3 Dim Strobe",       "3 Cell Dim Strobe",    10,  3, true,  true,  false, false},  // L13
    {"6x5 Dim Strobe",        "5 Cell Dim Strobe",     6,  5, true,  true,  false, false},  // L14
    {"Mirror 15x1 RGB",       "1 Cell",               15,  1, false, false, true,  false},  // L15
    {"Mirror 5x3 RGB",        "3 Cell",                5,  3, false, false, true,  false},  // L16
    {"Mirror 5x3 Dim Strobe", "3 Cell Dim Strobe",     5,  3, true,  true,  true,  false},  // L17
    {"Fine 60x1 RGB",         "1 Cell",               60,  1, false, false, false, false},  // L18
    {"Fine 30x2 RGB",         "2 Cell",               30,  2, false, false, false, false},  // L19
    {"Fine 15x4 RGB",         "4 Cell",               15,  4, false, false, false, false},  // L20
};

uint8_t preset_count() { return (uint8_t)(sizeof(kPresets) / sizeof(kPresets[0])); }

const Layout *preset(uint8_t id) { return (id >= 1 && id <= preset_count()) ? &kPresets[id - 1] : nullptr; }

uint16_t channels_per_fixture(const Layout &l) { return (uint16_t)((l.dimmer ? 1 : 0) + 3U * l.cells + (l.strobe ? 1 : 0)); }
uint16_t total_channels(const Layout &l) { return (uint16_t)(channels_per_fixture(l) * l.fixtures); }
uint8_t source_cells(const Layout &l) { return (uint8_t)(l.fixtures * l.cells); }
uint8_t strip_cells(const Layout &l) { return (uint8_t)(source_cells(l) * (l.mirror ? 2 : 1)); }

bool decode(const Layout &l, const uint8_t *dmx, uint16_t length, uint16_t address, uint8_t *rgb, uint8_t *strobeVal,
            uint8_t *cellFixture) {
  const uint16_t n = source_cells(l);
  if (!dmx || address == 0 || n == 0 || strip_cells(l) > MAX_CELLS || l.fixtures * (l.mirror ? 2U : 1U) > MAX_CELLS) return false;
  const uint16_t per = channels_per_fixture(l);
  auto ch = [&](uint32_t offset) -> uint8_t {
    const uint32_t i = (uint32_t)address - 1U + offset;
    return i < length ? dmx[i] : 0;
  };
  uint16_t cell = 0;
  for (uint8_t f = 0; f < l.fixtures; f++) {
    const uint32_t base = (uint32_t)f * per;
    const uint8_t dim = l.dimmer ? ch(base) : 255;
    const uint32_t first = base + (l.dimmer ? 1 : 0);
    strobeVal[f] = l.strobe ? ch(first + 3U * l.cells) : 0;
    for (uint8_t c = 0; c < l.cells; c++, cell++) {
      for (uint8_t k = 0; k < 3; k++) rgb[cell * 3 + k] = (uint8_t)((ch(first + 3U * c + k) * (uint16_t)dim + 127U) / 255U);
      cellFixture[cell] = f;
    }
  }
  if (l.mirror) {  // strip = cells n-1 .. 0, then 0 .. n-1: SoundSwitch's first cell sits at the centre
    for (uint16_t i = 0; i < n; i++) {  // copy to the right half as-is
      for (uint8_t k = 0; k < 3; k++) rgb[(n + i) * 3 + k] = rgb[i * 3 + k];
      cellFixture[n + i] = cellFixture[i];
    }
    for (uint16_t i = 0; i < n / 2; i++) {  // reverse the first half in place
      const uint16_t j = (uint16_t)(n - 1U - i);
      for (uint8_t k = 0; k < 3; k++) {
        const uint8_t t = rgb[i * 3 + k];
        rgb[i * 3 + k] = rgb[j * 3 + k];
        rgb[j * 3 + k] = t;
      }
      const uint8_t t = cellFixture[i];
      cellFixture[i] = cellFixture[j];
      cellFixture[j] = t;
    }
  }
  return true;
}

uint8_t strobe_hz(uint8_t value, uint8_t maxHz) {
  if (value < 10 || maxHz == 0) return 0;
  return (uint8_t)(1U + ((uint16_t)(value - 10U) * (maxHz - 1U)) / 245U);
}

}  // namespace layout
