#!/usr/bin/env python3
"""
ReWinGo barcode scanner sidecar — "show the camera a product, get its code".

Opens a camera, watches for a 1D barcode (EAN/UPC) or QR for a few seconds,
prints ONE JSON line with the decoded code, and exits. HEADLESS (no cv2
window). The kiosk then looks the code up on Open Food Facts and shows the
admin a popup (name, image, suggested slot, points).

Decoding uses OpenCV's built-in cv2.barcode.BarcodeDetector + QRCodeDetector
— no pyzbar / libzbar needed.

stdout (one JSON object per line):
  {"e":"barcode","code":"5449000000996","format":"barcode"}
  {"e":"none"}                         ← nothing decoded before timeout
  {"e":"error","msg":"..."}

Env overrides:
  BARCODE_USB    1 = use a USB webcam (cv2.VideoCapture); else CSI picamera2
  BARCODE_CAM    camera index                       (default 0)
  BARCODE_SECS   seconds to watch                    (default 6)
Test without hardware:
  barcode_scan.py --image /path/to/photo_with_barcode.jpg
"""
import os, sys, json, time

USB  = os.environ.get("BARCODE_USB", "1") == "1"   # default: USB webcam
CAM  = int(os.environ.get("BARCODE_CAM", "0"))
SECS = float(os.environ.get("BARCODE_SECS", "6"))


def emit(obj):
    sys.stdout.write(json.dumps(obj) + "\n")
    sys.stdout.flush()


def decode(detector, qr, frame):
    """Return a decoded string or None."""
    # 1D barcode — API return shape varies across OpenCV builds; be defensive.
    try:
        res = detector.detectAndDecode(frame)
        infos = None
        if isinstance(res, tuple):
            if len(res) == 4:   _, infos, _, _ = res
            elif len(res) == 3: infos, _, _ = res
        seq = infos if isinstance(infos, (list, tuple)) else ([infos] if infos else [])
        for s in seq:
            if s:
                return str(s)
    except Exception:
        pass
    # QR fallback.
    try:
        data, _, _ = qr.detectAndDecode(frame)
        if data:
            return str(data)
    except Exception:
        pass
    return None


def main():
    args = sys.argv[1:]
    try:
        import cv2
    except Exception as e:
        emit({"e": "error", "msg": f"cv2 import failed: {e}"}); emit({"e": "none"}); return 0

    detector = cv2.barcode.BarcodeDetector()
    qr       = cv2.QRCodeDetector()

    if "--image" in args:
        path = args[args.index("--image") + 1]
        img = cv2.imread(path)
        if img is None:
            emit({"e": "error", "msg": f"cannot read {path}"}); emit({"e": "none"}); return 0
        code = decode(detector, qr, img)
        emit({"e": "barcode", "code": code, "format": "barcode"} if code else {"e": "none"})
        return 0

    # ── Open a camera ──
    cap = None
    picam = None
    try:
        if USB:
            cap = cv2.VideoCapture(CAM)
            if not cap.isOpened():
                cap.release(); cap = None
        if cap is None:
            from picamera2 import Picamera2
            picam = Picamera2(CAM)
            picam.configure(picam.create_preview_configuration(
                main={"format": "RGB888", "size": (1280, 720)}))
            picam.start()
    except Exception as e:
        emit({"e": "error", "msg": f"camera open failed: {e}"}); emit({"e": "none"}); return 0

    t_end = time.time() + SECS
    found = None
    try:
        while time.time() < t_end:
            if cap is not None:
                ok, frame = cap.read()
                if not ok:
                    continue
            else:
                rgb = picam.capture_array()
                frame = cv2.cvtColor(rgb, cv2.COLOR_RGB2BGR)
            found = decode(detector, qr, frame)
            if found:
                break
    except Exception as e:
        emit({"e": "error", "msg": f"scan failed: {e}"})
    finally:
        if cap is not None:
            cap.release()
        if picam is not None:
            try: picam.stop()
            except Exception: pass

    emit({"e": "barcode", "code": found, "format": "barcode"} if found else {"e": "none"})
    return 0


if __name__ == "__main__":
    sys.exit(main())
