#!/bin/bash
# rewingo-backend — start the Flask MongoDB-bridge backend.
#
# Activates the virtualenv that postinst created, sources the .env, and
# runs server.py. Run from a terminal or wire into a systemd unit when
# you're ready for auto-start.

set -e

PREFIX=/opt/rewingo
VENV="$PREFIX/backend/venv"
SERVER="$PREFIX/backend/server.py"
ENV_FILE=/etc/rewingo/.env

[ -f "$SERVER" ] || { echo "rewingo-backend: $SERVER missing" >&2; exit 1; }
[ -d "$VENV"   ] || { echo "rewingo-backend: venv missing — re-install"  >&2; exit 1; }

if [ -f "$ENV_FILE" ]; then
  set -a
  # shellcheck disable=SC1090
  source "$ENV_FILE"
  set +a
else
  echo "rewingo-backend: warning — $ENV_FILE not found." >&2
  echo "                Copy /opt/rewingo/backend/.env.example to $ENV_FILE" >&2
fi

# shellcheck disable=SC1091
source "$VENV/bin/activate"
exec python "$SERVER" "$@"
