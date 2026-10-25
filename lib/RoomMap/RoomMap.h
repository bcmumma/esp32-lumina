// Two room receivers, one S3 (Phase 15, owner 2026-10-25). The S3 broadcasts one packet with
// four strand slots; room 1's receiver drives slots 1-2 and room 2's drives slots 3-4, each on
// its own first two outputs, so both rooms are wired the same way. Same show in both rooms.
// Pure logic, no Arduino.
#pragma once

#include <stdint.h>

#include "LuminaProtocol.h"

namespace room {

constexpr uint8_t kRooms = 2;
constexpr uint8_t kStrandsPerRoom = 2;

// Rewrites the packet as this room sees it: its two strand slots become local strands 1-2,
// local strands 3-4 are switched off, and activeStrands counts only this room's active slots.
// A room with no active slots gets everything off.
void view(AudioLedPacket &p, uint8_t room);

}  // namespace room
