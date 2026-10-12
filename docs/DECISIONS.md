# Decisions

Entries made before this workflow are summarized from the earlier handoff and have no full source records. New consequential decisions use the format in `docs/PROJECT_INSTRUCTIONS.md` section 3.

## Pre-existing

| Decision | Reason | Rejected alternative |
|---|---|---|
| Preserve v22 on `main` before v23 | Known rollback point and demo version | Refactoring in place |
| Move to VS Code + PlatformIO with `.cpp`/`.h` modules | Modularity, library management, multiple board environments, Git, tests, CI, professional structure | One giant `.ino` forever |
| Keep the PCM1808 | Audio-reactive lighting from any source when Serato or SoundSwitch isn't running | Relying only on Serato/SoundSwitch |
| Stop chasing perfect beat detection | Couldn't tune it reliably; SoundSwitch gives precise timing | More beat-detector work |
| Home Wi-Fi station mode | Keep normal internet access | AP-only as the primary mode |
| Recorder removed for good | Not needed; simpler firmware | Keeping it |
| Implement Art-Net directly in Lumina | Preserve the custom platform; more technically interesting | Flashing WLED as the final firmware (reference only) |
| Support both intelligent-fixture and direct-pixel modes; intelligent is primary | Fixture mode is efficient and scalable; pixel mode allows full creative control | Only one of the two |
| SoundSwitch owns show timing; Art-Net is DMX transport | Use the DJ software's track awareness | Parsing Serato data in firmware |
| Config storage: dual-slot blob (`cfgA`/`cfgB`) with version, sequence, CRC32, and read-back | Fixed the v20 save failure | Many individual Preferences keys |
| Ableton Link only if SoundSwitch timing proves insufficient | Avoid unneeded complexity | Adding it up front |

## 2026-10-11

| Decision | Reason | Reconsider if |
|---|---|---|
| Use 3–5 **local** tracks for SoundSwitch scripting; keep Spotify for normal DJing | SoundSwitch support: autoscripts can't be created for streaming tracks, and Spotify isn't listed for Serato ([source](https://support.soundswitch.com/en/support/solutions/articles/69000847400-music-streaming-with-soundswitch), retrieved 2026-10-11, article undated) | A SoundSwitch release adds streaming autoscripts or Spotify |
| Packet captures stay out of the public repo; only sanitized summaries are committed | Captures contain LAN addresses | — |
| Phases are the unit of work; `/start-milestone Phase N` authorizes one phase | Matches the existing roadmap | — |
| v22 sketches live in `legacy/v22/<sketch_name>/<sketch_name>.ino` on `develop-v23` (moved with `git mv` to keep history); `main` stays untouched | Arduino requires each `.ino` in a folder of the same name, and it merges every `.ino` in one folder into one sketch, so sender and receiver must be separate. This also keeps the root clean for v23 | — |
| **All development on the Windows PC** (repo, Claude Code, PlatformIO, uploads, Wireshark). The Mac only runs Serato and SoundSwitch | Your choice; keeps the DJ machine clean and stable | — |
