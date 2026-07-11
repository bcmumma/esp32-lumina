# Lumina v1.0.0 Release Notes

## Release Name

Lumina v1.0 — Working Prototype Release

## Summary

Lumina v1.0 is the first stable prototype release of a distributed ESP32 smart lighting platform. This version proves the full end-to-end system from line-level audio input to wireless LED rendering.

The system captures stereo audio through a PCM1808 I2S ADC, processes the audio on an ESP32-S3, sends compact lighting-control packets over ESP-NOW, and renders effects on a separate ESP32 LED receiver driving WS2815 addressable LEDs.

## Major Features

* PCM1808 stereo I2S audio input
* ESP32-S3 sender node
* ESP32-WROOM LED receiver node
* ESP-NOW wireless communication
* FFT-based audio analysis
* Beat/onset detection
* Browser-based configuration UI
* Multi-strand LED support
* LED color calibration
* Hardware configuration page
* Mobile-friendly LED control page
* Recorder helper support
* WS2815 / WS2812-style LED support
* Saveable settings

## System Pipeline

```text
Audio Source
    ↓
PCM1808 ADC
    ↓
ESP32-S3 Sender
    ↓
FFT / Beat Detection / Web UI
    ↓
ESP-NOW Packets
    ↓
ESP32 Receiver
    ↓
WS2815 LED Output
```

## Included Firmware

* `firmware/sender/lumina_sender_v1_0.ino`
* `firmware/receiver/lumina_receiver_v1_0.ino`

## Included Tools

* `tools/recorder/pcm1808_web_recorder_agent.py`
* `tools/audio_test/make_led_tuning_wav.py`

## Known Working Hardware

### Sender

* ESP32-S3 development board
* PCM1808 I2S ADC module
* RCA line-level audio source

### Receiver

* ESP32-WROOM development board
* WS2815 12V addressable LED strip
* 12V LED power supply

## Known Limitations

* Firmware is still Arduino-sketch based and should be modularized in v2.
* Beat detection is functional but not yet professionally tuned.
* Wi-Fi/ESP-NOW channel alignment is manual.
* Home Assistant integration is not included in v1.0.
* MQTT integration is not included in v1.0.
* AI scene generation is not included in v1.0.
* Diagnostics and performance profiling are limited.

## Recommended Next Version

v2.0 should focus on:

* Migrating to PlatformIO
* Splitting sender and receiver firmware into modules
* Improving the lighting engine
* Improving beat/BPM tracking
* Adding diagnostics
* Expanding effects
* Preparing Home Assistant and MQTT integration

## Release Status

This release should be considered a working prototype and foundation for future development.

It is suitable for demonstration, documentation, GitHub publication, and resume/portfolio use.
