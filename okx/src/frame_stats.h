#pragma once

#include <rex/ui/overlay/debug_overlay.h>

namespace okx {

// Measured guest (game) frame rate, updated once per second.
rex::ui::FrameStats GetGuestFrameStats();

}  // namespace okx
