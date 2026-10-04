// Launcher art and metadata taken from the player's own game files and from
// the game itself on first play (nothing is shipped with the port):
//  - title.bmp: a capture of the game's title screen (header background)
//  - achievements.toml: achievement names/descriptions read by the runtime
//  - achievement icons: arcadefiles/*.png from the XBLA package

#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace rex::system {
struct AchievementInfo;
}
namespace rex::ui {
struct RawImage;
}

namespace okx::art {

struct Image {
  int width = 0, height = 0;
  std::vector<uint8_t> rgba;
  explicit operator bool() const { return width > 0 && height > 0; }
};

std::filesystem::path CacheDir(const std::filesystem::path& user_data_root);
std::filesystem::path TitleCapturePath(const std::filesystem::path& user_data_root);
std::filesystem::path AchievementCachePath(const std::filesystem::path& user_data_root);
// Where ReXGlue keeps unlocked achievements for this title.
std::filesystem::path AchievementUnlockPath(const std::filesystem::path& user_data_root);

bool SaveTitleCapture(const rex::ui::RawImage& image, const std::filesystem::path& path);
Image LoadImage(const std::filesystem::path& path);  // .bmp (ours) or .png

void WriteAchievementCache(const std::vector<rex::system::AchievementInfo>& achievements,
                           const std::filesystem::path& path);

// Achievement ID -> icon file (from ArcadeInfo.xml in the game folder).
std::map<uint32_t, std::filesystem::path> AchievementIcons(const std::filesystem::path& game_dir);

}  // namespace okx::art
