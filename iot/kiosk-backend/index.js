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
const { initSchema, getConfig, DEFAULT_POINT_VALUE_EGP } = require('./schema');

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
    await initSchema(db);   // ensure indexes + seed config (pointValueEGP)
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
    'rewingo/+/register',     // a kiosk created a pending account to be claimed
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
    if (kind === 'tx') {
      // Freeze the EGP value at tx time and roll the totals into the user +
      // machine documents (per-machine; grand total = transactions w/o filter).
      const cfg = await getConfig(db);
      const pv  = cfg.pointValueEGP || DEFAULT_POINT_VALUE_EGP;
      const userId  = body.userId || body.user_id || null;
      const points  = Number(body.points != null ? body.points : (body.amount || 0));
      const details = body.details || body.meta || {};
      const tx = {
        machineId, userId, ts,
        type:   body.type || (points >= 0 ? 'earn' : 'spend'),
        source: body.source || body.kind || 'adjust',
        points,
        egpValue: +(points * pv).toFixed(4),
        details,
      };
      await db.collection('transactions').insertOne(tx);

      if (userId) {
        const inc = { points };
        if (points >= 0) inc['stats.pointsEarned'] = points;
        else             inc['stats.pointsSpent']  = -points;
        if (tx.source === 'recycle') {
          inc['recycle.bottles'] = details.bottles || 0;
          inc['recycle.cans']    = details.cans || 0;
          inc['recycle.total']   = (details.bottles || 0) + (details.cans || 0);
        }
        if (tx.source === 'vending') inc['stats.vendingSpent'] = -points;
        await db.collection('users').updateOne(
          { _id: userId },
          { $inc: inc, $set: { lastSeen: ts },
            $setOnInsert: { _id: userId, role: 'user', createdAt: ts } },
          { upsert: true });
      }
      await db.collection('machines').updateOne(
        { _id: machineId },
        { $inc: { 'stats.totalTransactions': 1,
                  'stats.pointsIssued': points > 0 ?  points : 0,
                  'stats.pointsSpent':  points < 0 ? -points : 0,
                  'stats.bottles': details.bottles || 0,
                  'stats.cans':    details.cans    || 0 },
          $set: { lastSeen: ts }, $setOnInsert: { _id: machineId, createdAt: ts } },
        { upsert: true });
    }
    else if (kind === 'fault')
      await db.collection('dispense_faults').insertOne({ machineId, ...body, ts });
    else if (kind === 'inventory')
      await db.collection('inventory').insertOne({ machineId, ...body, ts });
    else if (kind === 'status') {
      const online = body.raw ? body.raw === 'online' : body.online !== false;
      await db.collection('machines').updateOne(
        { _id: machineId },
        { $set: { status: online ? 'online' : 'offline', statusRaw: body, lastSeen: ts },
          $setOnInsert: { _id: machineId, createdAt: ts } },
        { upsert: true });
    }
    else if (kind === 'register' && body.token)
      // A user registered their face at the kiosk and wants to connect the
      // phone app. Stash a pending account keyed by the one-time token; the
      // app claims it by scanning "REWINGO-CLAIM:<token>" (see POST /claim).
      await db.collection('pending_accounts').updateOne(
        { token: body.token },
        { $set: { token: body.token, name: body.name || '', phone: body.phone || '',
                  machineId, points: 0, claimed: false, ts } },
        { upsert: true });
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

// GET /config — admins (and the kiosk) READ the point value in EGP. Read-only:
// pointValueEGP is changed only by the developer (in schema.js / Mongo directly).
app.get('/config', async (_req, res) => {
  if (!db) return res.json({ pointValueEGP: DEFAULT_POINT_VALUE_EGP, currency: 'EGP' });
  try { res.json(await getConfig(db)); }
  catch (e) { res.status(500).json({ error: e.message }); }
});

// GET /machines — the registry of all machines + a grand total across them.
app.get('/machines', async (_req, res) => {
  if (!db) return res.status(503).json({ error: 'database unavailable' });
  try {
    const machines = await db.collection('machines').find({}).toArray();
    const total = machines.reduce((a, m) => {
      const s = m.stats || {};
      a.transactions += s.totalTransactions || 0;
      a.pointsIssued += s.pointsIssued || 0;
      a.pointsSpent  += s.pointsSpent  || 0;
      a.bottles      += s.bottles || 0;
      a.cans         += s.cans || 0;
      return a;
    }, { transactions: 0, pointsIssued: 0, pointsSpent: 0, bottles: 0, cans: 0 });
    res.json({ machines, total });
  } catch (e) { res.status(500).json({ error: e.message }); }
});

// POST /link  { machineId, user: { id, name, points } }
// machineId comes from the scanned "REWINGO:<machineId>" code.
app.post('/link', (req, res) => {
  const { machineId, user } = req.body || {};
  if (!machineId || !user)
    return res.status(400).json({ error: 'machineId and user are required' });
  sendUserToMachine(machineId, user);
  res.json({ ok: true });
});

// POST /claim  { token, appUser: { id, name?, email? } }
// The phone app scanned "REWINGO-CLAIM:<token>" and claims the pending account
// created at the kiosk, AUTO-LINKING it to the already-logged-in app user — no
// password needed. Returns the linked account so the app can show it.
app.post('/claim', async (req, res) => {
  const { token, appUser } = req.body || {};
  if (!token || !appUser || !appUser.id)
    return res.status(400).json({ error: 'token and appUser.id are required' });
  if (!db) return res.status(503).json({ error: 'database unavailable' });
  try {
    const pending = await db.collection('pending_accounts').findOne({ token });
    if (!pending)
      return res.status(404).json({ error: 'invalid or expired claim code' });
    if (pending.claimed)
      return res.status(409).json({ error: 'this code was already used' });

    const account = {
      appUserId:     appUser.id,
      name:          pending.name,
      phone:         pending.phone,
      points:        pending.points || 0,
      faceMachineId: pending.machineId,
      email:         appUser.email || null,
      claimedAt:     new Date(),
    };
    await db.collection('accounts').updateOne(
      { appUserId: appUser.id }, { $set: account }, { upsert: true });
    await db.collection('pending_accounts').updateOne(
      { token }, { $set: { claimed: true, claimedBy: appUser.id, claimedAt: new Date() } });

    console.log('[claim] linked', pending.phone, '→ app user', appUser.id);
    res.json({ ok: true, account });
  } catch (e) {
    console.error('[claim] failed:', e.message);
    res.status(500).json({ error: 'claim failed' });
  }
});

// Start the HTTP server immediately so /health + /link work even before (or
// without) Mongo. Mongo connects in the background and is resilient to failure.
app.listen(PORT, () => console.log('[http] kiosk-backend listening on :' + PORT));
initMongo();
