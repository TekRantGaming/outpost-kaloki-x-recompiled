## Outpost Kaloki X PC Port v1.0.1

Play the Xbox 360 edition of Outpost Kaloki X natively on Windows, with a launcher and modern PC options.

### New in v1.0.1
- **No more antivirus warnings.** Some antivirus programs, including Microsoft Defender, wrongly flagged `rexruntime.dll` from the ReXGlue SDK as a threat. The builder now uses `rexruntime.dll` rebuilt from the same ReXGlue v0.10.0 source, which they don't flag (antivirus false positive, rexglue/rexglue-sdk#485). Nothing else changed: the game plays exactly like v1.0.0. If you built v1.0.0, run the new builder again.

### How to install
1. Download the **OutpostKalokiX-Builder** zip below and unzip it.
2. Double-click **Build Outpost Kaloki X.bat**.
3. Let it install the build tools if it asks (a one-time download of about 6 GB).
4. Pick your own Outpost Kaloki X Xbox Live Arcade package (title ID `584107DB`).
5. After 10 to 20 minutes the game is ready in the **OutpostKalokiX** folder.

The builder makes the PC version on your computer from your own copy of the game. **No game files are included or downloaded.**

### What's in it
- A launcher with art from your copy of the game
- Up to 8K resolution, FXAA, 2x MSAA and 16x texture filtering
- 30 to 240 fps or unlimited, at the game's normal speed
- Xbox 360 style achievement pop-ups with your choice of sound
- Camera inversion and speed, deadzone, vibration, button remapping, keyboard and mouse
- The full game unlocked

Linux AppImage coming soon.

This build was made with AI (Claude Code). See the README for details.
