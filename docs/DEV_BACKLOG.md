# Dev Backlog

Nothing here is a commitment. An item becomes work only through `docs/PROJECT_STATE.md`.

## Later Features

- An expanded intelligent fixture, plus global and per-strand fixtures.
- A serious SoundSwitch AutoScript library workflow.
- A diagnostics page (active source, Art-Net rate, RSSI, ESP-NOW stats, heap).
- Multiple synchronized receiver nodes.
- Home Assistant and MQTT integration.
- AI-generated lighting scenes from an LLM. If it's ever local, it could share the homelab Ollama.
- Ableton Link, only if SoundSwitch timing proves insufficient.
- A custom PCB.
- An over-the-air update path for both boards (authenticated).

## Portfolio and Product

- README with diagrams, wiring, build and flash steps, a flashing-light safety warning, and demo videos.
- License: MIT was suggested earlier. Check what's in the repo.
- Commercialization is possible later. Pause before publishing anything that looks commercially differentiating.

## Tools to Evaluate

| Tool | When | Notes |
|---|---|---|
| `tshark` (comes with Wireshark for Windows) | Phases 5–6 | Lets Claude Code capture and summarize Art-Net from the CLI on the PC |
| PlatformIO native test environment | Phases 8–10 | Host tests for the config blob, packets, and the Art-Net parser |
| GitHub Actions build CI | After Phase 8 | Builds both environments on every push; good for the portfolio |
| WLED | Reference only | For comparing Art-Net behavior, never as the final firmware |
