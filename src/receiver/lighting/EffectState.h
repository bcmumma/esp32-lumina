// Per-strand effect runtime state. Reset whenever a strand is reconfigured.
#pragma once

#include <stdint.h>
#include "LuminaProtocol.h"

struct StrandRuntime {
  uint16_t pos;
  uint16_t aux;
  uint8_t flash;
  uint8_t sparkle;
  uint8_t meterPeak;
  uint8_t prevEnergy;
  uint8_t prevTreble;
  uint8_t phase;
  uint32_t lastTriggerMs;
  uint16_t ring[4];     // Kick Ripple: radius + 1 of each travelling ring (0 = unused)
  uint8_t ringHue[4];
};

extern StrandRuntime rt[MAX_STRANDS];
