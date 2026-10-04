#include "art.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <regex>

#include <rex/system/achievement_store.h>
#include <rex/ui/image_decode.h>
#include <rex/ui/presenter.h>

namespace okx::art {
namespace {

constexpr uint32_t kTitleId = 0x584107DB;

std::vector<uint8_t> ReadFile(const std::filesystem::path& path) {
  std::ifstream f(path, std::ios::binary | std::ios::ate);
  if (!f) return {};
  std::vector<uint8_t> data(static_cast<size_t>(f.tellg()));
  f.seekg(0);
  f.read(reinterpret_cast<char*>(data.data()), data.size());
  return f ? data : std::vector<uint8_t>{};
}

template <typename T>
void Put(std::string& out, T v) {
  out.append(reinterpret_cast<const char*>(&v), sizeof(v));
}

std::string TomlString(const std::string& s) {
  std::string out = "\"";
  for (char c : s) {
    if (c == '"' || c == '\\') out += '\\';
    if (c == '\n') {
      out += "\\n";
      continue;
    }
    out += c;
  }
  return out + "\"";
}

}  // namespace

std::filesystem::path CacheDir(const std::filesystem::path& user_data_root) {
  return user_data_root / "launcher";
}
std::filesystem::path TitleCapturePath(const std::filesystem::path& user_data_root) {
  return CacheDir(user_data_root) / "title.bmp";
}
std::filesystem::path AchievementCachePath(const std::filesystem::path& user_data_root) {
  return CacheDir(user_data_root) / "achievements.toml";
}
std::filesystem::path AchievementUnlockPath(const std::filesystem::path& user_data_root) {
  char name[16];
  std::snprintf(name, sizeof(name), "%08X.toml", kTitleId);
  return user_data_root / "achievements" / name;
}

bool SaveTitleCapture(const rex::ui::RawImage& image, const std::filesystem::path& path) {
  if (!image.width || !image.height || image.data.empty()) return false;
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  // 32-bit top-down BMP (BGRA).
  const uint32_t row = image.width * 4, pixels = row * image.height;
  std::string out;
  out += "BM";
  Put<uint32_t>(out, 54 + pixels);
  Put<uint32_t>(out, 0);
  Put<uint32_t>(out, 54);
  Put<uint32_t>(out, 40);
  Put<int32_t>(out, int32_t(image.width));
  Put<int32_t>(out, -int32_t(image.height));
  Put<uint16_t>(out, 1);
  Put<uint16_t>(out, 32);
  Put<uint32_t>(out, 0);
  Put<uint32_t>(out, pixels);
  Put<int32_t>(out, 2835);
  Put<int32_t>(out, 2835);
  Put<uint32_t>(out, 0);
  Put<uint32_t>(out, 0);
  out.reserve(out.size() + pixels);
  for (uint32_t y = 0; y < image.height; ++y) {
    const uint8_t* src = image.data.data() + y * image.stride;
    for (uint32_t x = 0; x < image.width; ++x, src += 4) {
      out += char(src[2]);
      out += char(src[1]);
      out += char(src[0]);
      out += char(0xFF);
    }
  }
  std::ofstream f(path, std::ios::binary | std::ios::trunc);
  f.write(out.data(), std::streamsize(out.size()));
  return bool(f);
}

Image LoadImage(const std::filesystem::path& path) {
  Image img;
  const auto data = ReadFile(path);
  if (data.size() < 54) return img;
  if (data[0] == 'B' && data[1] == 'M') {
    int32_t w, h;
    uint32_t off;
    uint16_t bpp;
    std::memcpy(&off, &data[10], 4);
    std::memcpy(&w, &data[18], 4);
    std::memcpy(&h, &data[22], 4);
    std::memcpy(&bpp, &data[28], 2);
    const bool top_down = h < 0;
    h = std::abs(h);
    if (bpp != 32 || w <= 0 || h <= 0 || off + size_t(w) * h * 4 > data.size()) return img;
    img.width = w;
    img.height = h;
    img.rgba.resize(size_t(w) * h * 4);
    for (int y = 0; y < h; ++y) {
      const uint8_t* src = &data[off + size_t(top_down ? y : h - 1 - y) * w * 4];
      uint8_t* dst = &img.rgba[size_t(y) * w * 4];
      for (int x = 0; x < w; ++x, src += 4, dst += 4) {
        dst[0] = src[2];
        dst[1] = src[1];
        dst[2] = src[0];
        dst[3] = src[3];
      }
    }
    return img;
  }
  img.rgba = rex::ui::DecodeImageRGBA(data.data(), data.size(), img.width, img.height);
  if (img.rgba.empty()) img.width = img.height = 0;
  return img;
}

void WriteAchievementCache(const std::vector<rex::system::AchievementInfo>& achievements,
                           const std::filesystem::path& path) {
  if (achievements.empty()) return;
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  std::string out = "# Written by the game for the launcher's Achievements page.\n";
  for (const auto& a : achievements) {
    out += "\n[[achievements]]\n";
    out += "id = " + std::to_string(a.id) + "\n";
    out += "label = " + TomlString(a.label) + "\n";
    out += "description = " + TomlString(a.description) + "\n";
    out += "unachieved_description = " + TomlString(a.unachieved_description) + "\n";
    out += "gamerscore = " + std::to_string(a.gamerscore) + "\n";
  }
  std::ofstream(path, std::ios::trunc) << out;
}

std::map<uint32_t, std::filesystem::path> AchievementIcons(const std::filesystem::path& game_dir) {
  std::map<uint32_t, std::filesystem::path> icons;
  const auto raw = ReadFile(game_dir / "ArcadeInfo.xml");
  if (raw.size() < 2) return icons;
  // UTF-16 (big-endian per the file's own note, but accept either) -> ASCII.
  const bool be = raw[0] == 0xFE || (raw[0] == 0 && raw[1] != 0);
  std::string text;
  for (size_t i = (raw[0] == 0xFE || raw[0] == 0xFF) ? 2 : 0; i + 1 < raw.size(); i += 2) {
    const unsigned c = be ? (raw[i] << 8 | raw[i + 1]) : (raw[i] | raw[i + 1] << 8);
    text += c < 0x80 ? char(c) : '?';
  }
  static const std::regex kEntry(R"re(AchievementInfo\s+ID="(\d+)"\s+ImagePath="([^"]+)")re",
                                 std::regex::icase);
  for (std::sregex_iterator it(text.begin(), text.end(), kEntry), end; it != end; ++it) {
    std::string rel = (*it)[2];
    std::replace(rel.begin(), rel.end(), '\\', '/');
    icons[uint32_t(std::stoul((*it)[1]))] = game_dir / rel;
  }
  return icons;
}

}  // namespace okx::art
