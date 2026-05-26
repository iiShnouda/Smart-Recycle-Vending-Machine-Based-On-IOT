#!/bin/bash
# rewingo — launcher for the Qt UI.
#
# Lives at /usr/local/bin/rewingo. Sets Qt env vars and exec's the
# binary out of /opt/rewingo/bin. Args are forwarded verbatim.

set -e

PREFIX=/opt/rewingo
BIN="$PREFIX/bin/rewingo"

if [ ! -x "$BIN" ]; then
  echo "rewingo: binary not found at $BIN — re-install the .deb?" >&2
  exit 1
fi

# QtVirtualKeyboard registers itself as the IME backend.
export QT_IM_MODULE=qtvirtualkeyboard

# Uncomment if you see "could not connect to display" when running
# headless / over SSH:
# export QT_QPA_PLATFORM=eglfs

exec "$BIN" "$@"
