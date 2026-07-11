// V13: Dynamic multi-strand LED receiver with live color calibration
// Starts with your known-working setup by default:
//   Strand 1: GPIO25, 10 LEDs, NEO_GRB + NEO_KHZ800
// Then accepts pin/count/type updates from the sender webpage Hardware tab.

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_idf_version.h>
#include <esp_system.h>
#include <Adafruit_NeoPixel.h>

// ============================================================
// Receiver Wi-Fi channel
// Set this to the channel printed by the sender Serial Monitor.
// Example: if sender says "Wi-Fi channel: 6", use 6 here.
// ============================================================
static constexpr uint8_t WIFI_CHANNEL = 1;

// Packet supports up to three LED strands. The sender webpage Hardware tab
// provides per-strand pin/count/type. A hard cap remains here for safety.
static constexpr uint8_t MAX_STRANDS = 3;
static constexpr uint16_t SAFETY_MAX_LEDS_PER_STRAND = 1000;

// Debug options
static constexpr bool BOOT_TEST = true;
static constexpr bool FORCE_VISIBLE_IF_BAD_SETTINGS = true;
static constexpr uint8_t SAFE_BRIGHTNESS = 220;

// ============================================================
// Packet format - must match sender.
// This packet layout must match the v13 sender. Packet version 8.
// ============================================================
static constexpr uint16_t PACKET_MAGIC = 0xA17D;
static constexpr uint8_t PACKET_VERSION = 8;

enum LedType : uint8_t {
  LED_TYPE_WS2812B_GRB = 0,
  LED_TYPE_WS2811_RGB_800 = 1,
  LED_TYPE_WS2811_GRB_800 = 2,
  LED_TYPE_WS2811_RGB_400 = 3,
  LED_TYPE_SK6812_RGB_GRB = 4,
  LED_TYPE_SK6812_RGBW_GRBW = 5,
  LED_TYPE_SK6812_RGBW_RGBW = 6
};

enum ColorMap : uint8_t {
  COLOR_MAP_RGB = 0,
  COLOR_MAP_RBG = 1,
  COLOR_MAP_GRB = 2,
  COLOR_MAP_GBR = 3,
  COLOR_MAP_BRG = 4,
  COLOR_MAP_BGR = 5
};

struct __attribute__((packed)) StrandSettings {
  uint8_t enabled;
  uint8_t mode;
  uint8_t brightness;
  uint8_t hue;
  uint8_t saturation;
  uint8_t speed;
  uint8_t density;
  uint8_t sensitivity;
  uint8_t strobe;
  uint8_t twinkle;
  uint8_t direction;
  uint8_t pin;
  uint8_t ledType;
  uint8_t reserved8;
  uint16_t ledCount;
  uint16_t reserved;
};

struct __attribute__((packed)) AudioLedPacket {
  uint16_t magic;
  uint8_t version;
  uint8_t size;

  uint16_t sequence;
  uint32_t ms;

  uint8_t volume;
  uint8_t bass;
  uint8_t mid;
  uint8_t treble;
  uint8_t left;
  uint8_t right;
  uint8_t beat;
  uint8_t clip;
  uint8_t beatPulse;
  uint8_t globalBrightness;
  uint8_t globalHue;
  uint8_t activeStrands;
  uint8_t flags;
  uint16_t maxLedsPerStrand;

  uint8_t colorMap;
  uint8_t redScale;
  uint8_t greenScale;
  uint8_t blueScale;
  uint8_t gamma10;
  uint8_t colorReserved[3];

  StrandSettings strand[MAX_STRANDS];
  uint16_t reserved;
};

// ============================================================
// State
// ============================================================
portMUX_TYPE packetMux = portMUX_INITIALIZER_UNLOCKED;
AudioLedPacket latestPacket = {};
uint8_t latestMac[6] = {};
volatile bool havePacket = false;
volatile uint32_t goodPackets = 0;
volatile uint32_t badPackets = 0;

uint16_t lastSequence = 0;
uint32_t lostPackets = 0;
uint32_t lastPacketMs = 0;
uint8_t lastBeatPulse = 0;
uint8_t beatFlash = 0;

// Runtime LED hardware. Each strand is allocated/reallocated only when the
// webpage hardware settings change. Defaults match your proven GPIO25 test.
Adafruit_NeoPixel *strips[MAX_STRANDS] = { nullptr, nullptr, nullptr };
bool configured[MAX_STRANDS] = { false, false, false };
uint8_t configuredPin[MAX_STRANDS] = { 255, 255, 255 };
uint16_t configuredCount[MAX_STRANDS] = { 0, 0, 0 };
uint8_t configuredType[MAX_STRANDS] = { LED_TYPE_WS2812B_GRB, LED_TYPE_WS2812B_GRB, LED_TYPE_WS2812B_GRB };

static constexpr uint8_t DEFAULT_PIN[MAX_STRANDS] = { 25, 26, 27 };
static constexpr uint16_t DEFAULT_COUNT[MAX_STRANDS] = { 10, 0, 0 };
static constexpr uint8_t DEFAULT_TYPE[MAX_STRANDS] = {
  LED_TYPE_WS2812B_GRB,
  LED_TYPE_WS2812B_GRB,
  LED_TYPE_WS2812B_GRB
};

// Current color calibration received from sender.
uint8_t activeColorMap = COLOR_MAP_RBG;
uint8_t activeRedScale = 255;
uint8_t activeGreenScale = 255;
uint8_t activeBlueScale = 255;
uint8_t activeGamma10 = 10;

struct StrandRuntime {
  uint16_t pos;
  uint8_t flash;
  uint8_t sparkle;
};

StrandRuntime rt[MAX_STRANDS];

// ============================================================
// Color helpers
// ============================================================
struct Rgb {
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

static uint8_t qadd8(uint8_t a, uint8_t b) {
  uint16_t s = (uint16_t)a + (uint16_t)b;
  return s > 255 ? 255 : (uint8_t)s;
}

static uint8_t scale8(uint8_t v, uint8_t scale) {
  return ((uint16_t)v * (uint16_t)scale) >> 8;
}

static uint8_t scale8_video(uint8_t v, uint8_t scale) {
  uint16_t out = ((uint16_t)v * (uint16_t)scale) >> 8;
  if (v && scale) out++;
  return out > 255 ? 255 : (uint8_t)out;
}

static Rgb hsv_to_rgb(uint8_t h, uint8_t s, uint8_t v) {
  if (s == 0) return {v, v, v};

  uint8_t region = h / 43;
  uint8_t remainder = (h - region * 43) * 6;

  uint8_t p = (v * (255 - s)) >> 8;
  uint8_t q = (v * (255 - ((s * remainder) >> 8))) >> 8;
  uint8_t t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;

  switch (region) {
    case 0: return {v, t, p};
    case 1: return {q, v, p};
    case 2: return {p, v, t};
    case 3: return {p, q, v};
    case 4: return {t, p, v};
    default: return {v, p, q};
  }
}

static Rgb scale_color(Rgb c, uint8_t scale) {
  c.r = scale8_video(c.r, scale);
  c.g = scale8_video(c.g, scale);
  c.b = scale8_video(c.b, scale);
  return c;
}

static Rgb add_color(Rgb a, Rgb b) {
  return {qadd8(a.r, b.r), qadd8(a.g, b.g), qadd8(a.b, b.b)};
}

static uint8_t correct_channel(uint8_t v, uint8_t scale, uint8_t gamma10) {
  float x = (float)v / 255.0f;
  float gamma = (float)gamma10 / 10.0f;
  if (gamma < 0.5f) gamma = 1.0f;
  float y = powf(x, gamma) * 255.0f;
  y = y * ((float)scale / 255.0f);
  if (y < 0.0f) return 0;
  if (y > 255.0f) return 255;
  return (uint8_t)(y + 0.5f);
}

static Rgb apply_color_calibration(Rgb c) {
  Rgb scaled = {
    correct_channel(c.r, activeRedScale, activeGamma10),
    correct_channel(c.g, activeGreenScale, activeGamma10),
    correct_channel(c.b, activeBlueScale, activeGamma10)
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

static neoPixelType pixel_type_from_id(uint8_t ledType) {
  switch (ledType) {
    case LED_TYPE_WS2811_RGB_800: return NEO_RGB + NEO_KHZ800;
    case LED_TYPE_WS2811_GRB_800: return NEO_GRB + NEO_KHZ800;
    case LED_TYPE_WS2811_RGB_400: return NEO_RGB + NEO_KHZ400;
    case LED_TYPE_SK6812_RGB_GRB: return NEO_GRB + NEO_KHZ800;
    case LED_TYPE_SK6812_RGBW_GRBW: return NEO_GRBW + NEO_KHZ800;
    case LED_TYPE_SK6812_RGBW_RGBW: return NEO_RGBW + NEO_KHZ800;
    case LED_TYPE_WS2812B_GRB:
    default: return NEO_GRB + NEO_KHZ800;
  }
}

static const char *pixel_type_name(uint8_t ledType) {
  switch (ledType) {
    case LED_TYPE_WS2811_RGB_800: return "WS2811 RGB 800k";
    case LED_TYPE_WS2811_GRB_800: return "WS2811 GRB 800k";
    case LED_TYPE_WS2811_RGB_400: return "WS2811 RGB 400k";
    case LED_TYPE_SK6812_RGB_GRB: return "SK6812 RGB GRB";
    case LED_TYPE_SK6812_RGBW_GRBW: return "SK6812 RGBW GRBW";
    case LED_TYPE_SK6812_RGBW_RGBW: return "SK6812 RGBW RGBW";
    case LED_TYPE_WS2812B_GRB:
    default: return "WS2812B/WS2815 GRB 800k";
  }
}

static bool usable_pin(uint8_t pin) {
  // Keep this permissive because ESP32 board variants expose different pins.
  // Avoid classic UART0 pins 1/3 and input-only pins 34-39 for LED data.
  if (pin == 1 || pin == 3) return false;
  if (pin >= 34 && pin <= 39) return false;
  return pin <= 48;
}

static void disable_strand(uint8_t s) {
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

static void configure_strand(uint8_t s, uint8_t pin, uint16_t count, uint8_t ledType) {
  if (s >= MAX_STRANDS) return;
  if (count > SAFETY_MAX_LEDS_PER_STRAND) count = SAFETY_MAX_LEDS_PER_STRAND;
  if (ledType > LED_TYPE_SK6812_RGBW_RGBW) ledType = LED_TYPE_WS2812B_GRB;

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

static void apply_hardware_from_packet(const AudioLedPacket &p) {
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

static uint32_t make_color(uint8_t s, Rgb c) {
  if (s >= MAX_STRANDS || !strips[s]) return 0;
  Rgb native = apply_color_calibration(c);
  return strips[s]->Color(native.r, native.g, native.b);
}

static Rgb read_color(uint8_t s, uint16_t i) {
  if (s >= MAX_STRANDS || !strips[s] || i >= configuredCount[s]) return {0, 0, 0};
  uint32_t c = strips[s]->getPixelColor(i);
  return {
    (uint8_t)((c >> 16) & 0xFF),
    (uint8_t)((c >> 8) & 0xFF),
    (uint8_t)(c & 0xFF)
  };
}

static uint16_t count_for(uint8_t s) {
  if (s >= MAX_STRANDS) return 0;
  if (!configured[s] || !strips[s]) return 0;
  return configuredCount[s];
}

static void set_pixel(uint8_t s, uint16_t i, Rgb c) {
  if (s >= MAX_STRANDS || !strips[s]) return;
  if (i >= count_for(s)) return;
  strips[s]->setPixelColor(i, make_color(s, c));
}

static void add_pixel(uint8_t s, uint16_t i, Rgb c) {
  if (s >= MAX_STRANDS || !strips[s]) return;
  if (i >= count_for(s)) return;
  Rgb old = read_color(s, i);
  strips[s]->setPixelColor(i, make_color(s, add_color(old, c)));
}

static void fill_strand(uint8_t s, Rgb c) {
  uint16_t n = count_for(s);
  for (uint16_t i = 0; i < n; i++) set_pixel(s, i, c);
}

static void clear_strand(uint8_t s) {
  if (s >= MAX_STRANDS || !strips[s]) return;
  strips[s]->clear();
}

static void fade_strand(uint8_t s, uint8_t amount) {
  uint16_t n = count_for(s);
  uint8_t keep = 255 - amount;
  for (uint16_t i = 0; i < n; i++) {
    set_pixel(s, i, scale_color(read_color(s, i), keep));
  }
}

static void show_all() {
  for (uint8_t s = 0; s < MAX_STRANDS; s++) {
    if (configured[s] && strips[s] && count_for(s) > 0) strips[s]->show();
  }
}

static uint8_t combined_brightness(const AudioLedPacket &p, const StrandSettings &st) {
  uint8_t gb = p.globalBrightness;
  uint8_t sb = st.brightness;
  if (FORCE_VISIBLE_IF_BAD_SETTINGS) {
    if (gb == 0) gb = SAFE_BRIGHTNESS;
    if (sb == 0) sb = SAFE_BRIGHTNESS;
  }
  return scale8_video(sb, gb);
}

static Rgb strand_color(const AudioLedPacket &p, const StrandSettings &st, uint8_t offset = 0, uint8_t value = 255) {
  return hsv_to_rgb(st.hue + p.globalHue + offset, st.saturation, value);
}

static uint8_t scaled_audio(uint8_t v, uint8_t sensitivity) {
  uint16_t x = ((uint16_t)v * (uint16_t)(sensitivity + 32)) / 160;
  return x > 255 ? 255 : (uint8_t)x;
}

// ============================================================
// Render modes
// Mode list from sender:
// 0 Off, 1 Space Pulse, 2 Beat Strobe, 3 Bass Comet, 4 Twinkle,
// 5 Spectrum, 6 VU, 7 Glitter Kick, 8 Void Scanner, 9 Solid Color,
// 10 Debug Chase
// ============================================================
static void render_off(uint8_t s) {
  clear_strand(s);
}

static void render_solid(uint8_t s, const AudioLedPacket &p, const StrandSettings &st) {
  uint8_t b = combined_brightness(p, st);
  fill_strand(s, strand_color(p, st, 0, b));
}

static void render_debug_chase(uint8_t s, const AudioLedPacket &p) {
  // Debug chase uses logical blue; Color Calibration maps it to your strip.
  // If this moves, ESP-NOW packets are controlling the LEDs.
  uint16_t n = count_for(s);
  if (n == 0) return;

  clear_strand(s);
  uint16_t pos = (p.sequence / 2) % n;

  Rgb blue = {0, 0, 255};
  Rgb dimBlue = {0, 0, 32};

  for (uint16_t i = 0; i < n; i++) set_pixel(s, i, dimBlue);
  set_pixel(s, pos, blue);
  if (pos > 0) set_pixel(s, pos - 1, {0, 0, 120});
}

static void render_space_pulse(uint8_t s, const AudioLedPacket &p, const StrandSettings &st) {
  uint8_t audio = scaled_audio(p.volume, st.sensitivity);
  uint8_t b = scale8_video(combined_brightness(p, st), max<uint8_t>(30, audio));
  fill_strand(s, strand_color(p, st, 0, b));
}

static void render_beat_strobe(uint8_t s, const AudioLedPacket &p, const StrandSettings &st, bool newBeat) {
  if (newBeat) rt[s].flash = 255;
  fade_strand(s, st.strobe > 0 ? st.strobe : 80);
  if (rt[s].flash > 10) {
    fill_strand(s, strand_color(p, st, 0, combined_brightness(p, st)));
    rt[s].flash = scale8(rt[s].flash, 150);
  }
}

static void render_bass_comet(uint8_t s, const AudioLedPacket &p, const StrandSettings &st) {
  uint16_t n = count_for(s);
  if (n == 0) return;
  fade_strand(s, 55);

  uint8_t step = max<uint8_t>(1, st.speed / 45);
  rt[s].pos = (rt[s].pos + step) % n;

  uint8_t b = scale8_video(combined_brightness(p, st), max<uint8_t>(60, scaled_audio(p.bass, st.sensitivity)));
  Rgb c = strand_color(p, st, 0, b);

  for (uint8_t t = 0; t < 6; t++) {
    int idx = (int)rt[s].pos - t;
    if (idx < 0) idx += n;
    add_pixel(s, (uint16_t)idx, scale_color(c, 255 - t * 35));
  }
}

static void render_twinkle(uint8_t s, const AudioLedPacket &p, const StrandSettings &st, bool newBeat) {
  fade_strand(s, 25);
  uint16_t n = count_for(s);
  if (n == 0) return;

  uint8_t chance = map(st.twinkle, 0, 255, 1, 18);
  if (newBeat) chance = min<uint8_t>(40, chance + 18);

  for (uint8_t k = 0; k < chance; k++) {
    uint16_t idx = random(n);
    uint8_t v = random(80, 255);
    add_pixel(s, idx, strand_color(p, st, random(0, 80), scale8_video(v, combined_brightness(p, st))));
  }
}

static void render_spectrum(uint8_t s, const AudioLedPacket &p, const StrandSettings &st) {
  uint16_t n = count_for(s);
  if (n == 0) return;

  uint16_t third = max<uint16_t>(1, n / 3);
  uint8_t bassB = scale8_video(combined_brightness(p, st), scaled_audio(p.bass, st.sensitivity));
  uint8_t midB = scale8_video(combined_brightness(p, st), scaled_audio(p.mid, st.sensitivity));
  uint8_t trebleB = scale8_video(combined_brightness(p, st), scaled_audio(p.treble, st.sensitivity));

  for (uint16_t i = 0; i < n; i++) {
    if (i < third) set_pixel(s, i, strand_color(p, st, 0, bassB));
    else if (i < third * 2) set_pixel(s, i, strand_color(p, st, 70, midB));
    else set_pixel(s, i, strand_color(p, st, 145, trebleB));
  }
}

static void render_vu(uint8_t s, const AudioLedPacket &p, const StrandSettings &st) {
  uint16_t n = count_for(s);
  if (n == 0) return;
  clear_strand(s);

  uint8_t audio = scaled_audio(p.volume, st.sensitivity);
  uint16_t lit = ((uint32_t)audio * n) / 255;
  uint8_t b = combined_brightness(p, st);

  for (uint16_t i = 0; i < lit && i < n; i++) {
    uint8_t offset = (uint8_t)((uint32_t)i * 110 / max<uint16_t>(1, n - 1));
    set_pixel(s, i, strand_color(p, st, offset, b));
  }
}

static void render_glitter_kick(uint8_t s, const AudioLedPacket &p, const StrandSettings &st, bool newBeat) {
  fade_strand(s, 40);
  uint16_t n = count_for(s);
  if (n == 0) return;

  uint8_t sparkles = st.density / 28;
  if (newBeat) sparkles += 16;

  for (uint8_t k = 0; k < sparkles; k++) {
    add_pixel(s, random(n), strand_color(p, st, random(0, 180), combined_brightness(p, st)));
  }
}

static void render_scanner(uint8_t s, const AudioLedPacket &p, const StrandSettings &st) {
  uint16_t n = count_for(s);
  if (n == 0) return;
  fade_strand(s, 35);

  uint8_t step = max<uint8_t>(1, st.speed / 50);
  rt[s].pos = (rt[s].pos + step) % (n * 2);
  int pos = rt[s].pos;
  if (pos >= n) pos = (n * 2 - 1) - pos;

  add_pixel(s, pos, strand_color(p, st, 0, combined_brightness(p, st)));
}

static void render_strand(uint8_t s, const AudioLedPacket &p, bool newBeat) {
  if (s >= MAX_STRANDS || !configured[s] || !strips[s] || count_for(s) == 0) return;

  StrandSettings st = p.strand[s];

  activeColorMap = (p.colorMap <= 5) ? p.colorMap : COLOR_MAP_RGB;
  activeRedScale = p.redScale;
  activeGreenScale = p.greenScale;
  activeBlueScale = p.blueScale;
  activeGamma10 = (p.gamma10 >= 10 && p.gamma10 <= 30) ? p.gamma10 : 10;

  // Hardware is configured separately by apply_hardware_from_packet().
  // Rendering uses the currently configured local strip.
  st.ledCount = count_for(s);

  if (FORCE_VISIBLE_IF_BAD_SETTINGS) {
    if (st.enabled == 0) st.enabled = 1;
    if (st.brightness == 0) st.brightness = SAFE_BRIGHTNESS;
    if (st.saturation == 0) st.saturation = 255;
    if (st.mode == 0) st.mode = 10;
  }

  if (!st.enabled) {
    render_off(s);
    return;
  }

  switch (st.mode) {
    case 0: render_off(s); break;
    case 1: render_space_pulse(s, p, st); break;
    case 2: render_beat_strobe(s, p, st, newBeat); break;
    case 3: render_bass_comet(s, p, st); break;
    case 4: render_twinkle(s, p, st, newBeat); break;
    case 5: render_spectrum(s, p, st); break;
    case 6: render_vu(s, p, st); break;
    case 7: render_glitter_kick(s, p, st, newBeat); break;
    case 8: render_scanner(s, p, st); break;
    case 9: render_solid(s, p, st); break;
    case 10: render_debug_chase(s, p); break;
    default: render_debug_chase(s, p); break;
  }
}

// ============================================================
// ESP-NOW
// ============================================================
static void copy_packet_from_callback(const uint8_t *mac, const uint8_t *data, int len) {
  if (len != sizeof(AudioLedPacket)) {
    badPackets++;
    return;
  }

  AudioLedPacket p = {};
  memcpy(&p, data, sizeof(p));

  if (p.magic != PACKET_MAGIC || p.version != PACKET_VERSION || p.size != sizeof(AudioLedPacket)) {
    badPackets++;
    return;
  }

  portENTER_CRITICAL(&packetMux);
  latestPacket = p;
  memcpy(latestMac, mac, 6);
  havePacket = true;
  goodPackets++;
  portEXIT_CRITICAL(&packetMux);
}

#if ESP_IDF_VERSION_MAJOR >= 5
void on_data_recv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  copy_packet_from_callback(info->src_addr, data, len);
}
#else
void on_data_recv(const uint8_t *mac, const uint8_t *data, int len) {
  copy_packet_from_callback(mac, data, len);
}
#endif

static void setup_wifi_espnow() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  WiFi.setSleep(false);
  esp_wifi_set_ps(WIFI_PS_NONE);
  esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);

  Serial.print("LED receiver MAC: ");
  Serial.println(WiFi.macAddress());
  Serial.print("Listening on Wi-Fi channel: ");
  Serial.println(WIFI_CHANNEL);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    while (true) delay(1000);
  }

  if (esp_now_register_recv_cb(on_data_recv) != ESP_OK) {
    Serial.println("ESP-NOW receive callback registration failed");
    while (true) delay(1000);
  }
}

// ============================================================
// Setup / loop
// ============================================================
static void setup_leds() {
  for (uint8_t s = 0; s < MAX_STRANDS; s++) {
    rt[s] = {};
    configure_strand(s, DEFAULT_PIN[s], DEFAULT_COUNT[s], DEFAULT_TYPE[s]);
  }
}

static void boot_test() {
  if (!BOOT_TEST) return;
  if (!configured[0] || !strips[0]) {
    Serial.println("Boot LED test skipped: Strand 1 is not configured.");
    return;
  }

  Serial.println("Boot LED test: exact known-working command on Strand 1");
  Serial.println("This should light GPIO25 / 10 LEDs the same way your short test did.");

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

void setup() {
  Serial.begin(115200);
  delay(1500);

  Serial.println();
  Serial.println("ESP32 LED Receiver V13 - dynamic 3-strand config + color calibration");
  Serial.print("Expected packet size: ");
  Serial.println(sizeof(AudioLedPacket));
  Serial.println("Default LED setup: Strand 1 GPIO25, 10 LEDs, NEO_GRB + NEO_KHZ800. Hardware tab can reconfigure pins/counts/types.");

  randomSeed((uint32_t)esp_random());
  setup_leds();
  boot_test();
  setup_wifi_espnow();

  Serial.println("Waiting for ESP-NOW packets...");
}

void loop() {
  AudioLedPacket p = {};
  bool got = false;
  uint32_t good = 0;
  uint32_t bad = 0;

  portENTER_CRITICAL(&packetMux);
  if (havePacket) {
    p = latestPacket;
    got = true;
    good = goodPackets;
    bad = badPackets;
  }
  portEXIT_CRITICAL(&packetMux);

  if (got) {
    uint32_t now = millis();
    if (lastPacketMs == 0) lastPacketMs = now;

    if (lastSequence != 0 && p.sequence != (uint16_t)(lastSequence + 1)) {
      lostPackets += (uint16_t)(p.sequence - (uint16_t)(lastSequence + 1));
    }
    lastSequence = p.sequence;
    lastPacketMs = now;

    // Apply browser-controlled hardware before rendering. This only reallocates
    // a strand when pin/count/type actually changes.
    apply_hardware_from_packet(p);

    bool newBeat = (p.beatPulse != lastBeatPulse);
    if (newBeat) {
      lastBeatPulse = p.beatPulse;
      beatFlash = 255;
    }

    for (uint8_t s = 0; s < MAX_STRANDS; s++) {
      render_strand(s, p, newBeat);
    }
    show_all();

    if (beatFlash > 0) beatFlash = scale8(beatFlash, 185);
  } else {
    delay(20);
  }

  if (lastPacketMs > 0 && millis() - lastPacketMs > 1500) {
    // If packets stop, dim out instead of staying frozen forever.
    for (uint8_t s = 0; s < MAX_STRANDS; s++) fade_strand(s, 20);
    show_all();
  }

  static uint32_t lastPrint = 0;
  if (millis() - lastPrint > 1000) {
    lastPrint = millis();
    if (got) {
      Serial.printf(
        "rx good=%lu bad=%lu lost=%lu seq=%u vol=%3u bass=%3u mid=%3u treb=%3u beat=%3u pulse=%3u gB=%3u modes=[%u,%u,%u] br=[%u,%u,%u] en=[%u,%u,%u] pins=[%u,%u,%u] counts=[%u,%u,%u] cmap=%u rgbScale=[%u,%u,%u] gamma10=%u\n",
        (unsigned long)good,
        (unsigned long)bad,
        (unsigned long)lostPackets,
        p.sequence,
        p.volume,
        p.bass,
        p.mid,
        p.treble,
        p.beat,
        p.beatPulse,
        p.globalBrightness,
        p.strand[0].mode, p.strand[1].mode, p.strand[2].mode,
        p.strand[0].brightness, p.strand[1].brightness, p.strand[2].brightness,
        p.strand[0].enabled, p.strand[1].enabled, p.strand[2].enabled,
        configuredPin[0], configuredPin[1], configuredPin[2],
        configuredCount[0], configuredCount[1], configuredCount[2],
        p.colorMap,
        p.redScale,
        p.greenScale,
        p.blueScale,
        p.gamma10
      );
    } else {
      Serial.println("No packets yet. Boot test should already have lit default GPIO25/10 LEDs.");
    }
  }

  delay(10);
}
