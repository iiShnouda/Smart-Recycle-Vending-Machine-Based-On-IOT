# Deploy the unified backend + iPhone-hotspot demo guide

This backend now does **everything** on one MongoDB Atlas DB: the kiosk MQTT→Mongo
bridge, the QR-login relay (`/link`, `/claim`), the point-value config (`/config`),
and the **mobile-app auth** (`/api/auth/register|login|me`). One account works on
both the app and the kiosk.

## 1. Deploy on Railway (do this once)
1. Railway → **New Project → Deploy from GitHub repo** → set **Root Directory** to
   `iot/kiosk-backend`. (It has `package.json` + `Procfile`; `npm start` runs it.)
2. Add **Variables** (Settings → Variables): `MQTT_HOST`, `MQTT_PORT`,
   `MQTT_USERNAME`, `MQTT_PASSWORD`, `MONGODB_URI` (the `mongodb+srv://…/rewingo`
   form is fine on Railway), `MONGODB_DB=rewingo`, `JWT_SECRET=<long random>`.
   **Do NOT set `PORT`** — Railway injects it.
3. Deploy → copy the public URL (e.g. `https://rewingo-xxxx.up.railway.app`).
4. Verify: open `<url>/health` → `{"ok":true,"mqtt":true,"mongo":true}`.
5. Point the **mobile app** at it: `lib/core/config/backend_config.dart` →
   `baseUrl = '<your Railway URL>'`. Rebuild the APK.

## 2. Why this survives the iPhone hotspot
The thing that breaks on a phone hotspot is the **SRV/TXT DNS** lookup that
`mongodb+srv://` needs. With the backend on Railway, **the Pi never does that
lookup** — the Atlas connection happens from Railway (full DNS). Over the hotspot
the Pi (and the phone) only make **plain A-record** connections, which work:

| Device | On hotspot it talks to | DNS type | Works? |
|---|---|---|---|
| Pi (kiosk) | HiveMQ MQTT (telemetry, QR login) | A | ✅ |
| Pi (kiosk) | GitHub (updates), api.qrserver (QR png) | A | ✅ |
| Phone (app) | Railway backend, HiveMQ, Supabase | A | ✅ |
| Railway | Atlas (`+srv`), HiveMQ | (on Railway) | ✅ |

Nothing on the hotspot does an SRV lookup → nothing breaks. The Pi gets a
private `172.20.10.x` IP, but every connection is **outbound**, so the NAT and
the changing IP don't matter (the backend is a fixed Railway URL).

## 3. Demo-day checklist
- [ ] Railway backend deployed + `/health` green (do this on any network first).
- [ ] **Stop the Pi's local backend** so there's only ONE bridge (otherwise both
      write to Atlas and you get duplicate transactions):
      `systemctl --user disable --now rewingo-kiosk-backend`
- [ ] APK rebuilt pointing at the Railway URL; log into the app once (so the
      token is cached).
- [ ] Connect the Pi to the iPhone hotspot; confirm the kiosk's MQTT connects
      (Admin → it should come online) — the kiosk log shows `Mqtt: Connected`.
- [ ] Test the loop: app QR scan → kiosk MainPage; recycle/vend → points update
      in the app (same Atlas user).
