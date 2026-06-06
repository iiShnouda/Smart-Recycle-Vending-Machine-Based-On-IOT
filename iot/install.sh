#!/usr/bin/env bash
#
# Deploy the ReWinGo MQTT -> MongoDB bridge as a systemd service on the Pi.
# Idempotent: re-run it to update the code or pick up new deps.
#
#   curl/clone the repo, then:   sudo bash iot/install.sh
#
set -euo pipefail

DEST=/opt/rewingo-bridge
SVC=rewingo-bridge.service
SRC="$(cd "$(dirname "$0")" && pwd)"
RUN_USER="${SUDO_USER:-rewingo}"

echo "==> Installing bridge to $DEST (service user: $RUN_USER)"
mkdir -p "$DEST"
cp "$SRC/bridge.py" "$SRC/requirements.txt" "$DEST/"

echo "==> Python venv + dependencies"
if [ ! -d "$DEST/venv" ]; then
    python3 -m venv "$DEST/venv"
fi
"$DEST/venv/bin/pip" install --quiet --upgrade pip
"$DEST/venv/bin/pip" install --quiet -r "$DEST/requirements.txt"

echo "==> Configuration (/etc/rewingo/.env)"
mkdir -p /etc/rewingo
if [ ! -f /etc/rewingo/.env ]; then
    cp "$SRC/.env.example" /etc/rewingo/.env
    chmod 600 /etc/rewingo/.env
    echo "    !! created /etc/rewingo/.env from the template."
    echo "    !! EDIT IT and set MONGODB_URI before the bridge can store anything."
else
    echo "    /etc/rewingo/.env already exists — leaving it untouched."
    if ! grep -q '^MONGODB_URI=' /etc/rewingo/.env; then
        echo "    NOTE: MONGODB_URI is not in your .env yet — append it:"
        echo "          MONGODB_URI=mongodb+srv://user:pass@cluster.../"
    fi
fi

# The service runs as $RUN_USER; make sure it can read its files.
chown -R "$RUN_USER":"$RUN_USER" "$DEST" 2>/dev/null || true

echo "==> systemd service"
sed "s/^User=rewingo/User=$RUN_USER/" "$SRC/$SVC" > "/etc/systemd/system/$SVC"
systemctl daemon-reload
systemctl enable "$SVC"
systemctl restart "$SVC"

echo
echo "==> Done."
echo "    Status:  systemctl status $SVC"
echo "    Logs:    journalctl -u $SVC -f"
