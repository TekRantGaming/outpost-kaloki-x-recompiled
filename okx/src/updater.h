// Updates from GitHub releases: the launcher asks for the newest release, and
// when it is newer than this build, downloads its Windows zip (or Linux
// AppImage), swaps in the new program files and restarts. Only the program
// (exe, dlls, text files) is replaced; the game folder, settings and saves are
// left alone. Nothing is contacted unless okx_check_updates allows it or the
// player asks.

#pragma once

#include <atomic>
#include <optional>
#include <string>

namespace okx::update {

// This build's version ("1.1.0"), from CMake's project(VERSION).
const char* CurrentVersion();

struct Release {
  std::string tag;    // "v1.1.0"
  std::string asset;  // download URL of this platform's zip / AppImage
  std::string page;   // release page, for the player
};

// Asks GitHub for the releases (blocking; call from a worker thread) and
// returns the newest published one that has a download for this platform,
// when it is newer than this build. `error` gets a message when the check
// itself failed.
std::optional<Release> CheckLatest(std::string* error = nullptr);

// Downloads the release and replaces the program files next to this exe (the
// running exe and dlls are renamed to *.old first, which Windows allows), or
// the running AppImage on Linux (blocking; call from a worker thread).
// `progress` (0..1, or -1 while unknown) is updated as it goes. Returns "" on
// success, else a message for the player.
std::string Install(const Release& release, std::atomic<float>* progress);

// Removes *.old files left by the previous update. Call at startup.
void CleanUpPreviousUpdate();

}  // namespace okx::update
