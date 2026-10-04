// Small OS helpers used by the app (kept out of the app header so it doesn't
// pull in <windows.h>).

#pragma once

#include <string_view>
#include <utility>

struct ImFontAtlas;

namespace okx {

bool IsShiftHeld();

// Primary monitor resolution in physical pixels.
std::pair<int, int> PrimaryScreenSize();

// Starts a new instance of this executable with the current command line plus
// `extra_args`.
void RelaunchSelf(std::wstring_view extra_args);

// Adds a system UI font to the ImGui atlas and makes it the default.
void LoadUiFont(ImFontAtlas* atlas);

}  // namespace okx
