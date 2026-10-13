// Lumina ESP-NOW protocol shared by the S3 sender and the WROOM receiver.
// Both boards must compile against this one definition (packet version 12).
#pragma once

#include <stdint.h>

static constexpr uint16_t PACKET_MAGIC = 0xA17D;
static constexpr uint8_t PACKET_VERSION = 12;
static constexpr uint8_t MAX_STRANDS = 4;

enum LedType : uint8_t {
  LED_TYPE_WS2815_WS2812_RGB_800 = 0,
  LED_TYPE_WS2811_RGB_800 = 1,
  LED_TYPE_WS2811_GRB_800 = 2,
  LED_TYPE_WS2811_RGB_400 = 3,
  LED_TYPE_SK6812_RGB_GRB = 4,
  LED_TYPE_SK6812_RGBW_GRBW = 5,
  LED_TYPE_SK6812_RGBW_RGBW = 6
};

// Logical RGB -> native strip Color(r,g,b) argument order.
// This is for color calibration without touching LED wiring.
enum ColorMap : uint8_t {
  COLOR_MAP_RGB = 0,
  COLOR_MAP_RBG = 1,
  COLOR_MAP_GRB = 2,
  COLOR_MAP_GBR = 3,
  COLOR_MAP_BRG = 4,
  COLOR_MAP_BGR = 5
};

struct __attribute__((packed)) StrandSettings {
  uint8_t enabled;      // 0/1
  uint8_t mode;         // LED pattern number
  uint8_t brightness;   // strand brightness 0-255
  uint8_t hue;          // base hue 0-255
  uint8_t saturation;   // saturation 0-255
  uint8_t speed;        // animation speed 0-255
  uint8_t density;      // twinkle/fill/density 0-255
  uint8_t sensitivity;  // how much audio affects this strand 0-255
  uint8_t strobe;       // strobe strength/decay 0-255
  uint8_t twinkle;      // twinkle/glitter amount 0-255
  uint8_t direction;    // 0 forward, 1 reverse, 2 mirror/center
  uint8_t pin;          // LED receiver GPIO pin, assigned from browser
  uint8_t ledType;      // browser-selected LED chipset/color order
  uint8_t reserved8;    // keeps following uint16_t on an even byte boundary
  uint16_t ledCount;    // active LED count for this strand, assigned from browser
  uint16_t reserved;    // future use
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
  uint8_t beat;       // 255 during beat hold, else 0
  uint8_t clip;
  uint8_t beatPulse;  // increments once for every detected beat
  uint8_t globalBrightness;
  uint8_t globalHue;
  uint8_t activeStrands; // how many strand slots the receiver should use, 1-4
  uint8_t flags;         // bit0=onset candidate, bit1=tempo-assisted candidate
  uint16_t maxLedsPerStrand; // browser-set safety/config cap

  // LED color calibration. The receiver applies this before writing pixels.
  uint8_t colorMap;      // 0 RGB, 1 RBG, 2 GRB, 3 GBR, 4 BRG, 5 BGR
  uint8_t redScale;      // 0-255 logical red correction
  uint8_t greenScale;    // 0-255 logical green correction
  uint8_t blueScale;     // 0-255 logical blue correction
  uint8_t gamma10;       // 10=1.0, 22=2.2, etc.

  // Beat diagnostics used by the receiver's Beat Tracker LED mode.
  // These replace three previously-unused reserved bytes, so packet layout
  // remains compact while PACKET_VERSION distinguishes the new semantics.
  uint8_t beatConfidence; // 0-255, adaptive onset score
  uint8_t beatPhase;      // 0-255, phase from one accepted beat to the next
  uint8_t beatBpm;        // integer BPM, 0 when tempo is not locked

  StrandSettings strand[MAX_STRANDS];
  uint16_t reserved;
};

static_assert(sizeof(AudioLedPacket) < 250, "AudioLedPacket must fit in an ESP-NOW v1 payload");
static_assert(sizeof(StrandSettings) == 18, "StrandSettings layout changed; update PACKET_VERSION on both boards");
static_assert(sizeof(AudioLedPacket) == 107, "AudioLedPacket layout changed; update PACKET_VERSION on both boards");
