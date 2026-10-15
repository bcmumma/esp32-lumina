// Phase 13 control-source router (lib/SourceRouter decides; this applies and persists).
// On every analyzed packet: stamp the strobe-safety bits, pick the active source
// (AUTO / STANDALONE / AUDIO / ARTNET), and apply LUMINA-16 or STANDALONE levels.
#pragma once

#include <Arduino.h>

#include "LuminaProtocol.h"
#include "SourceRouter.h"

// 25% until the strip current is measured (90 W budget, docs/HARDWARE.md).
static constexpr uint8_t LUMINA16_BRIGHTNESS_CAP_MAX = 64;

struct ControlState {
  router::Source source;        // saved; boot default AUTO
  router::Source autoFallback;  // saved; AUDIO or STANDALONE
  uint8_t standaloneLevel;      // saved
  uint8_t strobeHz;             // saved; 1..STROBE_MAX_HZ
  bool strobeEnabled;           // never saved: always off at boot
  uint16_t address;             // fixture's 1-based DMX start address in the listen universe
  uint8_t brightnessCap;        // 0..LUMINA16_BRIGHTNESS_CAP_MAX
};

extern ControlState control;

// Loads the saved source/fallback/strobe-rate/standalone record (defaults if missing or corrupt).
void load_control_settings();
// Saves that record (only the persistent fields). Returns false on a storage error.
bool save_control_settings();

// Called on every analyzed packet, before it is sent.
void control_apply(AudioLedPacket &p, uint32_t nowMs);
void append_control_json(String &s, uint32_t nowMs);
