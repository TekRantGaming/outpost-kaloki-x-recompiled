// outpost_kaloki_x - ReXGlue Recompiled Project
//
// Customize your app by overriding virtual hooks from rex::ReXApp.

#pragma once

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>

#include <imgui.h>

#include <rex/audio/sdl/sdl_audio_system.h>
#include <rex/cvar.h>
#include <rex/filesystem.h>
#include <rex/rex_app.h>
#include <rex/runtime.h>
#include <rex/ui/window.h>
#include <rex/ui/windowed_app_context.h>

#include "frame_stats.h"
#include "launcher.h"
#include "platform.h"
#include "settings.h"

class OutpostKalokiXApp : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(
      rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<OutpostKalokiXApp>(new OutpostKalokiXApp(ctx, "outpost_kaloki_x",
        PPCImageConfig));
  }

 protected:
  void OnConfigurePaths(rex::PathConfig& paths) override {
    // Register the GPU plugin's cvars before the config is read so the
    // launcher can edit and save them, and set the port's own defaults.
    okx::PreloadGpuPlugin();
    okx::ApplyPortDefaults();
    if (paths.game_data_root.empty())
      paths.game_data_root = rex::filesystem::GetExecutableFolder() / "game";
  }

  std::optional<rex::PathConfig> OnFinalizePaths(
      const rex::PathConfig& defaults, std::function<void(rex::PathConfig)> resume) override {
    const bool skip_once = REXCVAR_GET(okx_skip_launcher);
    rex::cvar::ResetToDefault("okx_skip_launcher");  // never persist it
    const bool files_ok = okx::GameFilesPresent(defaults.game_data_root);
    const bool show = !files_ok || okx::IsShiftHeld() || (REXCVAR_GET(okx_launcher) && !skip_once);
    if (!show) return defaults;

    okx::LauncherCallbacks cb;
    cb.play = [this, resume, defaults] {
      app_context().CallInUIThreadDeferred([resume, defaults] { resume(defaults); });
    };
    cb.restart_and_play = [this] {
      okx::RelaunchSelf(L"--okx_skip_launcher=true");
      app_context().CallInUIThreadDeferred([this] { app_context().QuitFromUIThread(); });
    };
    cb.quit = [this] {
      app_context().CallInUIThreadDeferred([this] { app_context().QuitFromUIThread(); });
    };
    cb.set_fullscreen = [this](bool on) {
      if (window()) window()->SetFullscreen(on);
    };
    cb.dpi_scale = [this] {
      return window() ? double(window()->GetDpi()) / window()->GetMediumDpi() : 1.0;
    };
    cb.screen_size = [] { return okx::PrimaryScreenSize(); };
    okx::ShowLauncher(imgui_drawer(), defaults.game_data_root, defaults.config_path, std::move(cb));
    return std::nullopt;
  }

  void OnConfigureFonts(ImFontAtlas* atlas) override {
    // Segoe UI reads far better than the default pixel font in the launcher
    // and overlays; fall back silently if it isn't installed.
    okx::LoadUiFont(atlas);
  }

  void OnPreSetup(rex::RuntimeConfig& config) override {
    if (!config.graphics && config.gpu_plugin.empty()) config.gpu_plugin = "xenos";
    if (!config.audio_factory)
      config.audio_factory = REX_AUDIO_BACKEND(rex::audio::sdl::SDLAudioSystem);
  }

  void OnPostSetup() override {
    okx::ApplyRuntimeOverrides();

    // Debug aid: set OKX_DUMP_IMAGE=<file> to write the decrypted guest image
    // (0x82000000-0x823A0000) for offline analysis.
    const char* dump_path = std::getenv("OKX_DUMP_IMAGE");
    if (!dump_path || !*dump_path) return;
    const uint8_t* base = runtime()->virtual_membase();
    std::ofstream out(dump_path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(base + PPCImageConfig.image_base),
              PPCImageConfig.image_size);
  }

  void OnCreateDialogs(rex::ui::ImGuiDrawer* drawer) override {
    (void)drawer;
    SetGuestFrameStats(okx::GetGuestFrameStats);  // F3 overlay "Guest: N FPS"
  }
};
