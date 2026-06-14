# ReWinGo — Database Schema (MongoDB, multi-machine)

This is the **cloud** database (MongoDB Atlas, database `rewingo`) that every
kiosk syncs to through `iot/kiosk-backend`. Each kiosk also keeps a **local
SQLite** cache (`src/database.cpp`) and a face-embedding DB (`faces.db`) so it
works offline; the cloud DB is the shared source of truth.

**Two scopes** (as required):
- **Global** collections are shared by all machines (users, product catalog,
  machines registry, config).
- **Per-machine** documents carry a `machineId` (products, transactions,
  inventory, faults). A "total across all machines" is just the same query
  without a `machineId` filter (or read `machines.stats`).

`machineId` = the kiosk's id (QSettings `kiosk/id`, also the MQTT topic base
`rewingo/<machineId>`).

---

## Global collections

### `users`  — one document per person (usable at any machine)
| field | type | notes |
|---|---|---|
| `_id` | string | kiosk-minted UUID (the local `users.id`) |
| `name` | string | |
| `email` | string \| null | set in the phone app after claim |
| `mobile` | string | unique; collected at registration |
| `passwordHash` | string \| null | **bcrypt** hash; set in the app, never the kiosk |
| `role` | string | `"user"` \| `"admin"` |
| `points` | int | current balance |
| `faceId` | int \| null | row id in the machine's `faces.db` / embedding ref |
| `recycle` | object | `{ bottles, cans, total }` lifetime counts |
| `stats` | object | `{ pointsEarned, pointsSpent, pointsSent, pointsReceived, vendingSpent }` |
| `appUserId` | string \| null | mobile-app user id once the account is claimed |
| `homeMachineId` | string | where they registered |
| `createdAt` / `updatedAt` / `lastSeen` | date | |
| `deleteAfter` | date \| null | consent / retention |

Indexes: `{ mobile: 1 }` unique, `{ email: 1 }` sparse, `{ appUserId: 1 }` sparse.

> Passwords are **never** handled on the kiosk. The kiosk stores name + mobile +
> face; the password/email are set in the phone app (claim flow, `POST /claim`).

### `product_catalog` — GLOBAL product category (master list, shared by all machines)
| field | type | notes |
|---|---|---|
| `_id` | string | UUID |
| `name` | string | |
| `imageUrl` | string | |
| `barcode` | string \| null | EAN/UPC |
| `brand` / `category` | string \| null | |
| `defaultWeightG` | number | typical unit weight (load-cell baseline) |
| `defaultPricePoints` | int | suggested price |
| `source` | string | `"manual"` \| `"lookup"` \| `"imported"` |
| `createdByMachine` | string | first kiosk to add it |
| `updatedAt` | date | |

Index: `{ name: 1 }`, `{ barcode: 1 }` sparse.

### `machines` — registry of ALL machines
| field | type | notes |
|---|---|---|
| `_id` | string | machineId |
| `name` / `location` | string | |
| `status` | string | `"online"` \| `"offline"` |
| `lastSeen` | date | from `status` heartbeat |
| `firmwareVersion` / `appVersion` | string | |
| `stats` | object | `{ totalTransactions, pointsIssued, pointsSpent, bottles, cans }` |
| `createdAt` | date | |

### `config` — singleton global config (`_id: "global"`)
| field | type | who can change |
|---|---|---|
| `pointValueEGP` | number | **DEV/DB only** (e.g. `0.05` → 1 pt = 0.05 EGP) |
| `currency` | string | `"EGP"` |
| `recycleRewards` | object | **admin-editable**: `{ bottle: pts, can: pts }` |
| `updatedBy` / `updatedAt` | | |

**Point-value rule (as specified):** `pointValueEGP` is set only by the software/DB
developer (seeded here / changed via a protected script). The **admin panel can
read it** (`GET /config`) to *show* "1 point = X EGP", but cannot change it.
Admins *can* change how many **points** things cost/earn — that's
`products.pricePoints` and `config.recycleRewards`, not the EGP value.

---

## Per-machine collections (carry `machineId`)

### `products` — a machine's slots (1..8)
| field | type | notes |
|---|---|---|
| `_id` | string | `"<machineId>:<slot>"` |
| `machineId` / `slot` | | |
| `catalogId` | string | → `product_catalog._id` |
| `name` / `imageUrl` | | denormalised for display |
| `pricePoints` | int | **admin-editable** |
| `unitWeightG` | number | one item's weight (load-cell accuracy) |
| `emptyShelfRaw` / `unitWeightRaw` | int | HX711 calibration (empty + per-unit raw) |
| `calibrated` | bool | |
| `count` | int | current inventory |
| `active` | bool | |
| `updatedAt` | date | |

Index: `{ machineId: 1, slot: 1 }` unique.

### `transactions` — every points movement (per machine; total = no filter)
| field | type | notes |
|---|---|---|
| `_id` | ObjectId | |
| `machineId` / `userId` | string | |
| `ts` | date | |
| `type` | string | `"earn"` \| `"spend"` \| `"send"` \| `"receive"` \| `"admin"` |
| `source` | string | `"recycle"` \| `"vending"` \| `"transfer"` \| `"adjust"` |
| `points` | int | signed delta (+earn / −spend) |
| `egpValue` | number | `points * pointValueEGP` at tx time (frozen) |
| `details` | object | recycle `{bottles,cans}` · vending `{slot,productId,productName,pricePoints}` · transfer `{counterpartUserId}` |

Indexes: `{ machineId: 1, ts: -1 }`, `{ userId: 1, ts: -1 }`, `{ ts: -1 }` (global total).

### `inventory` — restock / scan events (already synced)
`{ machineId, slot, delta, source, count, ts }`

### `dispense_faults` — failed vends (already synced)
`{ machineId, slot, reason, ts }`

### `pending_accounts` — the connect-the-app claim flow (0.12.0)
`{ token, name, phone, machineId, points, claimed, claimedBy, ts }`

---

## How totals work
- **Per machine:** filter any collection by `machineId`.
- **All machines (grand total):** same query without the filter, or read the
  rolled-up counters in `machines.stats` (updated on each transaction).

## Where each piece is written
| data | written by |
|---|---|
| users, points, recycle counts | kiosk → MQTT `rewingo/<id>/tx` → backend upserts `users` + inserts `transactions` |
| products / calibration | kiosk admin → MQTT `products` → backend upserts `products` (+ `product_catalog`) |
| machine status / heartbeat | kiosk → MQTT `status` → backend upserts `machines` |
| pending account / claim | kiosk → `register`; app → `POST /claim` |
| `config.pointValueEGP` | **developer** (seed in `schema.js` / protected script) |
