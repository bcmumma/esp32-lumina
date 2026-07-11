# Setup

## Arduino Setup

Install:

- ESP32 board package by Espressif
- Adafruit NeoPixel library

## Sender

Upload `lumina_sender_v1_0.ino` to the ESP32-S3.

Use Serial Monitor at 115200 baud.

Set Wi-Fi SSID and password in the sender firmware before upload.

After boot, the sender prints its local web address and Wi-Fi channel.

## Receiver

Upload `lumina_receiver_v1_0.ino` to the ESP32-WROOM.

Set the receiver Wi-Fi channel to match the sender channel.

Use Serial Monitor at 115200 baud.

## Web UI

Open the sender IP address in a browser.

Configure LED settings, hardware settings, and audio tuning.
