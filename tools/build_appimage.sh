#!/usr/bin/env bash
# Builds the Linux / Steam Deck AppImage (run inside Linux or WSL from the
# repository root). Adapted from Earthworm Jim HD Recompiled's script.
#
# Needs your own default.xex in okx/assets (setup.ps1 puts it there from your
# package): the build translates it again. A Windows checkout can be built here
# through /mnt/c. No game files go into the AppImage: players install them from
# their own package in the launcher.
#
#   tools/build_appimage.sh [version]
#
# Output: dist/OutpostKalokiX-<version>-linux-x86_64.AppImage
set -euo pipefail

VERSION="${1:-dev}"
SDK_VERSION=0.10.0
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# Build on the Linux filesystem: much faster than /mnt/c, and file names keep their case.
WORK="${OKX_WORK:-$HOME/okx-linux-build}"
SUDO=""; [ "$(id -u)" -ne 0 ] && SUDO=sudo

if [ ! -f "$ROOT/okx/assets/default.xex" ]; then
  echo "okx/assets/default.xex is missing: run setup.ps1 with your package first." >&2
  exit 1
fi

echo "== Packages"
$SUDO apt-get update -qq
$SUDO apt-get install -y -qq wget gnupg lsb-release ca-certificates >/dev/null
if ! command -v clang++-20 >/dev/null && ! $SUDO apt-get install -y -qq clang-20 lld-20 >/dev/null; then
  # Older releases: clang 20 from the official LLVM repository.
  CODENAME="$(lsb_release -cs)"
  wget -qO- https://apt.llvm.org/llvm-snapshot.gpg.key | $SUDO gpg --dearmor --yes -o /usr/share/keyrings/llvm.gpg
  echo "deb [signed-by=/usr/share/keyrings/llvm.gpg] http://apt.llvm.org/$CODENAME/ llvm-toolchain-$CODENAME-20 main" |
    $SUDO tee /etc/apt/sources.list.d/llvm-20.list
  $SUDO apt-get update -qq
  $SUDO apt-get install -y -qq clang-20 lld-20 >/dev/null
fi
$SUDO apt-get install -y -qq g++ cmake ninja-build pkg-config file unzip rsync desktop-file-utils \
  libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxinerama-dev libxss-dev \
  libxkbcommon-dev libwayland-dev libdecor-0-dev libegl-dev libgl-dev libvulkan-dev \
  libasound2-dev libpulse-dev libpipewire-0.3-dev libdbus-1-dev libudev-dev libgtk-3-dev zenity >/dev/null

echo "== Source"
mkdir -p "$WORK/okx" "$WORK/tools"
rsync -a --delete --exclude out --exclude assets --exclude image.bin --exclude '*.log' \
  --exclude 'missing_funcs.all.toml' "$ROOT/okx/" "$WORK/okx/"
# The build translates default.xex again (it stays here, never in the AppImage).
mkdir -p "$WORK/okx/assets"
cp "$ROOT/okx/assets/default.xex" "$WORK/okx/assets/"

echo "== ReXGlue SDK $SDK_VERSION (linux-amd64)"
if [ ! -d "$WORK/tools/rexglue/linux-amd64" ]; then
  mkdir -p "$WORK/tools/rexglue"
  wget -q -O /tmp/rexglue-linux.zip \
    "https://github.com/rexglue/rexglue-sdk/releases/download/v$SDK_VERSION/rexglue-sdk-$SDK_VERSION-linux-amd64.zip"
  unzip -q -o /tmp/rexglue-linux.zip -d "$WORK/tools/rexglue"
fi

echo "== Build"
cd "$WORK/okx"
cmake --preset okx-linux-release
cmake --build --preset okx-linux-release
BIN="$WORK/okx/out/build/okx-linux-release"

echo "== AppDir"
APPDIR="$WORK/AppDir"
rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/bin" "$APPDIR/usr/lib"
cp "$BIN/outpost_kaloki_x" "$APPDIR/usr/bin/"
# The runtime, the GPU plugin and anything else the build put beside the binary.
find "$BIN" -maxdepth 1 -name '*.so*' -exec cp -a {} "$APPDIR/usr/bin/" \;
for lib in librexruntime.so librexgpu-xenos.so libTracyClient.so; do
  [ -e "$APPDIR/usr/bin/$lib" ] || [ ! -e "$WORK/tools/rexglue/linux-amd64/lib/$lib" ] || \
    cp -a "$WORK/tools/rexglue/linux-amd64/lib/$lib" "$APPDIR/usr/bin/"
done
# The SDK needs a GCC 13 C++ runtime (GLIBCXX_3.4.32); bundle it for older
# systems. SteamOS 3 has it already; AppRun only uses the bundled copy when the
# system's is older.
for lib in libstdc++.so.6 libgcc_s.so.1; do
  cp -L "$(g++ -print-file-name=$lib 2>/dev/null || true)" "$APPDIR/usr/lib/" 2>/dev/null ||
    cp -L "/usr/lib/x86_64-linux-gnu/$lib" "$APPDIR/usr/lib/"
done
cp "$ROOT/docs/images/icon.png" "$APPDIR/outpost_kaloki_x.png"
cat > "$APPDIR/outpost_kaloki_x.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Outpost Kaloki X
Comment=Outpost Kaloki X, Xbox 360 version, native PC port
Exec=outpost_kaloki_x
Icon=outpost_kaloki_x
Categories=Game;
Terminal=false
EOF
cat > "$APPDIR/AppRun" <<'EOF'
#!/bin/sh
HERE="$(dirname "$(readlink -f "$0")")"
LIBS="$HERE/usr/bin"
# Use the bundled C++ runtime only when the system's is older than the one
# the game needs; a newer system copy is kept for the graphics drivers.
SYS_CXX="$(ldconfig -p 2>/dev/null | awk '/libstdc\+\+\.so\.6 .*x86-64/ {print $NF; exit}')"
if [ -z "$SYS_CXX" ] || ! grep -q GLIBCXX_3.4.32 "$SYS_CXX" 2>/dev/null; then LIBS="$LIBS:$HERE/usr/lib"; fi
export LD_LIBRARY_PATH="$LIBS${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
exec "$HERE/usr/bin/outpost_kaloki_x" "$@"
EOF
chmod +x "$APPDIR/AppRun"

echo "== Missing libraries check"
LD_LIBRARY_PATH="$APPDIR/usr/bin" ldd "$APPDIR/usr/bin/outpost_kaloki_x" | grep "not found" || echo "none"

echo "== AppImage"
TOOL="$WORK/appimagetool-x86_64.AppImage"
[ -x "$TOOL" ] || { wget -q -O "$TOOL" \
  https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage && chmod +x "$TOOL"; }
mkdir -p "$ROOT/dist"
OUT="$ROOT/dist/OutpostKalokiX-$VERSION-linux-x86_64.AppImage"
ARCH=x86_64 "$TOOL" --appimage-extract-and-run "$APPDIR" "$OUT"
ls -la "$OUT"
