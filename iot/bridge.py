#!/usr/bin/env python3
"""
rewingo-bridge — the MQTT → MongoDB half of the ReWinGo IoT pipe.

The kiosk (the Qt app) already PUBLISHES every database write to a Mosquitto
broker under

    rewingo/<kiosk_id>/<collection>

This service is the missing SUBSCRIBER: it listens to every kiosk and persists
each message into the matching MongoDB collection (MongoDB Atlas, or any
mongodb:// / mongodb+srv:// URI). Run it once, next to the broker — typically
on the Pi itself. One bridge can serve any number of kiosks.

    kiosk (Qt) ──publish──▶ Mosquitto ──this bridge──▶ MongoDB Atlas

Topic → collection routing
──────────────────────────
    rewingo/<id>/transactions     →  transactions      (append / insert)
    rewingo/<id>/dispense_faults  →  dispense_faults    (append / insert)
    rewingo/<id>/inventory        →  inventory_events   (append / insert)
    rewingo/<id>/products         →  products           (upsert on kiosk_id+slot)
    rewingo/<id>/product_catalog  →  product_catalog    (upsert on id)
    rewingo/<id>/status           →  kiosks             (online/offline + last_seen)
    rewingo/<id>/heartbeat        →  kiosks             (uptime/version + last_seen)
    rewingo/<id>/<anything-else>  →  events             (raw, nothing is lost)

Every stored document is tagged with `kiosk_id` (from the topic), `_topic`,
and `_received_at` (server UTC time).

Configuration
─────────────
Read from /etc/rewingo/.env (the same file the kiosk uses) — KEY=VALUE lines,
`#` comments ignored. Real environment variables override the file. Override
the file location with REWINGO_ENV.

    MQTT_HOST      broker host            (default 127.0.0.1)
    MQTT_PORT      broker port            (default 1883)
    MQTT_USERNAME  broker user            (default empty / anonymous)
    MQTT_PASSWORD  broker pass
    MQTT_TLS       "1" to use TLS         (default 0 — local broker is plain)
    MQTT_TOPIC     subscription filter    (default rewingo/#)
    MONGODB_URI    mongodb+srv://...      (REQUIRED — no default)
    MONGODB_DB     database name          (default rewingo)
    LOG_LEVEL      DEBUG/INFO/WARNING      (default INFO)

Dependencies: paho-mqtt>=2.0, pymongo[srv]>=4.6  (see requirements.txt).
"""

from __future__ import annotations

import json
import logging
import os
import signal
import sys
import time
from datetime import datetime, timezone

import paho.mqtt.client as mqtt
from pymongo import MongoClient
from pymongo.errors import PyMongoError

LOG = logging.getLogger("rewingo-bridge")

# ── topic suffix → (mongo collection, write mode) ────────────────────────────
# write mode:  None            → insert_one (append-only log)
#              (keys, ...)      → update_one upsert, filter built from those keys
#              "status"/"heartbeat" → special kiosk-presence handlers
ROUTES = {
    "transactions":    ("transactions",     None),
    "dispense_faults": ("dispense_faults",  None),
    "faults":          ("dispense_faults",  None),     # alias, just in case
    "inventory":       ("inventory_events", None),
    "products":        ("products",         ("kiosk_id", "slot")),
    "product_catalog": ("product_catalog",  ("id",)),
    "status":          ("kiosks",           "status"),
    "heartbeat":       ("kiosks",           "heartbeat"),
}
FALLBACK_COLLECTION = "events"   # unknown suffixes land here so nothing is lost

WRITE_RETRIES = 3
WRITE_BACKOFF = 0.5              # seconds, grows linearly per retry


# ── config ───────────────────────────────────────────────────────────────────
def parse_env_file(path: str) -> dict:
    """Tiny KEY=VALUE parser, matching how the Qt app reads /etc/rewingo/.env."""
    out: dict[str, str] = {}
    try:
        with open(path, "r", encoding="utf-8") as fh:
            for raw in fh:
                line = raw.strip()
                if not line or line.startswith("#") or "=" not in line:
                    continue
                key, _, val = line.partition("=")
                out[key.strip()] = val.strip().strip('"').strip("'")
    except FileNotFoundError:
        LOG.warning("env file %s not found — relying on process environment", path)
    return out


def load_config() -> dict:
    env_path = os.environ.get("REWINGO_ENV", "/etc/rewingo/.env")
    file_cfg = parse_env_file(env_path)

    def get(key: str, default: str = "") -> str:
        # real environment wins over the file
        return os.environ.get(key, file_cfg.get(key, default)).strip()

    cfg = {
        "mqtt_host": get("MQTT_HOST", "127.0.0.1") or "127.0.0.1",
        "mqtt_port": int(get("MQTT_PORT", "1883") or "1883"),
        "mqtt_user": get("MQTT_USERNAME"),
        "mqtt_pass": get("MQTT_PASSWORD"),
        "mqtt_tls":  get("MQTT_TLS", "0") not in ("", "0", "false", "False"),
        "topic":     get("MQTT_TOPIC", "rewingo/#") or "rewingo/#",
        "mongo_uri": get("MONGODB_URI"),
        "mongo_db":  get("MONGODB_DB", "rewingo") or "rewingo",
        "log_level": get("LOG_LEVEL", "INFO") or "INFO",
    }
    return cfg


# ── mongo writers ─────────────────────────────────────────────────────────────
def _with_retry(fn) -> bool:
    """Run a Mongo write, retrying transient failures. Returns True on success."""
    for attempt in range(1, WRITE_RETRIES + 1):
        try:
            fn()
            return True
        except PyMongoError as exc:
            if attempt == WRITE_RETRIES:
                LOG.error("mongo write failed after %d tries: %s", attempt, exc)
                return False
            LOG.warning("mongo write failed (try %d/%d): %s — retrying",
                        attempt, WRITE_RETRIES, exc)
            time.sleep(WRITE_BACKOFF * attempt)
    return False


def store(db, kiosk_id: str, suffix: str, topic: str, payload: str) -> None:
    collection, mode = ROUTES.get(suffix, (FALLBACK_COLLECTION, None))
    now = datetime.now(timezone.utc)

    # Decode the payload. Most messages are JSON objects; /status is a bare
    # string ("online"/"offline"); anything else we keep under "value".
    try:
        parsed = json.loads(payload)
    except (json.JSONDecodeError, ValueError):
        parsed = payload
    doc = dict(parsed) if isinstance(parsed, dict) else {"value": parsed}

    doc.setdefault("kiosk_id", kiosk_id)
    doc["_topic"] = topic
    doc["_received_at"] = now

    coll = db[collection]

    # ── kiosk presence (status / heartbeat) → one row per kiosk ──────────────
    if mode in ("status", "heartbeat"):
        set_fields = {"kiosk_id": kiosk_id, "last_seen": now}
        if mode == "status":
            set_fields["status"] = payload.strip() or "unknown"
        else:  # heartbeat — merge whatever fields the kiosk sent
            for k, v in doc.items():
                if not k.startswith("_") and k != "kiosk_id":
                    set_fields[k] = v
            set_fields["status"] = "online"
        ok = _with_retry(lambda: coll.update_one(
            {"_id": kiosk_id}, {"$set": set_fields}, upsert=True))
        if ok:
            LOG.log(logging.DEBUG if mode == "heartbeat" else logging.INFO,
                    "kiosk %s → %s (%s)", kiosk_id, collection, set_fields.get("status"))
        return

    # ── state collections (products / catalog) → upsert on a natural key ─────
    if isinstance(mode, tuple):
        filt = {k: doc.get(k) for k in mode}
        # Don't upsert with a half-empty key (would create junk rows).
        if any(v is None for v in filt.values()):
            LOG.warning("skipping %s: missing key field(s) %s", collection, filt)
            return
        ok = _with_retry(lambda: coll.update_one(
            filt, {"$set": doc, "$setOnInsert": {"_created_at": now}}, upsert=True))
        if ok:
            LOG.info("upsert %s %s", collection, filt)
        return

    # ── append-only logs (transactions / faults / inventory / events) ────────
    ok = _with_retry(lambda: coll.insert_one(doc))
    if ok:
        LOG.info("insert %s (kiosk=%s)", collection, kiosk_id)


# ── mqtt callbacks (paho v2 signatures) ───────────────────────────────────────
def on_connect(client, userdata, flags, reason_code, properties=None):
    if reason_code == 0 or getattr(reason_code, "is_failure", False) is False:
        topic = userdata["topic"]
        client.subscribe(topic, qos=1)
        LOG.info("connected to broker, subscribed to %s", topic)
    else:
        LOG.error("broker connect failed: %s", reason_code)


def on_disconnect(client, userdata, flags=None, reason_code=None, properties=None):
    LOG.warning("disconnected from broker (%s) — paho will auto-reconnect",
                reason_code)


def on_message(client, userdata, msg):
    db = userdata["db"]
    try:
        payload = msg.payload.decode("utf-8", errors="replace")
        parts = msg.topic.split("/")
        # expected: rewingo/<kiosk_id>/<suffix>[/...]
        if len(parts) < 3:
            LOG.debug("ignoring odd topic %s", msg.topic)
            return
        kiosk_id, suffix = parts[1], parts[2]
        store(db, kiosk_id, suffix, msg.topic, payload)
    except Exception:                       # never let one bad message kill the loop
        LOG.exception("failed to handle message on %s", msg.topic)


# ── main ──────────────────────────────────────────────────────────────────────
def main() -> int:
    cfg = load_config()
    logging.basicConfig(
        level=getattr(logging, cfg["log_level"].upper(), logging.INFO),
        format="%(asctime)s %(levelname)-7s %(message)s")

    # No Mongo URI yet (e.g. Atlas not set up) — idle quietly instead of
    # crash-looping, so the unit stays "active" and springs to life the moment
    # the operator adds MONGODB_URI and restarts. SIGTERM still stops us.
    if not cfg["mongo_uri"]:
        LOG.warning("MONGODB_URI not set in /etc/rewingo/.env — bridge idle, "
                    "waiting for it. Add the URI then: systemctl restart rewingo-bridge")
        while not cfg["mongo_uri"]:
            time.sleep(30)
            cfg = load_config()
        LOG.info("MONGODB_URI now present — continuing startup")

    LOG.info("rewingo-bridge starting — broker %s:%d, db '%s'",
             cfg["mqtt_host"], cfg["mqtt_port"], cfg["mongo_db"])

    # MongoDB — verify we can actually reach it before we start consuming.
    mongo = MongoClient(cfg["mongo_uri"], appname="rewingo-bridge",
                        serverSelectionTimeoutMS=8000)
    try:
        mongo.admin.command("ping")
        LOG.info("MongoDB reachable")
    except PyMongoError as exc:
        # Don't crash-loop: keep running, writes will retry as Atlas comes back.
        LOG.warning("MongoDB not reachable at startup (%s) — continuing, will retry on write", exc)
    db = mongo[cfg["mongo_db"]]

    # clean_session=False + a fixed client_id => the broker queues our QoS-1
    # messages while the bridge is briefly down (e.g. a redeploy/restart), so
    # transactions published in that window still arrive once we reconnect.
    client = mqtt.Client(
        mqtt.CallbackAPIVersion.VERSION2,
        client_id="rewingo-bridge",
        clean_session=False,
        userdata={"db": db, "topic": cfg["topic"]})
    client.on_connect = on_connect
    client.on_disconnect = on_disconnect
    client.on_message = on_message
    if cfg["mqtt_user"]:
        client.username_pw_set(cfg["mqtt_user"], cfg["mqtt_pass"])
    if cfg["mqtt_tls"]:
        client.tls_set()                    # system CA bundle, for cloud brokers
    # auto-reconnect backoff
    client.reconnect_delay_set(min_delay=1, max_delay=30)

    stop = {"flag": False}

    def _shutdown(*_):
        LOG.info("shutting down")
        stop["flag"] = True
        try:
            client.disconnect()
        except Exception:
            pass

    signal.signal(signal.SIGTERM, _shutdown)
    signal.signal(signal.SIGINT, _shutdown)

    # Keep trying the first connect; loop_forever() handles reconnects after.
    while not stop["flag"]:
        try:
            client.connect(cfg["mqtt_host"], cfg["mqtt_port"], keepalive=60)
            break
        except OSError as exc:
            LOG.warning("broker not up yet (%s) — retrying in 5s", exc)
            time.sleep(5)

    if not stop["flag"]:
        client.loop_forever(retry_first_connection=True)

    mongo.close()
    LOG.info("stopped")
    return 0


if __name__ == "__main__":
    sys.exit(main())
