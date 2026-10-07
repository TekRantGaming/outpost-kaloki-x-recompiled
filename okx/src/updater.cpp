#include "updater.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <regex>
#include <system_error>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winhttp.h>
#endif

#include <rex/logging.h>

#ifndef OKX_VERSION
#define OKX_VERSION "0.0.0"
#endif

namespace okx::update {
namespace {

namespace fs = std::filesystem;

// The release list, not /releases/latest: that one skips prereleases and can
// lag behind when releases are published out of order.
constexpr const char* kReleasesApi =
    "https://api.github.com/repos/TekRantGaming/outpost-kaloki-x-recompiled/releases?per_page=20";
constexpr const char* kReleasePage = "https://github.com/TekRantGaming/outpost-kaloki-x-recompiled/releases/tag/";
// The builder zip of a release: OutpostKalokiX-Builder-v1.1.0-windows.zip.
constexpr const char* kZipSuffix = "-windows.zip";

// GETs `url` (following redirects) into `out`. Returns "" on success.
std::string HttpGet(const std::string& url, std::string& out, std::atomic<float>* progress = nullptr) {
  out.clear();
#if defined(_WIN32)
  const int wlen = MultiByteToWideChar(CP_UTF8, 0, url.c_str(), -1, nullptr, 0);
  std::wstring wurl(wlen > 0 ? size_t(wlen - 1) : 0, L'\0');
  if (wlen > 1) MultiByteToWideChar(CP_UTF8, 0, url.c_str(), -1, wurl.data(), wlen);
  URL_COMPONENTSW parts{};
  parts.dwStructSize = sizeof(parts);
  wchar_t host[256], path[4096];
  parts.lpszHostName = host;
  parts.dwHostNameLength = 256;
  parts.lpszUrlPath = path;
  parts.dwUrlPathLength = 4096;
  if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &parts)) return "Not a valid web address.";
  // GitHub's API refuses requests without a User-Agent.
  const std::wstring agent = L"OutpostKalokiX-Launcher/" + std::wstring(OKX_VERSION, OKX_VERSION + sizeof(OKX_VERSION) - 1);
  HINTERNET session = WinHttpOpen(agent.c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
                                  WINHTTP_NO_PROXY_BYPASS, 0);
  if (!session) return "Could not start the download.";
  std::string error = "Could not reach GitHub. Check your internet connection and try again.";
  if (HINTERNET conn = WinHttpConnect(session, host, parts.nPort, 0)) {
    const DWORD flags = parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0;
    if (HINTERNET req = WinHttpOpenRequest(conn, L"GET", path, nullptr, WINHTTP_NO_REFERER,
                                           WINHTTP_DEFAULT_ACCEPT_TYPES, flags)) {
      DWORD code = 0, len = sizeof(code);
      if (WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
          WinHttpReceiveResponse(req, nullptr) &&
          WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &code, &len,
                              nullptr)) {
        if (code != 200) {
          error = "GitHub answered " + std::to_string(code) + ".";
        } else {
          DWORD content = 0;
          len = sizeof(content);
          const bool sized = WinHttpQueryHeaders(req, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
                                                 nullptr, &content, &len, nullptr);
          error.clear();
          for (DWORD avail = 0; WinHttpQueryDataAvailable(req, &avail) && avail;) {
            const size_t at = out.size();
            out.resize(at + avail);
            DWORD read = 0;
            if (!WinHttpReadData(req, out.data() + at, avail, &read)) {
              error = "The download was interrupted.";
              break;
            }
            out.resize(at + read);
            if (progress) *progress = sized && content ? float(double(out.size()) / content) : -1.0f;
          }
        }
      }
      WinHttpCloseHandle(req);
    }
    WinHttpCloseHandle(conn);
  }
  WinHttpCloseHandle(session);
  return error;
#else
  // Only plain web addresses reach the shell (no quotes or other specials).
  for (char c : url)
    if (c == '\'' || c == '"' || c == '\\' || c == '`' || c == '$' || (unsigned char)c < 0x21)
      return "Not a valid web address.";
  FILE* p = popen(("curl -fsSL '" + url + "'").c_str(), "r");
  if (!p) return "curl is needed to check for updates.";
  char buf[1 << 16];
  for (size_t n; (n = fread(buf, 1, sizeof(buf), p)) > 0;) out.append(buf, n);
  if (pclose(p) != 0) return "Could not reach GitHub. Check your internet connection and try again.";
  (void)progress;
  return "";
#endif
}

// "v1.2.3" / "1.2.3-beta" -> {1, 2, 3}
std::vector<int> ParseVersion(const std::string& text) {
  std::vector<int> parts;
  std::smatch m;
  std::string rest = text;
  static const std::regex number(R"((\d+))");
  while (parts.size() < 4 && std::regex_search(rest, m, number)) {
    parts.push_back(std::stoi(m[1].str()));
    rest = m.suffix().str();
    if (rest.empty() || rest[0] != '.') break;
  }
  while (parts.size() < 3) parts.push_back(0);
  return parts;
}

// The first "key": "value" string in a JSON text, from `from` on.
std::string JsonString(const std::string& json, const std::string& key, size_t from = 0, size_t* at = nullptr) {
  const std::string needle = "\"" + key + "\"";
  size_t k = json.find(needle, from);
  if (k == std::string::npos) return {};
  size_t q1 = json.find('"', json.find(':', k + needle.size()) + 1);
  if (q1 == std::string::npos) return {};
  std::string value;
  for (size_t i = q1 + 1; i < json.size() && json[i] != '"'; ++i) {
    if (json[i] == '\\' && i + 1 < json.size()) ++i;
    value += json[i];
  }
  if (at) *at = k;
  return value;
}

#if defined(_WIN32)
// Runs a command hidden and waits; returns its exit code (-1 if it could not start).
int RunHidden(std::wstring command) {
  STARTUPINFOW si{sizeof(si)};
  si.dwFlags = STARTF_USESHOWWINDOW;
  si.wShowWindow = SW_HIDE;
  PROCESS_INFORMATION pi{};
  if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si,
                      &pi))
    return -1;
  WaitForSingleObject(pi.hProcess, INFINITE);
  DWORD code = 1;
  GetExitCodeProcess(pi.hProcess, &code);
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  return int(code);
}
#endif

}  // namespace

const char* CurrentVersion() {
  // Testing aid: OKX_UPDATE_TEST_VERSION=0.0.1 makes this build look old, so
  // the update check can be tried against the live releases.
  static const char* test = std::getenv("OKX_UPDATE_TEST_VERSION");
  return test && *test ? test : OKX_VERSION;
}

std::optional<Release> CheckLatest(std::string* error) {
  std::string json;
  if (std::string err = HttpGet(kReleasesApi, json); !err.empty()) {
    if (error) *error = err;
    return std::nullopt;
  }
  // Each release object holds "tag_name", "draft" and its "assets" (with
  // "browser_download_url"s) before the next release's "tag_name". Keep the
  // newest published one that has a Windows builder zip.
  std::optional<Release> best;
  size_t pos = 0;
  while (true) {
    size_t tag_at = 0;
    const std::string tag = JsonString(json, "tag_name", pos, &tag_at);
    if (tag.empty()) break;
    size_t next_at = 0;
    const bool has_next = !JsonString(json, "tag_name", tag_at + 1, &next_at).empty();
    const std::string object = json.substr(tag_at, (has_next ? next_at : json.size()) - tag_at);
    pos = tag_at + 1;
    if (object.find("\"draft\": true") != std::string::npos || object.find("\"draft\":true") != std::string::npos)
      continue;
    Release r{tag, {}, kReleasePage + tag};
    for (size_t a = 0;;) {
      size_t url_at = 0;
      const std::string url = JsonString(object, "browser_download_url", a, &url_at);
      if (url.empty()) break;
      if (url.ends_with(kZipSuffix)) {
        r.zip = url;
        break;
      }
      a = url_at + 1;
    }
    if (r.zip.empty()) continue;
    if (!best || ParseVersion(r.tag) > ParseVersion(best->tag)) best = r;
  }
  if (!best) {
    if (error && json.find("tag_name") == std::string::npos) *error = "GitHub did not return any releases.";
    return std::nullopt;
  }
  if (ParseVersion(best->tag) <= ParseVersion(CurrentVersion())) return std::nullopt;
  REXLOG_INFO("OKX: update {} available (this is {})", best->tag, CurrentVersion());
  return best;
}

std::string StartRebuild(const Release& release, const fs::path& game_dir, const fs::path& exe_dir,
                         std::atomic<float>* progress) {
#if defined(_WIN32)
  std::error_code ec;
  if (!fs::exists(game_dir / "default.xex", ec)) return "The game files are not installed.";

  // The new builder goes where a player would unzip it: next to the builder
  // this game was built with (exe_dir is <builder>\OutpostKalokiX by default),
  // or next to the game folder otherwise.
  fs::path parent = exe_dir.parent_path();
  if (fs::exists(parent / "Build-OutpostKalokiX.ps1", ec)) parent = parent.parent_path();
  const std::string zip_name = release.zip.substr(release.zip.find_last_of('/') + 1);
  const fs::path builder = parent / fs::path(zip_name).stem();  // the zip's top folder
  {
    const fs::path probe = parent / "okx_update_write_test.tmp";
    std::ofstream(probe).put('x');
    if (!fs::exists(probe, ec)) return "Cannot write to " + parent.string() + ".";
    fs::remove(probe, ec);
  }

  const fs::path work = fs::temp_directory_path(ec) / "outpost_kaloki_x_update";
  fs::create_directories(work, ec);
  const fs::path zip = work / zip_name;
  std::string data;
  if (std::string err = HttpGet(release.zip, data, progress); !err.empty()) return err;
  {
    std::ofstream out(zip, std::ios::binary | std::ios::trunc);
    out.write(data.data(), std::streamsize(data.size()));
    if (!out) return "Cannot write " + zip.string() + ".";
  }
  if (progress) *progress = -1.0f;

  wchar_t system_dir[MAX_PATH];
  GetSystemDirectoryW(system_dir, MAX_PATH);
  const fs::path tar = fs::path(system_dir) / "tar.exe";
  if (const int code = RunHidden(L"\"" + tar.wstring() + L"\" -xf \"" + zip.wstring() + L"\" -C \"" +
                                 parent.wstring() + L"\"");
      code != 0)
    return "Could not unpack the new builder (tar exit code " + std::to_string(code) + ").";
  fs::remove_all(work, ec);
  const fs::path script = builder / "Build-OutpostKalokiX.ps1";
  if (!fs::exists(script, ec)) return "The download does not contain the builder.";

  // Its own console window: it shows the build and asks nothing (-Yes), then
  // starts the updated game. The launcher quits so the files can be replaced.
  std::wstring cmd = L"powershell.exe -NoProfile -ExecutionPolicy Bypass -File \"" + script.wstring() +
                     L"\" -GameDir \"" + game_dir.wstring() + L"\" -OutDir \"" + exe_dir.wstring() +
                     L"\" -Yes -NoShortcut";
  STARTUPINFOW si{sizeof(si)};
  PROCESS_INFORMATION pi{};
  if (!CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, CREATE_NEW_CONSOLE, nullptr,
                      builder.wstring().c_str(), &si, &pi))
    return "Could not start the builder.";
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  REXLOG_INFO("OKX: started the {} builder in {}", release.tag, builder.string());
  return "";
#else
  (void)game_dir;
  (void)exe_dir;
  (void)progress;
  return "Download the new version from " + release.page;
#endif
}

}  // namespace okx::update
