# rewingo-bridge — MQTT → MongoDB

The kiosk publishes every database write to a local **Mosquitto** broker. This
small Python service subscribes to the broker and stores each message in
**MongoDB** (Atlas or self-hosted). It's the second half of the IoT pipe:

```
  kiosk (Qt app) ──publish──▶  Mosquitto  ──rewingo-bridge──▶  MongoDB Atlas
      (already done)            (on the Pi)     (this folder)
```

The Qt app **already publishes** — see `Database::setMqttClient()`. Nothing was
landing in Mongo because there was no subscriber after the old Flask backend was
removed. This bridge is that subscriber.

## What gets stored

| MQTT topic (`rewingo/<id>/…`) | Mongo collection | Write |
|---|---|---|
| `transactions` | `transactions` | insert (append) |
| `dispense_faults` | `dispense_faults` | insert (append) |
| `inventory` | `inventory_events` | insert (append) |
| `products` | `products` | upsert on `kiosk_id`+`slot` |
| `product_catalog` | `product_catalog` | upsert on `id` |
| `status` (LWT) | `kiosks` | online/offline + `last_seen` |
| `heartbeat` | `kiosks` | uptime/version + `last_seen` |
| anything else | `events` | insert (nothing is lost) |

Every document is tagged with `kiosk_id`, the source `_topic`, and a server
`_received_at` timestamp. One bridge handles any number of kiosks.

## Deploy on the Pi

```bash
# from the repo checkout on the Pi
sudo bash iot/install.sh
```

That creates a venv in `/opt/rewingo-bridge`, installs deps, drops a systemd
unit, and starts it. Then edit the config and restart:

```bash
sudo nano /etc/rewingo/.env          # set MONGODB_URI (and MQTT_HOST=127.0.0.1)
sudo systemctl restart rewingo-bridge
journalctl -u rewingo-bridge -f      # watch it relay messages
```

### Two settings that matter

1. **`MONGODB_URI`** — from Atlas: *Cluster → Connect → Drivers*. Put your DB
   password in the string. Required; the bridge refuses to start without it.
2. **`MQTT_HOST=127.0.0.1`** in `/etc/rewingo/.env` — the *kiosk* reads the same
   file. If `MQTT_HOST` is empty the app runs **offline** and publishes nothing,
   so the bridge would sit idle. Point the kiosk at the local broker
   (`MQTT_HOST=127.0.0.1`, `MQTT_PORT=1883`, `MQTT_TLS=0`).

## Local broker (Mosquitto) quick check

```bash
sudo apt install -y mosquitto mosquitto-clients
sudo systemctl enable --now mosquitto
# watch everything the kiosk emits:
mosquitto_sub -h 127.0.0.1 -t 'rewingo/#' -v
```

## Run it by hand (debugging)

```bash
cd iot
python3 -m venv venv && ./venv/bin/pip install -r requirements.txt
MONGODB_URI='mongodb+srv://…' MQTT_HOST=127.0.0.1 LOG_LEVEL=DEBUG \
    ./venv/bin/python bridge.py
```

## Verify data is landing

In `mongosh` or Atlas → *Collections*:

```js
use rewingo
db.kiosks.find()                       // one row per kiosk, status + last_seen
db.transactions.find().sort({_received_at:-1}).limit(5)
```

## Config reference

All keys live in `/etc/rewingo/.env` (real env vars override). See
[`.env.example`](.env.example) for the full list: `MQTT_HOST/PORT/USERNAME/
PASSWORD/TLS/TOPIC`, `MONGODB_URI`, `MONGODB_DB`, `LOG_LEVEL`.
