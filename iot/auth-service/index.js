// ReWinGo auth service — runs in the cloud (24/7), connected to HiveMQ + Mongo.
//
// This is the ONE shared backend for every kiosk and every phone. It does NOT
// need any machine's IP: it connects to HiveMQ Cloud (the broker) and to
// MongoDB Atlas (the database), and answers login/register/byPhone requests
// over MQTT. Run it on any always-on Node host (Render, Koyeb, Fly, a VM…).
//
// It is identical to the Pi's mqtt_auth.js, plus a tiny HTTP health endpoint
// that PaaS hosts ping to keep the instance alive. Configure via env vars
// (see .env.example) — NO secrets in this file.
require('dotenv').config();
const http = require('http');
const mqtt = require('mqtt');
const bcrypt = require('bcryptjs');
const jwt = require('jsonwebtoken');
const { MongoClient } = require('mongodb');

const JWT_SECRET = process.env.JWT_SECRET || 'rewingo-dev-secret-change-me';
const {
  MONGODB_URI, MONGODB_DB = 'rewingo',
  MQTT_HOST, MQTT_PORT = 8883, MQTT_USERNAME, MQTT_PASSWORD,
  PORT = 8080,
} = process.env;
const BROKER_URL = `mqtts://${MQTT_HOST}:${MQTT_PORT}`;

// ── Health endpoint (keeps the host awake + lets you verify it's up) ────────
http.createServer((req, res) => {
  res.writeHead(200, { 'Content-Type': 'text/plain' });
  res.end('rewingo-auth ok\n');
}).listen(PORT, () => console.log('[health] listening on', PORT));

// ── MongoDB Atlas ───────────────────────────────────────────────────────────
let db = null;
async function initMongo() {
  if (!MONGODB_URI) { console.error('[auth] no MONGODB_URI'); return; }
  const client = new MongoClient(MONGODB_URI, { serverSelectionTimeoutMS: 8000 });
  await client.connect();
  db = client.db(MONGODB_DB);
  console.log('[auth] mongo connected:', MONGODB_DB);
}

function publicUser(u) {
  return {
    id: String(u._id), _id: String(u._id),
    firstName: u.firstName || (u.name || '').split(' ')[0] || '',
    lastName:  u.lastName  || (u.name || '').split(' ').slice(1).join(' ') || '',
    age: u.age || 0, email: u.email || '', mobile: u.mobile || '',
    name: u.name || [u.firstName, u.lastName].filter(Boolean).join(' '),
    points: u.points || 0, role: u.role || 'user',
  };
}

async function doRegister(b) {
  const { firstName, lastName, age, email, password, mobile } = b;
  if (!email || !password) return { ok: false, error: 'email and password are required' };
  const existing = await db.collection('users').findOne({ email: email.toLowerCase() });
  if (existing) return { ok: false, error: 'email already registered' };
  const passwordHash = await bcrypt.hash(password, 10);
  const doc = {
    firstName: firstName || '', lastName: lastName || '', age: age || 0,
    name: [firstName, lastName].filter(Boolean).join(' '),
    email: email.toLowerCase(), mobile: mobile || null, passwordHash,
    role: 'user', points: 0, createdAt: new Date(),
  };
  const r = await db.collection('users').insertOne(doc);
  doc._id = r.insertedId;
  const token = jwt.sign({ uid: String(doc._id) }, JWT_SECRET, { expiresIn: '30d' });
  return { ok: true, token, user: publicUser(doc) };
}

async function doLogin(b) {
  const { email, password } = b;
  if (!email || !password) return { ok: false, error: 'email and password are required' };
  const u = await db.collection('users').findOne({ email: email.toLowerCase() });
  if (!u || !u.passwordHash || !(await bcrypt.compare(password, u.passwordHash)))
    return { ok: false, error: 'invalid email or password' };
  const token = jwt.sign({ uid: String(u._id) }, JWT_SECRET, { expiresIn: '30d' });
  return { ok: true, token, user: publicUser(u) };
}

async function doByPhone(b) {
  const phone = String(b.phone || b.mobile || '').trim();
  if (!phone) return { ok: false, error: 'no phone' };
  const u = await db.collection('users').findOne({ mobile: phone });
  if (!u) return { ok: false, error: 'no account with that phone' };
  return { ok: true, user: publicUser(u) };
}

// ── HiveMQ (the broker every kiosk + phone connects to) ─────────────────────
const mq = mqtt.connect(BROKER_URL,
    { username: MQTT_USERNAME, password: MQTT_PASSWORD, reconnectPeriod: 3000 });
mq.on('connect', () => {
  console.log('[auth] connected to HiveMQ', MQTT_HOST);
  mq.subscribe(['rewingo/auth/req', 'rewingo/user/req'],
               e => e && console.error('[auth] subscribe error', e.message));
});
mq.on('error', e => console.error('[auth] mqtt error', e.message));
mq.on('message', async (topic, buf) => {
  let b = {}; try { b = JSON.parse(buf.toString() || '{}'); } catch {}
  const reqId = b.reqId || b.requestId;
  if (!reqId) return;
  let out, resTopic;
  try {
    if (topic === 'rewingo/auth/req') {
      resTopic = 'rewingo/auth/res/' + reqId;
      if (!db) out = { ok: false, error: 'database unavailable' };
      else if (b.action === 'register') out = await doRegister(b);
      else if (b.action === 'login')    out = await doLogin(b);
      else out = { ok: false, error: 'unknown action' };
    } else if (topic === 'rewingo/user/req') {
      resTopic = 'rewingo/user/res/' + reqId;
      if (!db) out = { ok: false, error: 'database unavailable' };
      else if (b.action === 'byPhone') out = await doByPhone(b);
      else out = { ok: false, error: 'unknown action' };
    } else return;
  } catch (e) { out = { ok: false, error: e.message }; }
  mq.publish(resTopic, JSON.stringify(out), { qos: 1 });
  console.log('[auth]', topic, b.action, '->', out.ok ? 'ok' : out.error);
});

initMongo();
