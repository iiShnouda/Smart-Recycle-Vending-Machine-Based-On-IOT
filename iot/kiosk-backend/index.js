// ReWinGo LCD/kiosk IoT backend.
//
// This is the kiosk's OWN backend — separate from the mobile-app backend.
// It does two jobs:
//   1. Subscribes to every kiosk's telemetry on HiveMQ and writes it to
//      MongoDB Atlas  (rewingo/<machineId>/{tx,status,fault,inventory}).
//   2. Relays a QR sign-in: when the phone app scans a kiosk's fixed QR
//      ("REWINGO:<machineId>"), it POSTs the user here and we publish them
//      to rewingo/<machineId>/login — the kiosk is subscribed and logs in.
//
// Run on Windows for dev:  npm install && npm start
require('dotenv').config();
// NOTE: use a STANDARD (non-SRV) MONGODB_URI. Some kiosk networks (phone
// hotspots) block the SRV/TXT DNS lookups that mongodb+srv:// requires; the
// standard seed-list URI resolves the shard hosts via plain A-records, which
// work. See iot/kiosk-backend/.env.example.
const mqtt    = require('mqtt');
const express = require('express');
const { MongoClient } = require('mongodb');

const {
  MQTT_HOST,
  MQTT_PORT = 8883,
  MQTT_USERNAME,
  MQTT_PASSWORD,
  MONGODB_URI,
  MONGODB_DB = 'rewingo',
  PORT = 3001,
} = process.env;

// ── MongoDB Atlas ─────────────────────────────────────────────────────────
let db = null;
async function initMongo() {
  if (!MONGODB_URI) {
    console.warn('[mongo] MONGODB_URI not set — running WITHOUT a database (MQTT still works).');
    return;
  }
  try {
    const client = new MongoClient(MONGODB_URI, { serverSelectionTimeoutMS: 8000 });
    await client.connect();
    await client.db(MONGODB_DB).command({ ping: 1 });
    db = client.db(MONGODB_DB);
    console.log('[mongo] connected to db:', MONGODB_DB);
  } catch (e) {
    console.error('[mongo] connect failed (continuing without DB):', e.message);
    db = null;
  }
}

// ── HiveMQ (MQTT over TLS) ────────────────────────────────────────────────
const mq = mqtt.connect(`mqtts://${MQTT_HOST}:${MQTT_PORT}`, {
  username: MQTT_USERNAME,
  password: MQTT_PASSWORD,
  reconnectPeriod: 3000,
});

mq.on('connect', () => {
  console.log('[mqtt] connected to HiveMQ:', MQTT_HOST);
  mq.subscribe([
    'rewingo/+/tx',
    'rewingo/+/status',
    'rewingo/+/fault',
    'rewingo/+/inventory',
  ], err => err && console.error('[mqtt] subscribe error', err.message));
});
mq.on('error',     e => console.error('[mqtt] error:', e.message));
mq.on('reconnect', () => console.log('[mqtt] reconnecting…'));

mq.on('message', async (topic, buf) => {
  // topic = rewingo/<machineId>/<kind>
  const [, machineId, kind] = topic.split('/');
  let body = {};
  try { body = JSON.parse(buf.toString() || '{}'); }
  catch { body = { raw: buf.toString() }; }

  console.log(`[mqtt] ${machineId}/${kind}`, body);
  if (!db) return;                         // no DB configured yet — just log

  const ts = new Date();
  try {
    if (kind === 'tx')
      await db.collection('transactions').insertOne({ machineId, ...body, ts });
    else if (kind === 'fault')
      await db.collection('dispense_faults').insertOne({ machineId, ...body, ts });
    else if (kind === 'inventory')
      await db.collection('inventory').insertOne({ machineId, ...body, ts });
    else if (kind === 'status')
      await db.collection('kiosks').updateOne(
        { machineId }, { $set: { machineId, status: body, ts } }, { upsert: true });
  } catch (e) {
    console.error('[mongo] write failed:', e.message);
  }
});

// Publish a linked user to a kiosk's /login topic (the QR sign-in result).
function sendUserToMachine(machineId, user) {
  mq.publish(`rewingo/${machineId}/login`, JSON.stringify({ user }), { qos: 1 });
  console.log('[link] sent user to', machineId, user);
}

// ── HTTP API (the phone app calls this after scanning a kiosk QR) ─────────
const app = express();
app.use(express.json());

app.get('/health', (_req, res) =>
  res.json({ ok: true, mqtt: mq.connected, mongo: !!db }));

// POST /link  { machineId, user: { id, name, points } }
// machineId comes from the scanned "REWINGO:<machineId>" code.
app.post('/link', (req, res) => {
  const { machineId, user } = req.body || {};
  if (!machineId || !user)
    return res.status(400).json({ error: 'machineId and user are required' });
  sendUserToMachine(machineId, user);
  res.json({ ok: true });
});

// Start the HTTP server immediately so /health + /link work even before (or
// without) Mongo. Mongo connects in the background and is resilient to failure.
app.listen(PORT, () => console.log('[http] kiosk-backend listening on :' + PORT));
initMongo();
