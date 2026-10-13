// PCM1808 ADC on the S3: proven pins and I2S format (docs/HARDWARE.md).
#pragma once

#include <Arduino.h>
#include <ESP_I2S.h>

static constexpr int PIN_MCLK = 20; // PCM1808 SCK / SCKI / MCLK
static constexpr int PIN_BCLK = 21; // PCM1808 BCK
static constexpr int PIN_LRCK = 47; // PCM1808 LRC / LRCK
static constexpr int PIN_DIN  = 48; // PCM1808 OUT / DOUT -> ESP32-S3

static constexpr uint32_t SAMPLE_RATE = 48000;
static constexpr int BLOCK_FRAMES = 256;

extern I2SClass I2S;
extern int32_t raw_samples[BLOCK_FRAMES * 2];

int16_t sample32_to_s16(int32_t s);
void setup_i2s();
