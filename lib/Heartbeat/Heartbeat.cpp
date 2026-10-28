#include "Heartbeat.h"

#include <string.h>

namespace heartbeat {

void reset(RoomStatus rooms[kRooms]) { memset(rooms, 0, sizeof(RoomStatus) * kRooms); }

uint8_t accept(RoomStatus rooms[kRooms], const uint8_t *data, int len, uint32_t nowMs) {
  if (!data || len != (int)sizeof(HeartbeatPacket)) return 0;
  HeartbeatPacket h;
  memcpy(&h, data, sizeof(h));
  if (h.magic != HEARTBEAT_MAGIC || h.version != HEARTBEAT_VERSION || h.room < 1 || h.room > kRooms) return 0;

  RoomStatus &r = rooms[h.room - 1];
  if (r.seen && h.uptimeMs < r.last.uptimeMs) {
    r.restarts++;  // it rebooted: its counters started again
    r.lossPermille = 0;
  } else if (r.seen) {
    const uint32_t dGood = h.good - r.last.good, dLost = h.lost - r.last.lost;
    r.lossPermille = (dGood + dLost) ? (uint16_t)((dLost * 1000UL) / (dGood + dLost)) : 0;
  }
  r.last = h;
  r.lastMs = nowMs ? nowMs : 1;
  r.seen = true;
  return h.room;
}

bool online(const RoomStatus &r, uint32_t nowMs) { return r.seen && nowMs - r.lastMs <= kOfflineMs; }

}  // namespace heartbeat
