# Architecture

Lumina is built around a distributed node architecture.

The sender node captures and analyzes audio.

The receiver node renders LED effects locally.

The sender does not stream individual LED pixel values. Instead, it sends compact feature and control packets.

This design reduces wireless bandwidth, improves reliability, and allows receiver nodes to render smooth animations independently.

## Sender Responsibilities

- Capture PCM1808 audio
- Analyze audio
- Detect beats/onsets
- Host web UI
- Store settings
- Send ESP-NOW packets

## Receiver Responsibilities

- Receive ESP-NOW packets
- Parse lighting settings
- Render LED effects
- Apply color calibration
- Drive LED strips
