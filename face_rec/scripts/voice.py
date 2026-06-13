"""Tiny non-blocking text-to-speech for the kiosk face sidecars.

Uses piper (installed in the venv) to synthesize a WAV and aplay to play it.
speak() returns immediately — synthesis + playback run on a daemon thread so
they never stall the camera loop. A lock serialises utterances so prompts don't
overlap. If piper or the voice model is missing it silently no-ops, so the
sidecars work with or without audio.

DEPLOY: copy to /opt/face_rec/scripts/voice.py on the Pi. Requires:
    /opt/face_rec/.venv/bin/piper            (pip install piper-tts)
    /opt/face_rec/voices/en_US-amy-low.onnx  (+ .onnx.json)
"""
import os, subprocess, threading

_PIPER = os.environ.get("PIPER_BIN",   "/opt/face_rec/.venv/bin/piper")
_VOICE = os.environ.get("PIPER_VOICE", "/opt/face_rec/voices/en_US-amy-low.onnx")
_WAV   = "/tmp/rewingo_say.wav"
_lock  = threading.Lock()


def available():
    return os.path.exists(_PIPER) and os.path.exists(_VOICE)


def _run(text):
    # Serialise so two prompts never talk over each other.
    with _lock:
        try:
            subprocess.run([_PIPER, "-m", _VOICE, "-f", _WAV],
                           input=text.encode("utf-8"),
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                           timeout=15)
            subprocess.run(["aplay", "-q", _WAV],
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                           timeout=15)
        except Exception:
            pass


def speak(text):
    """Fire-and-forget. No-op if TTS isn't installed."""
    if not text or not available():
        return
    threading.Thread(target=_run, args=(text,), daemon=True).start()
