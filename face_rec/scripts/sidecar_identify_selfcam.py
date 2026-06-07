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

MODEL_PATH = Path("models") / "arcface.onnx"
YUNET_PATH = Path("models") / "face_detection_yunet_2023mar.onnx"
THRESHOLD       = 0.55
RECOGNITION_SEC = float(os.environ.get("FACE_SECONDS", "8"))
CAM_INDEX       = int(os.environ.get("FACE_CAM_INDEX", "1"))   # 1 = Logitech
PREVIEW_PATH    = os.environ.get("FACE_PREVIEW", "/tmp/rewingo_face.jpg")


def emit(o):
    sys.stdout.write(json.dumps(o) + "\n")
    sys.stdout.flush()


def l2n(v, e=1e-10): return v / (np.linalg.norm(v) + e)
def cosine(a, b): return float(np.dot(a, b) / (np.linalg.norm(a) * np.linalg.norm(b) + 1e-10))


def open_camera():
    """Return (kind, handle). Prefer Picamera2 (works for the UVC + CSI cams
    on this Pi); fall back to cv2.VideoCapture."""
    try:
        from picamera2 import Picamera2
        p = Picamera2(CAM_INDEX)
        p.configure(p.create_preview_configuration(
            main={"format": "RGB888", "size": (640, 480)}))
        p.start(); time.sleep(0.4)
        return ("picam", p)
    except Exception:
        cap = cv2.VideoCapture(0)
        return ("cv2", cap) if cap.isOpened() else (None, None)


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

    emit({"e": "stage", "stage": "RECOGNIZE", "seconds": RECOGNITION_SEC})
    best_name, best_score, seen = "UNKNOWN", -1.0, 0
    t_end = time.time() + RECOGNITION_SEC
    while time.time() < t_end:
        fr = grab(kind, h)
        if fr is None:
            continue
        # Write a live preview frame for the kiosk UI. Atomic rename so the
        # QML Image never reads a half-written file.
        try:
            _tmp = PREVIEW_PATH + ".tmp"
            cv2.imwrite(_tmp, cv2.resize(fr, (480, 360)),
                        [cv2.IMWRITE_JPEG_QUALITY, 70])
            os.replace(_tmp, PREVIEW_PATH)
        except Exception:
            pass
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
        for _u, nm, dbe in users:
            s = cosine(emb, dbe)
            if s > best_score:
                best_score, best_name = s, nm

    try:
        h.stop() if kind == "picam" else h.release()
    except Exception:
        pass

    if best_score >= THRESHOLD:
        emit({"e": "identified", "name": best_name, "score": round(best_score, 3), "frames": seen})
    else:
        emit({"e": "unknown", "best": best_name, "score": round(best_score, 3),
              "threshold": THRESHOLD, "frames": seen})


if __name__ == "__main__":
    try:
        main()
    except Exception as e:
        emit({"e": "error", "msg": f"{type(e).__name__}: {e}"})
