# Outpost Kaloki X — Recompiled

A native PC port of the 2005 Xbox Live Arcade game **Outpost Kaloki X** (NinjaBee), the enhanced
Xbox 360 edition of *Outpost Kaloki*, built by statically recompiling the original Xbox 360 executable
to C++ with the [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk).

**Status:** early — builds and boots to the title screen on Windows (D3D12 + SDL audio).
Gameplay, saving and achievements are not yet tested. The supported build is the original 2005 release
(v0.0.1.1); the October 2006 title update, which fixed some achievement and gold-medal bugs, is not yet
applied.

## Why recompile? Isn't there a PC version?

There is, but it isn't this game. The original *Outpost Kaloki* came out on Windows in 2004 and is still
sold on [itch.io](https://ninjabee.itch.io/outpost-kaloki). The Xbox 360 release a year later,
*Outpost Kaloki X*, was a substantially expanded edition, and the PC version was never updated to match.
The only way to play the definitive version has been on a 360 or in an emulator.

What *Outpost Kaloki X* has over the PC original:

| | PC *Outpost Kaloki* (2004) | Xbox 360 *Outpost Kaloki X* (2005) |
|---|---|---|
| Levels | Base set | **More than twice as many levels**[^wiki] |
| Story campaigns | Single "save the princess" story[^itch] | **Two full campaigns**: Adventure Story (10+ chapters) and War Story (6+ missions), 25+ levels combined[^pr][^ach] |
| Scenarios | Fewer | **11 sandbox and scenario levels** incl. The Eight-Port Challenge, The Hammer and Survival[^pr][^ach] |
| Challenges | — | **Time challenges and leaderboards** with gold-medal times[^wiki] |
| Graphics | Original | **Enhanced graphics**[^wiki][^itch] |
| Achievements | — | **12 achievements**[^ach] |
| Interface | Mouse/keyboard | **Redesigned for a controller**[^pr] |
| Downloadable content | — | Extra scenarios (The Swarm, Fireworks) and a planned downloadable story[^pr][^dlc] *(DLC not yet supported here)* |

NinjaBee's own PC store page acknowledges the Xbox edition has improved graphics and additional levels, with
no PC update planned.[^itch] This project aims to keep that expanded edition playable natively on modern PCs.

[^wiki]: [Wikipedia: Outpost Kaloki](https://en.wikipedia.org/wiki/Outpost_Kaloki)
[^pr]: [NinjaBee press release, 2005](https://gamedeveloper.com/press-release/outpost-kaloki-x-to-debut-on-xbox-360-live-arcade)
[^itch]: [Outpost Kaloki on itch.io (NinjaBee)](https://ninjabee.itch.io/outpost-kaloki)
[^dlc]: [The Swarm Scenario on Deku Deals](https://www.dekudeals.com/items/outpost-kaloki-x-the-swarm-scenario-outpost-kaloki)
[^ach]: Chapter, mission and scenario names come from the game's own achievement data
    (extract it with `rexglue init achievements`).

## Getting started

> [!IMPORTANT]
> This repository contains **no game code or assets**. You must provide your own legally
> obtained copy of the Outpost Kaloki X XBLA package. Do not open issues asking for game files.

> [!NOTE]
> **AI disclosure:** this project is developed almost entirely with Claude Code (Anthropic).
> The analysis fixes, tooling, runtime workarounds and docs in this repo were written by the AI
> under the direction of the repository owner.

## Requirements (Windows)
- Your own dump of the XBLA package (title ID `584107DB`)
- Visual Studio 2022 Build Tools: *Desktop development with C++* + *C++ Clang Compiler for Windows* + *MSBuild support for LLVM (clang-cl)*
- CMake 3.25+ and Ninja

```bash
winget install Kitware.CMake Ninja-build.Ninja
```

## Build
```powershell
.\setup.ps1 -Package "C:\path\to\Outpost Kaloki X"   # downloads SDK, extracts game, runs codegen
okx\build.bat                                         # compiles (RelWithDebInfo)
okx\run.bat                                           # launches
```

The game runs as the **full version** by default (XBLA titles shipped as trials that unlock on purchase,
which is no longer possible). Use `okx\run.bat --license_mask=0` to play the trial.

## Features
- **Launcher** before the game starts (turn it off in the launcher; hold Shift at startup to bring it back):
  - **Game**: install the game files straight from your XBLA package, full-game / trial toggle
  - **Display**: fullscreen / windowed, window size, **frame-rate cap (30 / 60 / 120 / 144 / 165 / 240 / unlimited)**,
    VSync, 16:9 letterbox or stretch
  - **Graphics**: internal resolution up to 6x (8K), FXAA, native 2x MSAA, anisotropic filtering up to 16x
  - **Controls**: invert the camera (right stick) and left stick per axis, remap any controller button,
    keyboard & mouse play with rebindable keys
- **Unlocked frame rate**: the Xbox 360 version ran at 30 FPS; this port defaults to 60 and can go higher.
  The game times everything by real elapsed time, so it runs at the correct speed at any frame rate.
- Full game unlocked by default.

## How it works
- `tools/extract_stfs.ps1` unpacks the STFS package (`default.xex` + data).
- `rexglue codegen` translates every PowerPC function in `default.xex` into C++ (`okx/generated/`, not committed).
- `okx/overrides.toml` and `okx/missing_funcs.toml` fix function boundaries the automatic analysis misses
  (functions only reachable through pointers: static initializers, vtables, callbacks).
- `okx/src/` holds the app: GPU/audio backend setup, license default, and a workaround for a ReXGlue
  floating-point exception bug.

See [NOTES.md](NOTES.md) for detailed technical notes on every fix.

## Credits
- [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk), [Xenia](https://github.com/xenia-project/xenia),
  [XenonRecomp](https://github.com/hedge-dev/XenonRecomp)
- Outpost Kaloki X © NinjaBee / Wahoo Studios. This project is not affiliated with or endorsed by
  NinjaBee, Microsoft or Xbox.
