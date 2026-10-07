// Updates from GitHub releases. The port is released as a builder (no game
// code is ever published), so an update rebuilds the game on this PC: the
// launcher downloads the new builder, unpacks it next to the current one and
// runs it on the game files that are already installed. The builder then
// replaces the program files; the game folder, settings and saves are kept.
// Nothing is contacted unless okx_check_updates allows it or the player asks.

#pragma once

#include <atomic>
#include <filesystem>
#include <optional>
#include <string>

namespace okx::update {

// This build's version ("1.1.0"), from CMake's project(VERSION).
const char* CurrentVersion();

struct Release {
  std::string tag;   // "v1.1.0"
  std::string zip;   // download URL of the Windows builder zip
  std::string page;  // release page, for the player
};

// Asks GitHub for the releases (blocking; call from a worker thread) and
// returns the newest published one that has a Windows builder zip, when it is
// newer than this build. `error` gets a message when the check itself failed.
std::optional<Release> CheckLatest(std::string* error = nullptr);

// Downloads the release's builder, unpacks it and starts it in its own window
// on `game_dir`, installing into `exe_dir` (blocking; call from a worker
// thread). The caller quits right after so the builder can replace the
// program files. `progress` (0..1, or -1 while unknown) is updated as it goes.
// Returns "" on success, else a message for the player.
std::string StartRebuild(const Release& release, const std::filesystem::path& game_dir,
                         const std::filesystem::path& exe_dir, std::atomic<float>* progress);

}  // namespace okx::update
