# Hardware

## Sender Node

- ESP32-S3 development board
- PCM1808 I2S ADC
- RCA line-level audio source
- USB power/data

## Receiver Node

- ESP32-WROOM development board
- WS2815 12V addressable LED strip
- 12V LED power supply

## Notes

The LED power supply ground must be connected to the ESP32 ground.

A resistor between the ESP32 data pin and LED DI is recommended.

A large capacitor across LED strip power is recommended.
