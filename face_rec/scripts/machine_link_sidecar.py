#!/usr/bin/env python3
"""machine_link_sidecar — bridges the kiosk to the backend's IoT WebSocket.

Connects to  <ws_base>?type=machine&machineId=<id>  using the system resolver
(works on networks where Qt's network stack can't resolve the host), announces
the machine online, and relays inbound JSON messages to stdout, one per line.
The C++ MachineLink reads stdout and, for a {"type":"login","token","user"}
event matching the current QR token, logs the user in.

Usage:  python machine_link_sidecar.py <ws_base_url> <machine_id>
Needs:  pip install websocket-client   (deploy to /opt/face_rec/scripts/)
"""
import sys, json, time

try:
    import websocket  # websocket-client
except Exception as e:  # pragma: no cover
    print(json.dumps({"type": "error", "msg": f"websocket-client missing: {e}"}), flush=True)
    sys.exit(1)

WS_BASE = sys.argv[1] if len(sys.argv) > 1 else ""
MACHINE = sys.argv[2] if len(sys.argv) > 2 else ""
URL = f"{WS_BASE}?type=machine&machineId={MACHINE}"


def emit(o):
    print(json.dumps(o), flush=True)


def on_open(ws):
    emit({"type": "connected"})
    try:
        ws.send(json.dumps({"type": "status", "status": "online"}))
    except Exception:
        pass


def on_message(ws, message):
    # Relay whatever the backend sends; the C++ side filters by token.
    try:
        obj = json.loads(message)
    except Exception:
        return
    if isinstance(obj, dict):
        emit(obj)


def on_error(ws, err):
    emit({"type": "ws_error", "msg": str(err)})


def on_close(ws, *a):
    emit({"type": "disconnected"})


def main():
    if not WS_BASE or not MACHINE:
        emit({"type": "error", "msg": "ws_base and machine_id required"})
        return
    while True:
        try:
            ws = websocket.WebSocketApp(
                URL, on_open=on_open, on_message=on_message,
                on_error=on_error, on_close=on_close)
            ws.run_forever(ping_interval=30, ping_timeout=10)
        except Exception as e:
            emit({"type": "ws_error", "msg": str(e)})
        time.sleep(5)   # reconnect loop


if __name__ == "__main__":
    main()
