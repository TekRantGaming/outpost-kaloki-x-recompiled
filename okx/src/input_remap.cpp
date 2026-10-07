// Controller remapping and stick inversion.
//
// sub_820E9678 is the title's XInputGetState(user, state) wrapper (it calls
// XamInputGetState) and the only place it reads the pad. After the real call
// we rewrite the guest XINPUT_STATE (big-endian):
//   +0 dwPacketNumber, +4 wButtons, +6 bLeftTrigger, +7 bRightTrigger,
//   +8 sThumbLX, +10 sThumbLY, +12 sThumbRX, +14 sThumbRY.
// Keyboard input arrives through the same path when ReXGlue's mnk_mode is on,
// so remaps and inversion apply to it too.

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <rex/hook.h>
#include <rex/logging.h>

#include "settings.h"

namespace {

constexpr uint8_t kTriggerPressThreshold = 30;  // XINPUT_GAMEPAD_TRIGGER_THRESHOLD

template <typename T>
T LoadBE(const uint8_t* p) {
  T v;
  std::memcpy(&v, p, sizeof(v));
  return std::byteswap(v);
}

template <typename T>
void StoreBE(uint8_t* p, T v) {
  v = std::byteswap(v);
  std::memcpy(p, &v, sizeof(v));
}

void Invert(uint8_t* p) {
  const int16_t v = LoadBE<int16_t>(p);
  StoreBE<int16_t>(p, v == INT16_MIN ? INT16_MAX : static_cast<int16_t>(-v));
}

// Radial deadzone (rescaled so output still starts at 0) and a gain, applied to
// one stick's X/Y pair.
void ShapeStick(uint8_t* xy, float deadzone, float gain) {
  if (deadzone <= 0.0f && gain == 1.0f) return;
  float x = LoadBE<int16_t>(xy) / 32767.0f, y = LoadBE<int16_t>(xy + 2) / 32767.0f;
  const float mag = std::sqrt(x * x + y * y);
  if (mag <= deadzone || mag == 0.0f) {
    x = y = 0.0f;
  } else {
    const float scaled = std::min(1.0f, (mag - deadzone) / (1.0f - deadzone) * gain);
    x = x / mag * scaled;
    y = y / mag * scaled;
  }
  StoreBE<int16_t>(xy, static_cast<int16_t>(std::clamp(x, -1.0f, 1.0f) * 32767.0f));
  StoreBE<int16_t>(xy + 2, static_cast<int16_t>(std::clamp(y, -1.0f, 1.0f) * 32767.0f));
}

void Remap(uint8_t* state) {
  using okx::Pad;
  const uint16_t in_buttons = LoadBE<uint16_t>(state + 4);
  const uint8_t in_lt = state[6], in_rt = state[7];

  uint16_t out_buttons = 0;
  uint8_t out_lt = 0, out_rt = 0;
  for (size_t i = 0; i < okx::kPadCount; ++i) {
    const auto physical = static_cast<Pad>(i);
    uint8_t analog;  // 0..255 strength of the physical control
    if (physical == Pad::kLeftTrigger) {
      analog = in_lt;
    } else if (physical == Pad::kRightTrigger) {
      analog = in_rt;
    } else {
      analog = (in_buttons & okx::GetPadInfo(physical).mask) ? 255 : 0;
    }
    if (analog == 0) continue;

    const Pad target = okx::GetMapping(physical);
    if (target == Pad::kNone) continue;
    if (target == Pad::kLeftTrigger) {
      out_lt = std::max(out_lt, analog);
    } else if (target == Pad::kRightTrigger) {
      out_rt = std::max(out_rt, analog);
    } else if (analog > kTriggerPressThreshold) {
      out_buttons |= okx::GetPadInfo(target).mask;
    }
  }
  StoreBE<uint16_t>(state + 4, out_buttons);
  state[6] = out_lt;
  state[7] = out_rt;

  if (REXCVAR_GET(okx_invert_ls_x)) Invert(state + 8);
  if (REXCVAR_GET(okx_invert_ls_y)) Invert(state + 10);
  if (REXCVAR_GET(okx_invert_rs_x)) Invert(state + 12);
  if (REXCVAR_GET(okx_invert_rs_y)) Invert(state + 14);

  const float deadzone = std::clamp(REXCVAR_GET(okx_deadzone), 0, 50) / 100.0f;
  const float camera = std::clamp(REXCVAR_GET(okx_camera_sensitivity), 10, 400) / 100.0f;
  ShapeStick(state + 8, deadzone, 1.0f);
  ShapeStick(state + 12, deadzone, camera);
}

// Developer test aid: OKX_DEV_INPUT="14:start;18:a;21:dpad_down" presses each
// button (ids as in okx_map_*) for 150 ms at that many seconds after the game
// first reads the pad, so test runs can reach menus without touching the
// player's real keyboard or controller. Ignored unless the variable is set.
struct DevPress {
  double at;
  uint16_t mask;
};

const std::vector<DevPress>& DevScript() {
  static const std::vector<DevPress> script = [] {
    std::vector<DevPress> out;
    const char* env = std::getenv("OKX_DEV_INPUT");
    if (!env) return out;
    std::string s(env);
    size_t pos = 0;
    while (pos < s.size()) {
      size_t end = s.find(';', pos);
      if (end == std::string::npos) end = s.size();
      const std::string item = s.substr(pos, end - pos);
      pos = end + 1;
      const size_t colon = item.find(':');
      if (colon == std::string::npos) continue;
      const okx::Pad pad = okx::ParsePad(item.substr(colon + 1));
      if (pad == okx::Pad::kNone || !okx::GetPadInfo(pad).mask) continue;
      out.push_back({std::atof(item.substr(0, colon).c_str()), okx::GetPadInfo(pad).mask});
    }
    return out;
  }();
  return script;
}

// Buttons the dev script holds right now (0 when none / not active).
uint16_t DevButtons() {
  const auto& script = DevScript();
  if (script.empty()) return 0;
  using Clock = std::chrono::steady_clock;
  static const Clock::time_point start = Clock::now();
  const double t = std::chrono::duration<double>(Clock::now() - start).count();
  uint16_t mask = 0;
  for (const auto& p : script)
    if (t >= p.at && t < p.at + 0.15) mask |= p.mask;
  static uint16_t last = 0;
  if (mask != last) REXLOG_INFO("OKX: dev input {:04X} at {:.2f}s", mask, t);
  last = mask;
  return mask;
}

}  // namespace

REX_EXTERN(__imp__sub_820E9678);
REX_HOOK_RAW(sub_820E9678) {
  const uint32_t state_ptr = ctx.r4.u32;
  __imp__sub_820E9678(ctx, base);
  if (!DevScript().empty() && state_ptr) {
    // Act as a connected pad even without one, and add the scripted presses.
    if (ctx.r3.u32 != 0) {
      std::memset(base + state_ptr, 0, 16);
      ctx.r3.u64 = 0;
    }
    static uint32_t packet = 0;
    StoreBE<uint32_t>(base + state_ptr, ++packet);
    StoreBE<uint16_t>(base + state_ptr + 4, LoadBE<uint16_t>(base + state_ptr + 4) | DevButtons());
    return;
  }
  if (ctx.r3.u32 == 0 && state_ptr) Remap(base + state_ptr);  // ERROR_SUCCESS
}

// sub_820E9688 is the title's XInputSetState(user, vibration) wrapper; scale or
// drop the guest XINPUT_VIBRATION {u16 left, u16 right} before it is applied.
REX_EXTERN(__imp__sub_820E9688);
REX_HOOK_RAW(sub_820E9688) {
  if (const uint32_t vib = ctx.r4.u32) {
    const float strength =
        REXCVAR_GET(okx_vibration) ? std::clamp(REXCVAR_GET(okx_vibration_strength), 0, 100) / 100.0f : 0.0f;
    for (uint32_t offset : {0u, 2u}) {
      uint8_t* motor = base + vib + offset;
      StoreBE<uint16_t>(motor, static_cast<uint16_t>(LoadBE<uint16_t>(motor) * strength));
    }
  }
  __imp__sub_820E9688(ctx, base);
}
