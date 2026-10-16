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
  uint8_t brightnessCap;        // saved; RIG_BRIGHTNESS_CAP_MIN..MAX, caps globalBrightness in every source
};

extern ControlState control;

// Loads the saved source/fallback/strobe-rate/standalone/cap record (defaults if missing or corrupt).
void load_control_settings();
// Saves that record (only the persistent fields). Returns false on a storage error.
bool save_control_settings();

// Called on every analyzed packet, before it is sent.
void control_apply(AudioLedPacket &p, uint32_t nowMs);
void append_control_json(String &s, uint32_t nowMs);
