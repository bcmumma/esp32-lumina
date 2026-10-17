#include "SourceRouter.h"

#include <ctype.h>
#include <string.h>

namespace router {

Source sanitize_fallback(Source s) { return s == Source::Standalone ? Source::Standalone : Source::Audio; }

Decision decide(const Config &c, bool haveFrame, uint32_t frameAgeMs, uint32_t litAgeMs) {
  const bool fresh = haveFrame && frameAgeMs <= c.timeoutMs;
  const bool litRecently = c.darkMs == 0 || litAgeMs <= c.darkMs;
  switch (c.selected) {
    case Source::Standalone:
      return {Source::Standalone, Reason::Selected, false, true};
    case Source::Audio:
      return {Source::Audio, Reason::Selected, false, false};
    case Source::Hybrid:
      return {Source::Hybrid, Reason::Selected, false, false, true};
    case Source::Artnet:
      if (!haveFrame) return {Source::Artnet, Reason::ArtnetNoData, false, false};
      return {Source::Artnet, fresh ? Reason::Selected : Reason::ArtnetStaleHold, true, false};
    case Source::Auto:
    default: {
      if (fresh && litRecently) return {Source::Artnet, Reason::AutoArtnetFresh, true, false};
      if (c.hybridHoldMs && litAgeMs != kNeverLit && litAgeMs <= c.hybridHoldMs)
        return {Source::Hybrid, Reason::AutoHybrid, false, false, true};
      Source fb = sanitize_fallback(c.autoFallback);
      return {fb, Reason::AutoFallback, false, fb == Source::Standalone};
    }
  }
}

void apply_standalone(AudioLedPacket &p, uint8_t level) {
  p.volume = level;
  p.bass = level;
  p.mid = level;
  p.treble = level;
  p.left = level;
  p.right = level;
  p.beat = 0;
  p.clip = 0;
}

const char *name(Source s) {
  switch (s) {
    case Source::Standalone: return "standalone";
    case Source::Audio: return "audio";
    case Source::Artnet: return "artnet";
    case Source::Hybrid: return "hybrid";
    case Source::Auto:
    default: return "auto";
  }
}

const char *reason_name(Reason r) {
  switch (r) {
    case Reason::AutoArtnetFresh: return "auto: art-net fresh";
    case Reason::AutoFallback: return "auto: no fresh art-net, fallback";
    case Reason::ArtnetStaleHold: return "artnet: stale, holding last frame";
    case Reason::ArtnetNoData: return "artnet: no frame yet";
    case Reason::AutoHybrid: return "auto: soundswitch stopped or dark, hybrid";
    case Reason::Selected:
    default: return "selected";
  }
}

bool parse(const char *text, Source &out) {
  if (text == nullptr) return false;
  char lower[16];
  size_t n = 0;
  for (; text[n] && n < sizeof(lower) - 1; n++) lower[n] = (char)tolower((unsigned char)text[n]);
  lower[n] = 0;
  if (strcmp(lower, "auto") == 0) { out = Source::Auto; return true; }
  if (strcmp(lower, "standalone") == 0) { out = Source::Standalone; return true; }
  if (strcmp(lower, "audio") == 0) { out = Source::Audio; return true; }
  if (strcmp(lower, "artnet") == 0) { out = Source::Artnet; return true; }
  if (strcmp(lower, "hybrid") == 0) { out = Source::Hybrid; return true; }
  return false;
}

}  // namespace router
