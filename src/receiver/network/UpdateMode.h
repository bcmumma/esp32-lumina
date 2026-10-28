// Wireless firmware updates for the room receivers (Phase 16).
// The S3 sends an "update mode" command over ESP-NOW; this receiver then joins the home Wi-Fi
// (same credentials as the S3, address from the router) and accepts a firmware upload over HTTP
// at /update (basic auth, password in secrets.h) while it keeps drawing the show. With no upload
// within kUpdateWindowMs it restarts and carries on as normal.
#pragma once

#include <Arduino.h>

static constexpr uint32_t kUpdateWindowMs = 10UL * 60UL * 1000UL;

// From the ESP-NOW callback: true if the payload was a command packet (handled or not for us).
bool update_mode_handle_command(const uint8_t *data, int len);

// From loop(): starts Wi-Fi + the update server when asked, serves it, and ends the window.
void update_mode_loop(uint32_t nowMs);

bool update_mode_active();
bool update_mode_wifi_connected();
uint32_t update_mode_ip();  // IPv4 as stored by IPAddress (0 = none)
