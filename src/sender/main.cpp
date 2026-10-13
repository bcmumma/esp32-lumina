// Phase 7 bench diagnostic for the S3 sender. Phase 8 replaces this with the v2 port.
// Prints chip info, then a PCM1808 level meter once per second: peak and RMS in dBFS of the
// ADC's true full scale, so the input level (Magni volume) can be set without clipping.
#include <Arduino.h>
#include <ESP_I2S.h>
#include <esp_chip_info.h>
#include <esp_idf_version.h>
#include <math.h>

// Proven pins and format, same as v2 (docs/HARDWARE.md).
static constexpr int PIN_MCLK = 20;
static constexpr int PIN_BCLK = 21;
static constexpr int PIN_LRCK = 47;
static constexpr int PIN_DIN = 48;
static constexpr uint32_t SAMPLE_RATE = 48000;
static constexpr int BLOCK_FRAMES = 256;

static I2SClass I2S;
static int32_t raw[BLOCK_FRAMES * 2];

static float to_dbfs(double v) { return v > 0 ? 20.0f * log10f((float)v) : -120.0f; }

void setup() {
  Serial.begin(115200);
  delay(500);

  esp_chip_info_t info;
  esp_chip_info(&info);

  Serial.println();
  Serial.println("LUMINA-P7 sender diag");
  Serial.printf("LUMINA-P7 chip=%s rev=%u cores=%u cpu=%luMHz\n", ESP.getChipModel(), ESP.getChipRevision(),
                info.cores, (unsigned long)ESP.getCpuFreqMHz());
  Serial.printf("LUMINA-P7 flash=%luMB psram=%luKB arduino=%s idf=%s\n",
                (unsigned long)(ESP.getFlashChipSize() / (1024 * 1024)), (unsigned long)(ESP.getPsramSize() / 1024),
                ESP_ARDUINO_VERSION_STR, esp_get_idf_version());

  I2S.setPins(PIN_BCLK, PIN_LRCK, -1, PIN_DIN, PIN_MCLK);
  bool ok = I2S.begin(I2S_MODE_STD, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO, -1, I2S_ROLE_MASTER);
  Serial.printf("LUMINA-P7 i2s=%s\n", ok ? "OK" : "FAIL");
}

void loop() {
  static double sumSq[2] = {0, 0};
  static int32_t peak[2] = {0, 0};
  static uint32_t frames = 0, clips = 0, last = 0;

  size_t got = I2S.readBytes((char *)raw, sizeof(raw));
  int n = (int)(got / sizeof(int32_t)) / 2;
  for (int i = 0; i < n; i++) {
    for (int ch = 0; ch < 2; ch++) {
      int32_t s = raw[i * 2 + ch];
      int32_t a = s == INT32_MIN ? INT32_MAX : abs(s);
      if (a > peak[ch]) peak[ch] = a;
      double f = (double)s / 2147483648.0;
      sumSq[ch] += f * f;
      if (a >= 2126008810) clips++;  // within 0.1 dB of full scale
    }
  }
  frames += n;

  uint32_t now = millis();
  if (now - last >= 1000 && frames > 0) {
    last = now;
    Serial.printf("LUMINA-P7 meter peakL=%.1fdBFS peakR=%.1fdBFS rmsL=%.1fdBFS rmsR=%.1fdBFS clips=%lu frames=%lu\n",
                  to_dbfs(peak[0] / 2147483648.0), to_dbfs(peak[1] / 2147483648.0), to_dbfs(sqrt(sumSq[0] / frames)),
                  to_dbfs(sqrt(sumSq[1] / frames)), (unsigned long)clips, (unsigned long)frames);
    sumSq[0] = sumSq[1] = 0;
    peak[0] = peak[1] = 0;
    frames = clips = 0;
  }
}
