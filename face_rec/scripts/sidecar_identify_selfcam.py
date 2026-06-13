"""Self-capturing Pi face-rec sidecar — opens the camera directly (no stdin
frames) and emits the SAME line-JSON protocol as scripts.sidecar_identify_cv:

    {"e":"stage","stage":"RECOGNIZE","seconds":6}
    {"e":"identified","name":"bavly","score":0.83,"frames":12}
    {"e":"unknown","best":"bavly","score":0.41,"threshold":0.55,"frames":9}
    {"e":"error","msg":"..."}

DEPLOY: copy to /opt/face_rec/scripts/sidecar_identify_selfcam.py on the Pi
(the kiosk launches it as `python -m scripts.sidecar_identify_selfcam` from
/opt/face_rec). Camera index via FACE_CAM_INDEX env (default 1 = Logitech).

Why self-capture: feeding QtMultimedia frames over stdin stalls on the Pi, so
the old sidecar blocked forever on its stdin read. Opening the camera here is
exactly what the standalone face_test.py does, and it works reliably.
"""
import sys, os, json, time
from pathlib import Path
import cv2
import numpy as np
import onnxruntime as ort
from app.db_utils import load_users
from scripts.voice import speak

MODEL_PATH = Path("models") / "arcface.onnx"
YUNET_PATH = Path("models") / "face_detection_yunet_2023mar.onnx"
THRESHOLD       = 0.55
RECOGNITION_SEC = float(os.environ.get("FACE_SECONDS", "8"))
CAM_INDEX       = int(os.environ.get("FACE_CAM_INDEX", "0"))   # 0 = /dev/video0 (C270)
PREVIEW_PATH    = os.environ.get("FACE_PREVIEW", "/tmp/rewingo_face.jpg")
# Detection + embedding is the slow part. Run it only every Nth frame so the
# preview (written every frame) stays smooth, and stop the moment we have a
# confident match so login is fast instead of always running the full window.
DETECT_EVERY    = int(os.environ.get("FACE_DETECT_EVERY", "2"))
CONFIRM_HITS    = int(os.environ.get("FACE_CONFIRM_HITS", "2"))


def emit(o):
    sys.stdout.write(json.dumps(o) + "\n")
    sys.stdout.flush()


def l2n(v, e=1e-10): return v / (np.linalg.norm(v) + e)
def cosine(a, b): return float(np.dot(a, b) / (np.linalg.norm(a) * np.linalg.norm(b) + 1e-10))


def open_camera():
    """Open the UVC webcam (Logitech C270 = /dev/video0) reliably and return
    (kind, handle) or (None, None).

    The V4L2 backend is the dependable path on this Pi — the default backend
    sometimes selects GStreamer and then delivers zero frames. We do NOT use
    Picamera2: it targets CSI/libcamera cameras (not UVC) and isn't installed
    here, so trying it first just wasted time. Finally we warm the camera up:
    the first grabs after opening a UVC cam fail while it powers up / negotiates
    the stream, and starting the recognise loop against a cold camera was why a
    run could report frames:0."""
    cap = cv2.VideoCapture(CAM_INDEX, cv2.CAP_V4L2)
    if not cap.isOpened():
        cap = cv2.VideoCapture(CAM_INDEX)        # last-resort default backend
    if not cap.isOpened():
        return (None, None)
    # MJPG @ 640x480 is well supported by the C270 and keeps USB bandwidth low.
    cap.set(cv2.CAP_PROP_FOURCC, cv2.VideoWriter_fourcc(*"MJPG"))
    cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)
    # Warm-up: discard failed/black frames for up to 2 s until one reads OK.
    t0 = time.time()
    while time.time() - t0 < 2.0:
        ok, _ = cap.read()
        if ok:
            break
        time.sleep(0.05)
    return ("cv2", cap)


def grab(kind, h):
    if kind == "picam":
        return cv2.cvtColor(h.capture_array(), cv2.COLOR_RGB2BGR)
    ok, f = h.read()
    return f if ok else None


def main():
    if not MODEL_PATH.exists():
        emit({"e": "error", "msg": f"arcface.onnx not found at {MODEL_PATH}"}); return
    try:
        users = load_users()
    except Exception as e:
        emit({"e": "error", "msg": f"DB error: {e}"}); return
    if not users:
        emit({"e": "error", "msg": "No users enrolled. Enroll first."}); return

    det  = cv2.FaceDetectorYN.create(str(YUNET_PATH), "", (320, 320),
                                     score_threshold=0.7, nms_threshold=0.3, top_k=50)
    sess = ort.InferenceSession(str(MODEL_PATH), providers=["CPUExecutionProvider"])
    iname  = sess.get_inputs()[0].name
    ishape = tuple(sess.get_inputs()[0].shape)

    kind, h = open_camera()
    if kind is None:
        emit({"e": "error", "msg": "camera open failed"}); return

    def embed(face):
        img = cv2.resize(face, (112, 112)).astype(np.float32)
        img = (img - 127.5) / 128.0
        if len(ishape) == 4 and ishape[1] == 3:
            img = np.transpose(img, (2, 0, 1))
        return l2n(sess.run(None, {iname: np.expand_dims(img, 0)})[0][0].astype(np.float32))

    def write_preview(fr):
        try:
            ok_enc, buf = cv2.imencode(".jpg", cv2.resize(fr, (480, 360)),
                                       [cv2.IMWRITE_JPEG_QUALITY, 70])
            if ok_enc:
                _tmp = PREVIEW_PATH + ".tmp"
                with open(_tmp, "wb") as _pf:
                    _pf.write(buf.tobytes())
                os.replace(_tmp, PREVIEW_PATH)
        except Exception:
            pass

    emit({"e": "stage", "stage": "RECOGNIZE", "seconds": RECOGNITION_SEC})
    speak("Please look at the camera")
    best_name, best_score, seen, grabbed = "UNKNOWN", -1.0, 0, 0
    hits, last_hit = 0, None
    fi = 0
    t_end = time.time() + RECOGNITION_SEC
    while time.time() < t_end:
        fr = grab(kind, h)
        if fr is None:
            continue
        grabbed += 1
        write_preview(fr)                  # EVERY frame → smooth preview
        fi += 1
        if fi % DETECT_EVERY != 0:         # throttle the slow detect + embed
            continue
        H, W = fr.shape[:2]
        det.setInputSize((W, H))
        _, faces = det.detect(fr)
        if faces is None or len(faces) == 0:
            continue
        b = max(faces, key=lambda f: f[-1])
        x, y, bw, bh = b[:4].astype(int)
        x1, y1 = max(0, x), max(0, y)
        x2, y2 = min(W, x + bw), min(H, y + bh)
        if x2 <= x1 or y2 <= y1:
            continue
        seen += 1
        emb = embed(fr[y1:y2, x1:x2])
        fr_name, fr_score = "UNKNOWN", -1.0
        for _u, nm, dbe in users:
            s = cosine(emb, dbe)
            if s > fr_score:
                fr_score, fr_name = s, nm
        if fr_score > best_score:
            best_score, best_name = fr_score, fr_name
        # Early exit: a couple of confident frames of the SAME person is enough
        # — don't make a registered user wait out the whole window.
        if fr_score >= THRESHOLD:
            hits = hits + 1 if fr_name == last_hit else 1
            last_hit = fr_name
            if hits >= CONFIRM_HITS:
                best_name, best_score = fr_name, fr_score
                break
        else:
            hits, last_hit = 0, None

    try:
        h.stop() if kind == "picam" else h.release()
    except Exception:
        pass

    # No raw frames at all → the camera is the problem (unplugged, busy, or it
    # never warmed up), NOT "face not recognised". Report it so the kiosk can
    # say so instead of silently routing to registration.
    if grabbed == 0:
        emit({"e": "error", "msg": "camera delivered no frames (check the webcam)"})
        return

    if best_score >= THRESHOLD:
        speak("Welcome " + best_name)
        emit({"e": "identified", "name": best_name, "score": round(best_score, 3),
              "frames": seen, "grabbed": grabbed})
    else:
        emit({"e": "unknown", "best": best_name, "score": round(best_score, 3),
              "threshold": THRESHOLD, "frames": seen, "grabbed": grabbed})


if __name__ == "__main__":
    try:
        main()
    except Exception as e:
        emit({"e": "error", "msg": f"{type(e).__name__}: {e}"})
