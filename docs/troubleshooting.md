# Troubleshooting

## LEDs do not light

Check:

- LED power supply is on
- ESP32 ground is connected to LED power ground
- Correct GPIO pin is configured
- Correct LED count is configured
- Correct LED type/color order is selected
- Brightness is not zero
- Strand is enabled
- Mode is not Off

## ESP-NOW packets not received

Check:

- Receiver Wi-Fi channel matches sender Wi-Fi channel
- Sender and receiver packet versions match
- Both boards are powered
- Both boards are using 2.4 GHz Wi-Fi/ESP-NOW

## Web page does not open

Check:

- Sender is connected to Wi-Fi
- Serial Monitor printed a valid IP address
- Computer/phone is on same network
- Correct static IP settings are used

## Audio not detected

Check:

- PCM1808 wiring
- MCLK/BCLK/LRCK signals
- RCA input signal
- PCM1808 mode pins
- Audio tuning floors/gain
