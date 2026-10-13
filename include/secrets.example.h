// Template for include/secrets.h, which is gitignored (the repo is public).
// Copy this file to include/secrets.h and fill in your values.
#pragma once

// 2.4 GHz home Wi-Fi the S3 sender joins.
#define LUMINA_WIFI_SSID "your-ssid"
#define LUMINA_WIFI_PASS "your-password"

// Static IP for the sender, or false to use DHCP. Comma-separated octets.
#define LUMINA_USE_STATIC_IP true
#define LUMINA_LOCAL_IP 192, 168, 1, 99
#define LUMINA_GATEWAY 192, 168, 1, 1
#define LUMINA_SUBNET 255, 255, 255, 0
#define LUMINA_DNS1 192, 168, 1, 1
#define LUMINA_DNS2 1, 1, 1, 1
