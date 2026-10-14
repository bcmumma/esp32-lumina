#include "ArtNet.h"

#include <string.h>

namespace artnet {

static const uint8_t kId[8] = {'A', 'r', 't', '-', 'N', 'e', 't', 0};

static uint16_t read_le16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint16_t read_be16(const uint8_t *p) { return (uint16_t)((p[0] << 8) | p[1]); }

PacketType classify(const uint8_t *buf, size_t len) {
  if (buf == nullptr || len < 10 || memcmp(buf, kId, sizeof(kId)) != 0) return PacketType::Invalid;
  switch (read_le16(buf + 8)) {
    case kOpPoll: return PacketType::Poll;
    case kOpDmx: return PacketType::Dmx;
    default: return PacketType::Other;
  }
}

bool parse_dmx(const uint8_t *buf, size_t len, DmxFrame &out) {
  if (classify(buf, len) != PacketType::Dmx || len < kHeaderSize) return false;
  if (read_be16(buf + 10) < kMinProtocolVersion) return false;
  const uint16_t length = read_be16(buf + 16);
  if (length == 0 || length > kMaxDmx || len < kHeaderSize + length) return false;
  out.sequence = buf[12];
  out.physical = buf[13];
  out.universe = (uint16_t)(((buf[15] & 0x7F) << 8) | buf[14]);
  out.length = length;
  out.data = buf + kHeaderSize;
  return true;
}

static void put_text(uint8_t *dst, size_t field, const char *text) {
  memset(dst, 0, field);
  if (text == nullptr) return;
  size_t n = strlen(text);
  if (n > field - 1) n = field - 1;  // keep a terminating NUL
  memcpy(dst, text, n);
}

size_t build_poll_reply(uint8_t *out, size_t cap, const uint8_t ip[4], const uint8_t mac[6], uint16_t universe,
                        const char *short_name, const char *long_name) {
  if (out == nullptr || cap < kPollReplySize) return 0;
  uint8_t *p = out;
  memset(p, 0, kPollReplySize);
  memcpy(p + 0, kId, 8);
  p[8] = kOpPollReply & 0xFF;            // OpCode, little-endian
  p[9] = kOpPollReply >> 8;
  memcpy(p + 10, ip, 4);                 // IP Address
  p[14] = kPort & 0xFF;                  // Port, little-endian
  p[15] = kPort >> 8;
  p[16] = 0;                             // VersInfoH
  p[17] = 1;                             // VersInfoL
  p[18] = (universe >> 8) & 0x7F;        // NetSwitch
  p[19] = (universe >> 4) & 0x0F;        // SubSwitch
  p[20] = 0x00;                          // OemHi
  p[21] = 0xFF;                          // Oem: 0x00FF = unknown
  p[22] = 0;                             // UBEA version
  p[23] = 0xD0;                          // Status1: indicators normal
  p[24] = 0;                             // EstaManLo
  p[25] = 0;                             // EstaManHi
  put_text(p + 26, 18, short_name);      // ShortName
  put_text(p + 44, 64, long_name);       // LongName
  put_text(p + 108, 64, "#0001 [0000] OK");  // NodeReport
  p[172] = 0;                            // NumPortsHi
  p[173] = 1;                            // NumPortsLo
  p[174] = 0x80;                         // PortTypes[0]: outputs DMX512 from Art-Net
  p[182] = 0x80;                         // GoodOutputA[0]: data being output
  p[190] = universe & 0x0F;              // SwOut[0]
  p[200] = 0x00;                         // Style: StNode
  if (mac != nullptr) memcpy(p + 201, mac, 6);  // MAC
  memcpy(p + 207, ip, 4);                // BindIp
  p[211] = 1;                            // BindIndex
  p[212] = 0x08;                         // Status2: supports 15-bit Port-Address
  return kPollReplySize;
}

}  // namespace artnet
