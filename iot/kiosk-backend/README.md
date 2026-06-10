# ReWinGo Kiosk Backend

The **LCD/kiosk** IoT backend — separate from the mobile-app backend. Bridges
**HiveMQ Cloud ↔ MongoDB Atlas** and relays the **QR sign-in**.

```
 Raspberry Pi (kiosk) ─┐                         ┌─ MongoDB Atlas
                       ├─ HiveMQ Cloud ─ this ───┤
 Mobile app ───────────┘   (MQTT/TLS)  backend   └─ HTTP /link (QR relay)
```

## What it does
1. Subscribes to `rewingo/+/tx`, `/status`, `/fault`, `/inventory` and writes
   each message to MongoDB (`transactions`, `kiosks`, `dispense_faults`,
   `inventory`).
2. `POST /link { machineId, user }` → publishes the user to
   `rewingo/<machineId>/login`. The kiosk is subscribed there and logs the
   user in. `machineId` is the value inside the scanned `REWINGO:<machineId>`.

> Supersedes the older Python `iot/bridge.py` for the MQTT→Mongo job. Pick one.

## Run (Windows / anywhere with Node 18+)
```bash
cd iot/kiosk-backend
copy .env.example .env      # then fill MONGODB_URI
npm install
npm start
```
Expected logs: `[mqtt] connected to HiveMQ` and `[mongo] connected`.
Check `GET http://localhost:3001/health`.

## QR sign-in — two ways for the phone app
- **Via this backend (recommended):** after scanning `REWINGO:<machineId>`,
  `POST /link` with `{ machineId, user:{ id, name, points } }`.
- **Direct:** the app publishes `{ user:{…} }` to `rewingo/<machineId>/login`
  over MQTT-WebSocket (port **8884**, path `/mqtt`). No backend hop.

## Deploy
Runs anywhere with outbound internet (Railway, a small VPS, or the Pi itself).
It only needs the HiveMQ creds + the Atlas URI in `.env`.
