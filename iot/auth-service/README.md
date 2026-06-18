# ReWinGo auth service (24/7 cloud backend)

The ONE shared backend for every kiosk and the phone app. It connects to
**HiveMQ Cloud** (broker) and **MongoDB Atlas** (database) and answers
login / register / byPhone over MQTT. No machine IP is involved.

Because everything meets at HiveMQ, moving the backend from the Pi to the
cloud needs **no app or kiosk change** — just run this in the cloud and stop
the Pi's copy.

## What it needs (env vars — see `.env.example`)
`MONGODB_URI`, `MONGODB_DB`, `JWT_SECRET` (same secret as the Pi),
`MQTT_HOST`, `MQTT_PORT`, `MQTT_USERNAME`, `MQTT_PASSWORD`.

## Deploy options (free, 24/7)

### A) Render (easiest)
1. Push this repo to GitHub (already there).
2. render.com → New → **Web Service** → connect the repo → root dir `iot/auth-service`.
3. Build: `npm install` · Start: `npm start`.
4. Add the env vars above (Environment tab).
5. Deploy. Note the URL it gives you, e.g. `https://rewingo-auth.onrender.com`.
6. Render's free web service sleeps after ~15 min idle — keep it awake with a
   free **UptimeRobot** HTTP monitor pinging that URL every 5 minutes.

### B) A free always-on VM (most reliable — Oracle Cloud Always Free)
1. Create an Oracle Cloud "Always Free" ARM VM (Ubuntu).
2. `sudo apt install nodejs npm`, copy this folder up, `npm install`.
3. Put the env vars in `.env`, run as a systemd service (same as the Pi's
   `rewingo-auth-mqtt.service`). Always on, never sleeps.

(Koyeb / Fly.io / Northflank work the same way as Render.)

## After it's live
Turn OFF the Pi's copy so only ONE backend answers:
```
sudo systemctl disable --now rewingo-auth-mqtt
```
Test it (no app needed) — publish a login over HiveMQ and watch for the reply,
or just sign in from the phone. The phone keeps working with the Pi powered off.
