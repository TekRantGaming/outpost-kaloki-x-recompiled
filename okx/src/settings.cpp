#include "settings.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <map>

#include <rex/logging.h>
#include <rex/string.h>

REXCVAR_DEFINE_BOOL(okx_launcher, true, "OKX",
                    "Show the launcher before starting the game (hold Shift at start to force it)");
REXCVAR_DEFINE_BOOL(okx_skip_launcher, false, "OKX",
                    "Internal: skip the launcher once (used when it relaunches the game)");
REXCVAR_DEFINE_BOOL(okx_check_updates, true, "OKX",
                    "Ask GitHub for a newer version when the launcher opens");
REXCVAR_DEFINE_INT32(okx_frame_rate, 60, "OKX/Video",
                     "Frame-rate cap: 30, 60, 120, 144, 165, 240, or 0 for unlimited");
REXCVAR_DEFINE_STRING(okx_render_quality, "native", "OKX/Video",
                      "Render resolution relative to the output: native, quality, balanced, performance, "
                      "ultra_performance, supersample, or custom (use resolution_scale)");
REXCVAR_DEFINE_BOOL(okx_show_fps, false, "OKX/Video", "Show a frame-rate counter (toggle in game with F2)");
REXCVAR_DEFINE_INT32(okx_deadzone, 0, "OKX/Controls", "Extra stick deadzone in percent (0-50)");
REXCVAR_DEFINE_INT32(okx_camera_sensitivity, 100, "OKX/Controls", "Camera (right stick) sensitivity in percent");
REXCVAR_DEFINE_BOOL(okx_achievement_toasts, true, "OKX/Achievements", "Show achievement notifications");
REXCVAR_DEFINE_BOOL(okx_achievement_sound, true, "OKX/Achievements", "Play the achievement sound");
REXCVAR_DEFINE_STRING(okx_achievement_sound_file, "", "OKX/Achievements",
                      "Achievement sound from the sounds folder (empty = built-in chime)");
REXCVAR_DEFINE_INT32(okx_achievement_volume, 80, "OKX/Achievements", "Achievement sound volume in percent");
REXCVAR_DEFINE_BOOL(okx_vibration, true, "OKX/Controls", "Controller vibration");
REXCVAR_DEFINE_INT32(okx_vibration_strength, 100, "OKX/Controls", "Vibration strength in percent");
REXCVAR_DEFINE_BOOL(okx_invert_rs_x, false, "OKX/Controls", "Invert right stick horizontal (camera)");
REXCVAR_DEFINE_BOOL(okx_invert_rs_y, false, "OKX/Controls", "Invert right stick vertical");
REXCVAR_DEFINE_BOOL(okx_invert_ls_x, false, "OKX/Controls", "Invert left stick horizontal");
REXCVAR_DEFINE_BOOL(okx_invert_ls_y, false, "OKX/Controls", "Invert left stick vertical");

// Button remapping: okx_map_<physical> = <game button> (or "none").
#define OKX_MAP_CVAR(id, def, label) \
  REXCVAR_DEFINE_STRING(okx_map_##id, def, "OKX/Controls/Remap", label " sends")
OKX_MAP_CVAR(dpad_up, "dpad_up", "D-pad up");
OKX_MAP_CVAR(dpad_down, "dpad_down", "D-pad down");
OKX_MAP_CVAR(dpad_left, "dpad_left", "D-pad left");
OKX_MAP_CVAR(dpad_right, "dpad_right", "D-pad right");
OKX_MAP_CVAR(start, "start", "Start");
OKX_MAP_CVAR(back, "back", "Back");
OKX_MAP_CVAR(ls, "ls", "Left stick click");
OKX_MAP_CVAR(rs, "rs", "Right stick click");
OKX_MAP_CVAR(lb, "lb", "Left bumper");
OKX_MAP_CVAR(rb, "rb", "Right bumper");
OKX_MAP_CVAR(a, "a", "A");
OKX_MAP_CVAR(b, "b", "B");
OKX_MAP_CVAR(x, "x", "X");
OKX_MAP_CVAR(y, "y", "Y");
OKX_MAP_CVAR(lt, "lt", "Left trigger");
OKX_MAP_CVAR(rt, "rt", "Right trigger");
#undef OKX_MAP_CVAR

namespace okx {
namespace {

constexpr std::array<PadInfo, kPadCount> kPads = {{
    {"dpad_up", "D-pad Up", 0x0001},
    {"dpad_down", "D-pad Down", 0x0002},
    {"dpad_left", "D-pad Left", 0x0004},
    {"dpad_right", "D-pad Right", 0x0008},
    {"start", "Start", 0x0010},
    {"back", "Back", 0x0020},
    {"ls", "Left Stick Click", 0x0040},
    {"rs", "Right Stick Click", 0x0080},
    {"lb", "Left Bumper", 0x0100},
    {"rb", "Right Bumper", 0x0200},
    {"a", "A", 0x1000},
    {"b", "B", 0x2000},
    {"x", "X", 0x4000},
    {"y", "Y", 0x8000},
    {"lt", "Left Trigger", 0},
    {"rt", "Right Trigger", 0},
}};

std::string& MapStorage(Pad p) {
  switch (p) {
    case Pad::kDpadUp: return REXCVAR_GET(okx_map_dpad_up);
    case Pad::kDpadDown: return REXCVAR_GET(okx_map_dpad_down);
    case Pad::kDpadLeft: return REXCVAR_GET(okx_map_dpad_left);
    case Pad::kDpadRight: return REXCVAR_GET(okx_map_dpad_right);
    case Pad::kStart: return REXCVAR_GET(okx_map_start);
    case Pad::kBack: return REXCVAR_GET(okx_map_back);
    case Pad::kLeftThumb: return REXCVAR_GET(okx_map_ls);
    case Pad::kRightThumb: return REXCVAR_GET(okx_map_rs);
    case Pad::kLeftShoulder: return REXCVAR_GET(okx_map_lb);
    case Pad::kRightShoulder: return REXCVAR_GET(okx_map_rb);
    case Pad::kA: return REXCVAR_GET(okx_map_a);
    case Pad::kB: return REXCVAR_GET(okx_map_b);
    case Pad::kX: return REXCVAR_GET(okx_map_x);
    case Pad::kY: return REXCVAR_GET(okx_map_y);
    case Pad::kLeftTrigger: return REXCVAR_GET(okx_map_lt);
    default: return REXCVAR_GET(okx_map_rt);
  }
}

}  // namespace

const PadInfo& GetPadInfo(Pad pad) { return kPads[static_cast<size_t>(pad)]; }

Pad ParsePad(std::string_view id) {
  std::string lower(id);
  for (auto& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  for (size_t i = 0; i < kPadCount; ++i)
    if (lower == kPads[i].id) return static_cast<Pad>(i);
  return Pad::kNone;
}

Pad GetMapping(Pad physical) { return ParsePad(MapStorage(physical)); }

void SetMapping(Pad physical, Pad target) {
  rex::cvar::SetFlagByName(std::string("okx_map_") + GetPadInfo(physical).id,
                           target == Pad::kNone ? "none" : GetPadInfo(target).id);
}

const std::array<RenderPreset, 6>& RenderPresets() {
  // Ratios follow the usual upscaler naming (output / render); the game renders
  // at integer multiples of its native 720p, so the nearest multiple is used.
  static const std::array<RenderPreset, 6> kPresets = {{
      {"supersample", "Supersample", 0.5},
      {"native", "Native", 1.0},
      {"quality", "Quality", 1.5},
      {"balanced", "Balanced", 1.7},
      {"performance", "Performance", 2.0},
      {"ultra_performance", "Ultra Performance", 3.0},
  }};
  return kPresets;
}

int RenderScaleFor(std::string_view preset, int output_height) {
  for (const auto& p : RenderPresets()) {
    if (preset != p.id) continue;
    const double target = std::max(1, output_height) / p.ratio;
    return std::clamp(static_cast<int>(std::lround(target / 720.0)), 1, 8);
  }
  return 0;  // custom
}

void ApplyRenderPreset(int output_height) {
  const int scale = RenderScaleFor(REXCVAR_GET(okx_render_quality), output_height);
  if (scale <= 0) return;
  // Set as the cvar's default so an explicit resolution_scale (custom) is untouched
  // and the derived value is not written to the config.
  SetCvarDefault("resolution_scale", std::to_string(scale));
  if (rex::cvar::GetFlagByName("resolution_scale") != std::to_string(scale))
    rex::cvar::SetFlagByName("resolution_scale", std::to_string(scale));
  REXLOG_INFO("OKX: render preset {} at {}p output -> {}x ({}p)", REXCVAR_GET(okx_render_quality),
              output_height, scale, scale * 720);
}

bool SaveSettings(const std::filesystem::path& path) {
  // Like rex::cvar::SaveConfig, but values that came from the command line or
  // environment (e.g. --game_data_root, --log_file) are one-off overrides and
  // are not written back. The file's own line for such a setting is kept, so a
  // one-off --okx_frame_rate=30 does not erase the player's saved choice.
  std::map<std::string, std::string> previous;  // name -> the file's line
  {
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {
      const auto eq = line.find('=');
      if (line.empty() || line[0] == '#' || eq == std::string::npos) continue;
      std::string name = line.substr(0, eq);
      while (!name.empty() && std::isspace(static_cast<unsigned char>(name.back()))) name.pop_back();
      previous[name] = line;
    }
  }
  std::string out = "# Outpost Kaloki X settings (edited by the launcher)\n";
  for (const auto& e : rex::cvar::GetRegistry()) {
    if (e.type == rex::cvar::FlagType::Command || e.is_debug_only) continue;
    if (IsPinnedCvar(e.name)) continue;  // set by the port at every start
    if (e.source == rex::cvar::Source::kCommandLine || e.source == rex::cvar::Source::kEnvironment) {
      if (auto it = previous.find(e.name); it != previous.end()) out += it->second + '\n';
      continue;
    }
    const std::string value = e.getter();
    if (value == e.default_value) continue;
    out += e.name + " = ";
    if (e.type == rex::cvar::FlagType::String) {
      out += '"';
      for (char c : value) {
        if (c == '"' || c == '\\') out += '\\';
        out += c;
      }
      out += '"';
    } else {
      out += value;
    }
    out += '\n';
  }
  std::ofstream file(path, std::ios::trunc);
  if (!file) {
    REXLOG_ERROR("OKX: cannot write settings to {}", path.string());
    return false;
  }
  file << out;
  return static_cast<bool>(file);
}

void SetCvarDefault(std::string_view name, std::string_view value) {
  for (auto& e : rex::cvar::GetRegistry()) {
    if (e.name != name) continue;
    e.default_value = value;
    if (e.source == rex::cvar::Source::kDefault) e.setter(value);  // config/CLI still win
    return;
  }
}

bool IsPinnedCvar(std::string_view name) {
  // The guest video mode (PinGuestVideoMode), and what ApplyRuntimeOverrides
  // forces: the port sets them at every start, so they are never saved or reset.
  return name == "video_mode_width" || name == "video_mode_height" || name == "vsync" ||
         name == "d3d12_submit_on_primary_buffer_end";
}

void PinGuestVideoMode() {
  // ReXGlue derives the console's video mode from window_width/height whenever
  // those are set and video_mode_width/height are not (VdQueryVideoMode). A
  // window of, say, 800x600 then tells the game the TV is 4:3: the game still
  // draws 16:9, but the presenter treats the picture as 4:3 and stretches it,
  // so "Letterbox 16:9" did nothing. Keep the console on 1280x720. ReXGlue's
  // check is "value != registered default", so the registered default is
  // cleared rather than the value changed. Only these two cvars are touched.
  for (auto& e : rex::cvar::GetRegistry()) {
    if (e.name != "video_mode_width" && e.name != "video_mode_height") continue;
    if (e.source == rex::cvar::Source::kDefault) e.setter(e.name == "video_mode_width" ? "1280" : "720");
    e.default_value.clear();
  }
}

void ApplyPortDefaults() {
  PinGuestVideoMode();
  // XBLA titles ship as trials that unlock via XamContentGetLicenseMask; the
  // port defaults to the full (purchased) license, like Xenia's license_mask = 1.
  SetCvarDefault("license_mask", "1");
  // Windowed by default so the launcher isn't a giant fullscreen dialog.
  SetCvarDefault("fullscreen", "false");
  // While a shader compiles in the background, the D3D12 backend skips every
  // draw that needs it, so things can be missing for a moment on first sight.
  // Waiting costs a short pause the first time only, since shaders are saved
  // for later runs. A GPU-plugin cvar: PreloadGpuPlugin registers it first.
  // (gpu_allow_invalid_fetch_constants, which Earthworm Jim HD needs, stays off:
  // this game logs no invalid fetch constants.)
  SetCvarDefault("async_shader_compilation", "false");
}

void ApplyRuntimeOverrides() {
  // ReXGlue submits GPU work at every primary ring-buffer end by default, which
  // stalls this title to ~27 ms per frame (the 30 FPS "lock"). Batching lets
  // it run at any rate; the title's delta-time keeps game speed correct.
  // Guest vsync only paces the emulated console (60 Hz vblank + coarse sleeps
  // in GPU waits). Frame pacing is done by okx_frame_rate instead.
#if defined(_WIN32)
  const auto forced = {"d3d12_submit_on_primary_buffer_end", "vsync"};
#else
  const auto forced = {"vsync"};  // Vulkan backend: no D3D12 submit setting
#endif
  for (const char* name : forced) {
    if (!rex::cvar::SetFlagByName(name, "false"))
      REXLOG_WARN("OKX: could not set {} (cvar not registered)", name);
  }
  REXLOG_INFO("OKX: frame-rate cap {}", REXCVAR_GET(okx_frame_rate));
}

}  // namespace okx
