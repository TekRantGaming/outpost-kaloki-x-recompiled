#include "platform.h"

#include <filesystem>
#include <string>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <imgui.h>

#include <rex/logging.h>

namespace okx {

bool IsShiftHeld() {
#if defined(_WIN32)
  return (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
#else
  return false;
#endif
}

std::pair<int, int> PrimaryScreenSize() {
#if defined(_WIN32)
  DEVMODEW mode{};
  mode.dmSize = sizeof(mode);
  if (EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS, &mode))
    return {int(mode.dmPelsWidth), int(mode.dmPelsHeight)};
#endif
  return {1 << 16, 1 << 16};
}

void RelaunchSelf(std::wstring_view extra_args) {
#if defined(_WIN32)
  std::wstring cmd = GetCommandLineW();
  cmd += L" ";
  cmd += extra_args;
  wchar_t exe[MAX_PATH];
  GetModuleFileNameW(nullptr, exe, MAX_PATH);
  STARTUPINFOW si{sizeof(si)};
  PROCESS_INFORMATION pi{};
  const auto dir = std::filesystem::path(exe).parent_path();
  if (CreateProcessW(exe, cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, dir.c_str(), &si, &pi)) {
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
  } else {
    REXLOG_ERROR("OKX: relaunch failed ({})", GetLastError());
  }
#endif
}

void LoadUiFont(ImFontAtlas* atlas) {
#if defined(_WIN32)
  wchar_t windir[MAX_PATH];
  if (!GetWindowsDirectoryW(windir, MAX_PATH)) return;
  const auto path = std::filesystem::path(windir) / "Fonts" / "segoeui.ttf";
  if (!std::filesystem::exists(path)) return;
  if (ImFont* font = atlas->AddFontFromFileTTF(path.string().c_str(), 18.0f))
    ImGui::GetIO().FontDefault = font;
#else
  (void)atlas;
#endif
}

}  // namespace okx
