// Phase 7 bench diagnostic for the WROOM receiver. Phase 8 replaces this with the v2 port.
// 1. Checks each strand data pin electrically (pull-up/pull-down and drive read-back).
// 2. Cycles the first few LEDs of strand 1 through red, green, blue, white at low brightness.
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <driver/gpio.h>
#include <esp_chip_info.h>
#include <esp_idf_version.h>

static const uint8_t STRAND_PINS[4] = {33, 25, 26, 27};  // proven order, see docs/HARDWARE.md
static const uint16_t TEST_LEDS = 10;                     // the known-working strand 1 boot test count
static const uint8_t TEST_LEVEL = 24;                     // out of 255; keeps current low on an unverified PSU

// Proven config: RGB order at 800 kHz.
static Adafruit_NeoPixel strand1(TEST_LEDS, STRAND_PINS[0], NEO_RGB + NEO_KHZ800);

static bool check_pin(uint8_t pin) {
  gpio_num_t g = (gpio_num_t)pin;
  gpio_reset_pin(g);

  pinMode(pin, INPUT_PULLUP);
  delay(20);
  int pullUp = digitalRead(pin);
  pinMode(pin, INPUT_PULLDOWN);
  delay(20);
  int pullDown = digitalRead(pin);

  gpio_reset_pin(g);
  gpio_set_pull_mode(g, GPIO_FLOATING);
  gpio_set_direction(g, GPIO_MODE_INPUT_OUTPUT);
  gpio_set_level(g, 1);
  delayMicroseconds(50);
  int driveHigh = gpio_get_level(g);
  gpio_set_level(g, 0);
  delayMicroseconds(50);
  int driveLow = gpio_get_level(g);

  gpio_reset_pin(g);

  bool ok = pullUp == 1 && pullDown == 0 && driveHigh == 1 && driveLow == 0;
  Serial.printf("LUMINA-P7 pin=%u pullup=%d pulldown=%d drive1=%d drive0=%d result=%s\n", pin, pullUp, pullDown,
                driveHigh, driveLow, ok ? "PASS" : "FAIL");
  return ok;
}

void setup() {
  Serial.begin(115200);
  delay(500);

  esp_chip_info_t info;
  esp_chip_info(&info);

  Serial.println();
  Serial.println("LUMINA-P7 receiver diag");
  Serial.printf("LUMINA-P7 chip=%s rev=%u cores=%u cpu=%luMHz\n", ESP.getChipModel(), ESP.getChipRevision(),
                info.cores, (unsigned long)ESP.getCpuFreqMHz());
  Serial.printf("LUMINA-P7 flash=%luMB psram=%luKB arduino=%s idf=%s\n",
                (unsigned long)(ESP.getFlashChipSize() / (1024 * 1024)), (unsigned long)(ESP.getPsramSize() / 1024),
                ESP_ARDUINO_VERSION_STR, esp_get_idf_version());

  int passed = 0;
  for (uint8_t pin : STRAND_PINS) passed += check_pin(pin) ? 1 : 0;
  Serial.printf("LUMINA-P7 pins passed=%d/4\n", passed);

  strand1.begin();
  strand1.clear();
  strand1.show();
}

void loop() {
  static const struct {
    const char *name;
    uint8_t r, g, b;
  } steps[] = {
    {"red", TEST_LEVEL, 0, 0},
    {"green", 0, TEST_LEVEL, 0},
    {"blue", 0, 0, TEST_LEVEL},
    {"white", TEST_LEVEL, TEST_LEVEL, TEST_LEVEL},
    {"off", 0, 0, 0},
  };
  static uint8_t step = 0;
  static uint32_t last = 0;

  uint32_t now = millis();
  if (last != 0 && now - last < 2000) return;
  last = now;

  const auto &s = steps[step];
  strand1.fill(strand1.Color(s.r, s.g, s.b));
  strand1.show();
  Serial.printf("LUMINA-P7 strand1 pin=%u leds=%u color=%s level=%u uptime=%lus\n", STRAND_PINS[0], TEST_LEDS, s.name,
                TEST_LEVEL, (unsigned long)(now / 1000));
  step = (step + 1) % (sizeof(steps) / sizeof(steps[0]));
}
