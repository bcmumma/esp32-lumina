# Decisions

Entries made before this workflow are summarized from the earlier handoff and have no full source records. New consequential decisions use the format in `docs/PROJECT_INSTRUCTIONS.md` section 3.

## Pre-existing

| Decision | Reason | Rejected alternative |
|---|---|---|
| Preserve v2 on `main` before v3 | Known rollback point and demo version | Refactoring in place |
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
| ~~v2 sketches move to `legacy/v22/<sketch_name>/<sketch_name>.ino`~~ Superseded 2026-10-12 | — | — |
| **All development on the Windows PC** (repo, Claude Code, PlatformIO, uploads, Wireshark). The Mac only runs Serato and SoundSwitch | Your choice; keeps the DJ machine clean and stable | — |

## 2026-10-12

| Decision | Reason | Reconsider if |
|---|---|---|
| Version names follow the repo: the stable Arduino build is **v2** (`firmware/*/lumina_*_v2.ino`, earlier called "v22"), and the PlatformIO rebuild is **v3** (earlier "v23"). The branch keeps the name `develop-v23` for now | Your choice; the docs now match the committed file names | The branch is renamed |
| The v2 sketches stay where they are in `firmware/sender/` and `firmware/receiver/`; no `legacy/` move | Your choice. Each folder holds both the v1.0 and v2 `.ino`, so the Arduino IDE would merge them into one sketch, but the v3 PlatformIO port copies the v2 source into its own project and doesn't need the Arduino folder rule | v2 has to be rebuilt in the Arduino IDE |
| The old `docs/hardware.md` and `docs/architecture.md` are merged into `HARDWARE.md` and `ARCHITECTURE.md` | Windows treats names that differ only in case as the same file | — |
