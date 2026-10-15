// Phase 12 control state (live-only; resets at boot, docs/DECISIONS.md).
// Stamps the strobe-safety bits on every packet, and when Art-Net control is on and
// frames are fresh, replaces the strands' effect settings with the LUMINA-16 fixture.
#pragma once

#include <Arduino.h>

#include "LuminaProtocol.h"

// 25% until the strip current is measured (90 W budget, docs/HARDWARE.md).
static constexpr uint8_t LUMINA16_BRIGHTNESS_CAP_MAX = 64;
// Art-Net older than this no longer drives the strands (they fall back to the web settings).
static constexpr uint32_t LUMINA16_FRESH_MS = 1000;

struct ControlState {
  bool artnetControl;     // default off
  uint16_t address;       // fixture's 1-based DMX start address in the listen universe
  uint8_t brightnessCap;  // 0..LUMINA16_BRIGHTNESS_CAP_MAX
  bool strobeEnabled;     // default off
  uint8_t strobeHz;       // 1..STROBE_MAX_HZ, default STROBE_DEFAULT_HZ
};

extern ControlState control;

// Called on every analyzed packet, before it is sent.
void control_apply(AudioLedPacket &p, uint32_t nowMs);
void append_control_json(String &s, uint32_t nowMs);
