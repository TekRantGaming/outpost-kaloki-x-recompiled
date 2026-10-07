// outpost_kaloki_x - ReXGlue Recompiled Project
//
// Customize your app by overriding virtual hooks from rex::ReXApp.

#pragma once

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

#include <imgui.h>

#include <rex/audio/sdl/sdl_audio_system.h>
#include <rex/cvar.h>
#include <rex/filesystem.h>
#include <rex/kernel/xboxkrnl/video.h>
#include <rex/logging.h>
#include <rex/rex_app.h>
#include <rex/runtime.h>
#include <rex/system/achievement_manager.h>
#include <rex/system/interfaces/graphics.h>
#include <rex/ui/immediate_drawer.h>
#include <rex/ui/presenter.h>
#include <rex/ui/window.h>
#include <rex/ui/windowed_app_context.h>

#include "art.h"
#include "frame_stats.h"
#include "launcher.h"
#include "overlay.h"
#include "platform.h"
#include "settings.h"
#include "toast.h"

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
    user_data_root_ = defaults.user_data_root;
    const bool skip_once = REXCVAR_GET(okx_skip_launcher);
    rex::cvar::ResetToDefault("okx_skip_launcher");  // never persist it
    const bool files_ok = okx::GameFilesPresent(defaults.game_data_root);
    const bool show = !files_ok || okx::IsShiftHeld() || (REXCVAR_GET(okx_launcher) && !skip_once);
    if (!show) {
      okx::ApplyRenderPreset(OutputSize().second);
      return defaults;
    }

    okx::LauncherCallbacks cb;
    cb.play = [this, resume, defaults] {
      app_context().CallInUIThreadDeferred([this, resume, defaults] {
        okx::ApplyRenderPreset(OutputSize().second);
        resume(defaults);
      });
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
    cb.output_size = [this] { return OutputSize(); };
    okx::ShowLauncher(imgui_drawer(), immediate_drawer(),
                      {defaults.game_data_root, defaults.user_data_root, defaults.config_path},
                      std::move(cb));
    return std::nullopt;
  }

  void OnConfigureFonts(ImFontAtlas* atlas) override { okx::LoadUiFont(atlas); }

  void OnPreSetup(rex::RuntimeConfig& config) override {
    if (!config.graphics && config.gpu_plugin.empty()) config.gpu_plugin = "xenos";
    if (!config.audio_factory)
      config.audio_factory = REX_AUDIO_BACKEND(rex::audio::sdl::SDLAudioSystem);
  }

  void OnPostSetup() override {
    okx::ApplyRuntimeOverrides();
    {
      rex::system::X_VIDEO_MODE mode{};
      rex::kernel::xboxkrnl::VdQueryVideoMode(&mode);
      REXLOG_INFO("OKX: guest video mode {}x{}, widescreen {}", uint32_t(mode.display_width),
                  uint32_t(mode.display_height), uint32_t(mode.is_widescreen));
    }

    // Give the launcher the achievement names (read from the game by the runtime).
    if (!user_data_root_.empty())
      okx::art::WriteAchievementCache(achievements().ListAchievements(),
                                      okx::art::AchievementCachePath(user_data_root_));
    ScheduleTitleCapture();
    ScheduleWelcomeAchievement();
    ScheduleDevCaptures();

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
    SetGuestFrameStats(okx::GetGuestFrameStats);  // F3 overlay "Guest: N FPS"
    okx::CreateFpsOverlay(drawer);
  }

  // Xbox 360-style toast with a chime for the game's achievements.
  std::unique_ptr<rex::ui::AchievementNotificationDialog> CreateAchievementNotificationDialog() override {
    auto toast = std::make_unique<okx::AchievementToast>(imgui_drawer(), immediate_drawer(), game_data_root(),
                                                         user_data_root_);
    toast_ = toast.get();
    return toast;
  }

 private:
  // Size the game is shown at: the monitor in fullscreen, else the window.
  std::pair<int, int> OutputSize() const {
    if (REXCVAR_QUERY(bool, fullscreen) || !window()) return okx::PrimaryScreenSize();
    return {int(window()->GetActualPhysicalWidth()), int(window()->GetActualPhysicalHeight())};
  }

  // On first play, keep a frame of the game's title screen as launcher art.
  void ScheduleTitleCapture() {
    if (user_data_root_.empty()) return;
    const auto path = okx::art::TitleCapturePath(user_data_root_);
    std::error_code ec;
    if (std::filesystem::exists(path, ec)) return;
    okx::RunAfterFirstFrame(14.0, [this, path] {
      app_context().CallInUIThread([this, path] {
        rex::ui::RawImage image;
        auto* gfx = runtime() ? runtime()->graphics_system() : nullptr;
        auto* presenter = gfx ? gfx->presenter() : nullptr;
        if (presenter && presenter->CaptureGuestOutput(image) && okx::art::SaveTitleCapture(image, path))
          REXLOG_INFO("OKX: saved launcher art {}x{}", image.width, image.height);
      });
    });
  }

  // Developer test aid: OKX_DEV_CAPTURE="18;25.5" saves the game's own output
  // (not the desktop) at those many seconds after the first frame, as
  // <user data>/dev_capture/<seconds>.bmp. Ignored unless the variable is set.
  void ScheduleDevCaptures() {
    const char* env = std::getenv("OKX_DEV_CAPTURE");
    if (!env || !*env || user_data_root_.empty()) return;
    std::string list(env);
    for (size_t pos = 0; pos < list.size();) {
      size_t end = list.find(';', pos);
      if (end == std::string::npos) end = list.size();
      const std::string item = list.substr(pos, end - pos);
      pos = end + 1;
      if (item.empty()) continue;
      const auto path = user_data_root_ / "dev_capture" / (item + ".bmp");
      okx::RunAfterFirstFrame(std::atof(item.c_str()), [this, path] {
        app_context().CallInUIThread([this, path] {
          rex::ui::RawImage image;
          auto* gfx = runtime() ? runtime()->graphics_system() : nullptr;
          auto* presenter = gfx ? gfx->presenter() : nullptr;
          std::error_code ec;
          std::filesystem::create_directories(path.parent_path(), ec);
          if (presenter && presenter->CaptureGuestOutput(image) && okx::art::SaveTitleCapture(image, path))
            REXLOG_INFO("OKX: dev capture {}", path.filename().string());
        });
      });
    }
  }

  // The port's own "Welcome" achievement: unlocks a few seconds into the first
  // play so players learn the game has achievements.
  void ScheduleWelcomeAchievement() {
    const auto& welcome = okx::PortAchievements().front();
    if (user_data_root_.empty() || okx::IsPortAchievementUnlocked(user_data_root_, welcome.id)) return;
    okx::RunAfterFirstFrame(6.0, [this, &welcome] {
      if (okx::UnlockPortAchievement(user_data_root_, welcome.id) && toast_) toast_->Show(welcome.title, 0, 0);
    });
  }

  std::filesystem::path user_data_root_;
  okx::AchievementToast* toast_ = nullptr;  // owned by ReXApp
};
