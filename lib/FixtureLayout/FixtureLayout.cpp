#include "FixtureLayout.h"

namespace layout {

// The owner's layouts (2026-10-25): L1 is the old L11 (6x5 Dim) that tested best; L2/L3 scale it
// for two 450-LED strands; L4-L6 are the same with a Strobe channel. Every one ends with the
// Lumina FX fixture. The S3 saves the id: append new layouts, don't reorder.
//                  name                    SoundSwitch mode      F   C   dim   strobe mirror hard  fx
static const Layout kPresets[] = {
    {"6x5 Dim + FX",          "5 Cell Dim",          6, 5, true, false, false, false, true},  // L1
    {"12x5 Dim + FX",         "5 Cell Dim",         12, 5, true, false, false, false, true},  // L2
    {"18x5 Dim + FX",         "5 Cell Dim",         18, 5, true, false, false, false, true},  // L3
    {"6x5 Dim Strobe + FX",   "5 Cell Dim Strobe",   6, 5, true, true,  false, false, true},  // L4
    {"12x5 Dim Strobe + FX",  "5 Cell Dim Strobe",  12, 5, true, true,  false, false, true},  // L5
    {"18x5 Dim Strobe + FX",  "5 Cell Dim Strobe",  18, 5, true, true,  false, false, true},  // L6
};

uint8_t preset_count() { return (uint8_t)(sizeof(kPresets) / sizeof(kPresets[0])); }

const Layout *preset(uint8_t id) { return (id >= 1 && id <= preset_count()) ? &kPresets[id - 1] : nullptr; }

uint16_t channels_per_fixture(const Layout &l) { return (uint16_t)((l.dimmer ? 1 : 0) + 3U * l.cells + (l.strobe ? 1 : 0)); }
uint16_t total_channels(const Layout &l) { return (uint16_t)(channels_per_fixture(l) * l.fixtures); }
uint8_t source_cells(const Layout &l) { return (uint8_t)(l.fixtures * l.cells); }
uint8_t strip_cells(const Layout &l) { return (uint8_t)(source_cells(l) * (l.mirror ? 2 : 1)); }

bool decode(const Layout &l, const uint8_t *dmx, uint16_t length, uint16_t address, uint8_t *rgb, uint8_t *cellStrobe) {
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
    const uint8_t strobe = l.strobe ? ch(first + 3U * l.cells) : 0;
    for (uint8_t c = 0; c < l.cells; c++, cell++) {
      const uint16_t scale = c == 0 ? dim : 255;  // the Dimmer is Cell 1's
      for (uint8_t k = 0; k < 3; k++) rgb[cell * 3 + k] = (uint8_t)((ch(first + 3U * c + k) * scale + 127U) / 255U);
      cellStrobe[cell] = c == l.cells - 1 ? strobe : 0;  // the Strobe is the last cell's
    }
  }
  if (l.mirror) {  // strip = cells n-1 .. 0, then 0 .. n-1: SoundSwitch's first cell sits at the centre
    for (uint16_t i = 0; i < n; i++) {  // copy to the right half as-is
      for (uint8_t k = 0; k < 3; k++) rgb[(n + i) * 3 + k] = rgb[i * 3 + k];
      cellStrobe[n + i] = cellStrobe[i];
    }
    for (uint16_t i = 0; i < n / 2; i++) {  // reverse the first half in place
      const uint16_t j = (uint16_t)(n - 1U - i);
      for (uint8_t k = 0; k < 3; k++) {
        const uint8_t t = rgb[i * 3 + k];
        rgb[i * 3 + k] = rgb[j * 3 + k];
        rgb[j * 3 + k] = t;
      }
      const uint8_t t = cellStrobe[i];
      cellStrobe[i] = cellStrobe[j];
      cellStrobe[j] = t;
    }
  }
  return true;
}

uint8_t strobe_hz(uint8_t value, uint8_t maxHz) {
  if (value < 10 || maxHz == 0) return 0;
  return (uint8_t)(1U + ((uint16_t)(value - 10U) * (maxHz - 1U)) / 245U);
}

}  // namespace layout
