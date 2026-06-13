#!/bin/bash
# rewingo-update-helper — finishes the in-app self-update flow.
#
# Called (detached) by UpdateChecker after it downloads the new .deb to /tmp.
#   Arg 1:  path to the .deb file
#
# This script is deliberately DEFENSIVE. Every "obvious" simplification here
# has bitten us on the real kiosk, so read the notes before touching it:
#
#  • NO `set -e`.  `dpkg -i` frequently returns NON-ZERO on this Pi because of
#    an unrelated, pre-existing pending trigger (initramfs-tools can't write
#    /boot). OUR package still installs perfectly. With `set -e` the script
#    aborted on that exit code *before relaunching the kiosk*, so the screen
#    went black and the update "didn't work". We therefore judge success by
#    READING BACK the installed version, never by dpkg's exit status.
#
#  • Install BEFORE killing the old kiosk.  Replacing the on-disk binary while
#    the old process is still running is safe on Linux — the running process
#    keeps its open inode. We only kill it *after* the new package is in place,
#    so whatever relaunches it picks up the NEW binary, never a half-written one.
#
#  • Relaunch via the session autostart loop, with a direct launch as fallback.
#    The kiosk is normally supervised by a labwc autostart `while` loop, so a
#    plain `pkill` makes it respawn the new binary. If there's no supervisor
#    (dev box), we start it ourselves with the GUI session env restored.

DEB="$1"
LOG=/var/log/rewingo-update.log
# Fall back to a user-writable log if /var/log isn't writable for us.
if ! { : >> "$LOG"; } 2>/dev/null; then
  LOG="$HOME/rewingo-update.log"
fi
exec >> "$LOG" 2>&1

echo "=== $(date -Iseconds) rewingo-update-helper start: $DEB ==="

if [ ! -f "$DEB" ]; then
  echo "RESULT:FAIL:no-deb:$DEB"
  exit 1
fi

# Version we're trying to land, parsed from rewingo_<ver>_arm64.deb.
WANT=$(basename "$DEB" | sed -n 's/^rewingo_\(.*\)_arm64\.deb$/\1/p')
PREV=$(dpkg-query -W -f='${Version}' rewingo 2>/dev/null || echo "?")
echo "prev=$PREV want=$WANT"

# 1. Install. The sudoers rule grants passwordless `dpkg -i` on /tmp/rewingo_*.deb
#    ONLY — so we never call any other sudo command here (a non-allowed sudo
#    would prompt/fail). Tolerate a non-zero exit: on this Pi `dpkg -i` returns
#    non-zero because of a pre-existing, unrelated initramfs-tools trigger that
#    can't write /boot — OUR package still installs fine, which step 2 verifies.
sudo /usr/bin/dpkg -i "$DEB"
echo "dpkg rc=$?"

# 2. Verify by reading the package DB back, NOT by trusting dpkg's exit code.
GOT=$(dpkg-query -W -f='${Version}' rewingo 2>/dev/null || echo "?")
echo "installed=$GOT"
rm -f "$DEB"

if [ -n "$WANT" ] && [ "$GOT" != "$WANT" ]; then
  echo "RESULT:FAIL:version:got=$GOT:want=$WANT"
  # Fall through and still relaunch SOMETHING so we don't leave a black screen.
fi

# 3. Swap the running kiosk. Kill the old process; the session autostart loop
#    relaunches the freshly-installed binary.
pkill -x rewingo 2>/dev/null || true

# Give an autostart supervisor a few seconds to bring it back on its own.
for _ in 1 2 3 4 5; do
  pgrep -x rewingo >/dev/null 2>&1 && break
  sleep 1
done

# Fallback: no supervisor relaunched it — start it ourselves with the GUI
# session environment restored (a detached helper doesn't inherit DISPLAY etc.).
if ! pgrep -x rewingo >/dev/null 2>&1; then
  echo "autostart did not relaunch — starting directly"
  export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}"
  export DISPLAY="${DISPLAY:-:0}"
  export XAUTHORITY="${XAUTHORITY:-$HOME/.Xauthority}"
  export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-xcb}"
  export QT_IM_MODULE="${QT_IM_MODULE:-qtvirtualkeyboard}"
  setsid nohup /usr/local/bin/rewingo > /dev/null 2>&1 < /dev/null &
  echo "relaunched directly pid=$! DISPLAY=$DISPLAY platform=$QT_QPA_PLATFORM"
else
  echo "autostart relaunched rewingo pid=$(pgrep -x rewingo | head -1)"
fi

if [ -n "$WANT" ] && [ "$GOT" != "$WANT" ]; then
  echo "=== $(date -Iseconds) rewingo-update-helper done (version mismatch) ==="
  exit 1
fi
echo "RESULT:OK:$GOT"
echo "=== $(date -Iseconds) rewingo-update-helper done ==="
