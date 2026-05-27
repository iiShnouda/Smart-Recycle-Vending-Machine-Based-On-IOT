#!/bin/bash
# build_deb.sh — configure, compile, and package as a .deb.
#
# From the project root:  bash packaging/build_deb.sh
# Output:  build/rewingo_<version>_arm64.deb
#
# First-time on a Pi:  sudo bash packaging/setup_pi.sh first.

set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"

cd "$PROJECT_ROOT"

# Maintainer scripts must be executable. Git on Windows strips +x —
# re-apply here so the .deb gets the right perms. We no longer ship
# the Flask backend or its launcher (rewingo-backend.sh) since the
# MQTT switch — only the kiosk launcher and the self-update helper.
chmod +x packaging/rewingo.sh
chmod +x packaging/rewingo-update-helper.sh 2>/dev/null || true
chmod +x packaging/debian/postinst packaging/debian/prerm
chmod +x packaging/setup_pi.sh

echo "═══════════════════════════════════════════════════════════════"
echo " ReWinGo .deb build"
echo " Project root: $PROJECT_ROOT"
echo "═══════════════════════════════════════════════════════════════"

# Compile .ts → .qm before cmake configure: the CMakeLists adds the
# .qm files as qrc resources directly (not via qt_add_translations) so
# they must exist on disk at configure time. They're gitignored
# because they're build artifacts, so a fresh clone is missing them.
echo "── Compiling translations (lrelease) ──"
for ts in "$PROJECT_ROOT"/resources/translations/*.ts; do
  [ -f "$ts" ] || continue
  qm="${ts%.ts}.qm"
  # Try Qt6's lrelease first, fall back to the unversioned binary.
  if command -v /usr/lib/qt6/bin/lrelease >/dev/null 2>&1; then
    /usr/lib/qt6/bin/lrelease "$ts" -qm "$qm"
  elif command -v lrelease-qt6 >/dev/null 2>&1; then
    lrelease-qt6 "$ts" -qm "$qm"
  else
    lrelease "$ts" -qm "$qm"
  fi
done

cmake -S "$PROJECT_ROOT" -B "$BUILD_DIR" \
      -DCMAKE_BUILD_TYPE=Release \
      -DUSE_OPENCV=ON

# Parallelism: a Pi 4 with 4 GB RAM gets OOM-killed (and sometimes
# panic-reboots) when 4 g++ instances all link Qt + OpenCV in
# parallel. -j2 keeps peak RSS under control. Override with the
# JOBS env var if you have a Pi 5 / lots of swap.
JOBS="${JOBS:-2}"
echo "── Compiling with -j${JOBS} (raise via JOBS=4 if you have RAM) ──"
cmake --build "$BUILD_DIR" -j"${JOBS}"

# CPack reads the DEB block in CMakeLists.txt and produces the file.
cmake --build "$BUILD_DIR" --target package

echo
echo "═══════════════════════════════════════════════════════════════"
echo " Built:"
ls -lh "$BUILD_DIR"/rewingo_*.deb
echo
echo " Install with:"
echo "    sudo apt install $BUILD_DIR/rewingo_*_arm64.deb"
echo "═══════════════════════════════════════════════════════════════"
