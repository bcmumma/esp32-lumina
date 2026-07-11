# esp32-lumina
# Lumina ESP32

Lumina is a distributed ESP32-based smart lighting platform featuring real-time audio analysis, browser-based configuration, ESP-NOW wireless control, and multi-strand addressable LED rendering.

The current v1.0 release is a working prototype that captures line-level audio from a PCM1808 I2S ADC, analyzes the signal on an ESP32-S3, broadcasts compact lighting-control packets over ESP-NOW, and renders effects on a separate ESP32 LED receiver.

## Project Status

| Field | Value |
|---|---|
| Version | v1.0.0 |
| Status | Working prototype |
| Platform | ESP32-S3 sender + ESP32-WROOM receiver |
| Primary LED target | WS2815 / WS2812-style addressable LEDs |
| Communication | ESP-NOW |
| Audio input | PCM1808 I2S ADC |
| Web UI | Built-in ESP32 web server |

## System Overview

```text
Audio Source
    │
    ▼
PCM1808 I2S ADC
    │
    ▼
ESP32-S3 Sender
    ├── I2S audio capture
    ├── FFT audio analysis
    ├── beat/onset detection
    ├── browser-based configuration UI
    ├── recorder control
    └── ESP-NOW packet broadcast
            │
            ▼
ESP32 LED Receiver
    ├── ESP-NOW packet receiver
    ├── multi-strand LED renderer
    ├── color calibration
    └── WS2815 / addressable LED output
```

## Features

* PCM1808 line-level stereo audio input
* ESP32-S3 I2S master-mode audio capture
* FFT-based spectral analysis
* Beat/onset detection
* ESP-NOW wireless sender/receiver architecture
* Browser-based web interface
* LED color and mode control
* Hardware configuration page
* Multi-strand LED support
* Color calibration
* Python-based recording helper
* Configurable LED effects
* Expandable architecture for future smart-home and AI scene control

## Hardware Used

### Sender Node

* ESP32-S3 development board
* PCM1808 I2S ADC module
* RCA line-level audio input
* USB power/data connection

### Receiver Node

* ESP32-WROOM development board
* WS2815 12V addressable LED strip
* External 12V LED power supply
* Shared ground between ESP32 and LED power supply

## Sender Wiring

```text
ESP32-S3 GPIO4  -> PCM1808 SCK / SCKI / MCLK
ESP32-S3 GPIO5  -> PCM1808 BCK
ESP32-S3 GPIO6  -> PCM1808 LRC / LRCK
PCM1808 OUT     -> ESP32-S3 GPIO7
GND             -> common GND
```

PCM1808 mode pins:

```text
MD0     -> GND
MD1     -> GND
FMT/FMY -> GND
```

## Receiver Wiring

Example working WS2815 test configuration:

```text
ESP32 GPIO25  -> WS2815 DI
ESP32 GND     -> LED power supply GND
12V PSU +     -> WS2815 +12V
12V PSU GND   -> WS2815 GND
```

Recommended:

```text
ESP32 GPIO25 -> 330 ohm resistor -> LED DI
1000 uF capacitor across 12V and GND near LED strip input
```

## Software Requirements

* Arduino IDE or Arduino CLI
* ESP32 board package by Espressif
* Adafruit NeoPixel library
* Python 3 for optional recording helper
* pyserial for optional recorder

## Arduino Settings

### Sender ESP32-S3

Recommended settings:

```text
Board: ESP32S3 Dev Module
USB CDC On Boot: Enabled
Serial Monitor Baud: 115200
```

### Receiver ESP32-WROOM

Recommended settings:

```text
Board: ESP32 Dev Module
Serial Monitor Baud: 115200
```

## Important ESP-NOW Note

The ESP-NOW receiver must use the same 2.4 GHz Wi-Fi channel as the sender.

The sender prints the active Wi-Fi channel in Serial Monitor:

```text
Wi-Fi channel: X
```

Set the receiver firmware to that same channel before uploading.

## Web Interface

The sender hosts a web page on its local IP address.

The page includes:

* LED Colors
* Hardware
* Audio Tune
* Recorder
* Settings
* Save controls

The LED Colors tab is the main control page. Hardware and audio tuning are separated so the front page stays simple and mobile-friendly.

## Recording Helper

The Python recorder helper can communicate with the sender and save audio recordings from the PCM1808 input.

Install pyserial:

```bash
pip install pyserial
```

Run the helper:

```bash
python pcm1808_web_recorder_agent.py
```

## Current Limitations

* v1.0 is a working prototype, not yet a fully modular firmware architecture.
* Beat detection works but still needs further refinement.
* Receiver Wi-Fi channel currently needs to be manually matched to the sender.
* Home Assistant and MQTT integration are planned but not implemented in v1.0.
* AI-generated lighting scenes are planned for a future release.

## Roadmap

Future goals include:

* PlatformIO migration
* Modular firmware architecture
* Improved lighting engine
* Expanded effects library
* Better BPM and beat tracking
* Home Assistant integration
* MQTT support
* AI-generated lighting scenes
* Multi-node synchronization
* Professional diagnostics dashboard

## Project Vision

Lumina is intended to grow into a distributed smart lighting platform, not just a music-reactive LED project.

The long-term goal is to support audio-reactive lighting, ambient scenes, smart-home automation, AI-generated scenes, and multiple synchronized LED receiver nodes.
