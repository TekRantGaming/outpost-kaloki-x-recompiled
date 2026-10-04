// outpost_kaloki_x - ReXGlue Recompiled Project
//
// Customize your app by overriding virtual hooks from rex::ReXApp.

#pragma once

#include <cstdlib>
#include <fstream>

#include <rex/audio/sdl/sdl_audio_system.h>
#include <rex/cvar.h>
#include <rex/rex_app.h>
#include <rex/runtime.h>

#include "frame_stats.h"

class OutpostKalokiXApp : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(
      rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<OutpostKalokiXApp>(new OutpostKalokiXApp(ctx, "outpost_kaloki_x",
        PPCImageConfig));
  }

  // Override virtual hooks for customization:
  // void OnPostInitLogging() override {}
  void OnPreSetup(rex::RuntimeConfig& config) override {
    // XBLA titles ship as trial builds and unlock via XamContentGetLicenseMask.
    // Default to the full (purchased) license, like Xenia's license_mask = 1;
    // an explicit --license_mask / config value still wins.
    if (!rex::cvar::HasNonDefaultValue("license_mask"))
      rex::cvar::SetFlagByName("license_mask", "1");

    if (!config.graphics && config.gpu_plugin.empty()) config.gpu_plugin = "xenos";
    if (!config.audio_factory)
      config.audio_factory = REX_AUDIO_BACKEND(rex::audio::sdl::SDLAudioSystem);
  }
  // void OnLoadXexImage(std::string& xex_image) override {}
  // void OnPostLoadXexImage() override {}
  // Debug aid: set OKX_DUMP_IMAGE=<file> to write the decrypted guest image
  // (0x82000000-0x823A0000) for offline analysis.
  void OnPostSetup() override {
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
  // std::unique_ptr<rex::ui::ImGuiDialog> CreateAchievementsOverlay() override;
  // std::unique_ptr<rex::ui::AchievementNotificationDialog>
  // CreateAchievementNotificationDialog() override;
  // void OnShutdown() override {}
  // void OnConfigurePaths(rex::PathConfig& paths) override {}
};
