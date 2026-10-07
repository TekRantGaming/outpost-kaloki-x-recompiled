## Outpost Kaloki X PC Port v1.1.0

Play the Xbox 360 edition of Outpost Kaloki X natively on Windows and Linux, with a launcher and modern PC options.

### Install
1. Unzip **OutpostKalokiX-v1.1.0-windows-x64.zip** anywhere and run **outpost_kaloki_x.exe**.
2. On the launcher's **Play** page click **Install from package...** and pick your own Outpost Kaloki X Xbox Live Arcade package.
3. Press **PLAY**.

**Linux:** download **OutpostKalokiX-v1.1.0-linux-x86_64.AppImage** instead, make it executable, run it and install from your package the same way.

**No game files are included.** You need your own package, this exact release:

| | |
| --- | --- |
| Title ID | `584107DB` (Xbox Live Arcade, content type `000D0000`) |
| Package size | 21,557,248 bytes |
| Game version | `default.xex` 0.0.1.1 (2005-11-04), 3,252,224 bytes, CRC32 `CCB0ACE9` |

The launcher checks both and tells you if your package is a different version.

### New in v1.1.0
- **Ready to play:** a normal download now, no more building it yourself. If you built v1.0.x, download this one and install your package in its launcher (your saves are kept in Documents\outpost_kaloki_x).
- **Linux AppImage.**
- **Updates:** the launcher checks GitHub when it opens and updates with one click (About page: Updates).
- **Letterbox works at any window size:** the picture keeps its 16:9 shape in a 4:3 or other window.
- **No missing effects on first sight:** effects wait to be prepared the first time they appear (Graphics page: Shader preparing).
- Settings given on the command line no longer erase your saved choices.
- Checked: saving and loading work, and the Leaderboards menu shows the game's own "not signed in to Xbox Live" message.
- VirusTotal scans, both clean: [Windows zip](https://www.virustotal.com/gui/file/885853359de0a11c0de3d9263558bc4000f88f81a604a9f9c5ad1350c83f7eca), [Linux AppImage](https://www.virustotal.com/gui/file/bac70557a432e36d8488c483ed917062b717d26704861277f57c38bb2611150c).

### What's in it
- A launcher with art from your copy of the game
- Up to 8K resolution, FXAA, 2x MSAA and 16x texture filtering
- 30 to 240 fps or unlimited, at the game's normal speed
- Xbox 360 style achievement pop-ups with your choice of sound
- Camera inversion and speed, deadzone, vibration, button remapping, keyboard and mouse
- The full game unlocked

Requires Windows 10 or 11 (64-bit) and a DirectX 12 graphics card, or 64-bit Linux with Vulkan.

This build was made with AI (Claude Code). See the README for details.
