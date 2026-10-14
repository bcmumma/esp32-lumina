// Art-Net parser tests. Packets are synthetic byte arrays shaped like the measured
// SoundSwitch traffic (530-byte ArtDmx, universe 0/1, protocol 14); no raw captures.
#include <Arduino.h>
#include <string.h>
#include <unity.h>
#include "ArtNet.h"

void setUp() {}
void tearDown() {}

static size_t make_dmx(uint8_t *buf, uint8_t net, uint8_t subuni, uint8_t seq, uint16_t length) {
  const uint8_t head[] = {'A', 'r', 't', '-', 'N', 'e', 't', 0, 0x00, 0x50, 0, 14, seq, 0, subuni, net,
                          (uint8_t)(length >> 8), (uint8_t)(length & 0xFF)};
  memcpy(buf, head, sizeof(head));
  for (uint16_t i = 0; i < length; i++) buf[18 + i] = 0;
  return 18 + length;
}

static void test_soundswitch_red_frame() {
  uint8_t buf[530];
  size_t n = make_dmx(buf, 0, 0, 42, 512);
  TEST_ASSERT_EQUAL_UINT(530, n);
  buf[18 + 0] = 230;  // CH1 red
  buf[18 + 3] = 230;  // CH4
  buf[18 + 9] = 230;  // CH10
  artnet::DmxFrame f;
  TEST_ASSERT_TRUE(artnet::parse_dmx(buf, n, f));
  TEST_ASSERT_EQUAL_UINT16(0, f.universe);
  TEST_ASSERT_EQUAL_UINT8(42, f.sequence);
  TEST_ASSERT_EQUAL_UINT16(512, f.length);
  TEST_ASSERT_EQUAL_UINT8(230, f.data[0]);
  TEST_ASSERT_EQUAL_UINT8(0, f.data[1]);
  TEST_ASSERT_EQUAL_UINT8(230, f.data[9]);
}

static void test_universe_split() {
  uint8_t buf[530];
  artnet::DmxFrame f;
  TEST_ASSERT_TRUE(artnet::parse_dmx(buf, make_dmx(buf, 0, 1, 1, 512), f));
  TEST_ASSERT_EQUAL_UINT16(1, f.universe);
  TEST_ASSERT_TRUE(artnet::parse_dmx(buf, make_dmx(buf, 0x03, 0x25, 1, 512), f));
  TEST_ASSERT_EQUAL_UINT16(0x0325, f.universe);
  TEST_ASSERT_TRUE(artnet::parse_dmx(buf, make_dmx(buf, 0xFF, 0x00, 1, 512), f));  // Net top bit ignored
  TEST_ASSERT_EQUAL_UINT16(0x7F00, f.universe);
}

static void test_classify() {
  uint8_t buf[530];
  TEST_ASSERT_EQUAL(artnet::PacketType::Dmx, artnet::classify(buf, make_dmx(buf, 0, 0, 1, 2)));
  const uint8_t poll[] = {'A', 'r', 't', '-', 'N', 'e', 't', 0, 0x00, 0x20, 0, 14, 0, 0};
  TEST_ASSERT_EQUAL(artnet::PacketType::Poll, artnet::classify(poll, sizeof(poll)));
  const uint8_t other[] = {'A', 'r', 't', '-', 'N', 'e', 't', 0, 0x00, 0x21};
  TEST_ASSERT_EQUAL(artnet::PacketType::Other, artnet::classify(other, sizeof(other)));
  const uint8_t junk[] = {'H', 'T', 'T', 'P', '/', '1', '.', '1', 0, 0};
  TEST_ASSERT_EQUAL(artnet::PacketType::Invalid, artnet::classify(junk, sizeof(junk)));
  TEST_ASSERT_EQUAL(artnet::PacketType::Invalid, artnet::classify(poll, 9));
  TEST_ASSERT_EQUAL(artnet::PacketType::Invalid, artnet::classify(nullptr, 100));
}

static void test_malformed_dmx_rejected() {
  uint8_t buf[530];
  artnet::DmxFrame f;
  size_t n = make_dmx(buf, 0, 0, 1, 512);
  TEST_ASSERT_FALSE(artnet::parse_dmx(buf, n - 1, f));   // truncated data
  TEST_ASSERT_FALSE(artnet::parse_dmx(buf, 17, f));      // truncated header
  buf[16] = 0x02; buf[17] = 0x01;                        // length 513
  TEST_ASSERT_FALSE(artnet::parse_dmx(buf, n, f));
  n = make_dmx(buf, 0, 0, 1, 512);
  buf[16] = 0; buf[17] = 0;                              // length 0
  TEST_ASSERT_FALSE(artnet::parse_dmx(buf, n, f));
  n = make_dmx(buf, 0, 0, 1, 512);
  buf[11] = 13;                                          // protocol version too old
  TEST_ASSERT_FALSE(artnet::parse_dmx(buf, n, f));
  n = make_dmx(buf, 0, 0, 1, 512);
  buf[3] = 'x';                                          // bad ID
  TEST_ASSERT_FALSE(artnet::parse_dmx(buf, n, f));
}

static void test_short_frame_accepted() {
  uint8_t buf[64];
  artnet::DmxFrame f;
  size_t n = make_dmx(buf, 0, 0, 7, 12);  // Lumina fixture only
  TEST_ASSERT_TRUE(artnet::parse_dmx(buf, n, f));
  TEST_ASSERT_EQUAL_UINT16(12, f.length);
}

static void test_poll_reply_layout() {
  uint8_t out[300];
  const uint8_t ip[4] = {192, 0, 2, 10};   // documentation address (RFC 5737)
  const uint8_t mac[6] = {0x02, 0, 0, 0, 0, 0x01};
  TEST_ASSERT_EQUAL_UINT(0, artnet::build_poll_reply(out, 238, ip, mac, 0, "x", "y"));
  size_t n = artnet::build_poll_reply(out, sizeof(out), ip, mac, 0, "Lumina S3", "Lumina S3 long name");
  TEST_ASSERT_EQUAL_UINT(239, n);
  TEST_ASSERT_EQUAL(artnet::PacketType::Other, artnet::classify(out, n));
  TEST_ASSERT_EQUAL_HEX8(0x00, out[8]);
  TEST_ASSERT_EQUAL_HEX8(0x21, out[9]);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(ip, out + 10, 4);
  TEST_ASSERT_EQUAL_HEX8(0x36, out[14]);   // 6454 = 0x1936, little-endian
  TEST_ASSERT_EQUAL_HEX8(0x19, out[15]);
  TEST_ASSERT_EQUAL_STRING("Lumina S3", (const char *)(out + 26));
  TEST_ASSERT_EQUAL_UINT8(0, out[26 + 17]);  // ShortName NUL-terminated
  TEST_ASSERT_EQUAL_UINT8(1, out[173]);      // one port
  TEST_ASSERT_EQUAL_HEX8(0x80, out[174]);    // DMX output port
  TEST_ASSERT_EQUAL_UINT8_ARRAY(mac, out + 201, 6);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(ip, out + 207, 4);
  TEST_ASSERT_EQUAL_HEX8(0x08, out[212]);
}

static void test_poll_reply_long_names_truncated() {
  uint8_t out[239];
  const uint8_t ip[4] = {192, 0, 2, 10};
  char longname[100];
  memset(longname, 'L', sizeof(longname) - 1);
  longname[sizeof(longname) - 1] = 0;
  TEST_ASSERT_EQUAL_UINT(239, artnet::build_poll_reply(out, sizeof(out), ip, nullptr, 0, "ABCDEFGHIJKLMNOPQRSTUVWXYZ", longname));
  TEST_ASSERT_EQUAL_UINT(17, strlen((const char *)(out + 26)));
  TEST_ASSERT_EQUAL_UINT(63, strlen((const char *)(out + 44)));
}

void setup() {
  delay(2000);
  UNITY_BEGIN();
  RUN_TEST(test_soundswitch_red_frame);
  RUN_TEST(test_universe_split);
  RUN_TEST(test_classify);
  RUN_TEST(test_malformed_dmx_rejected);
  RUN_TEST(test_short_frame_accepted);
  RUN_TEST(test_poll_reply_layout);
  RUN_TEST(test_poll_reply_long_names_truncated);
  UNITY_END();
}

void loop() {}
