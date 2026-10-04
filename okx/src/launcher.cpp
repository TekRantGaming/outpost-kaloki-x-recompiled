#include "launcher.h"

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>
#pragma comment(lib, "comdlg32.lib")
#endif

#include <imgui.h>

#include <rex/cvar.h>
#include <rex/filesystem.h>
#include <rex/logging.h>
#include <rex/ui/keybinds.h>
#include <rex/ui/virtual_key.h>

#include "settings.h"
#include "stfs.h"

namespace okx {
namespace {

constexpr uint32_t kTitleId = 0x584107DB;

// --- cvar helpers (all settings are edited live through the registry) ---

std::string Get(const char* name) { return rex::cvar::GetFlagByName(name); }
bool GetBool(const char* name) { return Get(name) == "true"; }
int GetInt(const char* name, int fallback = 0) {
  try {
    return std::stoi(Get(name));
  } catch (...) {
    return fallback;
  }
}
void Set(const char* name, const std::string& value) { rex::cvar::SetFlagByName(name, value); }
void SetBool(const char* name, bool v) { Set(name, v ? "true" : "false"); }
void SetInt(const char* name, int v) { Set(name, std::to_string(v)); }

const rex::cvar::FlagEntry* FindFlag(std::string_view name) {
  for (auto& e : rex::cvar::GetRegistry())
    if (e.name == name) return &e;
  return nullptr;
}

void HelpMarker(const char* text) {
  ImGui::SameLine();
  ImGui::TextDisabled("(?)");
  if (ImGui::BeginItemTooltip()) {
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 30.0f);
    ImGui::TextUnformatted(text);
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
  }
}

// Labeled combo over (label, value) pairs bound to a cvar's string value.
// Options the cvar doesn't accept in this build (its allowed-values list) are
// hidden; with nothing to choose, the setting is shown as unavailable.
bool ComboCvar(const char* label, const char* cvar,
               std::vector<std::pair<const char*, std::string>> options) {
  if (const auto* flag = FindFlag(cvar); flag && !flag->constraints.allowed_values.empty()) {
    const auto& allowed = flag->constraints.allowed_values;
    std::erase_if(options, [&](const auto& o) {
      return std::find(allowed.begin(), allowed.end(), o.second) == allowed.end();
    });
  }
  if (options.size() <= 1 || !FindFlag(cvar)) {
    ImGui::BeginDisabled();
    ImGui::TextUnformatted(label);
    ImGui::SameLine();
    ImGui::TextUnformatted("- not available in this build");
    ImGui::EndDisabled();
    return false;
  }
  const std::string cur = Get(cvar);
  int idx = 0;
  for (size_t i = 0; i < options.size(); ++i)
    if (options[i].second == cur) idx = static_cast<int>(i);
  bool changed = false;
  if (ImGui::BeginCombo(label, options[idx].first)) {
    for (size_t i = 0; i < options.size(); ++i) {
      if (ImGui::Selectable(options[i].first, static_cast<int>(i) == idx)) {
        Set(cvar, options[i].second);
        changed = true;
      }
    }
    ImGui::EndCombo();
  }
  return changed;
}

bool CheckboxCvar(const char* label, const char* cvar, bool invert = false) {
  bool v = GetBool(cvar) != invert;
  if (ImGui::Checkbox(label, &v)) {
    SetBool(cvar, v != invert);
    return true;
  }
  return false;
}

// ImGuiKey -> Win32 virtual-key code (ReXGlue's VirtualKey uses the same values).
int ImGuiKeyToVk(ImGuiKey k) {
  if (k >= ImGuiKey_A && k <= ImGuiKey_Z) return 'A' + (k - ImGuiKey_A);
  if (k >= ImGuiKey_0 && k <= ImGuiKey_9) return '0' + (k - ImGuiKey_0);
  if (k >= ImGuiKey_F1 && k <= ImGuiKey_F12) return 0x70 + (k - ImGuiKey_F1);
  if (k >= ImGuiKey_Keypad0 && k <= ImGuiKey_Keypad9) return 0x60 + (k - ImGuiKey_Keypad0);
  switch (k) {
    case ImGuiKey_Tab: return 0x09;
    case ImGuiKey_LeftArrow: return 0x25;
    case ImGuiKey_RightArrow: return 0x27;
    case ImGuiKey_UpArrow: return 0x26;
    case ImGuiKey_DownArrow: return 0x28;
    case ImGuiKey_PageUp: return 0x21;
    case ImGuiKey_PageDown: return 0x22;
    case ImGuiKey_Home: return 0x24;
    case ImGuiKey_End: return 0x23;
    case ImGuiKey_Insert: return 0x2D;
    case ImGuiKey_Delete: return 0x2E;
    case ImGuiKey_Backspace: return 0x08;
    case ImGuiKey_Space: return 0x20;
    case ImGuiKey_Enter: case ImGuiKey_KeypadEnter: return 0x0D;
    case ImGuiKey_LeftShift: return 0xA0;
    case ImGuiKey_RightShift: return 0xA1;
    case ImGuiKey_LeftCtrl: return 0xA2;
    case ImGuiKey_RightCtrl: return 0xA3;
    case ImGuiKey_LeftAlt: return 0xA4;
    case ImGuiKey_RightAlt: return 0xA5;
    case ImGuiKey_Semicolon: return 0xBA;
    case ImGuiKey_Equal: return 0xBB;
    case ImGuiKey_Comma: return 0xBC;
    case ImGuiKey_Minus: return 0xBD;
    case ImGuiKey_Period: return 0xBE;
    case ImGuiKey_Slash: return 0xBF;
    case ImGuiKey_GraveAccent: return 0xC0;
    case ImGuiKey_LeftBracket: return 0xDB;
    case ImGuiKey_Backslash: return 0xDC;
    case ImGuiKey_RightBracket: return 0xDD;
    case ImGuiKey_Apostrophe: return 0xDE;
    case ImGuiKey_KeypadDecimal: return 0x6E;
    case ImGuiKey_KeypadDivide: return 0x6F;
    case ImGuiKey_KeypadMultiply: return 0x6A;
    case ImGuiKey_KeypadSubtract: return 0x6D;
    case ImGuiKey_KeypadAdd: return 0x6B;
    default: return 0;
  }
}

#if defined(_WIN32)
std::filesystem::path BrowseForPackage() {
  wchar_t file[MAX_PATH] = L"";
  OPENFILENAMEW ofn{};
  ofn.lStructSize = sizeof(ofn);
  ofn.lpstrFilter = L"Xbox 360 package (no extension)\0*.*\0";
  ofn.lpstrFile = file;
  ofn.nMaxFile = MAX_PATH;
  ofn.lpstrTitle = L"Select your Outpost Kaloki X XBLA package";
  ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
  return GetOpenFileNameW(&ofn) ? std::filesystem::path(file) : std::filesystem::path();
}
#else
std::filesystem::path BrowseForPackage() { return {}; }
#endif

// Settings that the presenter/window read before the launcher runs; changing
// them needs a relaunch to take effect.
constexpr const char* kRestartCvars[] = {"present_effect", "window_width", "window_height", "monitor"};

class Launcher final : public rex::ui::ImGuiDialog {
 public:
  Launcher(rex::ui::ImGuiDrawer* drawer, std::filesystem::path game_dir,
           std::filesystem::path config_path, LauncherCallbacks callbacks)
      : ImGuiDialog(drawer),
        game_dir_(std::move(game_dir)),
        config_path_(std::move(config_path)),
        cb_(std::move(callbacks)) {
    for (const char* name : kRestartCvars) restart_baseline_.push_back(Get(name));
    files_ok_ = GameFilesPresent(game_dir_);
  }

  ~Launcher() override {
    if (install_thread_.joinable()) {
      progress_.cancel = true;
      install_thread_.join();
    }
  }

 protected:
  void OnDraw(ImGuiIO& io) override {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const ImVec2 size(std::min(vp->WorkSize.x * 0.9f, ImGui::GetFontSize() * 52.0f),
                      std::min(vp->WorkSize.y * 0.9f, ImGui::GetFontSize() * 36.0f));
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + vp->WorkSize.y * 0.5f),
                            ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::Begin("Outpost Kaloki X##launcher", nullptr,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoSavedSettings);

    ImGui::TextUnformatted("Outpost Kaloki X");
    ImGui::SameLine();
    ImGui::TextDisabled("  native PC recompilation");
    ImGui::Separator();

    const float footer = ImGui::GetFrameHeightWithSpacing() * 1.6f;
    ImGui::BeginChild("##tabs", ImVec2(0, -footer));
    ImGui::PushItemWidth(ImGui::GetFontSize() * 18.0f);
    if (ImGui::BeginTabBar("##launcher_tabs")) {
      if (ImGui::BeginTabItem("Game")) { DrawGameTab(); ImGui::EndTabItem(); }
      if (ImGui::BeginTabItem("Display")) { DrawDisplayTab(); ImGui::EndTabItem(); }
      if (ImGui::BeginTabItem("Graphics")) { DrawGraphicsTab(); ImGui::EndTabItem(); }
      if (ImGui::BeginTabItem("Controls")) { DrawControlsTab(); ImGui::EndTabItem(); }
      ImGui::EndTabBar();
    }
    ImGui::PopItemWidth();
    ImGui::EndChild();
    DrawFooter();
    ImGui::End();
    (void)io;
  }

 private:
  // ---------------------------------------------------------------- Game ---
  void DrawGameTab() {
    ImGui::SeparatorText("Game files");
    if (installing_) {
      const double total = std::max<double>(1.0, double(progress_.bytes_total.load()));
      ImGui::TextUnformatted("Installing...");
      ImGui::ProgressBar(float(progress_.bytes_done.load() / total), ImVec2(-1, 0));
      if (ImGui::Button("Cancel")) progress_.cancel = true;
    } else if (files_ok_) {
      ImGui::TextColored(ImVec4(0.45f, 0.85f, 0.45f, 1), "Installed");
      ImGui::SameLine();
      ImGui::TextDisabled("%s", game_dir_.string().c_str());
      if (ImGui::Button("Reinstall from XBLA package...")) StartInstall();
    } else {
      ImGui::TextColored(ImVec4(0.95f, 0.6f, 0.3f, 1), "Not installed");
      ImGui::TextWrapped(
          "Select your own Outpost Kaloki X XBLA package (the file with no extension from your "
          "Xbox 360 or emulator content folder). Its files are extracted to:");
      ImGui::TextDisabled("%s", game_dir_.string().c_str());
      if (ImGui::Button("Install from XBLA package...")) StartInstall();
    }
    if (!install_message_.empty()) ImGui::TextWrapped("%s", install_message_.c_str());
    FinishInstallIfDone();

    ImGui::SeparatorText("Game");
    CheckboxCvarUint("Full game (unlocked)", "license_mask");
    HelpMarker(
        "XBLA games shipped as trials that unlocked when purchased, which is no longer possible. "
        "Leave this on to play the full game; turn it off to play the trial.");

    ImGui::SeparatorText("Launcher");
    CheckboxCvar("Show this launcher at startup", "okx_launcher");
    HelpMarker("When off, the game starts directly. Hold Shift while starting to show the launcher anyway.");
  }

  void CheckboxCvarUint(const char* label, const char* cvar) {
    bool v = GetInt(cvar) != 0;
    if (ImGui::Checkbox(label, &v)) SetInt(cvar, v ? 1 : 0);
  }

  void StartInstall() {
    const auto pkg = BrowseForPackage();
    if (pkg.empty()) return;
    const uint32_t title = stfs::ReadTitleId(pkg);
    if (title == 0) {
      install_message_ = "That file is not an Xbox 360 package.";
      return;
    }
    if (title != kTitleId) {
      char buf[160];
      std::snprintf(buf, sizeof(buf), "That package is title %08X, not Outpost Kaloki X (%08X).", title, kTitleId);
      install_message_ = buf;
      return;
    }
    install_message_.clear();
    progress_.cancel = false;
    installing_ = true;
    install_done_ = false;
    install_thread_ = std::thread([this, pkg] {
      install_result_ = stfs::Extract(pkg, game_dir_, &progress_);
      install_done_ = true;
    });
  }

  void FinishInstallIfDone() {
    if (!installing_ || !install_done_) return;
    install_thread_.join();
    installing_ = false;
    files_ok_ = GameFilesPresent(game_dir_);
    install_message_ = install_result_.empty() ? (files_ok_ ? "Installed successfully." : "Extraction finished but default.xex is missing.")
                                               : "Install failed: " + install_result_;
  }

  // ------------------------------------------------------------- Display ---
  void DrawDisplayTab() {
    ImGui::SeparatorText("Window");
    bool fullscreen = GetBool("fullscreen");
    if (ImGui::RadioButton("Fullscreen", fullscreen)) SetFullscreen(true);
    ImGui::SameLine();
    if (ImGui::RadioButton("Windowed", !fullscreen)) SetFullscreen(false);

    ImGui::BeginDisabled(fullscreen);
    // window_width/height are in logical pixels (96 DPI); offer real pixel sizes
    // that fit on the screen and convert with the window's DPI scale.
    const double scale = cb_.dpi_scale ? cb_.dpi_scale() : 1.0;
    auto to_physical = [&](int logical) { return int(logical * scale + 0.5); };
    const int w = to_physical(GetInt("window_width")), h = to_physical(GetInt("window_height"));
    static const std::pair<int, int> kSizes[] = {{0, 0},       {1280, 720},  {1600, 900},
                                                 {1920, 1080}, {2560, 1440}, {3200, 1800}};
    auto size_label = [](int sw, int sh) {
      return sw == 0 ? std::string("Default") : std::to_string(sw) + " x " + std::to_string(sh);
    };
    const auto [screen_w, screen_h] = cb_.screen_size ? cb_.screen_size() : std::pair<int, int>{1 << 16, 1 << 16};
    auto close_to = [](int a, int b) { return std::abs(a - b) <= 2; };
    if (ImGui::BeginCombo("Window size", size_label(w, h).c_str())) {
      for (auto [sw, sh] : kSizes) {
        if (sw >= screen_w || sh >= screen_h) continue;  // must fit with its frame
        if (ImGui::Selectable(size_label(sw, sh).c_str(), close_to(sw, w) && close_to(sh, h))) {
          SetInt("window_width", int(sw / scale + 0.5));
          SetInt("window_height", int(sh / scale + 0.5));
        }
      }
      ImGui::EndCombo();
    }
    ImGui::EndDisabled();
    HelpMarker("Size of the window in windowed mode. Applies when the game starts.");

    ImGui::SeparatorText("Frame rate");
    const int fps = GetInt("okx_frame_rate", 60);
    auto fps_label = [](int f) { return f <= 0 ? std::string("Unlimited") : std::to_string(f) + " FPS"; };
    if (ImGui::BeginCombo("Frame-rate cap", fps_label(fps).c_str())) {
      for (int choice : kFrameRateChoices)
        if (ImGui::Selectable(fps_label(choice).c_str(), choice == fps)) SetInt("okx_frame_rate", choice);
      ImGui::EndCombo();
    }
    HelpMarker(
        "The original game ran at 30 FPS on Xbox 360. It times everything by real elapsed time, "
        "so it plays at the correct speed at any frame rate.");
    CheckboxCvar("VSync (no tearing)", "d3d12_allow_variable_refresh_rate_and_tearing", /*invert=*/true);
    HelpMarker("Off gives the lowest latency and lets variable-refresh (G-Sync/FreeSync) displays run freely, but can tear.");

    ImGui::SeparatorText("Image");
    CheckboxCvar("Keep 16:9 aspect ratio (letterbox)", "present_letterbox");
    HelpMarker("Off stretches the image to fill the screen.");
  }

  void SetFullscreen(bool on) {
    SetBool("fullscreen", on);
    if (cb_.set_fullscreen) cb_.set_fullscreen(on);
  }

  // ------------------------------------------------------------ Graphics ---
  void DrawGraphicsTab() {
    ImGui::SeparatorText("Resolution");
    ComboCvar("Internal resolution", "resolution_scale",
              {{"1x  -  1280 x 720 (original)", "1"},
               {"2x  -  2560 x 1440", "2"},
               {"3x  -  3840 x 2160 (4K)", "3"},
               {"4x  -  5120 x 2880", "4"},
               {"5x  -  6400 x 3600", "5"},
               {"6x  -  7680 x 4320 (8K)", "6"}});
    HelpMarker("Renders the game at a multiple of its native 720p resolution. Higher is sharper but heavier on the GPU.");
    ComboCvar("Upscaling filter", "present_effect",
              {{"Bilinear", "bilinear"}, {"AMD FidelityFX CAS (sharpen)", "cas"}, {"AMD FidelityFX FSR 1 (upscale)", "fsr"}});
    HelpMarker("How the rendered image is scaled to your screen. CAS sharpens; FSR upscales with edge reconstruction.");

    ImGui::SeparatorText("Anti-aliasing");
    ComboCvar("Post-process AA", "swap_post_effect",
              {{"Off", "none"}, {"FXAA", "fxaa"}, {"FXAA (extreme quality)", "fxaa_extreme"}});
    CheckboxCvar("Native 2x MSAA", "native_2x_msaa");
    HelpMarker("Uses real 2x multisampling where the game requests MSAA on the Xbox 360 GPU.");

    ImGui::SeparatorText("Textures");
    ComboCvar("Anisotropic filtering", "anisotropic_override",
              {{"Game default", "-1"}, {"Off", "0"}, {"2x", "2"}, {"4x", "3"}, {"8x", "4"}, {"16x", "5"}});
  }

  // ------------------------------------------------------------ Controls ---
  void DrawControlsTab() {
    ImGui::SeparatorText("Camera");
    CheckboxCvar("Invert camera horizontal (right stick X)", "okx_invert_rs_x");
    CheckboxCvar("Invert camera vertical (right stick Y)", "okx_invert_rs_y");
    CheckboxCvar("Invert left stick horizontal", "okx_invert_ls_x");
    CheckboxCvar("Invert left stick vertical", "okx_invert_ls_y");

    ImGui::SeparatorText("Keyboard & mouse");
    CheckboxCvar("Play with keyboard & mouse", "mnk_mode");
    HelpMarker("Emulates an Xbox controller from the keyboard. Bindings are below.");
    if (GetBool("mnk_mode")) {
      CheckboxCvar("Mouse moves the right stick (camera)", "mnk_mouse");
      float sens = std::stof(Get("mnk_sensitivity").empty() ? "1" : Get("mnk_sensitivity"));
      if (ImGui::SliderFloat("Mouse sensitivity", &sens, 0.1f, 5.0f, "%.2f")) Set("mnk_sensitivity", std::to_string(sens));
    }

    if (ImGui::CollapsingHeader("Controller button remapping")) DrawRemapTable();
    if (ImGui::CollapsingHeader("Keyboard bindings")) DrawKeyboardTable();
  }

  void DrawRemapTable() {
    if (ImGui::Button("Reset to defaults##remap"))
      for (size_t i = 0; i < kPadCount; ++i) SetMapping(static_cast<Pad>(i), static_cast<Pad>(i));
    if (!ImGui::BeginTable("##remap", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) return;
    ImGui::TableSetupColumn("Your controller");
    ImGui::TableSetupColumn("Sends to the game");
    ImGui::TableHeadersRow();
    for (size_t i = 0; i < kPadCount; ++i) {
      const auto physical = static_cast<Pad>(i);
      const Pad target = GetMapping(physical);
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextUnformatted(GetPadInfo(physical).label);
      ImGui::TableSetColumnIndex(1);
      ImGui::PushID(static_cast<int>(i));
      ImGui::SetNextItemWidth(-1);
      const char* preview = target == Pad::kNone ? "(nothing)" : GetPadInfo(target).label;
      if (ImGui::BeginCombo("##t", preview)) {
        if (ImGui::Selectable("(nothing)", target == Pad::kNone)) SetMapping(physical, Pad::kNone);
        for (size_t j = 0; j < kPadCount; ++j)
          if (ImGui::Selectable(GetPadInfo(static_cast<Pad>(j)).label, target == static_cast<Pad>(j)))
            SetMapping(physical, static_cast<Pad>(j));
        ImGui::EndCombo();
      }
      ImGui::PopID();
    }
    ImGui::EndTable();
  }

  void DrawKeyboardTable() {
    std::vector<const rex::cvar::FlagEntry*> binds;
    for (auto& e : rex::cvar::GetRegistry())
      if (e.category == "Input/Keybinds/Controller") binds.push_back(&e);
    if (ImGui::Button("Reset to defaults##keys"))
      for (auto* e : binds) rex::cvar::ResetToDefault(e->name);
    ImGui::SameLine();
    ImGui::TextDisabled("Click a binding, then press a key (Esc cancels).");
    if (!ImGui::BeginTable("##keys", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) return;
    ImGui::TableSetupColumn("Controller input");
    ImGui::TableSetupColumn("Key(s)");
    ImGui::TableHeadersRow();
    for (auto* e : binds) {
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextUnformatted(e->description.c_str());
      ImGui::TableSetColumnIndex(1);
      ImGui::PushID(e->name.c_str());
      const bool capturing = capturing_ == e->name;
      const std::string value = e->getter();
      if (ImGui::Button(capturing ? "Press a key..." : (value.empty() ? "(none)" : value.c_str()), ImVec2(-1, 0)))
        capturing_ = e->name;
      ImGui::PopID();
      if (capturing) CaptureKey(e->name);
    }
    ImGui::EndTable();
  }

  void CaptureKey(const std::string& cvar) {
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
      capturing_.clear();
      return;
    }
    for (int k = ImGuiKey_NamedKey_BEGIN; k < ImGuiKey_NamedKey_END; ++k) {
      const auto key = static_cast<ImGuiKey>(k);
      if (key == ImGuiKey_Escape || !ImGui::IsKeyPressed(key, false)) continue;
      const int vk = ImGuiKeyToVk(key);
      if (!vk) continue;
      const std::string name = rex::ui::VirtualKeyToString(static_cast<rex::ui::VirtualKey>(vk));
      if (!name.empty()) rex::cvar::SetFlagByName(cvar, name);
      capturing_.clear();
      return;
    }
  }

  // -------------------------------------------------------------- Footer ---
  void DrawFooter() {
    ImGui::Separator();
    const float bw = ImGui::GetFontSize() * 7.0f;
    const float bh = ImGui::GetFrameHeight() * 1.3f;
    if (ImGui::Button("Quit", ImVec2(bw, bh)) && cb_.quit) cb_.quit();
    ImGui::SameLine();
    if (ImGui::Button("Save", ImVec2(bw, bh))) Save();
    if (!status_.empty()) {
      ImGui::SameLine();
      ImGui::TextDisabled("%s", status_.c_str());
    }
    ImGui::SameLine(ImGui::GetContentRegionMax().x - bw * 1.5f);
    ImGui::BeginDisabled(!files_ok_ || installing_);
    if (ImGui::Button("PLAY  >", ImVec2(bw * 1.5f, bh))) Play();
    ImGui::EndDisabled();
  }

  void Save() {
    SaveSettings(config_path_);
    status_ = "Settings saved.";
  }

  void Play() {
    SaveSettings(config_path_);
    bool needs_restart = false;
    for (size_t i = 0; i < std::size(kRestartCvars); ++i)
      if (Get(kRestartCvars[i]) != restart_baseline_[i]) needs_restart = true;
    auto action = needs_restart ? cb_.restart_and_play : cb_.play;
    Close();  // deletes this dialog after the current draw
    if (action) action();
  }

  std::filesystem::path game_dir_;
  std::filesystem::path config_path_;
  LauncherCallbacks cb_;
  std::vector<std::string> restart_baseline_;
  bool files_ok_ = false;
  std::string status_;
  std::string capturing_;

  stfs::Progress progress_;
  std::thread install_thread_;
  std::atomic<bool> install_done_{false};
  bool installing_ = false;
  std::string install_result_;
  std::string install_message_;
};

}  // namespace

bool GameFilesPresent(const std::filesystem::path& game_dir) {
  std::error_code ec;
  return !game_dir.empty() && std::filesystem::exists(game_dir / "default.xex", ec) &&
         std::filesystem::exists(game_dir / "fsxb2" / "blockfiles" / "Full_Common.blk.bz2", ec);
}

void PreloadGpuPlugin() {
#if defined(_WIN32)
  const auto dir = rex::filesystem::GetExecutableFolder();
  for (const char* name : {"rexgpu-xenosrd.dll", "rexgpu-xenos.dll", "rexgpu-xenosd.dll"}) {
    if (std::filesystem::exists(dir / name) && LoadLibraryW((dir / name).c_str())) return;
  }
  REXLOG_WARN("OKX: GPU plugin not found for preload; graphics settings unavailable in launcher");
#endif
}

void ShowLauncher(rex::ui::ImGuiDrawer* drawer, std::filesystem::path game_dir,
                  std::filesystem::path config_path, LauncherCallbacks callbacks) {
  new Launcher(drawer, std::move(game_dir), std::move(config_path), std::move(callbacks));
}

}  // namespace okx
