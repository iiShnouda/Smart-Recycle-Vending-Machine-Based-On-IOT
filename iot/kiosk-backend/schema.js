// MongoDB schema bootstrap for the ReWinGo cloud DB.
//
// Ensures indexes and seeds the singleton `config` doc. The point value in EGP
// (`pointValueEGP`) is DEVELOPER-only: we seed it on first create and NEVER
// overwrite it here. Admins can read it (GET /config) but not change it; admins
// change *points* (product prices, recycle rewards), not the EGP value.
//
// See DATABASE_SCHEMA.md (repo root) for the full schema.

const DEFAULT_POINT_VALUE_EGP = 0.4;  // 1 point = 0.4 EGP — change here (dev only)
// Recycle rewards (points per accepted item). Bottles split by size at the
// kiosk via the camera: small bottle = 1, large bottle = 2, can = 2.
const DEFAULT_RECYCLE_REWARDS = { smallBottle: 1, largeBottle: 2, can: 2 };

async function initSchema(db) {
  // One conflicting/pre-existing index must NOT abort the whole init (which
  // would null the DB connection). Each createIndex is best-effort.
  const idx = async (coll, keys, opts) => {
    try { await db.collection(coll).createIndex(keys, opts || {}); }
    catch (e) { console.warn(`[schema] skip index ${coll} ${JSON.stringify(keys)}: ${e.message}`); }
  };

  await idx('users', { mobile: 1 }, { unique: true, sparse: true });
  await idx('users', { email: 1 }, { sparse: true });
  await idx('users', { appUserId: 1 }, { sparse: true });
  await idx('product_catalog', { name: 1 });
  await idx('product_catalog', { barcode: 1 }, { sparse: true });
  await idx('products', { machineId: 1, slot: 1 }, { unique: true });
  await idx('transactions', { machineId: 1, ts: -1 });
  await idx('transactions', { userId: 1, ts: -1 });
  await idx('transactions', { ts: -1 });
  await idx('pending_accounts', { token: 1 }, { unique: true });

  // Seed config only if absent — never clobber a dev-set pointValueEGP.
  try {
    await db.collection('config').updateOne(
      { _id: 'global' },
      {
        $setOnInsert: {
          _id: 'global',
          pointValueEGP: DEFAULT_POINT_VALUE_EGP,
          currency: 'EGP',
          recycleRewards: DEFAULT_RECYCLE_REWARDS,
          updatedBy: 'seed',
          updatedAt: new Date(),
        },
      },
      { upsert: true });
  } catch (e) {
    console.warn('[schema] config seed:', e.message);
  }

  console.log('[schema] indexes ensured + config seeded');
}

async function getConfig(db) {
  return (await db.collection('config').findOne({ _id: 'global' })) || {
    pointValueEGP: DEFAULT_POINT_VALUE_EGP, currency: 'EGP',
    recycleRewards: DEFAULT_RECYCLE_REWARDS,
  };
}

module.exports = { initSchema, getConfig, DEFAULT_POINT_VALUE_EGP };
