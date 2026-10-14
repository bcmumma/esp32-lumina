#include "ArtNetReceiver.h"

#include <WiFi.h>
#include <WiFiUdp.h>

#include "ArtNet.h"

namespace {

constexpr int kMaxUniverses = 4;     // stats slots; SoundSwitch sends 0 and 1
constexpr int kMaxPacketsPerCall = 16;
constexpr int kRecentFrames = 64;
constexpr int kRecentChannels = 12;  // Lumina fixture: 4 strands x RGB
constexpr uint32_t kActiveWindowMs = 2000;

struct UniverseStats {
  bool used;
  uint16_t universe;
  uint32_t frames;
  uint32_t windowFrames;
  uint32_t fps;
  uint32_t gaps;
  uint8_t lastSeq;
  uint16_t length;
  uint32_t lastRxMs;
  IPAddress source;
};

struct RecentFrame {
  uint8_t seq;
  uint32_t ms;
  uint8_t ch[kRecentChannels];
};

WiFiUDP udp;
bool started = false;
uint8_t rxBuf[600];
uint8_t replyBuf[artnet::kPollReplySize];

UniverseStats stats[kMaxUniverses];
uint8_t dmx[artnet::kMaxDmx];
uint16_t dmxLength = 0;
RecentFrame recent[kRecentFrames];
int recentHead = 0;
int recentCount = 0;

uint32_t pollsSeen = 0;
uint32_t repliesSent = 0;
uint32_t invalidPackets = 0;
uint32_t windowStartMs = 0;
IPAddress lastPoller;

UniverseStats *stats_for(uint16_t universe) {
  for (auto &s : stats) {
    if (s.used && s.universe == universe) return &s;
  }
  for (auto &s : stats) {
    if (!s.used) {
      s = {};
      s.used = true;
      s.universe = universe;
      return &s;
    }
  }
  return nullptr;  // more universes than slots: ignore the extras
}

void answer_poll(const IPAddress &poller) {
  pollsSeen++;
  lastPoller = poller;
  IPAddress local = WiFi.localIP();
  uint8_t ip[4] = {local[0], local[1], local[2], local[3]};
  uint8_t mac[6];
  WiFi.macAddress(mac);
  size_t n = artnet::build_poll_reply(replyBuf, sizeof(replyBuf), ip, mac, ARTNET_LISTEN_UNIVERSE, "Lumina S3",
                                      "Lumina S3 sender - Art-Net receive (parse/report only)");
  // Same delivery as the PC monitor that SoundSwitch accepted: unicast to the poller plus broadcast.
  const IPAddress targets[2] = {poller, IPAddress(255, 255, 255, 255)};
  for (const IPAddress &t : targets) {
    if (udp.beginPacket(t, artnet::kPort)) {
      udp.write(replyBuf, n);
      if (udp.endPacket()) repliesSent++;
    }
  }
}

void take_dmx(const artnet::DmxFrame &f, const IPAddress &source, uint32_t nowMs) {
  UniverseStats *s = stats_for(f.universe);
  if (s == nullptr) return;
  // Art-Net sequence runs 1..255 then wraps to 1; 0 means sequencing is off.
  if (f.sequence != 0 && s->lastSeq != 0 && f.sequence != (uint8_t)(s->lastSeq % 255 + 1)) s->gaps++;
  if (f.sequence != 0) s->lastSeq = f.sequence;
  s->frames++;
  s->windowFrames++;
  s->length = f.length;
  s->lastRxMs = nowMs;
  s->source = source;

  if (f.universe != ARTNET_LISTEN_UNIVERSE) return;
  dmxLength = f.length;
  memcpy(dmx, f.data, f.length);
  if (f.length < artnet::kMaxDmx) memset(dmx + f.length, 0, artnet::kMaxDmx - f.length);

  RecentFrame &r = recent[recentHead];
  r.seq = f.sequence;
  r.ms = nowMs;
  for (int i = 0; i < kRecentChannels; i++) r.ch[i] = dmx[i];
  recentHead = (recentHead + 1) % kRecentFrames;
  if (recentCount < kRecentFrames) recentCount++;
}

void print_status(uint32_t nowMs) {
  for (const auto &s : stats) {
    if (!s.used || s.universe != ARTNET_LISTEN_UNIVERSE || nowMs - s.lastRxMs > kActiveWindowMs) continue;
    Serial.printf("artnet univ=%u fps=%lu gaps=%lu len=%u seq=%u src=%s polls=%lu ch1-12=[%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u]\n",
                  s.universe, (unsigned long)s.fps, (unsigned long)s.gaps, s.length, s.lastSeq,
                  s.source.toString().c_str(), (unsigned long)pollsSeen, dmx[0], dmx[1], dmx[2], dmx[3], dmx[4],
                  dmx[5], dmx[6], dmx[7], dmx[8], dmx[9], dmx[10], dmx[11]);
  }
}

}  // namespace

void setup_artnet() {
  started = udp.begin(artnet::kPort) == 1;
  Serial.printf("Art-Net receive on UDP %u: %s (listen universe %u, advertises %s)\n", artnet::kPort,
                started ? "OK" : "FAILED", ARTNET_LISTEN_UNIVERSE, WiFi.localIP().toString().c_str());
}

void artnet_service(uint32_t nowMs) {
  if (!started) return;

  for (int i = 0; i < kMaxPacketsPerCall; i++) {
    int size = udp.parsePacket();
    if (size <= 0) break;
    IPAddress source = udp.remoteIP();
    int n = udp.read(rxBuf, sizeof(rxBuf));
    if (n <= 0) continue;
    switch (artnet::classify(rxBuf, (size_t)n)) {
      case artnet::PacketType::Poll:
        answer_poll(source);
        break;
      case artnet::PacketType::Dmx: {
        artnet::DmxFrame f;
        if (artnet::parse_dmx(rxBuf, (size_t)n, f)) take_dmx(f, source, nowMs);
        else invalidPackets++;
        break;
      }
      case artnet::PacketType::Other:
        break;
      case artnet::PacketType::Invalid:
        invalidPackets++;
        break;
    }
  }

  if (nowMs - windowStartMs >= 1000) {
    for (auto &s : stats) {
      if (!s.used) continue;
      s.fps = s.windowFrames;
      s.windowFrames = 0;
    }
    windowStartMs = nowMs;
    print_status(nowMs);
  }
}

void append_artnet_json(String &s, int channels) {
  uint32_t now = millis();
  if (channels < 0) channels = 0;
  if (channels > (int)artnet::kMaxDmx) channels = artnet::kMaxDmx;

  s += "{\"listening\":";
  s += started ? "true" : "false";
  s += ",\"listenUniverse\":" + String(ARTNET_LISTEN_UNIVERSE);
  s += ",\"advertisedIp\":\"" + WiFi.localIP().toString() + "\"";
  s += ",\"polls\":" + String(pollsSeen);
  s += ",\"repliesSent\":" + String(repliesSent);
  s += ",\"invalid\":" + String(invalidPackets);
  s += ",\"lastPoller\":\"" + lastPoller.toString() + "\"";
  s += ",\"nowMs\":" + String(now);
  s += ",\"universes\":[";
  bool first = true;
  for (const auto &u : stats) {
    if (!u.used) continue;
    if (!first) s += ",";
    first = false;
    s += "{\"u\":" + String(u.universe) + ",\"frames\":" + String(u.frames) + ",\"fps\":" + String(u.fps) +
         ",\"gaps\":" + String(u.gaps) + ",\"len\":" + String(u.length) + ",\"seq\":" + String(u.lastSeq) +
         ",\"ageMs\":" + String(now - u.lastRxMs) + ",\"src\":\"" + u.source.toString() + "\"}";
  }
  s += "],\"length\":" + String(dmxLength) + ",\"ch\":[";
  for (int i = 0; i < channels; i++) {
    if (i) s += ",";
    s += String(dmx[i]);
  }
  s += "],\"recent\":[";
  for (int i = 0; i < recentCount; i++) {
    const RecentFrame &r = recent[(recentHead - recentCount + i + kRecentFrames) % kRecentFrames];
    if (i) s += ",";
    s += "[" + String(r.seq) + "," + String(r.ms);
    for (int c = 0; c < kRecentChannels; c++) s += "," + String(r.ch[c]);
    s += "]";
  }
  s += "]}";
}
