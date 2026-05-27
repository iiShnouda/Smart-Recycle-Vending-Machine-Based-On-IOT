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

cmake -S "$PROJECT_ROOT" -B "$BUILD_DIR" \
      -DCMAKE_BUILD_TYPE=Release \
      -DUSE_OPENCV=ON

cmake --build "$BUILD_DIR" -j

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
