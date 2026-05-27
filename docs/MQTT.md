# Setting up the MQTT pipe

The kiosk publishes every interesting event over **MQTT** — a lightweight
pub/sub protocol designed for IoT. No Flask backend, no MongoDB driver
baked into the binary. Just one TCP connection from the Pi to an MQTT
broker; everything else listens in.

```
   Kiosk (rewingo on Pi)
        │ TLS / TCP   ← MQTT_HOST, MQTT_USERNAME/PASSWORD in /etc/rewingo/.env
        ▼
   MQTT broker (HiveMQ Cloud, EMQX, Mosquitto, …)
        ▲
        │ subscribe to rewingo/<kiosk-id>/#
        │
   Dashboard / phone / Node-RED / your laptop
```

Pick **one** of three broker options below, fill in the env, restart the
kiosk.

---

## Option A — HiveMQ Cloud (free tier, easiest)

1. <https://www.hivemq.com/mqtt-cloud-broker/> → **Try Free**.
2. Sign up, create a cluster (the free serverless one — 100 connections,
   10 GB/month is generous for a kiosk).
3. Cluster page → **Access Management** → **+ Add User**.
   - Pick a username, autogen a password, **save it**.
   - Permissions: at minimum the user needs publish + subscribe rights on
     `rewingo/#`. For dev, just give them full access.
4. Cluster page → **Overview** → copy the **Cluster URL** (looks like
   `abc12345.s2.eu.hivemq.cloud`) and the port (always **8883** for TLS).

Drop these into `/etc/rewingo/.env` on the Pi:

```
MQTT_HOST=abc12345.s2.eu.hivemq.cloud
MQTT_PORT=8883
MQTT_USERNAME=your-username
MQTT_PASSWORD=your-password
MQTT_TLS=1
```

## Option B — EMQX Cloud Serverless (also free)

1. <https://www.emqx.com/en/cloud> → Sign up → Create a **Serverless**
   deployment.
2. Once green, copy the **Connection Information**: address, port (8883),
   and create an auth user under **Authentication & ACL**.
3. Put the same four values into `/etc/rewingo/.env`. Same shape as
   HiveMQ — only the hostname differs.

## Option C — Self-hosted Mosquitto on the Pi itself

For air-gapped / on-premises setups, install Mosquitto on the same Pi:

```bash
sudo apt install mosquitto mosquitto-clients
sudo systemctl enable --now mosquitto
```

Then `/etc/rewingo/.env`:

```
MQTT_HOST=127.0.0.1
MQTT_PORT=1883
MQTT_USERNAME=
MQTT_PASSWORD=
MQTT_TLS=0
```

By default Mosquitto allows anonymous local connections — fine for a
kiosk on its own LAN. To lock it down:

```bash
sudo nano /etc/mosquitto/mosquitto.conf
# add:
#   listener 1883
#   password_file /etc/mosquitto/passwd
#   allow_anonymous false
sudo mosquitto_passwd -c /etc/mosquitto/passwd kiosk
sudo systemctl restart mosquitto
```

Then put `kiosk` + your password in the env.

---

## Restart the kiosk so it picks up the new env

```bash
ssh rewingo@<pi-ip>
sudo nano /etc/rewingo/.env       # paste your values
# Restart the kiosk — it reads /etc/rewingo/.env once on startup.
pkill -x rewingo || true
rewingo &
```

You should see `[Mqtt] Connecting to … (tls=1) as <kiosk-id>` then
`[Mqtt] Connected to broker` in the logs (and the kiosk publishes
`online` retained on `rewingo/<kiosk-id>/status`).

## Watch the events live

From any machine with `mosquitto-clients` installed (laptop, phone via
Termux, another Pi):

```bash
# Replace with your real broker + creds:
mosquitto_sub -h abc12345.s2.eu.hivemq.cloud -p 8883 \
              -u your-username -P your-password \
              -t 'rewingo/#' -v
```

Output looks like:

```
rewingo/95e76447-…/status        online
rewingo/95e76447-…/dispense_faults  {"slot":3,"reason":"STALL",…}
rewingo/95e76447-…/restock_events   {"slot":1,"delta":+20,…}
rewingo/95e76447-…/products         {"slot":3,"active":false,…}
rewingo/95e76447-…/transactions     {"kind":"vending","slot":2,…}
```

The `<kiosk-id>` is the UUID we generated at first boot — `cat
/var/lib/rewingo/…` or look in Settings → kiosk/id.

## Topic layout the kiosk uses

| Topic suffix | Payload | When |
|---|---|---|
| `status` | `online` / `offline` | On connect (retained) + on LWT |
| `transactions` | JSON: kind, slot, amount, user_id, ts | Every recycle / vending |
| `dispense_faults` | JSON: slot, reason, weight_before, … | Every dispense fault |
| `restock_events` | JSON: slot, prev_count, new_count, delta, source | Every inventory delta |
| `products` | JSON: slot, active flag | Whenever a product is enabled/disabled |
| `product_catalog` | JSON: id, name, image_url, … | When admin adds a SKU |
| `cmd` *(subscribe)* | (server → kiosk) | Future: remote restart, show msg |

## Quick sanity check from the Pi

```bash
# Verify the kiosk is publishing
mosquitto_sub -h 127.0.0.1 -t 'rewingo/#' -v   # if running local Mosquitto

# Or hit your cloud broker:
mosquitto_sub -h $MQTT_HOST -p $MQTT_PORT \
              -u $MQTT_USERNAME -P $MQTT_PASSWORD \
              -t 'rewingo/#' -v
```

If you see `rewingo/<kiosk-id>/status online` the chain works.

## Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| `Connect failed (rc=5: Connection Refused: not authorised)` | Wrong username/password | Re-check `/etc/rewingo/.env` and the broker's user page |
| `Connect failed (rc=4: Connection Refused: bad user/pass)` | Same as above | Same |
| `mqtt: server certificate verify failed` | Broker uses a cert your Pi doesn't trust | Default `/etc/ssl/certs/ca-certificates.crt` covers every public CA; for private brokers, drop your CA in `mosquitto_tls_set` |
| `Could not contact server` | Wrong hostname or firewall blocks 8883 | `nc -zv $MQTT_HOST 8883` from the Pi |
| Connects then disconnects every 60 s | `MQTT_USERNAME` set but the broker has a Connection Limit at 1 and an old client is still holding the slot | Make sure no other kiosk shares the same `kiosk-id` |
| `MQTT_HOST not set` warning at boot | `.env` is empty or has typos | The kiosk runs offline-only — fix `.env` and restart |

## Why MQTT instead of HTTP + MongoDB

What we had before: kiosk → HTTP → local Flask → pymongo → MongoDB Atlas.
That's three processes and a Python venv that we have to keep alive.

MQTT collapses it: kiosk → broker. One TCP socket. Built-in last-will
("offline" published automatically if the kiosk crashes), retained
messages (new subscribers see the latest state on connect), QoS levels,
TLS. And you can subscribe from anything — a Node-RED dashboard, a
Grafana plugin, a Home Assistant integration — without writing
backend code.

If you ever want persistence in a database, run a single subscriber on
your server that listens to `rewingo/#` and writes to whatever store
you like. The kiosk doesn't care.
