#include "ControlRouter.h"

#include <Preferences.h>

#include "Lumina16.h"
#include "../config/ConfigManager.h"
#include "../network/ArtNetReceiver.h"

ControlState control = {router::Source::Auto, router::Source::Audio, router::kDefaultStandaloneLevel, STROBE_DEFAULT_HZ,
                        false, 1, LUMINA16_BRIGHTNESS_CAP_MAX};

namespace {

// Separate small record so the main dual-slot config blob is untouched (docs/DECISIONS.md).
constexpr char kNamespace[] = "lumina_ctl";
constexpr char kKey[] = "rec";
constexpr uint32_t kMagic = 0x4C435431UL;  // "LCT1"
constexpr uint8_t kVersion = 1;

struct ControlRecord {
  uint32_t magic;
  uint8_t version;
  uint8_t source;
  uint8_t autoFallback;
  uint8_t strobeHz;
  uint8_t standaloneLevel;
  uint8_t reserved[3];
  uint32_t crc32;
};

uint32_t record_crc(const ControlRecord &r) {
  return config_crc32(reinterpret_cast<const uint8_t *>(&r), offsetof(ControlRecord, crc32));
}

router::Decision lastDecision = {router::Source::Audio, router::Reason::Selected, false, false};
bool haveDecision = false;
uint32_t switches = 0;
lumina16::Params lastParams = {};
bool haveParams = false;
uint32_t appliedPackets = 0;

router::Config router_config() {
  return {control.source, router::sanitize_fallback(control.autoFallback), router::kDefaultTimeoutMs, control.standaloneLevel};
}

}  // namespace

void load_control_settings() {
  Preferences store;
  ControlRecord r = {};
  bool ok = false;
  if (store.begin(kNamespace, true)) {
    ok = store.getBytes(kKey, &r, sizeof(r)) == sizeof(r) && r.magic == kMagic && r.version == kVersion &&
         r.crc32 == record_crc(r) && r.source <= (uint8_t)router::Source::Artnet;
    store.end();
  }
  if (ok) {
    control.source = (router::Source)r.source;
    control.autoFallback = router::sanitize_fallback((router::Source)r.autoFallback);
    control.strobeHz = strobe_clamp_hz(r.strobeHz);
    control.standaloneLevel = r.standaloneLevel;
  }
  control.strobeEnabled = false;  // never restored: strobe is always off at boot
  Serial.printf("Control: source=%s fallback=%s strobe off (cap %u/s) standalone=%u (%s)\n", router::name(control.source),
                router::name(control.autoFallback), control.strobeHz, control.standaloneLevel,
                ok ? "loaded" : "defaults");
}

bool save_control_settings() {
  ControlRecord r = {};
  r.magic = kMagic;
  r.version = kVersion;
  r.source = (uint8_t)control.source;
  r.autoFallback = (uint8_t)router::sanitize_fallback(control.autoFallback);
  r.strobeHz = control.strobeHz;
  r.standaloneLevel = control.standaloneLevel;
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
      lumina16::apply(p, params, control.brightnessCap);
      lastParams = params;
      haveParams = true;
      appliedPackets++;
    }
  }
  if (d.standaloneLevels) router::apply_standalone(p, control.standaloneLevel);
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
  s += ",\"brightnessCapMax\":" + String(LUMINA16_BRIGHTNESS_CAP_MAX);
  s += ",\"strobeEnabled\":";
  s += control.strobeEnabled ? "true" : "false";
  s += ",\"strobeHz\":" + String(control.strobeHz);
  s += ",\"strobeMaxHz\":" + String(STROBE_MAX_HZ);
  if (haveParams) {
    const lumina16::Params &q = lastParams;
    s += ",\"lumina16\":{\"dimmer\":" + String(q.dimmer) + ",\"mode\":" + String(q.mode) + ",\"speed\":" + String(q.speed) +
         ",\"sensitivity\":" + String(q.sensitivity) + ",\"density\":" + String(q.density) + ",\"strobe\":" + String(q.strobe) +
         ",\"direction\":" + String(q.direction) + ",\"variation\":" + String(q.variation) + ",\"hue\":" + String(q.hue) +
         ",\"saturation\":" + String(q.saturation) + ",\"value\":" + String(q.value) +
         ",\"brightness\":" + String(lumina16::brightness(q, control.brightnessCap)) + "}";
  }
  s += "}";
}
