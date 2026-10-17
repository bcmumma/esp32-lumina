// Phase 13 control-source arbitration (docs/ARCHITECTURE.md, docs/DECISIONS.md).
// Pure decision logic: which source drives the strands right now, and why.
//   STANDALONE  effects run with the audio fields held at a steady level
//   AUDIO       today's behaviour: web settings + PCM1808 levels
//   ARTNET      LUMINA-16 from Art-Net; forced mode holds the last frame when it goes stale
//   HYBRID      (Phase 15) the AUDIO look, coloured by SoundSwitch's latest lit colour
//   AUTO        ARTNET while lit frames are fresh; HYBRID for a while after SoundSwitch stops or
//               goes dark; then the fallback (AUDIO or STANDALONE)
#pragma once

#include <stdint.h>

#include "LuminaProtocol.h"

namespace router {

enum class Source : uint8_t { Auto = 0, Standalone = 1, Audio = 2, Artnet = 3, Hybrid = 4 };

enum class Reason : uint8_t {
  Selected = 0,     // the selected source is active as chosen
  AutoArtnetFresh,  // AUTO: Art-Net frames are fresh
  AutoFallback,     // AUTO: no fresh Art-Net, so the fallback source drives
  ArtnetStaleHold,  // forced ARTNET: frames stopped, holding the last one
  ArtnetNoData,     // forced ARTNET: no frame ever received, web settings show
  AutoHybrid,       // AUTO: SoundSwitch stopped or went dark recently, so HYBRID drives
};

constexpr uint32_t kDefaultTimeoutMs = 1000;  // above the 480 ms worst stall (Phase 11)
constexpr uint8_t kDefaultStandaloneLevel = 128;
constexpr uint32_t kDefaultDarkMs = 3000;          // fresh but all-dark frames this long count as stopped
constexpr uint32_t kDefaultHybridHoldMs = 10000;   // HYBRID this long after the last lit frame, then fallback
constexpr uint32_t kNeverLit = 0xFFFFFFFFUL;

struct Config {
  Source selected;
  Source autoFallback;  // Audio or Standalone only
  uint32_t timeoutMs;
  uint8_t standaloneLevel;
  uint32_t darkMs;        // 0 = don't judge darkness (Phase 13 behaviour)
  uint32_t hybridHoldMs;  // 0 = no HYBRID stage in AUTO (Phase 13 behaviour)
};

struct Decision {
  Source active;
  Reason reason;
  bool applyArtnet;       // apply LUMINA-16 from the latest frame
  bool standaloneLevels;  // hold the packet's audio fields at standaloneLevel
  bool hybridColour = false;  // tint the AUDIO look with SoundSwitch's latest lit colour
};

// litAgeMs: time since the last frame with any light in the fixture's channels (kNeverLit if none).
Decision decide(const Config &c, bool haveFrame, uint32_t frameAgeMs, uint32_t litAgeMs);

// AUTO may only fall back to AUDIO or STANDALONE.
Source sanitize_fallback(Source s);

// Overwrites the packet's audio levels with a steady value (STANDALONE).
void apply_standalone(AudioLedPacket &p, uint8_t level);

const char *name(Source s);
const char *reason_name(Reason r);
// Case-insensitive "auto" / "standalone" / "audio" / "artnet" / "hybrid". False if unknown.
bool parse(const char *text, Source &out);

}  // namespace router
