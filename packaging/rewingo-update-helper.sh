#!/bin/bash
# rewingo-update-helper — finishes the in-app self-update flow.
#
# Called by UpdateChecker after it has downloaded the new .deb to /tmp.
#   Arg 1:  path to the .deb file
#
# Lifecycle:
#   1. Wait briefly for the old rewingo binary to exit (it triggered us
#      and then quit, but dpkg -i hates running against a still-open ELF).
#   2. dpkg -i — needs sudo. We rely on /etc/sudoers.d/rewingo to grant
#      passwordless dpkg -i on this exact command path.
#   3. Relaunch /usr/local/bin/rewingo so the kiosk comes back up.
#
# Why not in-app: a running binary can't safely overwrite itself. Linux
# allows it but Qt's plugin loader can dlopen() new plugins from the
# replaced file and crash. Always do dpkg work from a separate process.

set -e

DEB="$1"
LOG=/var/log/rewingo-update.log
exec >> "$LOG" 2>&1

echo "=== $(date -Iseconds) rewingo-update-helper start: $DEB ==="

if [ ! -f "$DEB" ]; then
  echo "✗ .deb not found at $DEB"
  exit 1
fi

# Wait up to 10 s for the old rewingo to actually exit.
for _ in 1 2 3 4 5 6 7 8 9 10; do
  if ! pgrep -x rewingo > /dev/null 2>&1; then
    break
  fi
  sleep 1
done
# Forcibly stop if still alive.
pkill -x rewingo 2>/dev/null || true
sleep 1

# Install. The sudoers rule restricts us to dpkg -i on /tmp/rewingo_*.deb.
sudo /usr/bin/dpkg -i "$DEB"
RC=$?
if [ $RC -ne 0 ]; then
  echo "✗ dpkg returned $RC"
  exit $RC
fi
echo "✔ dpkg install complete"

# Clean up the downloaded file.
rm -f "$DEB"

# Relaunch the kiosk. We use setsid + nohup so the new process detaches
# from this helper script's session.
setsid nohup /usr/local/bin/rewingo > /dev/null 2>&1 &
echo "✔ rewingo relaunched (pid=$!)"

echo "=== $(date -Iseconds) rewingo-update-helper done ==="
