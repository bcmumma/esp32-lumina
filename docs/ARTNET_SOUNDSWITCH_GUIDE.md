# SoundSwitch → Art-Net Guide (DJ Track, Phases 4–6)

A click-by-click walkthrough for the Mac work and the PC checks that come before any Art-Net firmware (Phase 10). Work top to bottom; each part ends with what to report back to Claude Code.

**The signal path you're building:**

```
Serato DJ Pro (Mac) ──► SoundSwitch (Mac) ──Art-Net, UDP 6454──► PC monitor now ──► S3 in Phase 10
```

**How to read this guide:**

- **Mac** steps are yours. **PC** steps you can run yourself, or ask Claude Code to run.
- A step marked **UNVERIFIED** comes from an official source that doesn't spell out the exact clicks, or from a reasonable inference. If the screen looks different, describe what you see and continue.
- Sources (checked 2026-10-12):
  - the SoundSwitch support articles linked at the end;
  - the SoundSwitch User Guide PDF (2021, SoundSwitch 2.3-era screenshots).
  - SoundSwitch changes between versions, so record your version first (Part 0).

---

## Part 0: Record versions and network (10 min)

**On the Mac:**

1. **macOS version:**
   1. Click the Apple menu  (top-left corner of the screen).
   2. Click **About This Mac**.
   3. Write down the macOS name and number, e.g. "Sonoma 14.6".
2. **SoundSwitch version:**
   1. Open SoundSwitch.
   2. In the menu bar at the top of the screen, click **SoundSwitch** (the app name, right of the Apple menu).
   3. Click **About SoundSwitch**.
   4. Write the version down. Art-Net needs **2.1 or newer** (SoundSwitch support article).
3. **Serato version:**
   1. Open Serato DJ Pro.
   2. Click **Serato DJ Pro** in the menu bar.
   3. Click **About Serato DJ Pro**.
4. **The Mac's IP address:**
   - **Newer macOS (Ventura and later):**
     1. Apple menu  → **System Settings**.
     2. Click **Wi-Fi**, or **Network** then your connection if the Mac uses Ethernet.
     3. Click **Details…** next to the connected network.
     4. Click **TCP/IP**.
     5. Note the **IP address**.
   - **Older macOS:**
     1. Apple menu  → **System Preferences**.
     2. Click **Network**.
     3. Select the connection, click **Advanced…**, then **TCP/IP**.

**On the PC:**

- In a PowerShell or VS Code terminal, run `ipconfig`.
- Note the **IPv4 Address** and **Subnet Mask** of the adapter that's connected (Wi-Fi or Ethernet).

**Check:** the Mac and the PC should be on the same subnet. With a 255.255.255.0 mask, the first three numbers of their IPs match. If they don't match, stop and tell Claude Code: changing VLANs is your call, and it may not be needed if SoundSwitch can unicast.

**Report back:**
- macOS, SoundSwitch, and Serato versions.
- Whether the Mac and PC share a subnet.
- Don't paste IP addresses into anything public. In chat with Claude Code it's fine; Claude keeps them in `CLAUDE.local.md`.

---

## Part 1: Phase 4, a scripted show in SoundSwitch (Mac, about 1 hour)

Streaming tracks (Spotify) **can't** be autoscripted, so use 3–5 **local** audio files: purchased or owned MP3/AIFF/WAV/FLAC (SoundSwitch support, "Music Streaming with SoundSwitch").

### 1.1 Make a test crate in Serato

1. Copy the 3–5 test files into a folder on the Mac, e.g. `Music/Lumina Test`.
2. Open **Serato DJ Pro**.
3. In the library at the bottom of the screen, click the **+** (add crate) button at the bottom-left of the Crates panel.
4. Type `Lumina Test` and press **Return**.
5. Open **Finder**, select the test files, and drag them onto the `Lumina Test` crate in Serato.
6. Analyze the files: drag the `Lumina Test` crate onto the **Analyze Files** button (left side of the library). Wait until the BPM column fills in.
7. Load each track on a deck and check that the beatgrid markers line up with the kick drums. Fix any that don't before scripting: SoundSwitch aligns its shows to Serato's beatgrid.

**Expected:** 3–5 tracks in `Lumina Test`, each showing a BPM.

### 1.2 Create the Lumina fixture profile (Fixture Manager)

The receiver treats DMX channels 1–12 as four RGB strands. SoundSwitch needs a fixture profile that matches:

| DMX channel | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Strand / color | 1 R | 1 G | 1 B | 2 R | 2 G | 2 B | 3 R | 3 G | 3 B | 4 R | 4 G | 4 B |

Build it as a **multi-cell** fixture so SoundSwitch can color each strand separately. Steps from SoundSwitch's "Fixture Manager: Creating a Multi-Cell Fixture Profile" article:

1. Open **Fixture Manager**. **UNVERIFIED:** it installs with SoundSwitch. Look in **Applications** for "SoundSwitch Fixture Manager", or check SoundSwitch's menus.
2. Click **File → New Fixture**.
3. Pick a manufacturer from the dropdown. Choose a generic or "Other" entry if one exists; if not, any placeholder works.
4. Type `Lumina 4-Strand RGB` in **Model**.
5. Click **Add Mode** and enter **12** channels.
6. Create **4 cells**: `Strand 1`, `Strand 2`, `Strand 3`, `Strand 4`.
   - Give each cell Red, Green, and Blue on the channels in the table above. Strand 1 is 1/2/3, Strand 2 is 4/5/6, and so on.
   - In SoundSwitch's RGB example, the channel number goes in the **Coarse** column.
   - Tip: build Strand 1 completely, then right-click it and choose **Duplicate**. Change the channel numbers on each copy.
7. **Don't create a `MASTER` cell.** SoundSwitch's article adds one for a master dimmer and strobe, but Lumina has neither. In SoundSwitch 2.11, **Check Fixture fails with an empty `MASTER` cell and passes without it** (verified by the owner, 2026-10-13).
8. Remove any default attributes you didn't add.
9. Click **Fixtures → Check Fixture**. The result appears in the bottom-left corner of the window.
10. If it passes, click **File → Save Workspace**. Don't use "Transfer to Public Cloud"; this profile is only for you.

**Expected:** "Check Fixture" passes, and the profile is saved locally.

### 1.3 Make the venue and patch the fixture (SoundSwitch Edit Mode)

From the SoundSwitch User Guide, "Setting Up DMX Fixtures":

1. Open **SoundSwitch**. If it opens in Performance Mode, click **File → Switch Modes → Edit Mode**.
2. Create the venue:
   1. Click the **+** (plus) icon at the top of the workspace.
   2. Type `Lumina Lab` and press **Return**.
3. Click the **DMX Map** icon in the toolbar. The DMX Map shows **Universe tabs**, the **Fixture Library**, and the map grid.
4. Make sure the **Universe 1** tab is selected.
5. Search the Fixture Library for `Lumina`.
6. Drag the 12-channel mode onto address **1**.
7. Check the DMX icon: **yellow** means channels overlap ("cross-patched"). It should stay normal.

**Expected:** `Lumina 4-Strand RGB` sits on Universe 1, channels 1–12, with no yellow warning.

### 1.4 Script a track

1. In Edit Mode, make the `Lumina Test` crate visible in SoundSwitch's library panel. SoundSwitch reads Serato's crates.
2. Load one test track into the SoundSwitch workspace.
3. Run **AutoScript** on it. **UNVERIFIED (exact clicks):** SoundSwitch's "Autoscripting Audio Files, Playlists and Crates" article only shows this in a video. The feature can script a single track, a playlist, or a whole crate; look for **AutoScript** on the track's right-click menu or in the toolbar.
4. Review the phrases SoundSwitch detects. To edit them, click the **padlock** icon (from "Phrase Editing"). **Quick Re-Autoscript** reruns it with the same style.
5. Change one phrase, e.g. its color, so you can prove your edit plays back later.
6. Save the project (**File → Save**). The show is stored as a tag **inside the audio file** (SoundSwitch support), which is another reason to use local files.

### 1.5 Play it back from Serato (the Phase 4 gate)

From the SoundSwitch User Guide, "Switching to Performance Mode":

1. In SoundSwitch, click **File → Switch Modes → Performance Mode**, then select **`Lumina Lab`**.
2. Switch to **Serato DJ Pro** and load the same track on a deck.
3. Check the connection indicator next to the Serato icon, top-left in SoundSwitch. The guide says **blue**; you saw **green** earlier, and either means connected. If there's no indicator, the guide suggests something (e.g. a firewall) is blocking the link between the apps.
4. Play the track. **Performance Mode's screen shows only the logo or MIDI controls (SoundSwitch 2.11), not the show.** To see the show follow Serato, run the Art-Net monitor (Part 2) and watch the channel values change; pausing Serato freezes them. If the show doesn't return after a pause, re-cue in Serato (see KNOWN_ISSUES).

**Phase 4 gate:** one track runs its saved show in sync with Serato.

**Report back:**
- Pass or fail, plus anything that didn't match these steps: menu names, the result of the `MASTER` check, the AutoScript location.
- No lights are needed yet: SoundSwitch's on-screen preview is enough for Phase 4.

---

## Part 2: Phase 5, Art-Net from SoundSwitch to the PC (Mac and PC, about 30 min)

The PC pretends to be an Art-Net node so you can see exactly what SoundSwitch sends, before any firmware exists.

### 2.1 Start the monitor on the PC

In VS Code's terminal, in the repo folder:

```powershell
.venv\Scripts\python.exe tools\testbench\artnet_listen.py
```

- It prints `Listening for Art-Net on UDP 6454…`, then `no Art-Net yet` every few seconds.
- **The first time, Windows may ask whether Python can use the network.** That's a firewall change, so it's your decision. If you allow it, tick **Private networks** only. If you decline, the monitor won't receive anything from the Mac. Tell Claude Code and we'll find another way.
- The monitor advertises this PC's address on the Mac's subnet (override with `--advertise <ip>`). **SoundSwitch sends unicast to exactly that address** (verified 2026-10-14).
- The monitor also answers Art-Net **discovery (ArtPoll)**, so it can appear in SoundSwitch as a device called **"Lumina PC Monitor"**. SoundSwitch's 2.1 announcement says it **broadcasts** Art-Net. A user report shows Art-Net devices appearing in SoundSwitch's hardware list. Answering discovery covers both cases.

### 2.2 Turn on Art-Net output in SoundSwitch

1. In SoundSwitch (Edit Mode), click the **Preferences** icon (top-right corner).
2. Click the **Hardware** tab. This is where the User Guide says your output hardware is listed and assigned a universe.
3. Look for an **Art-Net** option or a device called **Lumina PC Monitor**. **UNVERIFIED:** the exact Art-Net controls. Support says Art-Net output exists "from version 2.1", but only explains it in a video.
   - If you see Art-Net settings, enable output and pick the network adapter the Mac uses (Wi-Fi or Ethernet).
   - If **Lumina PC Monitor** appears, assign it to **Universe One**. The User Guide: "In most cases you will want to select assign Universe One."
4. If nothing Art-Net-related appears, take a screenshot of the Hardware tab and send it to Claude Code.

### 2.3 Send a known color (the simple end-to-end test)

**Verified 2026-10-14 (SoundSwitch 2.11):**
- "Lumina PC Monitor" shows up and connects automatically, with no Hardware-tab changes needed.
- Colour override buttons alone send **all zeros**. Something must be **playing**: the Autoloop with its playhead moving.

1. Still in Edit Mode, click the Autoloop icon in the bottom-right corner to show the **Autoloop Banks**.
2. Open the first Autoloop, **"Red"** in **"Bank 1: Dance"** (User Guide, "Testing").
3. Press **Space** to play it.

**Expected on the PC monitor:**

```
  12.0s univ=0  40 fps gaps=0 len=512  S1=255,0,0  S2=255,0,0  S3=255,0,0  S4=255,0,0
```

- **Red on every strand:** channels 1, 4, 7, and 10 high; green and blue at 0. The brightness may pulse with the Autoloop.
- `univ=` shows the Art-Net universe. **Verified 2026-10-14: SoundSwitch "Universe One" = Art-Net universe 0.** SoundSwitch also sends universe 1, all zeros.
- `fps` is SoundSwitch's send rate. `gaps` counts skipped sequence numbers.

### 2.4 If nothing arrives

Work down this list; each step rules out one layer:

1. **The monitor itself works:** with SoundSwitch closed, run the self-test in Part 4.1.
2. **Same subnet** (Part 0): if the Mac and PC are on different subnets or VLANs, broadcasts won't cross. Tell Claude Code; routing or VLAN changes are your call.
3. **Firewall:** if you declined the Windows prompt, or it never appeared, Windows may be dropping UDP 6454. Tell Claude Code before changing any firewall settings.
4. **Is the Mac sending at all?** This uses Terminal and a built-in tool; nothing gets installed:
   1. Open **Finder → Applications → Utilities → Terminal**.
   2. Type `sudo tcpdump -n -c 10 udp port 6454` and press **Return**.
   3. Enter your Mac login password. **Nothing appears while you type**; that's normal. Press **Return**.
   4. With the Red Autoloop playing, lines should appear. The address after `>` is where SoundSwitch sends: an address ending in `.255` is a broadcast; your PC's IP means unicast.
   5. Press **Control-C** to stop. Copy the lines (Command-C) and send them to Claude Code.

**Report back:**
- A copy of the monitor's output with the Red Autoloop playing.
- Whether SoundSwitch listed "Lumina PC Monitor".
- What the Hardware tab showed.

---

## Part 3: Phase 6, characterize the stream (PC, 10 min; Claude can run it)

Phase 10 is designed around these facts. Measure them with the Red Autoloop playing:

1. **Rate, universe, and length:**
   - Run `.venv\Scripts\python.exe tools\testbench\artnet_listen.py --seconds 60 --csv captures\artnet-summary.csv`.
   - The `captures\` folder is gitignored.
2. **Broadcast or unicast:** run the monitor once with `--no-reply` (it won't answer discovery).
   - If frames still arrive, SoundSwitch **broadcasts**.
   - If they only arrive when the monitor answers discovery, SoundSwitch **unicasts to discovered nodes**. The S3 must then answer ArtPoll too, which is a Phase 10 requirement.
3. **Behavior on stop:** pause the Autoloop.
   - Note whether frames stop entirely or keep coming with zeros.
   - This decides Lumina's timeout fallback (Phase 13).
4. **Optional, exact packet capture:** Wireshark/`tshark` (Phase 5 in the roadmap). Installing Wireshark (with Npcap) is a system-wide install, so ask Claude Code to propose it first.
   - Raw `.pcapng` files stay in `captures\`, never in Git.

**Summary to record** (Claude Code adds it to `docs/ARCHITECTURE.md` with no IPs):

| Item | Value |
|---|---|
| SoundSwitch version | |
| Art-Net universe for "Universe One" | |
| Frames per second (playing / paused) | |
| DMX length per frame | |
| Broadcast or unicast | |
| Needs ArtPoll reply? | |

**Phase 5–6 gate:** the PC sees SoundSwitch's Art-Net, and the table above is filled in. Phase 10 (S3 Art-Net receiver) can start after that.

---

## Part 4: Simple Art-Net tests

### 4.1 PC self-test (no Mac, no SoundSwitch, 1 min)

This proves the monitor works before you blame the network. Open two terminals in the repo folder:

```powershell
# Terminal 1: monitor, listening on this PC only (no firewall prompt)
.venv\Scripts\python.exe tools\testbench\artnet_listen.py --bind 127.0.0.1 --seconds 15

# Terminal 2: send a known pattern to it
.venv\Scripts\python.exe tools\testbench\artnet_send.py --to 127.0.0.1 --seconds 8
```

**Expected:** about 40 fps, `gaps=0`, `len=512`, and the colors rotate one strand per second:

```
   3.0s univ=0  40 fps gaps=0 len=512  S1=64,0,0   S2=0,64,0   S3=0,0,64   S4=64,64,64
   4.0s univ=0  40 fps gaps=0 len=512  S1=0,64,0   S2=0,0,64   S3=64,64,64 S4=64,0,0
```

(Claude Code ran this on 2026-10-12: 40 fps, 0 gaps, pattern correct.)

### 4.2 SoundSwitch end-to-end

This is Part 2.3: the Red Autoloop on the Mac should show red on S1–S4 in the PC monitor.

### 4.3 Later: Art-Net into the S3 (Phase 10)

Once the S3 has its Art-Net receiver, the same sender drives it directly, with no SoundSwitch needed:

```powershell
.venv\Scripts\python.exe tools\testbench\artnet_send.py --to <S3 IP> --seconds 10
```

The S3's diagnostics should then report the same universe, frame rate, and channel values. That comparison is the Phase 10 gate.

---

## Sources

- [Music Streaming with SoundSwitch](https://support.soundswitch.com/en/support/solutions/articles/69000847400-music-streaming-with-soundswitch): no autoscripts for streaming tracks.
- [Connecting third-party visualizers using Art-Net](https://support.soundswitch.com/en/support/solutions/articles/69000847590-soundswitch-connecting-third-party-visualizers-using-art-net): Art-Net from version 2.1 (steps shown in video only).
- [Fixture Manager: creating a multi-cell fixture profile](https://support.soundswitch.com/en/support/solutions/articles/69000844509-soundswitch-fixture-manager-creating-a-multi-cell-fixture-profile)
- [Fixture Manager: creating a profile for an RGB par can](https://support.soundswitch.com/en/support/solutions/articles/69000844507-soundswitch-fixture-manager-creating-a-profile-for-an-rgb-par-can)
- [Adding lighting fixtures in SoundSwitch](https://support.soundswitch.com/en/support/solutions/articles/69000847589-adding-lighting-fixtures-in-soundswitch)
- [Autoscripting audio files, playlists and crates](https://support.soundswitch.com/en/support/solutions/articles/69000847098-autoscripting-audio-files-playlists-and-crates) and [Phrase editing](https://support.soundswitch.com/en/support/solutions/articles/69000844233-soundswitch-phrase-editing)
- [SoundSwitch User Guide (PDF, 2021)](https://cdn.inmusicbrands.com/soundswitch/files/User%20Guide.pdf): Preferences → Hardware tab and universe assignment, venues, DMX Map, the "Red" Autoloop, File → Switch Modes, the Serato connection indicator.
- [SoundSwitch 2.1 announcement (2019)](https://www.audiomasterclass.com/blog/soundswitch-announces-2-1-dj-lighting-control-software-release-with-new-connectivity-options-2): "SoundSwitch now broadcasts Art-NET".
- [OLA issue #1779 (2022, user report)](https://github.com/OpenLightingProject/ola/issues/1779): Art-Net devices appearing in SoundSwitch's hardware overview.

---

## Part 5: Phase 12, the `LUMINA-16` intelligent fixture (Mac, about 30 min)

Instead of 12 raw RGB channels, Lumina now takes **16 control channels** and renders the effect itself, the same on all four strands. Channels 12–16 are reserved in v1, so leave them unassigned.

| CH | Function | What it does on Lumina |
|---|---|---|
| 1 | Master dimmer | Brightness (capped at 25% for now) |
| 2 | Effect | 0–42 Solid Color, 43–85 Neon Scanner, 86–127 Laser Sweep, 128–170 Rave Chase, 171–213 DnB Chopper, 214–255 White Lightning |
| 3 | Speed | Animation speed |
| 4 | Reactivity | How strongly the effect reacts to the music (don't call it "Intensity": that's SoundSwitch's name for the dimmer) |
| 5 | Density | Width / fill / number of elements |
| 6 | Strobe | Strobe strength. Flashing only happens if strobe is enabled on Lumina (off by default, max 3/s) |
| 7 | Direction | 0–84 forward, 85–169 reverse, 170–255 mirror |
| 8 | Variation | Extra sparkle/variation |
| 9–11 | Red / Green / Blue | Main colour (equal R=G=B gives white) |
| 12–16 | (reserved) | Ignored in v1 |

### 5.1 Build the fixture (Fixture Manager)

Same tool as Part 1.2. The steps follow SoundSwitch's "Creating a profile for an RGB par can" article. **UNVERIFIED:** exact labels in SoundSwitch 2.11.

1. Open **Fixture Manager**, then **File → New Fixture**.
2. Pick the same manufacturer as before; Model: `Lumina LUMINA-16`.
3. **Add Mode** with **16** channels (e.g. named `Main Mode`). Use **one cell**, named e.g. `Main`, **not** `MASTER`; it's a single fixture, not 4 strands. Verified 2026-10-14, SoundSwitch 2.11.
4. Add these functions, each with **1** channel at the given channel number (the **Coarse** column):
   - **Intensity** (SoundSwitch's built-in dimmer function; there's no "Dimmer" in 2.11) → channel **1**
   - **Strobe** (shutter/strobe) → channel **6**
   - **Red** → **9**, **Green** → **10**, **Blue** → **11**
5. Add the control channels SoundSwitch doesn't drive directly as **Attributes**:
   - Effect → **2**
   - Speed → **3**
   - Reactivity → **4**
   - Density → **5**
   - Direction → **7**
   - Variation → **8**
6. Leave 12–16 empty, and remove any default functions you didn't add.
7. Click **Fixtures → Check Fixture**. If it complains about a **MASTER** cell, a missing function, or unused channels, tell Claude Code exactly what it says.
8. Click **File → Save Workspace**.

### 5.2 Patch it (SoundSwitch Edit Mode)

1. Create a new venue so the old one stays intact: click **+** at the top of the workspace, type `Lumina 16`, and press **Return**.
2. Open the **DMX Map**, select **Universe 1**, search for `LUMINA-16`, and drag the 16-channel mode onto address **1**.
3. In **Preferences → Hardware**, check that **Lumina S3** is still on **Universe One**.

### 5.3 Gate test: a SoundSwitch control changes the strip

1. Tell Claude Code you're ready. Claude switches **Art-Net control on** (`/api/control?artnet=1`). Strobe stays off.
2. With the `Lumina 16` venue selected, play the **"Red"** Autoloop. Strand 1 should turn red (dimmed by the 25% cap).
3. Change the colour (another Autoloop, or a colour override while it plays). **The strip should change colour live.** The webcam watches too.
4. Optional: in a script or Autoloop, set the **Effect** attribute to another band (e.g. 128 for Rave Chase) and watch the effect change.

Without Art-Net control (Claude switches it off, or Lumina reboots), Lumina goes back to its own web settings and audio mode.
