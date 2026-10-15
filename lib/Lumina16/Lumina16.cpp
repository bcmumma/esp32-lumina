#include "Lumina16.h"

namespace lumina16 {

uint8_t effect_mode(uint8_t ch) {
  uint8_t band = (uint8_t)(((uint16_t)ch * kEffectCount) / 256U);
  return kEffectModes[band < kEffectCount ? band : kEffectCount - 1];
}

uint8_t direction(uint8_t ch) {
  if (ch < 85) return 0;
  if (ch < 170) return 1;
  return 2;
}

void rgb_to_hsv(uint8_t r, uint8_t g, uint8_t b, uint8_t &h, uint8_t &s, uint8_t &v) {
  uint8_t mx = r > g ? (r > b ? r : b) : (g > b ? g : b);
  uint8_t mn = r < g ? (r < b ? r : b) : (g < b ? g : b);
  v = mx;
  if (mx == 0 || mx == mn) {  // black or grey/white: no hue
    h = 0;
    s = 0;
    return;
  }
  uint16_t delta = (uint16_t)(mx - mn);
  s = (uint8_t)((delta * 255U + mx / 2U) / mx);
  int32_t hue;
  if (mx == r) {
    hue = 0 + (43 * ((int32_t)g - (int32_t)b)) / (int32_t)delta;
  } else if (mx == g) {
    hue = 86 + (43 * ((int32_t)b - (int32_t)r)) / (int32_t)delta;
  } else {
    hue = 172 + (43 * ((int32_t)r - (int32_t)g)) / (int32_t)delta;
  }
  // The receiver's wheel is 6 regions x 43 = 258 steps, so wrap negatives by 258
  // (magenta -> 215) and clamp the two steps an 8-bit hue can't hold.
  if (hue < 0) hue += 258;
  h = (uint8_t)(hue > 255 ? 255 : hue);
}

bool decode(const uint8_t *dmx, uint16_t length, uint16_t address, Params &out) {
  if (dmx == nullptr || address < 1 || (uint32_t)address - 1U + kChannels > length) return false;
  const uint8_t *c = dmx + (address - 1);
  out.dimmer = c[0];
  out.mode = effect_mode(c[1]);
  out.speed = c[2];
  out.sensitivity = c[3];
  out.density = c[4];
  out.strobe = c[5];
  out.direction = direction(c[6]);
  out.variation = c[7];
  rgb_to_hsv(c[8], c[9], c[10], out.hue, out.saturation, out.value);
  return true;
}

uint8_t brightness(const Params &p, uint8_t cap) {
  uint32_t b = (uint32_t)p.dimmer * p.value / 255U;
  return (uint8_t)(b * cap / 255U);
}

void apply(AudioLedPacket &pkt, const Params &p, uint8_t brightnessCap) {
  const uint8_t b = brightness(p, brightnessCap);
  pkt.globalHue = 0;
  for (uint8_t s = 0; s < MAX_STRANDS; s++) {
    StrandSettings &st = pkt.strand[s];
    st.mode = p.mode;
    st.brightness = b;
    st.hue = p.hue;
    st.saturation = p.saturation;
    st.speed = p.speed;
    st.density = p.density;
    st.sensitivity = p.sensitivity;
    st.strobe = p.strobe;
    st.twinkle = p.variation;
    st.direction = p.direction;
  }
}

}  // namespace lumina16
