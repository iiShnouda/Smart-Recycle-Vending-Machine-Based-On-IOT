"""Self-capturing enrollment sidecar for the kiosk.

Unlike scripts.enroll_user (which opens cv2 windows and waits for SPACE key
presses — impossible on a touchscreen kiosk), this OPENS THE CAMERA ITSELF,
auto-advances through 3 head poses with NO screen press, writes a live preview
frame to /tmp/rewingo_face.jpg (the kiosk shows it in the round window), and
stores the averaged ArcFace embedding in the SAME faces.db that
scripts.sidecar_identify_selfcam reads — so an enrolled user is recognised at
login.

The user's name is read from the first stdin line (the kiosk writes it).

Line-JSON protocol (one object per line):
    {"e":"stage","stage":"FRONT","count":0,"total":3}
    {"e":"progress","count":1,"total":3}
    {"e":"enrolled","name":"Bavly","user_id":7,"frames":210}
    {"e":"error","msg":"..."}

DEPLOY: copy to /opt/face_rec/scripts/enroll_selfcam.py on the Pi.
"""
import sys, os, json, time, subprocess, threading
from pathlib import Path
import cv2
import numpy as np
import onnxruntime as ort
from app.db_utils import init_db, insert_user
from scripts.voice import speak

MODEL_PATH   = Path("models") / "arcface.onnx"
YUNET_PATH   = Path("models") / "face_detection_yunet_2023mar.onnx"
def _resolve_cam():
    """Pick the USB webcam robustly: FACE_CAM_INDEX override (index or path),
    else the stable /dev/v4l/by-id UVC capture node (survives /dev/video index
    shifts when the CSI camera grabs video0), else index 1."""
    import glob
    env = os.environ.get("FACE_CAM_INDEX", "").strip()
    if env:
        return int(env) if env.lstrip("-").isdigit() else env
    uvc = sorted(glob.glob("/dev/v4l/by-id/*-video-index0"))
    if uvc:
        return uvc[0]                       # USB webcam (e.g. C270) capture node
    return 1                                # CSI usually takes video0, USB cam video1
CAM_INDEX    = _resolve_cam()
PREVIEW_PATH = os.environ.get("FACE_PREVIEW", "/tmp/rewingo_face.jpg")
TOTAL_POSES  = 3
STEP_TIMEOUT = float(os.environ.get("ENROLL_STEP_SEC", "7"))   # per-pose cap
# Detection is the slow part on the Pi CPU. Run it only every Nth frame so the
# preview (written every frame) stays smooth, and embed only ONCE per pose.
DETECT_EVERY = int(os.environ.get("FACE_DETECT_EVERY", "2"))


def emit(o):
    sys.stdout.write(json.dumps(o) + "\n")
    sys.stdout.flush()


def l2n(v, e=1e-10):
    return v / (np.linalg.norm(v) + e)


def open_camera():
    """Open the UVC webcam via V4L2 with a warm-up — same dependable path as
    the identify sidecar (the default backend can deliver zero frames, and the
    first grabs after open fail while the camera powers on)."""
    cap = cv2.VideoCapture(CAM_INDEX, cv2.CAP_V4L2)
    if not cap.isOpened():
        cap = cv2.VideoCapture(CAM_INDEX)
    if not cap.isOpened():
        return None
    cap.set(cv2.CAP_PROP_FOURCC, cv2.VideoWriter_fourcc(*"MJPG"))
    cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)
    t0 = time.time()
    while time.time() - t0 < 2.0:
        ok, _ = cap.read()
        if ok:
            break
        time.sleep(0.05)
    return cap


def write_preview(fr):
    try:
        ok, buf = cv2.imencode(".jpg", cv2.resize(fr, (480, 360)),
                               [cv2.IMWRITE_JPEG_QUALITY, 70])
        if ok:
            tmp = PREVIEW_PATH + ".tmp"
            with open(tmp, "wb") as f:
                f.write(buf.tobytes())
            os.replace(tmp, PREVIEW_PATH)
    except Exception:
        pass


def yaw_of(face):
    """Rough head yaw from YuNet landmarks: nose-x offset from the eye midline,
    normalised by eye span. >0 → turned one way, <0 → the other."""
    r_eye_x, l_eye_x, nose_x = float(face[4]), float(face[6]), float(face[8])
    mid  = (r_eye_x + l_eye_x) / 2.0
    span = abs(l_eye_x - r_eye_x) + 1e-6
    return (nose_x - mid) / span


def main():
    if not MODEL_PATH.exists():
        emit({"e": "error", "msg": "arcface.onnx not found"}); return

    # First stdin line is "name<TAB>mobile" (mobile optional) — the kiosk
    # writes it. Fall back gracefully.
    try:
        raw = sys.stdin.readline().strip()
    except Exception:
        raw = ""
    parts = raw.split("\t")
    name  = parts[0].strip() if parts and parts[0].strip() else "New User"
    phone = parts[1].strip() if len(parts) > 1 else ""

    init_db()
    det = cv2.FaceDetectorYN.create(str(YUNET_PATH), "", (320, 320),
                                    score_threshold=0.7, nms_threshold=0.3, top_k=5)
    sess = ort.InferenceSession(str(MODEL_PATH), providers=["CPUExecutionProvider"])
    iname  = sess.get_inputs()[0].name
    ishape = tuple(sess.get_inputs()[0].shape)

    cap = open_camera()
    if cap is None:
        emit({"e": "error", "msg": "camera open failed"}); return

    def embed(face_bgr):
        img = cv2.resize(face_bgr, (112, 112)).astype(np.float32)
        img = (img - 127.5) / 128.0
        if len(ishape) == 4 and ishape[1] == 3:
            img = np.transpose(img, (2, 0, 1))
        return l2n(sess.run(None, {iname: np.expand_dims(img, 0)})[0][0].astype(np.float32))

    # Three poses, auto-advanced. The predicate is a *preference*: if the user
    # doesn't turn enough within STEP_TIMEOUT we still capture the last good
    # face, so enrollment always completes (no screen press, no getting stuck).
    poses = [
        ("FRONT", "Look straight at the camera",          lambda y: abs(y) < 0.12),
        ("LEFT",  "Please turn your head to the left",    lambda y: y <= -0.13),
        ("RIGHT", "Please turn your head to the right",   lambda y: y >= 0.13),
    ]

    embeddings = []
    grabbed = 0
    fi = 0
    for idx, (label, prompt, pred) in enumerate(poses):
        emit({"e": "stage", "stage": label, "count": idx, "total": TOTAL_POSES})
        speak(prompt)
        t_end = time.time() + STEP_TIMEOUT
        captured_crop = None
        last_crop = None
        while time.time() < t_end:
            ok, fr = cap.read()
            if not ok or fr is None:
                continue
            grabbed += 1
            write_preview(fr)              # EVERY frame → smooth preview
            fi += 1
            if fi % DETECT_EVERY != 0:     # throttle the slow detection
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
            last_crop = fr[y1:y2, x1:x2].copy()
            if pred(yaw_of(b)):
                captured_crop = last_crop
                break
        # Embed ONLY ONCE per pose (ArcFace is the expensive bit) — not every
        # frame, which is what made registration crawl at ~2 fps.
        use_crop = captured_crop if captured_crop is not None else last_crop
        if use_crop is None:
            cap.release()
            if grabbed == 0:
                emit({"e": "error", "msg": "camera delivered no frames (check the webcam)"})
            else:
                emit({"e": "error", "msg": "no face detected — please face the camera"})
            return
        embeddings.append(embed(use_crop))
        emit({"e": "progress", "count": len(embeddings), "total": TOTAL_POSES})

    cap.release()
    avg = l2n(np.mean(np.vstack(embeddings), axis=0).astype(np.float32))
    # Store with the phone number if db_utils supports it; fall back to the
    # name-only signature on older deployments.
    try:
        user_id = insert_user(name, avg, phone)
    except TypeError:
        user_id = insert_user(name, avg)
    speak("All done " + name + ". You can now use face login.")
    emit({"e": "enrolled", "name": name, "user_id": int(user_id),
          "phone": phone, "frames": grabbed})


if __name__ == "__main__":
    try:
        main()
    except Exception as e:
        emit({"e": "error", "msg": f"{type(e).__name__}: {e}"})
