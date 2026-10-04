# Outpost Kaloki X — Recompiled

A native PC port of the 2005 Xbox Live Arcade game **Outpost Kaloki X** (NinjaBee), built by
statically recompiling the original Xbox 360 executable to C++ with the
[ReXGlue SDK](https://github.com/rexglue/rexglue-sdk).

**Status:** early — builds and boots to the title screen on Windows (D3D12 + SDL audio).
Gameplay, saving and achievements are not yet tested.

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
