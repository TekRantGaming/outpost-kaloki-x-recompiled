#include "settings.h"

#include <cctype>
#include <fstream>

#include <rex/logging.h>
#include <rex/string.h>

REXCVAR_DEFINE_BOOL(okx_launcher, true, "OKX",
                    "Show the launcher before starting the game (hold Shift at start to force it)");
REXCVAR_DEFINE_BOOL(okx_skip_launcher, false, "OKX",
                    "Internal: skip the launcher once (used when it relaunches the game)");
REXCVAR_DEFINE_INT32(okx_frame_rate, 60, "OKX/Video",
                     "Frame-rate cap: 30, 60, 120, 144, 165, 240, or 0 for unlimited");
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

bool SaveSettings(const std::filesystem::path& path) {
  // Like rex::cvar::SaveConfig, but values that came from the command line or
  // environment (e.g. --game_data_root, --log_file) are one-off overrides and
  // are not written back.
  std::string out = "# Outpost Kaloki X settings (edited by the launcher)\n";
  for (const auto& e : rex::cvar::GetRegistry()) {
    if (e.type == rex::cvar::FlagType::Command || e.is_debug_only) continue;
    if (e.source == rex::cvar::Source::kCommandLine || e.source == rex::cvar::Source::kEnvironment) continue;
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

void ApplyPortDefaults() {
  // XBLA titles ship as trials that unlock via XamContentGetLicenseMask; the
  // port defaults to the full (purchased) license, like Xenia's license_mask = 1.
  SetCvarDefault("license_mask", "1");
  // Windowed by default so the launcher isn't a giant fullscreen dialog.
  SetCvarDefault("fullscreen", "false");
}

void ApplyRuntimeOverrides() {
  // ReXGlue submits GPU work at every primary ring-buffer end by default, which
  // stalls this title to ~27 ms per frame (the 30 FPS "lock"). Batching lets
  // it run at any rate; the title's delta-time keeps game speed correct.
  // Guest vsync only paces the emulated console (60 Hz vblank + coarse sleeps
  // in GPU waits). Frame pacing is done by okx_frame_rate instead.
  for (const char* name : {"d3d12_submit_on_primary_buffer_end", "vsync"}) {
    if (!rex::cvar::SetFlagByName(name, "false"))
      REXLOG_WARN("OKX: could not set {} (cvar not registered)", name);
  }
  REXLOG_INFO("OKX: frame-rate cap {}", REXCVAR_GET(okx_frame_rate));
}

}  // namespace okx
