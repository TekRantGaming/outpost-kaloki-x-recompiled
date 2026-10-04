// Guest frame-rate measurement.
//
// sub_820F3CF8 is the title's D3D swap routine (builds the swap packet and
// calls VdSwap), so it runs once per presented guest frame. Count calls to
// get the real game frame rate (the host presenter can run faster), log it
// once a second, and feed ReXGlue's F3 debug overlay. Also applies the
// okx_frame_rate cap.

#include "frame_stats.h"

#include <atomic>
#include <bit>
#include <cstring>
#include <chrono>
#include <mutex>
#include <thread>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <rex/hook.h>
#include <rex/logging.h>

#include "settings.h"

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

struct Deferred {
  double after;
  std::function<void()> fn;
};
std::vector<Deferred> g_deferred;
Clock::time_point g_first_frame{};

void RunDeferredIfDue(Clock::time_point now) {
  std::vector<std::function<void()>> due;
  {
    std::lock_guard lock(g_mutex);
    if (g_first_frame == Clock::time_point{}) g_first_frame = now;
    if (g_deferred.empty()) return;
    const double elapsed = std::chrono::duration<double>(now - g_first_frame).count();
    for (auto it = g_deferred.begin(); it != g_deferred.end();) {
      if (elapsed >= it->after) {
        due.push_back(std::move(it->fn));
        it = g_deferred.erase(it);
      } else {
        ++it;
      }
    }
  }
  for (auto& fn : due) fn();
}

void OnGuestSwap() {
  RunDeferredIfDue(Clock::now());
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

void RunAfterFirstFrame(double seconds, std::function<void()> fn) {
  std::lock_guard lock(g_mutex);
  g_deferred.push_back({seconds, std::move(fn)});
}

}  // namespace okx

namespace okx {
namespace {

// Frame limiter for okx_frame_rate: high-resolution waitable timer for the bulk
// of the wait, then a short spin for precision.
void LimitFrameRate() {
  const int32_t fps = REXCVAR_GET(okx_frame_rate);
  static Clock::time_point next{};
  if (fps <= 0) {
    next = {};
    return;
  }
  const auto period = std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(1.0 / fps));
  auto now = Clock::now();
  if (next == Clock::time_point{} || now - next > period) {
    next = now + period;  // first frame, or fell behind: resync
    return;
  }
#if defined(_WIN32)
  static HANDLE timer =
      CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
  const auto coarse = next - now - std::chrono::microseconds(500);
  if (timer && coarse > Clock::duration::zero()) {
    LARGE_INTEGER due;
    due.QuadPart = -std::chrono::duration_cast<std::chrono::nanoseconds>(coarse).count() / 100;
    SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE);
    WaitForSingleObject(timer, INFINITE);
  }
#endif
  while (Clock::now() < next) std::this_thread::yield();
  next += period;
}

}  // namespace
}  // namespace okx

REX_EXTERN(__imp__sub_820F3CF8);
REX_HOOK_RAW(sub_820F3CF8) {
  okx::LimitFrameRate();
  // Frame delta computed by the title's timer (sub_820DE550), in 1/60 s units.
  uint32_t dt_bits;
  std::memcpy(&dt_bits, base + 0x822D24E8, sizeof(dt_bits));
  okx::AddDelta(std::bit_cast<float>(std::byteswap(dt_bits)));
  okx::OnGuestSwap();
  const auto start = okx::Clock::now();
  __imp__sub_820F3CF8(ctx, base);
  okx::AddSwapTime(std::chrono::duration<double, std::milli>(okx::Clock::now() - start).count());
}
