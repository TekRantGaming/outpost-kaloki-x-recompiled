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
okx\run.bat              # launch (launcher first; hold Shift to force it when disabled)
okx\run.bat --okx_frame_rate=0 --okx_launcher=false   # any setting can be overridden per run
```
Settings live in `outpost_kaloki_x.toml` next to the exe (written by the launcher; only non-default values,
command-line overrides are never saved). Game files default to `game\` next to the exe (the launcher installs
there from an XBLA package); `run.bat` points at `okx\assets` instead.

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

## Frame rate (src/frame_stats.cpp, settings.cpp)
- **The 30 FPS lock was an emulation artifact.** ReXGlue's `d3d12_submit_on_primary_buffer_end=true` (default)
  submits + waits on the host GPU at every ring-buffer end → ~27 ms/frame, which then rounds to 2 vblanks at
  60 Hz = 30 FPS. With it off the title runs uncapped (110 FPS = user's system cap).
- Guest `vsync` only paces the emulated console: 60 Hz fake vblank + `Sleep(wait/256 ms)` per poll in
  `WAIT_REG_MEM` (command_processor.cpp:1031). Host present is always immediate + tearing allowed
  (`d3d12_allow_variable_refresh_rate_and_tearing`). So the port forces guest `vsync=false` and paces with its
  own limiter (`okx_frame_rate`: high-res waitable timer + spin in the swap hook). Launcher "VSync" maps to the
  tearing flag.
- Game speed stays correct: `sub_820DE550` is a delta-time timer (QPC via `sub_820E94D8`, scale 60 → delta in
  1/60 s units, stored at `0x822D24E8`). Measured FPS × delta = 60 at 30/60/110 FPS.
- `sub_820F3CF8` = D3D swap (calls VdSwap), hooked for FPS counting (F3 overlay) + limiter.
- Busy-wait seen in profiles: `sub_820F5B50`/`sub_82105B08` = D3D "wait for GPU fence" spin.
- Overrides must be applied in `OnPostSetup`: GPU-plugin cvars don't exist until the plugin loads
  (SetFlagByName on an unregistered cvar silently fails; CLI values are queued and work).

## Input (src/input_remap.cpp)
- `sub_820E9678` = XInputGetState wrapper (only pad read). Post-call rewrite of guest XINPUT_STATE (BE):
  button/trigger remap (`okx_map_<button>`), stick inversion (`okx_invert_{ls,rs}_{x,y}`). Right stick = camera.
- Keyboard: ReXGlue `mnk_mode` + `keybind_*` cvars feed the same path, so remaps/inversion apply too.
- No gamertag setting: the title never imports XamUserGetName/GetGamerTag (profile name is hardcoded "User"
  in ReXGlue user_profile.cpp; also used as the save folder name).

## Launcher (src/launcher.cpp)
- Design follows the SMS launcher: header banner, sidebar pages (Play, Display, Graphics, Gameplay, Controls,
  Achievements, About), label+description rows with segmented controls, pill PLAY button, Enter/Esc/Ctrl+S.
  Theme = deep-space blues + Kaloki-planet green. Its ImGui style is restored when it closes.
- Art comes only from the player's files / the running game (src/art.cpp): `arcadefiles/titleicon.png` +
  achievement icons mapped via `ArcadeInfo.xml`; header = title-screen capture taken 14 s after the first frame
  of the first play (`Documents/outpost_kaloki_x/launcher/title.bmp`, via GraphicsSystem presenter
  `CaptureGuestOutput`); achievement names written to `launcher/achievements.toml` in OnPostSetup; unlock state
  read from ReXGlue's `achievements/584107DB.toml`. Procedural starfield + planet until then.
- Game art inside `Full_Common.blk.bz2` (FE_logo etc.) is PTC-compressed (`PTC+MSHM`, Microsoft Progressive
  Transform Codec) - not decodable outside the game. Block file format: u32 chunk count, u32 total size, then
  per chunk (u32 uncompressed, u32 compressed, bzip2 data); archive = 1741 x 0x60-byte entries
  (name[0x44], BE offset, BE size, ...) then data.
- Upscalers: user chose none. DLSS/FSR2+ need engine motion vectors (absent); prebuilt SDK lacks FidelityFX.
  `okx_render_quality` presets instead (Supersample/Native/Quality/Balanced/Performance/Ultra Performance =
  output/render ratios 0.5/1/1.5/1.7/2/3, rounded to integer multiples of 720p), applied before the GPU starts.
- Shown from `OnFinalizePaths` (window + ImGui exist, runtime not yet built) when `okx_launcher`, Shift held,
  or game files missing. PLAY → `CallInUIThreadDeferred(resume)`.
- GPU plugin DLL is preloaded in `OnConfigurePaths` (before config load) so its cvars (resolution_scale,
  swap_post_effect, native_2x_msaa, anisotropic_override) are registered, loaded and saveable.
- Presenter/window cvars (`present_effect`, `window_width/height`, `monitor`) are read before the launcher →
  changing them relaunches the exe with `--okx_skip_launcher=true`.
- `window_width/height` are logical (96-DPI) pixels; launcher converts with the window DPI.
- Combos only offer values the cvar allows: the prebuilt SDK has no FidelityFX, so `present_effect` only
  accepts `bilinear` (CAS/FSR hidden).
- Installer: C++ STFS extractor (`src/stfs.cpp`, port of tools/StfsExtract.cs), checks title ID 584107DB.
- Port defaults (`ApplyPortDefaults`, changed *defaults* so config/CLI still win): `license_mask=1`,
  `fullscreen=false`.

## Dev tools
- `OKX_PROFILE=<s>` → profile.txt per-thread hotspots (symbolize with VS `llvm-symbolizer --relative-address`).
- `OKX_DUMP_IMAGE=<file>` → decrypted guest image. `tools/snap_window.ps1`, `tools/click_window.ps1`
  (screenshot / click the game window for automated UI checks).

## Known log noise (harmless so far)
- `NtCreateFile 'saved'` fails at boot (before any save content exists); `viewer.ini` / `gametest.init` are dev files.

## Next
- Play-test with a controller: camera inversion, remapping, keyboard mode, gameplay at 60+ FPS (check physics/
  animation timing in actual gameplay, not just the title screen), saving, achievements, audio.
- Report the FPSCR bug and the submit-on-primary-buffer-end stall upstream.
