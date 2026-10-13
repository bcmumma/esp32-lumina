#include "WiFiManager.h"
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <esp_wifi.h>
#include "soc/soc_caps.h"
#include "../audio/PCM1808.h"

extern WebServer server;  // serviced while waiting for the first join

// Credentials and LAN addresses live in the gitignored include/secrets.h
// (template: include/secrets.example.h) because the repo is public.
#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "Missing include/secrets.h: copy include/secrets.example.h to include/secrets.h and fill it in."
#endif

static const char *WIFI_SSID = LUMINA_WIFI_SSID;
static const char *WIFI_PASS = LUMINA_WIFI_PASS;
static const char *WIFI_HOSTNAME = "lumina-sender";

// Reserve the static address in pfSense/DHCP, or set LUMINA_USE_STATIC_IP false.
static constexpr bool USE_STATIC_IP = LUMINA_USE_STATIC_IP;
IPAddress local_ip(LUMINA_LOCAL_IP);
IPAddress gateway(LUMINA_GATEWAY);
IPAddress subnet(LUMINA_SUBNET);
IPAddress dns1(LUMINA_DNS1);
IPAddress dns2(LUMINA_DNS2);
// Arduino-ESP32 WiFi.config() accepts two DNS servers. Configure 1.1.1.1
// as an upstream/fallback inside pfSense or Pi-hole if desired. Do not pass
// a third argument here because that overload does not exist.

static constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 30000;
static constexpr uint32_t WIFI_RECONNECT_INTERVAL_MS = 5000;
uint8_t active_wifi_channel = 1;
static bool wifiWasConnected = false;
static uint32_t lastWifiReconnectAttemptMs = 0;

static bool statusRgbUsable = false;

static int onboard_rgb_physical_pin() {
#if defined(RGB_BUILTIN)
  int pin = (int)RGB_BUILTIN;
  #if defined(SOC_GPIO_PIN_COUNT)
    if (pin >= SOC_GPIO_PIN_COUNT) pin -= SOC_GPIO_PIN_COUNT;
  #endif
  return pin;
#else
  return -1;
#endif
}

static bool pin_conflicts_with_pcm1808(int pin) {
  return pin == PIN_MCLK || pin == PIN_BCLK || pin == PIN_LRCK || pin == PIN_DIN;
}

void status_rgb_write(uint8_t r, uint8_t g, uint8_t b) {
#if defined(RGB_BUILTIN)
  if (statusRgbUsable) rgbLedWrite(RGB_BUILTIN, r, g, b);
#else
  (void)r; (void)g; (void)b;
#endif
}

void setup_status_rgb() {
#if defined(RGB_BUILTIN)
  int physicalPin = onboard_rgb_physical_pin();
  Serial.print("Onboard RGB physical GPIO: ");
  Serial.println(physicalPin);

  if (pin_conflicts_with_pcm1808(physicalPin)) {
    statusRgbUsable = false;
    Serial.println("WARNING: onboard RGB status LED disabled because its GPIO conflicts with PCM1808 wiring.");
    Serial.println("If this is GPIO48, move PCM1808 DOUT to another free GPIO before enabling the onboard RGB LED.");
    return;
  }

  statusRgbUsable = true;
  status_rgb_write(0, 0, 0);
#else
  Serial.println("No RGB_BUILTIN definition for the selected Arduino board; onboard RGB status is disabled.");
#endif
}

void show_connecting_rgb(uint32_t nowMs) {
  if (!statusRgbUsable) return;
  bool on = ((nowMs / 300U) & 1U) == 0;
  status_rgb_write(0, 0, on ? STATUS_RGB_BRIGHTNESS : 0);
}

void show_connected_rgb() {
  status_rgb_write(0, STATUS_RGB_BRIGHTNESS, 0);
}

void show_disconnected_rgb(uint32_t nowMs) {
  if (!statusRgbUsable) return;
  bool on = ((nowMs / 350U) & 1U) == 0;
  status_rgb_write(on ? STATUS_RGB_BRIGHTNESS : 0, 0, 0);
}

void setup_wifi() {
  WiFi.useStaticBuffers(true);
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.setHostname(WIFI_HOSTNAME);
  WiFi.disconnect(false, false);
  delay(100);

  esp_wifi_set_ps(WIFI_PS_NONE);
  // Set HT20 before joining. v2 set it after the join, which made the station
  // leave the AP (Reason 8 ASSOC_LEAVE) and rejoin ~8 s later (docs/KNOWN_ISSUES.md).
  esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT20);

  if (USE_STATIC_IP) {
    bool configOk = WiFi.config(local_ip, gateway, subnet, dns1, dns2);
    Serial.print("Static IP config: ");
    Serial.println(configOk ? "OK" : "FAIL");
  }

  Serial.print("Connecting to 2.4 GHz Wi-Fi: ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  uint32_t attemptStart = millis();
  while (WiFi.status() != WL_CONNECTED) {
    show_connecting_rgb(millis());
    server.handleClient();
    delay(25);

    if (millis() - attemptStart >= WIFI_CONNECT_TIMEOUT_MS) {
      Serial.println();
      Serial.println("Wi-Fi connection timed out. Retrying...");
      status_rgb_write(STATUS_RGB_BRIGHTNESS, 0, 0);
      WiFi.disconnect(false, false);
      delay(500);
      WiFi.begin(WIFI_SSID, WIFI_PASS);
      attemptStart = millis();
    }
  }

  active_wifi_channel = WiFi.channel();
  wifiWasConnected = true;
  show_connected_rgb();

  Serial.println();
  Serial.println("Wi-Fi connected.");
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());
  Serial.print("Controller page: http://");
  Serial.println(WiFi.localIP());
  Serial.print("Wi-Fi RSSI: ");
  Serial.print(WiFi.RSSI());
  Serial.println(" dBm");
  Serial.print("Wi-Fi / ESP-NOW channel: ");
  Serial.println(active_wifi_channel);
  Serial.print("Sender STA MAC: ");
  Serial.println(WiFi.macAddress());
  Serial.print("DNS 1: ");
  Serial.println(WiFi.dnsIP(0));
  Serial.print("DNS 2: ");
  Serial.println(WiFi.dnsIP(1));

  if (MDNS.begin("lumina")) {
    MDNS.addService("http", "tcp", 80);
    Serial.println("mDNS page: http://lumina.local");
  } else {
    Serial.println("mDNS startup failed; use the numeric IP address.");
  }
}

void maintain_wifi_connection() {
  uint32_t now = millis();
  bool connected = WiFi.status() == WL_CONNECTED;

  if (connected) {
    if (!wifiWasConnected) {
      wifiWasConnected = true;
      active_wifi_channel = WiFi.channel();
      show_connected_rgb();
      Serial.print("Wi-Fi reconnected. IP: ");
      Serial.print(WiFi.localIP());
      Serial.print(" channel: ");
      Serial.println(active_wifi_channel);
    }
    return;
  }

  if (wifiWasConnected) {
    wifiWasConnected = false;
    Serial.println("Wi-Fi disconnected; attempting recovery.");
  }

  show_disconnected_rgb(now);
  if (now - lastWifiReconnectAttemptMs >= WIFI_RECONNECT_INTERVAL_MS) {
    lastWifiReconnectAttemptMs = now;
    WiFi.reconnect();
  }
}
