// In-game overlays for the port (frame counter).

#pragma once

namespace rex::ui {
class ImGuiDrawer;
}

namespace okx {

// Creates the frame-rate counter (shown when okx_show_fps is on; F2 toggles).
void CreateFpsOverlay(rex::ui::ImGuiDrawer* drawer);

}  // namespace okx
