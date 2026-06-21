#!/usr/bin/env python3
"""
ReWinGo headless recycle classifier — the camera "brain" for one item.

The STM32 sequences the lane and, when an item sits at the camera position,
sends `EVT,CAMERA`. The kiosk launches THIS script, which:
  • opens the CSI camera (IMX219, cam 0 — NOT the Logitech face cam),
  • runs the recycle YOLO model for a short burst,
  • decides bottle / can / reject,
  • prints ONE JSON verdict line and exits.

It is fully HEADLESS — no cv2 window. The kiosk owns the screen and forwards
the verdict to the STM32 as `VERDICT BOTTLE|CAN|REJECT`. (The cv2-window
scripts, recycle_test.py / recycle.sh, are dev test tools only.)

stdout protocol (one JSON object per line, unbuffered):
  {"e":"verdict","verdict":"bottle","cls":"large_bottle","conf":0.87}
  {"e":"error","msg":"..."}     ← on failure, followed by a reject verdict so
                                  the lane always clears (item handed back).

Class mapping is by NAME so it works with both the old 2-class model
(bottle, can) and the new 3-class model (small_bottle, large_bottle, can):
    name contains "bottle" -> bottle
    name contains "can"    -> can
    otherwise / nothing    -> reject

Env overrides (all optional):
  RECYCLE_MODEL  model path (.onnx/.pt or *_ncnn_model dir)
  RECYCLE_NAMES  path to a .names file (one class per line)
  RECYCLE_CAM    camera index                        (default 0 = CSI)
  RECYCLE_CONF   accept confidence threshold          (default 0.70)
  RECYCLE_BURST  seconds to watch the item            (default 3.0)
  RECYCLE_IMGSZ  inference size                       (default 320)

Test without the rig:
  recycle_classify.py --selftest            # load model, print names, exit
  recycle_classify.py --image sample.jpg    # classify one image (no camera)
"""
import os, sys, json, time, glob

# ── Resolve model + names (prefer the kiosk's deployed 3-class model) ────────
HOME = os.path.expanduser("~")
_KIOSK_MODELS = os.path.join(HOME, ".local/share/ReWinGo/ReWinGo/models")
_TRAIN_W = os.path.join(HOME, "final_yolo_dataset_204/runs/detect/train/weights")

def _default_model():
    cands = [
        os.path.join(_KIOSK_MODELS, "recycle.onnx"),        # NEW 3-class
        os.path.join(_TRAIN_W, "best_ncnn_model"),          # old 2-class (fast)
        os.path.join(_TRAIN_W, "best.pt"),
    ]
    for c in cands:
        if os.path.exists(c):
            return c
    return cands[0]

MODEL = os.environ.get("RECYCLE_MODEL", _default_model())
NAMES = os.environ.get("RECYCLE_NAMES", os.path.join(_KIOSK_MODELS, "recycle.names"))
CAM   = int(os.environ.get("RECYCLE_CAM", "0"))
CONF  = float(os.environ.get("RECYCLE_CONF", "0.70"))
BURST = float(os.environ.get("RECYCLE_BURST", "3.0"))
IMGSZ = int(os.environ.get("RECYCLE_IMGSZ", "320"))
HINT  = 0.35   # keep low-conf boxes for logging; ACCEPT gate is CONF


def emit(obj):
    sys.stdout.write(json.dumps(obj) + "\n")
    sys.stdout.flush()


def load_names():
    """Class-index -> name. Prefer the .names file; fall back to the model's."""
    if NAMES and os.path.exists(NAMES):
        with open(NAMES) as f:
            ls = [ln.strip() for ln in f if ln.strip()]
        if ls:
            return {i: n for i, n in enumerate(ls)}
    return None


def verdict_for(name):
    n = (name or "").lower()
    if "bottle" in n:
        return "bottle"
    if "can" in n:
        return "can"
    return None


def decide(detections):
    """detections: list of (name, conf). Pick the best bottle/can >= CONF."""
    best = None
    for name, conf in detections:
        v = verdict_for(name)
        if v is None or conf < CONF:
            continue
        if best is None or conf > best[2]:
            best = (v, name, conf)
    if best is None:
        emit({"e": "verdict", "verdict": "reject", "cls": None, "conf": 0.0})
        return
    emit({"e": "verdict", "verdict": best[0], "cls": best[1], "conf": round(best[2], 3)})


def run_model(model, frame_bgr):
    """One inference -> list of (name, conf)."""
    r = model.predict(frame_bgr, imgsz=IMGSZ, conf=HINT, verbose=False)[0]
    names = NAME_MAP or model.names
    out = []
    if r.boxes is not None:
        for b in r.boxes:
            ci = int(b.cls)
            out.append((names.get(ci, str(ci)), float(b.conf)))
    return out


def main():
    global NAME_MAP
    args = sys.argv[1:]
    try:
        from ultralytics import YOLO
    except Exception as e:
        emit({"e": "error", "msg": f"ultralytics import failed: {e}"})
        emit({"e": "verdict", "verdict": "reject", "cls": None, "conf": 0.0})
        return 0

    if not os.path.exists(MODEL):
        emit({"e": "error", "msg": f"model not found: {MODEL}"})
        emit({"e": "verdict", "verdict": "reject", "cls": None, "conf": 0.0})
        return 0

    try:
        model = YOLO(MODEL, task="detect")
    except Exception as e:
        emit({"e": "error", "msg": f"model load failed: {e}"})
        emit({"e": "verdict", "verdict": "reject", "cls": None, "conf": 0.0})
        return 0

    NAME_MAP = load_names()

    if "--selftest" in args:
        emit({"e": "info", "model": MODEL, "names": NAME_MAP or model.names})
        return 0

    # ── Test mode: classify a single image (no camera) ──
    if "--image" in args:
        import cv2
        path = args[args.index("--image") + 1]
        img = cv2.imread(path)
        if img is None:
            emit({"e": "error", "msg": f"cannot read image: {path}"})
            emit({"e": "verdict", "verdict": "reject", "cls": None, "conf": 0.0})
            return 0
        decide(run_model(model, img))
        return 0

    # ── Production: open the CSI camera and watch for a short burst ──
    try:
        import cv2
        from picamera2 import Picamera2
        picam = Picamera2(CAM)
        picam.configure(picam.create_preview_configuration(
            main={"format": "RGB888", "size": (1280, 720)}))
        picam.start()
    except Exception as e:
        emit({"e": "error", "msg": f"camera open failed: {e}"})
        emit({"e": "verdict", "verdict": "reject", "cls": None, "conf": 0.0})
        return 0

    detections = []
    t_end = time.time() + BURST
    try:
        # Warm-up frame (first capture is often dark / mid-exposure).
        picam.capture_array()
        while time.time() < t_end:
            frame_rgb = picam.capture_array()
            frame_bgr = cv2.cvtColor(frame_rgb, cv2.COLOR_RGB2BGR)
            detections.extend(run_model(model, frame_bgr))
    except Exception as e:
        emit({"e": "error", "msg": f"inference failed: {e}"})
    finally:
        try:
            picam.stop()
        except Exception:
            pass

    decide(detections)
    return 0


NAME_MAP = None
if __name__ == "__main__":
    sys.exit(main())
