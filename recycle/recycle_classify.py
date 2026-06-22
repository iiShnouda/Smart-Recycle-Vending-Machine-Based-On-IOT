#!/usr/bin/env python3
"""
ReWinGo headless recycle classifier — the camera "brain" for one item.

The STM32 sequences the lane and, when an item sits at the camera position,
sends `EVT,CAMERA`. The kiosk launches THIS script, which:
  • opens the CSI camera (IMX219, cam 0 — NOT the Logitech face cam),
  • runs the recycle YOLO model for a short burst,
  • decides bottle / can / reject,
  • prints ONE JSON verdict line and exits.

By default it ALSO pops a live cv2 window (RECYCLE_WINDOW=1) showing the camera
feed, detection boxes, per-box confidence, the accept threshold and the running
verdict — so you can watch what it decides. Set RECYCLE_WINDOW=0 for a fully
headless run. The kiosk forwards the verdict on to the belt/servo flow.

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
        os.path.join(os.path.dirname(__file__), "best.pt"), # New custom model
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
CONF  = float(os.environ.get("RECYCLE_CONF", "0.66"))
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
    # RECYCLE_WINDOW=1 (default) pops a live cv2 window on the Pi screen with
    # the detection boxes, the per-box confidence, the accept threshold and the
    # running best verdict — so you can SEE what the camera decides. Set
    # RECYCLE_WINDOW=0 for a fully-headless run.
    SHOW = os.environ.get("RECYCLE_WINDOW", "1") != "0"
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

    WIN = "ReWinGo Recycle — detecting"
    if SHOW:
        try:
            cv2.namedWindow(WIN, cv2.WINDOW_NORMAL)
            cv2.resizeWindow(WIN, 960, 540)
            cv2.moveWindow(WIN, 60, 60)
        except Exception:
            SHOW = False

    names_map = NAME_MAP or model.names
    detections = []
    best = ("reject", None, 0.0)   # (verdict, cls, conf) — best accepted so far
    last_disp = None
    t_end = time.time() + BURST
    try:
        picam.capture_array()      # warm-up (first capture is often dark)
        while time.time() < t_end:
            frame_rgb = picam.capture_array()
            frame_bgr = cv2.cvtColor(frame_rgb, cv2.COLOR_RGB2BGR)
            r = model.predict(frame_bgr, imgsz=IMGSZ, conf=HINT, verbose=False)[0]

            disp = frame_bgr.copy() if SHOW else None
            if r.boxes is not None:
                for b in r.boxes:
                    ci = int(b.cls); nm = names_map.get(ci, str(ci)); cf = float(b.conf)
                    detections.append((nm, cf))
                    v = verdict_for(nm)
                    accepted = (v is not None and cf >= CONF)
                    if accepted and cf > best[2]:
                        best = (v, nm, cf)
                    if SHOW:
                        x1, y1, x2, y2 = [int(z) for z in b.xyxy[0]]
                        col = (0, 200, 0) if accepted else (0, 165, 255)
                        cv2.rectangle(disp, (x1, y1), (x2, y2), col, 2)
                        cv2.putText(disp, f"{nm} {cf:.0%}", (x1, max(14, y1 - 8)),
                                    cv2.FONT_HERSHEY_SIMPLEX, 0.7, col, 2)
            if SHOW:
                cv2.rectangle(disp, (0, 0), (disp.shape[1], 44), (32, 32, 32), -1)
                cv2.putText(disp,
                            f"accept >= {CONF:.0%}   best: {best[0].upper()} {best[2]:.0%}",
                            (12, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.8, (255, 255, 255), 2)
                last_disp = disp
                cv2.imshow(WIN, disp)
                cv2.waitKey(1)
    except Exception as e:
        emit({"e": "error", "msg": f"inference failed: {e}"})
    finally:
        try:
            picam.stop()
        except Exception:
            pass

    # Hold the final verdict on screen for ~1.5 s so the operator can read it.
    if SHOW and last_disp is not None:
        try:
            txt = best[0].upper() if best[1] else "REJECT"
            col = (0, 200, 0) if best[1] else (0, 0, 255)
            cv2.rectangle(last_disp, (0, last_disp.shape[0] - 60),
                          (last_disp.shape[1], last_disp.shape[0]), (20, 20, 20), -1)
            cv2.putText(last_disp, f"VERDICT: {txt}  ({best[2]:.0%})",
                        (12, last_disp.shape[0] - 18),
                        cv2.FONT_HERSHEY_SIMPLEX, 1.0, col, 3)
            cv2.imshow(WIN, last_disp)
            cv2.waitKey(1500)
            cv2.destroyWindow(WIN)
            cv2.waitKey(1)
        except Exception:
            pass

    decide(detections)
    return 0


NAME_MAP = None
if __name__ == "__main__":
    sys.exit(main())
