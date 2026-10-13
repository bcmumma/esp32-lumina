// Lighting state the sender owns and sends to the receiver in every packet.
#pragma once

#include "LuminaProtocol.h"

// Default LED receiver GPIO assignments. The sender does not electrically
// drive these pins; it sends them to the receiver in the ESP-NOW packet.
// The Hardware tab can still override them and save the new values.
static constexpr uint8_t DEFAULT_LED_PINS[MAX_STRANDS] = {33, 25, 26, 27};
static constexpr uint16_t DEFAULT_LED_COUNTS[MAX_STRANDS] = {10, 0, 0, 0};

struct LedConfig {
  uint8_t globalBrightness;
  uint8_t globalHue;
  uint8_t activeStrands;
  uint16_t maxLedsPerStrand;
  uint8_t colorMap;
  uint8_t redScale;
  uint8_t greenScale;
  uint8_t blueScale;
  uint8_t gamma10;
  StrandSettings strand[MAX_STRANDS];
};

extern LedConfig ledCfg;

void set_default_led_config();
