// Guest frame-rate measurement.
//
// sub_820F3CF8 is the title's D3D swap routine (builds the swap packet and
// calls VdSwap), so it runs once per presented guest frame. Count calls to
// get the real game frame rate (the host presenter can run faster), log it
// once a second, and feed ReXGlue's F3 debug overlay.

#include "frame_stats.h"

#include <atomic>
#include <bit>
#include <cstring>
#include <chrono>
#include <mutex>

#include <rex/hook.h>
#include <rex/logging.h>

namespace okx {
namespace {

using Clock = std::chrono::steady_clock;

std::atomic<uint64_t> g_frames{0};
std::mutex g_mutex;
Clock::time_point g_window_start = Clock::now();
uint64_t g_window_frames = 0;
double g_window_swap_ms = 0;  // time spent inside the swap call this window
double g_window_delta = 0;    // sum of the title's per-frame delta this window
rex::ui::FrameStats g_stats;

void OnGuestSwap() {
  const uint64_t total = g_frames.fetch_add(1) + 1;
  std::lock_guard lock(g_mutex);
  ++g_window_frames;
  const auto now = Clock::now();
  const double elapsed = std::chrono::duration<double>(now - g_window_start).count();
  if (elapsed >= 1.0) {
    g_stats.fps = g_window_frames / elapsed;
    g_stats.frame_time_ms = elapsed * 1000.0 / g_window_frames;
    g_stats.frame_count = total;
    REXLOG_INFO("Guest FPS: {:.1f} ({:.2f} ms/frame, {:.2f} ms in swap, game dt {:.3f})",
                g_stats.fps, g_stats.frame_time_ms, g_window_swap_ms / g_window_frames,
                g_window_delta / g_window_frames);
    g_window_start = now;
    g_window_frames = 0;
    g_window_swap_ms = 0;
    g_window_delta = 0;
  }
}

void AddDelta(float dt) {
  std::lock_guard lock(g_mutex);
  g_window_delta += dt;
}

void AddSwapTime(double ms) {
  std::lock_guard lock(g_mutex);
  g_window_swap_ms += ms;
}

}  // namespace

rex::ui::FrameStats GetGuestFrameStats() {
  std::lock_guard lock(g_mutex);
  return g_stats;
}

}  // namespace okx

REX_EXTERN(__imp__sub_820F3CF8);
REX_HOOK_RAW(sub_820F3CF8) {
  // Frame delta computed by the title's timer (sub_820DE550), in 1/60 s units.
  uint32_t dt_bits;
  std::memcpy(&dt_bits, base + 0x822D24E8, sizeof(dt_bits));
  okx::AddDelta(std::bit_cast<float>(std::byteswap(dt_bits)));
  okx::OnGuestSwap();
  const auto start = okx::Clock::now();
  __imp__sub_820F3CF8(ctx, base);
  okx::AddSwapTime(std::chrono::duration<double, std::milli>(okx::Clock::now() - start).count());
}
