#!/bin/bash
# setup_pi.sh — install build deps on a fresh Pi.
#
# Tested on Pi OS Bookworm 64-bit (Pi 4 + Pi 5). Idempotent.
# Run once before build_deb.sh:
#     sudo bash packaging/setup_pi.sh

set -euo pipefail

if [ "$EUID" -ne 0 ]; then
  echo "Run as root:  sudo bash $0" >&2
  exit 1
fi

apt-get update

# Pi OS Bookworm package names. NOTE:
#   - QuickControls2 dev headers are bundled inside qt6-declarative-dev
#     (no separate `qt6-quickcontrols2-dev` exists on Bookworm).
#   - The Virtual Keyboard QML module is shipped as
#     `qml6-module-qtquick-virtualkeyboard`, NOT `qt6-virtualkeyboard-plugin`.
apt-get install -y \
  build-essential cmake ninja-build pkg-config git \
  qt6-base-dev qt6-declarative-dev \
  qt6-multimedia-dev qt6-serialport-dev qt6-virtualkeyboard-dev \
  qt6-quick3d-dev \
  qt6-l10n-tools \
  qml6-module-qtquick qml6-module-qtquick-controls \
  qml6-module-qtquick-layouts qml6-module-qtquick-templates \
  qml6-module-qtquick-window qml6-module-qtmultimedia \
  qml6-module-qt-labs-settings \
  qml6-module-qtquick-virtualkeyboard \
  qml6-module-qtquick3d \
  qt6-svg-plugins \
  libopencv-dev \
  libmosquitto-dev mosquitto-clients ca-certificates

# Lets non-root run the kiosk against the STM32 USB-CDC without sudo.
if id -u pi >/dev/null 2>&1; then
  usermod -aG dialout pi || true
fi

cat <<EOF

═══════════════════════════════════════════════════════════════
 Build environment ready. Next:

     bash packaging/build_deb.sh
═══════════════════════════════════════════════════════════════
EOF
