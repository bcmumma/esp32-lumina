// Art-Net 4 packet handling (pure C++, no networking): ArtDmx decode and ArtPollReply build.
// Measured SoundSwitch behavior (docs/ARCHITECTURE.md): it unicasts ArtDmx only to nodes
// that answer ArtPoll, at the IP the node advertises in its ArtPollReply.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace artnet {

constexpr uint16_t kPort = 6454;
constexpr uint16_t kOpPoll = 0x2000;
constexpr uint16_t kOpPollReply = 0x2100;
constexpr uint16_t kOpDmx = 0x5000;
constexpr uint8_t kMinProtocolVersion = 14;
constexpr size_t kHeaderSize = 18;        // ArtDmx bytes before the DMX data
constexpr size_t kMaxDmx = 512;
constexpr size_t kPollReplySize = 239;

enum class PacketType : uint8_t { Invalid, Poll, Dmx, Other };

struct DmxFrame {
  uint16_t universe;     // 15-bit Port-Address: Net(7) << 8 | SubUni(8)
  uint8_t sequence;      // 0 = sequencing disabled
  uint8_t physical;
  uint16_t length;       // number of DMX channels in data
  const uint8_t *data;   // points into the caller's buffer
};

// Art-Net ID and opcode check only. Returns Invalid for non-Art-Net data.
PacketType classify(const uint8_t *buf, size_t len);

// Validates and decodes an ArtDmx packet. False if anything is malformed.
bool parse_dmx(const uint8_t *buf, size_t len, DmxFrame &out);

// Builds a 239-byte ArtPollReply advertising one DMX output port on `universe`.
// Returns the number of bytes written, or 0 if `cap` is too small.
size_t build_poll_reply(uint8_t *out, size_t cap, const uint8_t ip[4], const uint8_t mac[6], uint16_t universe,
                        const char *short_name, const char *long_name);

}  // namespace artnet
