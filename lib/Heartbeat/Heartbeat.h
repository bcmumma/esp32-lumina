// Room status on the S3 (Phase 16): turns receiver heartbeats into "online / offline" and a
// recent packet-loss figure per room. Pure logic, no Arduino.
#pragma once

#include <stdint.h>

#include "LuminaProtocol.h"

namespace heartbeat {

constexpr uint8_t kRooms = 2;
constexpr uint32_t kOfflineMs = 5000;  // no heartbeat this long = offline

struct RoomStatus {
  bool seen;               // ever heard from this room since the S3 booted
  uint32_t lastMs;         // S3 time of the last heartbeat
  HeartbeatPacket last;    // the last heartbeat
  uint16_t lossPermille;   // packet loss between the last two heartbeats, 0..1000
  uint32_t restarts;       // times the receiver's uptime went backwards (it rebooted)
};

void reset(RoomStatus rooms[kRooms]);

// Parses one received ESP-NOW payload. Returns the room (1 or 2) it updated, or 0 if it
// isn't a valid heartbeat.
uint8_t accept(RoomStatus rooms[kRooms], const uint8_t *data, int len, uint32_t nowMs);

bool online(const RoomStatus &r, uint32_t nowMs);

}  // namespace heartbeat
