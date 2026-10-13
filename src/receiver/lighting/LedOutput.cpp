#include "LedOutput.h"
#include "EffectState.h"

// Runtime LED hardware. Each strand is allocated/reallocated only when the
// webpage hardware settings change. Default data pins are GPIO33, GPIO25, GPIO26, and GPIO27.
Adafruit_NeoPixel *strips[MAX_STRANDS] = { nullptr, nullptr, nullptr, nullptr };
bool configured[MAX_STRANDS] = { false, false, false, false };
uint8_t configuredPin[MAX_STRANDS] = { 255, 255, 255, 255 };
uint16_t configuredCount[MAX_STRANDS] = { 0, 0, 0, 0 };
uint8_t configuredType[MAX_STRANDS] = { LED_TYPE_WS2815_WS2812_RGB_800, LED_TYPE_WS2815_WS2812_RGB_800, LED_TYPE_WS2815_WS2812_RGB_800, LED_TYPE_WS2815_WS2812_RGB_800 };

// Current color calibration received from sender.
uint8_t activeColorMap = COLOR_MAP_RGB;
uint8_t activeRedScale = 255;
uint8_t activeGreenScale = 255;
uint8_t activeBlueScale = 255;
uint8_t activeGamma10 = 10;
uint8_t calibrationLutR[256] = {};
uint8_t calibrationLutG[256] = {};
uint8_t calibrationLutB[256] = {};

uint8_t build_corrected_value(uint8_t v, uint8_t scale, uint8_t gamma10) {
  float x = (float)v / 255.0f;
  float gamma = (float)gamma10 / 10.0f;
  if (gamma < 0.5f) gamma = 1.0f;
  float y = powf(x, gamma) * 255.0f;
  y = y * ((float)scale / 255.0f);
  if (y < 0.0f) return 0;
  if (y > 255.0f) return 255;
  return (uint8_t)(y + 0.5f);
}

void rebuild_color_luts() {
  for (int i = 0; i < 256; i++) {
    calibrationLutR[i] = build_corrected_value((uint8_t)i, activeRedScale, activeGamma10);
    calibrationLutG[i] = build_corrected_value((uint8_t)i, activeGreenScale, activeGamma10);
    calibrationLutB[i] = build_corrected_value((uint8_t)i, activeBlueScale, activeGamma10);
  }
}

void update_color_calibration(const AudioLedPacket &p) {
  uint8_t newMap = (p.colorMap <= 5) ? p.colorMap : COLOR_MAP_RGB;
  uint8_t newGamma = (p.gamma10 >= 10 && p.gamma10 <= 30) ? p.gamma10 : 10;
  bool changed = newMap != activeColorMap ||
                 p.redScale != activeRedScale ||
                 p.greenScale != activeGreenScale ||
                 p.blueScale != activeBlueScale ||
                 newGamma != activeGamma10;

  activeColorMap = newMap;
  activeRedScale = p.redScale;
  activeGreenScale = p.greenScale;
  activeBlueScale = p.blueScale;
  activeGamma10 = newGamma;

  if (changed) rebuild_color_luts();
}

Rgb apply_color_calibration(Rgb c) {
  Rgb scaled = {
    calibrationLutR[c.r],
    calibrationLutG[c.g],
    calibrationLutB[c.b]
  };

  switch (activeColorMap) {
    case COLOR_MAP_RBG: return {scaled.r, scaled.b, scaled.g};
    case COLOR_MAP_GRB: return {scaled.g, scaled.r, scaled.b};
    case COLOR_MAP_GBR: return {scaled.g, scaled.b, scaled.r};
    case COLOR_MAP_BRG: return {scaled.b, scaled.r, scaled.g};
    case COLOR_MAP_BGR: return {scaled.b, scaled.g, scaled.r};
    case COLOR_MAP_RGB:
    default: return scaled;
  }
}

neoPixelType pixel_type_from_id(uint8_t ledType) {
  switch (ledType) {
    case LED_TYPE_WS2811_RGB_800: return NEO_RGB + NEO_KHZ800;
    case LED_TYPE_WS2811_GRB_800: return NEO_GRB + NEO_KHZ800;
    case LED_TYPE_WS2811_RGB_400: return NEO_RGB + NEO_KHZ400;
    case LED_TYPE_SK6812_RGB_GRB: return NEO_GRB + NEO_KHZ800;
    case LED_TYPE_SK6812_RGBW_GRBW: return NEO_GRBW + NEO_KHZ800;
    case LED_TYPE_SK6812_RGBW_RGBW: return NEO_RGBW + NEO_KHZ800;
    case LED_TYPE_WS2815_WS2812_RGB_800:
    default: return NEO_RGB + NEO_KHZ800;
  }
}

const char *pixel_type_name(uint8_t ledType) {
  switch (ledType) {
    case LED_TYPE_WS2811_RGB_800: return "WS2811 RGB 800k";
    case LED_TYPE_WS2811_GRB_800: return "WS2811 GRB 800k";
    case LED_TYPE_WS2811_RGB_400: return "WS2811 RGB 400k";
    case LED_TYPE_SK6812_RGB_GRB: return "SK6812 RGB GRB";
    case LED_TYPE_SK6812_RGBW_GRBW: return "SK6812 RGBW GRBW";
    case LED_TYPE_SK6812_RGBW_RGBW: return "SK6812 RGBW RGBW";
    case LED_TYPE_WS2815_WS2812_RGB_800:
    default: return "WS2815/WS2812 RGB 800k";
  }
}

bool usable_pin(uint8_t pin) {
  // Keep this permissive because ESP32 board variants expose different pins.
  // Avoid classic UART0 pins 1/3 and input-only pins 34-39 for LED data.
  if (pin == 1 || pin == 3) return false;
  if (pin >= 34 && pin <= 39) return false;
  return pin <= 48;
}

void disable_strand(uint8_t s) {
  if (s >= MAX_STRANDS) return;
  if (strips[s]) {
    strips[s]->clear();
    strips[s]->show();
    delete strips[s];
    strips[s] = nullptr;
  }
  configured[s] = false;
  configuredCount[s] = 0;
}

void configure_strand(uint8_t s, uint8_t pin, uint16_t count, uint8_t ledType) {
  if (s >= MAX_STRANDS) return;
  if (count > SAFETY_MAX_LEDS_PER_STRAND) count = SAFETY_MAX_LEDS_PER_STRAND;
  if (ledType > LED_TYPE_SK6812_RGBW_RGBW) ledType = LED_TYPE_WS2815_WS2812_RGB_800;

  if (count == 0 || !usable_pin(pin)) {
    if (configured[s]) Serial.printf("Strand %u disabled: pin=%u count=%u\n", s + 1, pin, count);
    disable_strand(s);
    configuredPin[s] = pin;
    configuredType[s] = ledType;
    return;
  }

  if (configured[s] && strips[s] && configuredPin[s] == pin && configuredCount[s] == count && configuredType[s] == ledType) {
    return;
  }

  disable_strand(s);

  strips[s] = new Adafruit_NeoPixel(count, pin, pixel_type_from_id(ledType));
  if (!strips[s]) {
    Serial.printf("Strand %u allocation failed: pin=%u count=%u\n", s + 1, pin, count);
    configured[s] = false;
    configuredCount[s] = 0;
    return;
  }

  strips[s]->begin();
  strips[s]->setBrightness(255);
  strips[s]->clear();
  strips[s]->show();

  configured[s] = true;
  configuredPin[s] = pin;
  configuredCount[s] = count;
  configuredType[s] = ledType;
  rt[s] = {};

  Serial.printf("Strand %u configured: pin=%u count=%u type=%s\n", s + 1, pin, count, pixel_type_name(ledType));
}

void apply_hardware_from_packet(const AudioLedPacket &p) {
  uint8_t active = p.activeStrands;
  if (active < 1) active = 1;
  if (active > MAX_STRANDS) active = MAX_STRANDS;

  uint16_t packetCap = p.maxLedsPerStrand;
  if (packetCap < 1) packetCap = 1;
  if (packetCap > SAFETY_MAX_LEDS_PER_STRAND) packetCap = SAFETY_MAX_LEDS_PER_STRAND;

  for (uint8_t s = 0; s < MAX_STRANDS; s++) {
    uint16_t wantedCount = p.strand[s].ledCount;
    if (wantedCount > packetCap) wantedCount = packetCap;

    if (s >= active || !p.strand[s].enabled || wantedCount == 0) {
      configure_strand(s, p.strand[s].pin, 0, p.strand[s].ledType);
    } else {
      configure_strand(s, p.strand[s].pin, wantedCount, p.strand[s].ledType);
    }
  }
}

uint32_t make_color(uint8_t s, Rgb c) {
  if (s >= MAX_STRANDS || !strips[s]) return 0;
  Rgb native = apply_color_calibration(c);
  return strips[s]->Color(native.r, native.g, native.b);
}

Rgb read_color(uint8_t s, uint16_t i) {
  if (s >= MAX_STRANDS || !strips[s] || i >= configuredCount[s]) return {0, 0, 0};
  uint32_t c = strips[s]->getPixelColor(i);
  return {
    (uint8_t)((c >> 16) & 0xFF),
    (uint8_t)((c >> 8) & 0xFF),
    (uint8_t)(c & 0xFF)
  };
}

uint16_t count_for(uint8_t s) {
  if (s >= MAX_STRANDS) return 0;
  if (!configured[s] || !strips[s]) return 0;
  return configuredCount[s];
}

void set_pixel_native(uint8_t s, uint16_t i, Rgb c) {
  if (s >= MAX_STRANDS || !strips[s]) return;
  if (i >= count_for(s)) return;
  strips[s]->setPixelColor(i, strips[s]->Color(c.r, c.g, c.b));
}

void set_pixel(uint8_t s, uint16_t i, Rgb c) {
  if (s >= MAX_STRANDS || !strips[s]) return;
  if (i >= count_for(s)) return;
  set_pixel_native(s, i, apply_color_calibration(c));
}

void add_pixel(uint8_t s, uint16_t i, Rgb c) {
  if (s >= MAX_STRANDS || !strips[s]) return;
  if (i >= count_for(s)) return;
  Rgb oldNative = read_color(s, i);
  Rgb addNative = apply_color_calibration(c);
  set_pixel_native(s, i, add_color(oldNative, addNative));
}

void fill_strand(uint8_t s, Rgb c) {
  uint16_t n = count_for(s);
  for (uint16_t i = 0; i < n; i++) set_pixel(s, i, c);
}

void clear_strand(uint8_t s) {
  if (s >= MAX_STRANDS || !strips[s]) return;
  strips[s]->clear();
}

void fade_strand(uint8_t s, uint8_t amount) {
  uint16_t n = count_for(s);
  uint8_t keep = 255 - amount;
  for (uint16_t i = 0; i < n; i++) {
    set_pixel_native(s, i, scale_color(read_color(s, i), keep));
  }
}

void show_all() {
  for (uint8_t s = 0; s < MAX_STRANDS; s++) {
    if (configured[s] && strips[s] && count_for(s) > 0) strips[s]->show();
  }
}

void boot_test() {
  if (!BOOT_TEST) return;
  if (!configured[0] || !strips[0]) {
    Serial.println("Boot LED test skipped: Strand 1 is not configured.");
    return;
  }

  Serial.println("Boot LED test: exact known-working command on Strand 1");
  Serial.println("This should light Strand 1 on GPIO33 / 10 LEDs using RGB 800kHz.");

  for (uint16_t i = 0; i < configuredCount[0]; i++) {
    strips[0]->setPixelColor(i, strips[0]->Color(0, 255, 0));
  }
  strips[0]->show();
  delay(1000);

  for (uint16_t i = 0; i < configuredCount[0]; i++) {
    strips[0]->setPixelColor(i, strips[0]->Color(255, 255, 255));
  }
  strips[0]->show();
  delay(600);

  strips[0]->clear();
  strips[0]->show();
}
