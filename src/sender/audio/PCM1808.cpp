#include "PCM1808.h"
#include "../config/ConfigManager.h"
#include "../util/Clamp.h"

I2SClass I2S;
int32_t raw_samples[BLOCK_FRAMES * 2];

int16_t sample32_to_s16(int32_t s) {
  int shift = clamp_int(cfg.shiftTo16, 12, 20);
  int32_t v = s >> shift;

  if (v > 32767) return 32767;
  if (v < -32768) return -32768;

  return (int16_t)v;
}

void setup_i2s() {
  I2S.setPins(PIN_BCLK, PIN_LRCK, -1, PIN_DIN, PIN_MCLK);

  bool ok = I2S.begin(
    I2S_MODE_STD,
    SAMPLE_RATE,
    I2S_DATA_BIT_WIDTH_32BIT,
    I2S_SLOT_MODE_STEREO,
    -1,
    I2S_ROLE_MASTER
  );

  Serial.print("I2S begin: ");
  Serial.println(ok ? "OK" : "FAIL");

  if (!ok) {
    while (true) delay(1000);
  }
}
