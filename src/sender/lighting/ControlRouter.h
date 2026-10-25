// Phase 13 control-source router (lib/SourceRouter decides; this applies and persists).
// On every analyzed packet: stamp the strobe-safety bits, pick the active source
// (AUTO / STANDALONE / AUDIO / ARTNET), and apply LUMINA-16 or STANDALONE levels.
#pragma once

#include <Arduino.h>

#include "LuminaProtocol.h"
#include "SourceRouter.h"

// Rig brightness cap, applied to every source (Phase 14). Owner removed the 25% limit
// (2026-10-16): default 100% = no cap. 4 x 150 LEDs at 9 W/m is about 15 A at full white
// vs a 90 W supply, so lower it before strands 2-4 are connected (docs/HARDWARE.md).
static constexpr uint8_t RIG_BRIGHTNESS_CAP_MAX = 255;
static constexpr uint8_t RIG_BRIGHTNESS_CAP_MIN = 8;

struct ControlState {
  router::Source source;        // saved; boot default AUTO
  router::Source autoFallback;  // saved; AUDIO or STANDALONE
  uint8_t standaloneLevel;      // saved
  uint8_t strobeHz;             // saved; 1..STROBE_MAX_HZ
  bool strobeEnabled;           // never saved: always off at boot
  uint16_t address;             // fixture's 1-based DMX start address in the listen universe
  uint8_t fixture;              // saved; 0 = LUMINA-16, 1..150 = N plain RGB cells, 151.. = named layout (lib/FixtureLayout)
  uint8_t kickFlash;            // saved; 0..255 rig-wide white flash on bass hits (0 = off)
  uint8_t brightnessCap;        // saved; RIG_BRIGHTNESS_CAP_MIN..MAX, caps globalBrightness in every source
  // Lumina FX fixture (Phase 15): 5 channels right after the cell layout. All saved.
  bool fxEnabled;
  uint8_t fxHit;                // kick look: 0 off, 1 ripple, 2 glitter, 3 pump*, 4 shatter, 5 comet, 6 duck*, 7 ping-pong* (* gated)
  bool beamOn;                  // FX looks that can be switched on the Show screen
  bool sparkleOn;
  bool fxStrobeOn;
  bool kicksAlways;             // false = kick looks only in busy sections
  bool splitStrands;            // saved; cell layouts: strand k shows its share of the cells, not all of them
  uint8_t beamStrength;         // 0..255
  uint8_t beamSpread;           // 1..8
};

extern ControlState control;

// Loads the saved source/fallback/strobe-rate/standalone/cap record (defaults if missing or corrupt).
void load_control_settings();
// Saves that record (only the persistent fields). Returns false on a storage error.
bool save_control_settings();

// Called on every analyzed packet, before it is sent.
void control_apply(AudioLedPacket &p, uint32_t nowMs);
void append_control_json(String &s, uint32_t nowMs);
// "LUMINA-16", "30 cells", or a named layout such as "10x3 Dim Strobe".
const char *control_fixture_name();
// JSON list of the named layouts for the page (/api/fixtures).
void append_fixture_list_json(String &s);
