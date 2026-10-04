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
#include <cstdint>
#include <cstring>

#include <rex/hook.h>

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
}

}  // namespace

REX_EXTERN(__imp__sub_820E9678);
REX_HOOK_RAW(sub_820E9678) {
  const uint32_t state_ptr = ctx.r4.u32;
  __imp__sub_820E9678(ctx, base);
  if (ctx.r3.u32 == 0 && state_ptr) Remap(base + state_ptr);  // ERROR_SUCCESS
}
