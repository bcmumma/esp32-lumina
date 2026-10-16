#include "ControlRouter.h"

#include <Preferences.h>

#include "Lumina16.h"
#include "../config/ConfigManager.h"
#include "../network/ArtNetReceiver.h"

ControlState control = {router::Source::Auto, router::Source::Audio, router::kDefaultStandaloneLevel, STROBE_DEFAULT_HZ,
                        false, 1, 0, RIG_BRIGHTNESS_CAP_MAX};

namespace {

// Separate small record so the main dual-slot config blob is untouched (docs/DECISIONS.md).
constexpr char kNamespace[] = "lumina_ctl";
constexpr char kKey[] = "rec";
constexpr uint32_t kMagic = 0x4C435431UL;  // "LCT1"
constexpr uint8_t kVersion = 2;  // v2 adds brightnessCap in a byte that v1 left zero; v1 still loads

struct ControlRecord {
  uint32_t magic;
  uint8_t version;
  uint8_t source;
  uint8_t autoFallback;
  uint8_t strobeHz;
  uint8_t standaloneLevel;
  uint8_t brightnessCap;
  uint8_t kickFlash;  // v2 reserved byte, 0 in older records = off
  uint8_t reserved[1];
  uint32_t crc32;
};

uint8_t clamp_cap(int v) {
  return (uint8_t)(v < RIG_BRIGHTNESS_CAP_MIN ? RIG_BRIGHTNESS_CAP_MIN : (v > RIG_BRIGHTNESS_CAP_MAX ? RIG_BRIGHTNESS_CAP_MAX : v));
}

uint32_t record_crc(const ControlRecord &r) {
  return config_crc32(reinterpret_cast<const uint8_t *>(&r), offsetof(ControlRecord, crc32));
}

router::Decision lastDecision = {router::Source::Audio, router::Reason::Selected, false, false};
bool haveDecision = false;
uint32_t switches = 0;
lumina16::Params lastParams = {};
bool haveParams = false;
uint32_t appliedPackets = 0;
uint8_t heldBeatPulse = 0;  // bass-hit pulse is frozen while audio isn't the active source

router::Config router_config() {
  return {control.source, router::sanitize_fallback(control.autoFallback), router::kDefaultTimeoutMs, control.standaloneLevel};
}

}  // namespace

void load_control_settings() {
  Preferences store;
  ControlRecord r = {};
  bool ok = false;
  if (store.begin(kNamespace, true)) {
    ok = store.getBytes(kKey, &r, sizeof(r)) == sizeof(r) && r.magic == kMagic && (r.version == 1 || r.version == kVersion) &&
         r.crc32 == record_crc(r) && r.source <= (uint8_t)router::Source::Artnet;
    store.end();
  }
  if (ok) {
    control.source = (router::Source)r.source;
    control.autoFallback = router::sanitize_fallback((router::Source)r.autoFallback);
    control.strobeHz = strobe_clamp_hz(r.strobeHz);
    control.standaloneLevel = r.standaloneLevel;
    control.brightnessCap = r.version >= 2 ? clamp_cap(r.brightnessCap) : RIG_BRIGHTNESS_CAP_MAX;
    control.kickFlash = r.version >= 2 ? r.kickFlash : 0;
  }
  control.strobeEnabled = false;  // never restored: strobe is always off at boot
  Serial.printf("Control: source=%s fallback=%s strobe off (cap %u/s) standalone=%u brightnessCap=%u/255 (%s)\n",
                router::name(control.source), router::name(control.autoFallback), control.strobeHz,
                control.standaloneLevel, control.brightnessCap, ok ? "loaded" : "defaults");
}

bool save_control_settings() {
  ControlRecord r = {};
  r.magic = kMagic;
  r.version = kVersion;
  r.source = (uint8_t)control.source;
  r.autoFallback = (uint8_t)router::sanitize_fallback(control.autoFallback);
  r.strobeHz = control.strobeHz;
  r.standaloneLevel = control.standaloneLevel;
  r.brightnessCap = clamp_cap(control.brightnessCap);
  r.kickFlash = control.kickFlash;
  r.crc32 = record_crc(r);
  Preferences store;
  if (!store.begin(kNamespace, false)) return false;
  bool ok = store.putBytes(kKey, &r, sizeof(r)) == sizeof(r);
  store.end();
  return ok;
}

void control_apply(AudioLedPacket &p, uint32_t nowMs) {
  p.flags = strobe_encode_flags(p.flags, control.strobeEnabled, control.strobeHz);

  uint16_t length = 0;
  uint32_t lastRxMs = 0;
  const uint8_t *dmx = artnet_listen_dmx(length, lastRxMs);
  const bool haveFrame = dmx != nullptr && lastRxMs != 0;
  const router::Decision d = router::decide(router_config(), haveFrame, haveFrame ? nowMs - lastRxMs : 0);

  if (haveDecision && (d.active != lastDecision.active || d.reason != lastDecision.reason)) {
    switches++;
    Serial.printf("Source: %s -> active %s (%s)\n", router::name(control.source), router::name(d.active),
                  router::reason_name(d.reason));
  }
  lastDecision = d;
  haveDecision = true;

  if (d.applyArtnet) {
    lumina16::Params params;
    if (lumina16::decode(dmx, length, control.address, params)) {
      lumina16::apply(p, params, 255);  // full dimmer range; the rig cap below limits it
      lastParams = params;
      haveParams = true;
      appliedPackets++;
    }
  }
  if (d.standaloneLevels) router::apply_standalone(p, control.standaloneLevel);

  // Bass hits only drive the lights when audio is the active source.
  if (d.active == router::Source::Audio) {
    heldBeatPulse = p.beatPulse;
  } else {
    p.beatPulse = heldBeatPulse;
    p.beat = 0;
  }

  // Rig brightness cap (power budget), after every source.
  const uint8_t cap = clamp_cap(control.brightnessCap);
  if (p.globalBrightness > cap) p.globalBrightness = cap;

  p.kickFlash = control.kickFlash;  // receiver flashes it only on bass hits, through the strobe gate
}

void append_control_json(String &s, uint32_t nowMs) {
  uint16_t length = 0;
  uint32_t lastRxMs = 0;
  artnet_listen_dmx(length, lastRxMs);
  s += "{\"source\":\"" + String(router::name(control.source)) + "\"";
  s += ",\"active\":\"" + String(router::name(lastDecision.active)) + "\"";
  s += ",\"reason\":\"" + String(router::reason_name(lastDecision.reason)) + "\"";
  s += ",\"autoFallback\":\"" + String(router::name(control.autoFallback)) + "\"";
  s += ",\"timeoutMs\":" + String(router::kDefaultTimeoutMs);
  s += ",\"standaloneLevel\":" + String(control.standaloneLevel);
  s += ",\"switches\":" + String(switches);
  s += ",\"artnetAgeMs\":" + String(lastRxMs ? nowMs - lastRxMs : 0xFFFFFFFFUL);
  s += ",\"applyingArtnet\":";
  s += lastDecision.applyArtnet ? "true" : "false";
  s += ",\"appliedPackets\":" + String(appliedPackets);
  s += ",\"address\":" + String(control.address);
  s += ",\"brightnessCap\":" + String(control.brightnessCap);
  s += ",\"brightnessCapMin\":" + String(RIG_BRIGHTNESS_CAP_MIN);
  s += ",\"brightnessCapMax\":" + String(RIG_BRIGHTNESS_CAP_MAX);
  s += ",\"strobeEnabled\":";
  s += control.strobeEnabled ? "true" : "false";
  s += ",\"strobeHz\":" + String(control.strobeHz);
  s += ",\"strobeMaxHz\":" + String(STROBE_MAX_HZ);
  s += ",\"kickFlash\":" + String(control.kickFlash);
  if (haveParams) {
    const lumina16::Params &q = lastParams;
    s += ",\"lumina16\":{\"dimmer\":" + String(q.dimmer) + ",\"mode\":" + String(q.mode) + ",\"speed\":" + String(q.speed) +
         ",\"sensitivity\":" + String(q.sensitivity) + ",\"density\":" + String(q.density) + ",\"strobe\":" + String(q.strobe) +
         ",\"direction\":" + String(q.direction) + ",\"variation\":" + String(q.variation) + ",\"hue\":" + String(q.hue) +
         ",\"saturation\":" + String(q.saturation) + ",\"value\":" + String(q.value) +
         ",\"brightness\":" + String(lumina16::brightness(q, 255)) + "}";
  }
  s += "}";
}
