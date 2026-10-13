// Home Wi-Fi station join/recovery, mDNS, and the onboard RGB status LED.
#pragma once

#include <Arduino.h>

// Onboard RGB status LED:
//   dim blue blink = connecting
//   green           = joined to home Wi-Fi
//   red blink       = disconnected/reconnecting
// The code automatically disables this LED if its physical GPIO conflicts
// with one of the proven PCM1808 pins. Many S3 boards place the RGB LED on
// GPIO48; this project currently uses GPIO48 as PCM1808 DOUT/DIN.
static constexpr uint8_t STATUS_RGB_BRIGHTNESS = 32;

void setup_status_rgb();
void status_rgb_write(uint8_t r, uint8_t g, uint8_t b);
void show_connecting_rgb(uint32_t nowMs);
void show_connected_rgb();
void show_disconnected_rgb(uint32_t nowMs);

extern uint8_t active_wifi_channel;

void setup_wifi();
void maintain_wifi_connection();
