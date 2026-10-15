// Phase 13 control-source arbitration (docs/ARCHITECTURE.md, docs/DECISIONS.md).
// Pure decision logic: which source drives the strands right now, and why.
//   STANDALONE  effects run with the audio fields held at a steady level
//   AUDIO       today's behaviour: web settings + PCM1808 levels
//   ARTNET      LUMINA-16 from Art-Net; forced mode holds the last frame when it goes stale
//   AUTO        ARTNET while frames are fresh, otherwise the fallback (AUDIO or STANDALONE)
#pragma once

#include <stdint.h>

#include "LuminaProtocol.h"

namespace router {

enum class Source : uint8_t { Auto = 0, Standalone = 1, Audio = 2, Artnet = 3 };

enum class Reason : uint8_t {
  Selected = 0,     // the selected source is active as chosen
  AutoArtnetFresh,  // AUTO: Art-Net frames are fresh
  AutoFallback,     // AUTO: no fresh Art-Net, so the fallback source drives
  ArtnetStaleHold,  // forced ARTNET: frames stopped, holding the last one
  ArtnetNoData,     // forced ARTNET: no frame ever received, web settings show
};

constexpr uint32_t kDefaultTimeoutMs = 1000;  // above the 480 ms worst stall (Phase 11)
constexpr uint8_t kDefaultStandaloneLevel = 128;

struct Config {
  Source selected;
  Source autoFallback;  // Audio or Standalone only
  uint32_t timeoutMs;
  uint8_t standaloneLevel;
};

struct Decision {
  Source active;
  Reason reason;
  bool applyArtnet;       // apply LUMINA-16 from the latest frame
  bool standaloneLevels;  // hold the packet's audio fields at standaloneLevel
};

Decision decide(const Config &c, bool haveFrame, uint32_t frameAgeMs);

// AUTO may only fall back to AUDIO or STANDALONE.
Source sanitize_fallback(Source s);

// Overwrites the packet's audio levels with a steady value (STANDALONE).
void apply_standalone(AudioLedPacket &p, uint8_t level);

const char *name(Source s);
const char *reason_name(Reason r);
// Case-insensitive "auto" / "standalone" / "audio" / "artnet". False if unknown.
bool parse(const char *text, Source &out);

}  // namespace router
