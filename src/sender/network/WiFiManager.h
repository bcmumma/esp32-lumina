// Home Wi-Fi station join/recovery, mDNS, and the onboard RGB status LED.
#pragma once

#include <Arduino.h>

// Onboard RGB status LED (off unless the owner switches it on, Advanced page):
//   dim blue blink = connecting
//   green           = joined to home Wi-Fi
//   red blink       = disconnected/reconnecting
// The LED is on GPIO48, which is PCM1808 DOUT in the current wiring, so it is never driven
// (status_rgb_usable() false); moving DOUT to another pin frees it.
static constexpr uint8_t STATUS_RGB_BRIGHTNESS = 32;

void setup_status_rgb();
void status_rgb_write(uint8_t r, uint8_t g, uint8_t b);
// Owner switch: false = always dark (start-up and errors included).
void status_rgb_set_enabled(bool on);
// False while the LED's pin is taken by the PCM1808 (the switch then does nothing).
bool status_rgb_usable();
void show_connecting_rgb(uint32_t nowMs);
void show_connected_rgb();
void show_disconnected_rgb(uint32_t nowMs);

extern uint8_t active_wifi_channel;

void setup_wifi();
void maintain_wifi_connection();
