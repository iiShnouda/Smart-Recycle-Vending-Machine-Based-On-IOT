// MongoDB schema bootstrap for the ReWinGo cloud DB.
//
// Ensures indexes and seeds the singleton `config` doc. The point value in EGP
// (`pointValueEGP`) is DEVELOPER-only: we seed it on first create and NEVER
// overwrite it here. Admins can read it (GET /config) but not change it; admins
// change *points* (product prices, recycle rewards), not the EGP value.
//
// See DATABASE_SCHEMA.md (repo root) for the full schema.

const DEFAULT_POINT_VALUE_EGP = 0.05; // 1 point = 0.05 EGP — change here (dev only)
const DEFAULT_RECYCLE_REWARDS = { bottle: 10, can: 10 };

async function initSchema(db) {
  await db.collection('users').createIndex({ mobile: 1 }, { unique: true, sparse: true });
  await db.collection('users').createIndex({ email: 1 }, { sparse: true });
  await db.collection('users').createIndex({ appUserId: 1 }, { sparse: true });

  await db.collection('product_catalog').createIndex({ name: 1 });
  await db.collection('product_catalog').createIndex({ barcode: 1 }, { sparse: true });

  await db.collection('products').createIndex({ machineId: 1, slot: 1 }, { unique: true });

  await db.collection('transactions').createIndex({ machineId: 1, ts: -1 });
  await db.collection('transactions').createIndex({ userId: 1, ts: -1 });
  await db.collection('transactions').createIndex({ ts: -1 });

  await db.collection('pending_accounts').createIndex({ token: 1 }, { unique: true });

  // Seed config only if absent — never clobber a dev-set pointValueEGP.
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

  console.log('[schema] indexes ensured + config seeded');
}

async function getConfig(db) {
  return (await db.collection('config').findOne({ _id: 'global' })) || {
    pointValueEGP: DEFAULT_POINT_VALUE_EGP, currency: 'EGP',
    recycleRewards: DEFAULT_RECYCLE_REWARDS,
  };
}

module.exports = { initSchema, getConfig, DEFAULT_POINT_VALUE_EGP };
