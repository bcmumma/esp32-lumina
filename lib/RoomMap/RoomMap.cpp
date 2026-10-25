#include "RoomMap.h"

namespace room {

// Local strands 3-4 are never driven by a room receiver; they keep the proven spare pins.
static const uint8_t kSparePins[2] = {26, 27};

void view(AudioLedPacket &p, uint8_t r) {
  if (r < 1) r = 1;
  if (r > kRooms) r = kRooms;
  const uint8_t base = (uint8_t)((r - 1U) * kStrandsPerRoom);
  int active = p.activeStrands < 1 ? 1 : (p.activeStrands > MAX_STRANDS ? MAX_STRANDS : p.activeStrands);
  int local = active - base;
  if (local < 0) local = 0;
  if (local > kStrandsPerRoom) local = kStrandsPerRoom;

  const StrandSettings mine[kStrandsPerRoom] = {p.strand[base], p.strand[base + 1]};
  for (uint8_t s = 0; s < MAX_STRANDS; s++) {
    if (s < kStrandsPerRoom) {
      p.strand[s] = mine[s];
    } else {
      p.strand[s] = StrandSettings{};
      p.strand[s].pin = kSparePins[s - kStrandsPerRoom];
    }
  }
  if (local == 0) {  // this room has no active strands: dark
    p.activeStrands = 1;
    p.strand[0].enabled = 0;
    p.strand[0].ledCount = 0;
  } else {
    p.activeStrands = (uint8_t)local;
  }
}

}  // namespace room
