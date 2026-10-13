// NeoPixel strands (proven pins 33/25/26/27, RGB 800 kHz), color calibration, and the boot LED test.
#pragma once

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "LuminaProtocol.h"
#include "Color.h"

// Packet supports up to four LED strands. The sender webpage Hardware tab
// provides per-strand pin/count/type. A hard cap remains here for safety.
static constexpr uint16_t SAFETY_MAX_LEDS_PER_STRAND = 1000;
static constexpr bool BOOT_TEST = true;

static constexpr uint8_t DEFAULT_LED_PINS[MAX_STRANDS] = { 33, 25, 26, 27 };
static constexpr uint16_t DEFAULT_LED_COUNTS[MAX_STRANDS] = { 10, 0, 0, 0 };
static constexpr uint8_t DEFAULT_TYPE[MAX_STRANDS] = {
  LED_TYPE_WS2815_WS2812_RGB_800,
  LED_TYPE_WS2815_WS2812_RGB_800,
  LED_TYPE_WS2815_WS2812_RGB_800,
  LED_TYPE_WS2815_WS2812_RGB_800
};

extern Adafruit_NeoPixel * strips[MAX_STRANDS];
extern bool configured[MAX_STRANDS];
extern uint8_t configuredPin[MAX_STRANDS];
extern uint16_t configuredCount[MAX_STRANDS];
extern uint8_t configuredType[MAX_STRANDS];
extern uint8_t activeColorMap;
extern uint8_t activeRedScale;
extern uint8_t activeGreenScale;
extern uint8_t activeBlueScale;
extern uint8_t activeGamma10;
extern uint8_t calibrationLutR[256];
extern uint8_t calibrationLutG[256];
extern uint8_t calibrationLutB[256];

uint8_t build_corrected_value(uint8_t v, uint8_t scale, uint8_t gamma10);
void rebuild_color_luts();
void update_color_calibration(const AudioLedPacket &p);
Rgb apply_color_calibration(Rgb c);
neoPixelType pixel_type_from_id(uint8_t ledType);
const char * pixel_type_name(uint8_t ledType);
bool usable_pin(uint8_t pin);
void disable_strand(uint8_t s);
void configure_strand(uint8_t s, uint8_t pin, uint16_t count, uint8_t ledType);
void apply_hardware_from_packet(const AudioLedPacket &p);
uint32_t make_color(uint8_t s, Rgb c);
Rgb read_color(uint8_t s, uint16_t i);
uint16_t count_for(uint8_t s);
void set_pixel_native(uint8_t s, uint16_t i, Rgb c);
void set_pixel(uint8_t s, uint16_t i, Rgb c);
void add_pixel(uint8_t s, uint16_t i, Rgb c);
void fill_strand(uint8_t s, Rgb c);
void clear_strand(uint8_t s);
void fade_strand(uint8_t s, uint8_t amount);
void show_all();
void boot_test();
