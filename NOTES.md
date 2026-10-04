# Outpost Kaloki X — Xbox 360 static recompilation

Toolchain: [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk) v0.10.0 (prebuilt, in `tools/rexglue/win-amd64`).
Build: VS 2022 Build Tools (clang 19.1.5 works despite the docs asking for 20), CMake 4.4.3, Ninja 1.13.2.

**Status (2026-10-04): builds and boots to the title screen** with D3D12 rendering and SDL audio.

## Game
- Package: XBLA STFS `LIVE` file (supply your own dump)
- Title ID `584107DB`, version 0.0.1.1, built 2005-11-04 (launch-era XBLA title)
- Contents: `default.xex` (3.2 MB) + `fsxb2/blockfiles/*.blk.bz2` (engine's own chunked bzip2 container, read by game code) + `arcadefiles/` (achievement/rating PNGs)

## Quick start
```
okx\build.bat            # configure + build (okx-relwithdebinfo by default)
okx\run.bat              # launch (full-game license by default)
okx\run.bat --license_mask=0   # launch as the trial
```

## Layout
| Path | What |
|---|---|
| `tools/extract_stfs.ps1`, `tools/StfsExtract.cs` | STFS extractor (no Python needed) |
| `tools/FindMissingFuncs.cs` | Finds functions only referenced via pointers (needs `okx/image.bin`) |
| `tools/snap_window.ps1` | Screenshot of just the game window |
| `game/` | Pristine extracted package |
| `okx/` | ReXGlue project (`assets/` = game files, `generated/` = recompiled C++) |
| `okx/overrides.toml` | Hand-made analysis fixes |
| `okx/missing_funcs.toml` | Scanner-found indirect-only functions (81) |
| `okx/src/outpost_kaloki_x_app.h` | App: GPU/audio setup, license mask default, image dump |
| `okx/src/fpe_guard.cpp` | Workaround for ReXGlue FP-exception bug (below) |

## Steps done
1. `tools/extract_stfs.ps1 -Package <pkg> -OutDir game`
2. `rexglue init --project-name outpost_kaloki_x --xex-path assets/default.xex --game-root assets` (inside `okx/`)
3. `rexglue codegen outpost_kaloki_x_manifest.toml` → 8340 + 83 functions, 0 errors
4. `build.bat` → `out/build/okx-relwithdebinfo/outpost_kaloki_x.exe`

## Analysis fixes
- `overrides.toml`:
  - `0x82294C98–0x82294D60`: message dispatcher only reachable via thunk `sub_8229A6E8` (vtable slot).
  - `0x82073660–0x820736B0`: qsort comparator; analyzer had split it at `0x82073680`.
- `missing_funcs.toml` (generated): static initializers (CRT `__xc_a` table — first runtime crash was
  `0x822C7110`), vtable/callback targets. Regenerate: run once with `OKX_DUMP_IMAGE=okx/image.bin`, then
  `Add-Type tools/FindMissingFuncs.cs; [FindMissingFuncs]::Run(image, partition.json, generated dir, 0x82060000, 0x822C807C, out)`.
  Then prune: candidates that cause unresolved branches are C++ EH catch funclets (`mr r8,r8` padding,
  referenced from EH tables in .rdata) living inside their parent function — remove them. Import thunks
  (`0x822C769C+`) are excluded automatically.

## Runtime fixes / settings
- **GPU**: `rexglue_setup_target(... GPU_PLUGINS xenos)` + `config.gpu_plugin = "xenos"` (else no rendering).
- **Audio**: `REX_AUDIO_BACKEND(rex::audio::sdl::SDLAudioSystem)`.
- **License**: `license_mask` cvar defaults to 1 (full game). ReXGlue's `XamContentGetLicenseMask`
  (`src/kernel/xam/xam_content.cpp`) returns this cvar verbatim, same as Xenia. Upstream default is 0 (trial).
- **FP exception bug (ReXGlue v0.10.0)**: only `XThread::Execute` calls `ctx->fpscr.InitHost()`. Guest
  callbacks run on host threads (audio worker, GPU interrupt) have cached `fpscr.csr == 0`, so the first
  `enableFlushMode()` writes MXCSR `0x8040` → all FP exceptions unmasked → `STATUS_FLOAT_INEXACT_RESULT`
  (0xC000008F) kills the process. `fpe_guard.cpp` masks + repairs `ctx->fpscr.csr` on first trap per thread.
  Proper fix upstream: seed `csr` via `InitHost()` in `FunctionDispatcher::Execute` / `ThreadState` ctor.

## Known log noise (harmless so far)
- `NtCreateFile 'saved'` fails at boot (before any save content exists); `viewer.ini` / `gametest.init` are dev files.

## Next
- Play-test: input, gameplay, saving (`XamContentCreate`), achievements, audio/XMA.
- Confirm no "Unlock Full Game" / trial limits with license_mask=1.
- Report the FPSCR bug upstream.
