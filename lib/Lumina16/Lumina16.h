// LUMINA-16 intelligent fixture v1 (docs/ARCHITECTURE.md, docs/DECISIONS.md).
// Decodes 16 DMX channels into the existing per-strand effect settings, applied
// identically to every strand. CH12-16 are reserved in v1 (no packet fields yet).
//
//   CH1 master dimmer   CH5 density     CH9-11 primary R/G/B
//   CH2 effect          CH6 strobe      CH12-14 secondary R/G/B (reserved)
//   CH3 speed           CH7 direction   CH15 transition (reserved)
//   CH4 intensity       CH8 variation   CH16 palette (reserved)
#pragma once

#include <stdint.h>

#include "LuminaProtocol.h"

namespace lumina16 {

constexpr uint8_t kChannels = 16;
constexpr uint8_t kEffectCount = 6;
// Receiver render modes for CH2 bands: Solid Color, Neon Scanner, Laser Sweep,
// Rave Chase, DnB Chopper, White Lightning.
constexpr uint8_t kEffectModes[kEffectCount] = {9, 17, 18, 19, 21, 27};

struct Params {
  uint8_t dimmer;
  uint8_t mode;
  uint8_t speed;
  uint8_t sensitivity;
  uint8_t density;
  uint8_t strobe;
  uint8_t direction;  // 0 forward, 1 reverse, 2 mirror/center
  uint8_t variation;
  uint8_t hue;         // receiver colour wheel (6 regions of 43)
  uint8_t saturation;  // 0 = white (valid)
  uint8_t value;       // max(R, G, B)
};

// CH2 value -> receiver mode (6 equal bands).
uint8_t effect_mode(uint8_t ch);
// CH7 value -> 0 forward (0-84), 1 reverse (85-169), 2 mirror (170-255).
uint8_t direction(uint8_t ch);
// Inverse of the receiver's hsv_to_rgb (8-bit, 43 hue steps per region).
void rgb_to_hsv(uint8_t r, uint8_t g, uint8_t b, uint8_t &h, uint8_t &s, uint8_t &v);

// Reads 16 channels starting at a 1-based DMX `address`. False if the frame is too short.
bool decode(const uint8_t *dmx, uint16_t length, uint16_t address, Params &out);

// Strand brightness: dimmer x colour value, then the safety cap (0-255).
uint8_t brightness(const Params &p, uint8_t cap);

// Overwrites the effect fields of every strand in `pkt` (hardware fields - pins,
// counts, types, enables - are left alone) and zeroes globalHue so hue is absolute.
void apply(AudioLedPacket &pkt, const Params &p, uint8_t brightnessCap);

}  // namespace lumina16
