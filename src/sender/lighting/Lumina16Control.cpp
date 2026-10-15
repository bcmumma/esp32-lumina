#include "Lumina16Control.h"

#include "Lumina16.h"
#include "../network/ArtNetReceiver.h"

ControlState control = {false, 1, LUMINA16_BRIGHTNESS_CAP_MAX, false, STROBE_DEFAULT_HZ};

namespace {
lumina16::Params lastParams = {};
bool haveParams = false;
bool lastApplied = false;
uint32_t appliedFrames = 0;
}  // namespace

void control_apply(AudioLedPacket &p, uint32_t nowMs) {
  p.flags = strobe_encode_flags(p.flags, control.strobeEnabled, control.strobeHz);
  lastApplied = false;
  if (!control.artnetControl) return;

  uint16_t length = 0;
  uint32_t lastRxMs = 0;
  const uint8_t *dmx = artnet_listen_dmx(length, lastRxMs);
  if (dmx == nullptr || lastRxMs == 0 || nowMs - lastRxMs > LUMINA16_FRESH_MS) return;

  lumina16::Params params;
  if (!lumina16::decode(dmx, length, control.address, params)) return;
  lumina16::apply(p, params, control.brightnessCap);
  lastParams = params;
  haveParams = true;
  lastApplied = true;
  appliedFrames++;
}

void append_control_json(String &s, uint32_t nowMs) {
  uint16_t length = 0;
  uint32_t lastRxMs = 0;
  artnet_listen_dmx(length, lastRxMs);
  s += "{\"artnetControl\":";
  s += control.artnetControl ? "true" : "false";
  s += ",\"address\":" + String(control.address);
  s += ",\"brightnessCap\":" + String(control.brightnessCap);
  s += ",\"brightnessCapMax\":" + String(LUMINA16_BRIGHTNESS_CAP_MAX);
  s += ",\"strobeEnabled\":";
  s += control.strobeEnabled ? "true" : "false";
  s += ",\"strobeHz\":" + String(control.strobeHz);
  s += ",\"strobeMaxHz\":" + String(STROBE_MAX_HZ);
  s += ",\"artnetAgeMs\":" + String(lastRxMs ? nowMs - lastRxMs : 0xFFFFFFFFUL);
  s += ",\"applying\":";
  s += lastApplied ? "true" : "false";
  s += ",\"appliedPackets\":" + String(appliedFrames);
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
