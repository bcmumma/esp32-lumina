// Cell frames (Phase 15): split a multi-cell colour frame into ESP-NOW packets,
// reassemble it on the receiver, and blend cells into per-LED colours. Pure logic.
#pragma once

#include <stdint.h>

#include "LuminaProtocol.h"

namespace cells {

struct Frame {
  uint8_t total;      // 0 = no cell frame yet
  uint16_t frameId;   // id of the latest part received
  uint32_t parts;     // valid parts accepted (diagnostic)
  uint8_t rgb[MAX_CELLS * 3];
};

void reset(Frame &f);

// Splits `total` cells (rgb, 3 bytes each) into packets. Returns how many were written
// (at most maxPackets); bytes[i] is the length to send for out[i].
uint8_t encode(const uint8_t *rgb, uint8_t total, uint16_t frameId, CellPacket *out, uint16_t *bytes, uint8_t maxPackets);

// Reads `total` RGB cells starting at the 1-based DMX `address`. Cells past the end of
// the frame read as black. False if total is 0 or too large, or dmx is null.
bool from_dmx(const uint8_t *dmx, uint16_t length, uint16_t address, uint8_t total, uint8_t *rgb);

// True if any cell has a non-zero channel (SoundSwitch sends all-zero frames when blacked out).
bool any_lit(const uint8_t *rgb, uint8_t total);

// Accepts one received packet into the frame. Each part overwrites only its own cells,
// so a lost part leaves those cells at their previous colours. False if invalid.
bool accept(Frame &f, const uint8_t *data, int len);

// Colour of LED i of n. smooth=true blends linearly between neighbouring cell centres;
// false gives hard-edged cells.
void sample(const Frame &f, uint16_t i, uint16_t n, bool smooth, uint8_t &r, uint8_t &g, uint8_t &b);

}  // namespace cells
