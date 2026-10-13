#include "LightingState.h"

LedConfig ledCfg;

void set_default_led_config() {
  // Proven receiver wiring from the completed diagnostics:
  //   Strand 1 -> GPIO33
  //   Strand 2 -> GPIO25
  //   Strand 3 -> GPIO26
  //   Strand 4 -> GPIO27
  // Proven native LED byte order: RGB at 800 kHz.
  // Browser hardware settings are still available for counts, enable flags, and future strip types.
  ledCfg.globalBrightness = 255;
  ledCfg.globalHue = 0;
  ledCfg.activeStrands = MAX_STRANDS;
  ledCfg.maxLedsPerStrand = 300;

  // Start with a neutral RGB calibration map because the receiver's native
  // NeoPixel type now matches the proven RGB byte order. You can still fine-tune
  // channel scales and gamma from the Hardware / Color Calibration section.
  ledCfg.colorMap = COLOR_MAP_RGB;
  ledCfg.redScale = 255;
  ledCfg.greenScale = 255;
  ledCfg.blueScale = 255;
  ledCfg.gamma10 = 10;

  // Fresh installs start in a dependable non-audio mode. Additional strands
  // remain disabled until their LED counts and enable states are configured.
  ledCfg.strand[0] = {1, 9, 220, 165, 255, 0, 0, 0, 0, 0, 0, DEFAULT_LED_PINS[0], LED_TYPE_WS2815_WS2812_RGB_800, 0, DEFAULT_LED_COUNTS[0], 0};
  ledCfg.strand[1] = {0, 15, 210, 190, 255, 48, 118, 0, 175, 24, 0, DEFAULT_LED_PINS[1], LED_TYPE_WS2815_WS2812_RGB_800, 0, DEFAULT_LED_COUNTS[1], 0};
  ledCfg.strand[2] = {0, 16, 220, 165, 255, 62, 48, 0, 205, 170, 0, DEFAULT_LED_PINS[2], LED_TYPE_WS2815_WS2812_RGB_800, 0, DEFAULT_LED_COUNTS[2], 0};
  ledCfg.strand[3] = {0, 17, 230, 200, 255, 118, 76, 0, 185, 8, 0, DEFAULT_LED_PINS[3], LED_TYPE_WS2815_WS2812_RGB_800, 0, DEFAULT_LED_COUNTS[3], 0};
}
