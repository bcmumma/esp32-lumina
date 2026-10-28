// Heartbeat tests: parsing, online/offline timing, loss between heartbeats, receiver reboots.
#include <Arduino.h>
#include <unity.h>
#include "Heartbeat.h"

void setUp() {}
void tearDown() {}

static HeartbeatPacket hb(uint8_t room, uint32_t uptime, uint32_t good, uint32_t lost) {
  HeartbeatPacket h = {};
  h.magic = HEARTBEAT_MAGIC;
  h.version = HEARTBEAT_VERSION;
  h.room = room;
  h.uptimeMs = uptime;
  h.good = good;
  h.lost = lost;
  h.fps = 60;
  h.count[0] = 450;
  h.channel = 6;
  h.rssi = -60;
  return h;
}

static void test_layout() {
  TEST_ASSERT_EQUAL_UINT(34, sizeof(HeartbeatPacket));
  TEST_ASSERT_EQUAL_UINT(8, sizeof(CommandPacket));
}

static void test_accept_and_online() {
  heartbeat::RoomStatus rooms[2];
  heartbeat::reset(rooms);
  TEST_ASSERT_FALSE(heartbeat::online(rooms[0], 1000));
  HeartbeatPacket h = hb(2, 5000, 100, 0);
  TEST_ASSERT_EQUAL_UINT8(2, heartbeat::accept(rooms, (const uint8_t *)&h, sizeof(h), 1000));
  TEST_ASSERT_TRUE(heartbeat::online(rooms[1], 1000 + heartbeat::kOfflineMs));
  TEST_ASSERT_FALSE(heartbeat::online(rooms[1], 1001 + heartbeat::kOfflineMs));
  TEST_ASSERT_FALSE(heartbeat::online(rooms[0], 1000));  // room 1 never heard
  TEST_ASSERT_EQUAL_UINT16(450, rooms[1].last.count[0]);
}

static void test_rejects_other_packets() {
  heartbeat::RoomStatus rooms[2];
  heartbeat::reset(rooms);
  HeartbeatPacket h = hb(1, 1, 1, 0);
  TEST_ASSERT_EQUAL_UINT8(0, heartbeat::accept(rooms, (const uint8_t *)&h, sizeof(h) - 1, 10));  // wrong length
  h.magic = PACKET_MAGIC;
  TEST_ASSERT_EQUAL_UINT8(0, heartbeat::accept(rooms, (const uint8_t *)&h, sizeof(h), 10));
  h = hb(3, 1, 1, 0);  // no room 3
  TEST_ASSERT_EQUAL_UINT8(0, heartbeat::accept(rooms, (const uint8_t *)&h, sizeof(h), 10));
  TEST_ASSERT_FALSE(rooms[0].seen);
}

static void test_loss_between_heartbeats() {
  heartbeat::RoomStatus rooms[2];
  heartbeat::reset(rooms);
  HeartbeatPacket a = hb(1, 1000, 1000, 50), b = hb(1, 2000, 1028, 52);  // 28 good, 2 lost
  heartbeat::accept(rooms, (const uint8_t *)&a, sizeof(a), 100);
  heartbeat::accept(rooms, (const uint8_t *)&b, sizeof(b), 1100);
  TEST_ASSERT_EQUAL_UINT16(66, rooms[0].lossPermille);  // 2 / 30 = 6.6%
}

static void test_update_flags_and_ip() {
  heartbeat::RoomStatus rooms[2];
  heartbeat::reset(rooms);
  HeartbeatPacket h = hb(1, 1000, 10, 0);
  h.flags = HEARTBEAT_FLAG_UPDATING | HEARTBEAT_FLAG_WIFI;
  h.ip = 0x6F0200C0;  // 192.0.2.111 (documentation range) as stored by IPAddress, little-endian octets
  heartbeat::accept(rooms, (const uint8_t *)&h, sizeof(h), 10);
  TEST_ASSERT_EQUAL_HEX8(0x03, rooms[0].last.flags);
  TEST_ASSERT_EQUAL_HEX32(0x6F0200C0, rooms[0].last.ip);
}

static void test_receiver_reboot() {
  heartbeat::RoomStatus rooms[2];
  heartbeat::reset(rooms);
  HeartbeatPacket a = hb(1, 900000, 50000, 300), b = hb(1, 1500, 40, 0);
  heartbeat::accept(rooms, (const uint8_t *)&a, sizeof(a), 100);
  heartbeat::accept(rooms, (const uint8_t *)&b, sizeof(b), 1100);
  TEST_ASSERT_EQUAL_UINT32(1, rooms[0].restarts);
  TEST_ASSERT_EQUAL_UINT16(0, rooms[0].lossPermille);
}

void setup() {
  delay(2000);
  UNITY_BEGIN();
  RUN_TEST(test_layout);
  RUN_TEST(test_accept_and_online);
  RUN_TEST(test_rejects_other_packets);
  RUN_TEST(test_loss_between_heartbeats);
  RUN_TEST(test_receiver_reboot);
  RUN_TEST(test_update_flags_and_ip);
  UNITY_END();
}

void loop() {}
