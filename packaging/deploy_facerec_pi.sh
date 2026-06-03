#!/bin/bash
# deploy_facerec_pi.sh — install the Python face-rec sidecar on the Pi.
#
# The kiosk (face_rec_sidecar.cpp) expects, on Linux:
#     /opt/face_rec/                       ← a copy of FaceRec_project
#     /opt/face_rec/.venv/bin/python       ← venv with the requirements
#
# HOW TO USE
#   1. From Windows, copy the project to the Pi:
#        scp -r C:/Users/Shnou/Downloads/FaceRec_project rewingo@<pi-ip>:/tmp/FaceRec_project
#   2. On the Pi:
#        sudo bash deploy_facerec_pi.sh /tmp/FaceRec_project
#
# ⚠ KNOWN RISK: mediapipe on arm64.
#   mediapipe ships aarch64 Linux wheels only for some versions. If
#   `pip install mediapipe==0.10.14` fails with "no matching distribution",
#   the liveness step (blink / head-turn, which needs MediaPipe FaceMesh)
#   can't run on the Pi. ArcFace recognition (onnxruntime) still works.
#   See the FALLBACK note at the bottom.

set -euo pipefail

SRC="${1:-/tmp/FaceRec_project}"
DEST=/opt/face_rec

if [ "$EUID" -ne 0 ]; then
  echo "Run as root:  sudo bash $0 <path-to-FaceRec_project>" >&2
  exit 1
fi
if [ ! -d "$SRC" ]; then
  echo "Source not found: $SRC" >&2
  echo "scp the FaceRec_project folder to the Pi first (see header)." >&2
  exit 1
fi

echo "── Installing system deps (python venv + OpenCV runtime) ──"
apt-get update
apt-get install -y python3-venv python3-dev libgl1 libglib2.0-0

echo "── Copying project → $DEST ──"
mkdir -p "$DEST"
# Copy everything EXCEPT any pre-existing venvs (they're machine-specific).
rsync -a --delete \
      --exclude '.venv/' --exclude 'fr_env/' --exclude '__pycache__/' \
      "$SRC"/ "$DEST"/

echo "── Creating venv + installing requirements ──"
python3 -m venv "$DEST/.venv"
"$DEST/.venv/bin/pip" install --upgrade pip

# onnxruntime + opencv + numpy install cleanly on arm64. mediapipe is the
# wildcard — try it, but don't abort the whole script if it fails.
"$DEST/.venv/bin/pip" install opencv-python onnxruntime numpy
if ! "$DEST/.venv/bin/pip" install "mediapipe==0.10.14"; then
  echo
  echo "⚠ mediapipe 0.10.14 has no arm64 wheel on this Pi."
  echo "  Trying the latest mediapipe that DOES have an aarch64 wheel…"
  if ! "$DEST/.venv/bin/pip" install mediapipe; then
    echo "⚠ No mediapipe available for arm64 — liveness will be unavailable."
    echo "  (ArcFace recognition still works; see FALLBACK in the header.)"
  fi
fi

# The kiosk runs as user 'rewingo'; let it read/write the DB + cache.
chown -R rewingo:rewingo "$DEST" 2>/dev/null || true

echo
echo "── Verifying imports ──"
"$DEST/.venv/bin/python" - <<'PY' || true
mods = {}
for m in ("cv2", "numpy", "onnxruntime", "mediapipe"):
    try:
        __import__(m); mods[m] = "OK"
    except Exception as e:
        mods[m] = f"FAIL: {e}"
for k, v in mods.items():
    print(f"  {k:12} {v}")
PY

cat <<EOF

═══════════════════════════════════════════════════════════════
 Face-rec sidecar deployed to $DEST
 Kiosk will auto-find it at $DEST/.venv/bin/python.
 Enroll a face first (on the Pi, with a camera attached):
     cd $DEST && .venv/bin/python -m scripts.enroll_user
═══════════════════════════════════════════════════════════════
EOF
