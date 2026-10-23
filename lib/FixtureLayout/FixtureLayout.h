// SoundSwitch fixture layouts (Phase 15): how the S3 reads a venue's DMX channels into
// strip cells. Each layout is F identical fixtures patched back to back from the start
// address; every fixture is [Dimmer?] + C x RGB + [Strobe?]. Pure logic, no Arduino.
#pragma once

#include <stdint.h>

#include "LuminaProtocol.h"

namespace layout {

struct Layout {
  const char *name;      // shown on the page; matches the SoundSwitch venue name "Lnn <name>"
  const char *mode;      // SoundSwitch Fixture Manager mode to patch
  uint8_t fixtures;      // F
  uint8_t cells;         // C, cells per fixture
  bool dimmer;           // first channel of each fixture scales its cells
  bool strobe;           // last channel of each fixture: 0-9 steady, 10-255 strobe (slow..fast)
  bool mirror;           // SoundSwitch drives half the strip; Lumina mirrors it from the centre
  bool hardEdges;        // cells as crisp blocks instead of blended
};

uint8_t preset_count();
// Preset id 1..preset_count(); nullptr if out of range.
const Layout *preset(uint8_t id);

uint16_t channels_per_fixture(const Layout &l);
uint16_t total_channels(const Layout &l);
uint8_t source_cells(const Layout &l);   // F x C, as SoundSwitch drives them
uint8_t strip_cells(const Layout &l);    // cells drawn on the strip (doubled when mirrored)

// Reads one frame. rgb gets strip_cells(l) cells (dimmer applied, mirrored if set);
// strobeVal gets each fixture's strobe channel (0 when the layout has none); sourceFixture
// maps each strip cell to its fixture (for strobing). False if the layout doesn't fit.
bool decode(const Layout &l, const uint8_t *dmx, uint16_t length, uint16_t address, uint8_t *rgb, uint8_t *strobeVal,
            uint8_t *cellFixture);

// Strobe channel value -> wanted flashes per second (0 = steady). Capped later by the rig gate.
uint8_t strobe_hz(uint8_t value, uint8_t maxHz);

// Saved fixture byte: 0 = LUMINA-16, 1..150 = N plain RGB cells, 151.. = preset (code - 150).
constexpr uint8_t kPresetBase = 150;
inline bool is_preset_code(uint8_t code) { return code > kPresetBase; }
inline uint8_t preset_id(uint8_t code) { return (uint8_t)(code - kPresetBase); }
inline uint8_t preset_code(uint8_t id) { return (uint8_t)(kPresetBase + id); }

}  // namespace layout
