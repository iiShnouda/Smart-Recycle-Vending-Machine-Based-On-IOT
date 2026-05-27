#include "../include/database.h"
#include "../include/logger.h"
#include "../include/mqtt_client.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QFile>
#include <QUuid>

// One QNetworkAccessManager shared across syncs.
static QNetworkAccessManager *netMgr()
{
    static QNetworkAccessManager m;
    return &m;
}

Database::Database(QObject *parent) : QObject(parent)
{
    connect(&m_syncTimer, &QTimer::timeout, this, &Database::syncTick);
    m_syncTimer.setInterval(30 * 1000); // try sync every 30 s
}

Database::~Database() { close(); }

bool Database::open(const QString &dbPath)
{
    m_db = QSqlDatabase::addDatabase("QSQLITE", "rewingo");
    m_db.setDatabaseName(dbPath);
    if (!m_db.open()) {
        Logger::error("DB", "Open failed", { {"error", m_db.lastError().text()},
                                              {"path",  dbPath} });
        return false;
    }
    exec("PRAGMA journal_mode=WAL");
    exec("PRAGMA foreign_keys=ON");
    createTables();
    Logger::info("DB", "Opened", { {"path", dbPath} });
    return true;
}

void Database::close()
{
    if (m_db.isOpen()) m_db.close();
    QSqlDatabase::removeDatabase("rewingo");
}

void Database::createTables()
{
    exec("CREATE TABLE IF NOT EXISTS users ("
         "  id TEXT PRIMARY KEY,"
         "  name TEXT,"
         "  points INTEGER DEFAULT 0,"
         "  consent_at TEXT,"
         "  last_seen TEXT,"
         "  face_embedding BLOB,"
         "  delete_after TEXT)");

    // For older DBs that already exist — add columns if missing (SQLite ignores
    // duplicates silently via try/catch on exec).
    exec("ALTER TABLE users ADD COLUMN face_embedding BLOB");
    exec("ALTER TABLE users ADD COLUMN delete_after TEXT");

    exec("CREATE TABLE IF NOT EXISTS products ("
         "  slot INTEGER PRIMARY KEY,"
         "  name TEXT,"
         "  price_points INTEGER DEFAULT 0,"
         "  image_path TEXT,"
         "  active INTEGER DEFAULT 1,"
         "  last_count INTEGER DEFAULT 0,"
         "  last_weight_g INTEGER DEFAULT 0,"
         "  empty_shelf_raw INTEGER DEFAULT 0,"  // HX711 reading with no product
         "  unit_weight_raw INTEGER DEFAULT 0)"); // raw counts per one item
    /* Migrate existing DBs that pre-date the calibration columns. */
    exec("ALTER TABLE products ADD COLUMN empty_shelf_raw INTEGER DEFAULT 0");
    exec("ALTER TABLE products ADD COLUMN unit_weight_raw INTEGER DEFAULT 0");

    exec("CREATE TABLE IF NOT EXISTS transactions ("
         "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
         "  ts TEXT NOT NULL,"
         "  kind TEXT NOT NULL,"            // 'recycle' | 'vending' | 'admin'
         "  user_id TEXT,"                  // pseudonymous — name in users table
         "  slot INTEGER,"
         "  amount INTEGER,"                // points earned (+) or spent (-)
         "  meta TEXT)");                   // JSON blob

    exec("CREATE INDEX IF NOT EXISTS idx_tx_ts   ON transactions(ts)");
    exec("CREATE INDEX IF NOT EXISTS idx_tx_user ON transactions(user_id)");

    exec("CREATE TABLE IF NOT EXISTS audit_log ("
         "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
         "  ts TEXT NOT NULL,"
         "  category TEXT NOT NULL,"
         "  msg TEXT NOT NULL,"
         "  meta TEXT,"
         "  synced INTEGER DEFAULT 0)");

    /* Shared product catalog. One row per *product type* (not per slot),
     * synced across kiosks via MongoDB. The slot's `products` row holds the
     * catalog_id so multiple slots can host the same SKU and any admin
     * configuring a slot just picks from this list.                       */
    exec("CREATE TABLE IF NOT EXISTS product_catalog ("
         "  id TEXT PRIMARY KEY,"            // UUID, generated locally
         "  name TEXT NOT NULL,"
         "  image_path TEXT,"                // local file path after cache
         "  image_url TEXT,"                 // original remote URL (if any)
         "  default_price_points INTEGER DEFAULT 0,"
         "  typical_weight_g INTEGER DEFAULT 0,"  // from OFF or admin entry
         "  barcode TEXT,"                   // EAN/UPC if known
         "  source TEXT DEFAULT 'manual',"   // 'manual' | 'lookup' | 'imported'
         "  created_kiosk TEXT,"             // which kiosk first added it
         "  updated_at TEXT)");
    exec("CREATE INDEX IF NOT EXISTS idx_catalog_name ON product_catalog(name)");

    /* Link slots → catalog. Nullable because legacy rows may not have one.  */
    exec("ALTER TABLE products ADD COLUMN catalog_id TEXT");

    /* Inventory change log — every observed count step, whether triggered
     * by a dispense, an admin restock (door-open → door-close cycle), or
     * an "appeared from nowhere" reading (off-power restock detected on
     * boot). Lets the admin see when each slot got refilled.            */
    exec("CREATE TABLE IF NOT EXISTS restock_events ("
         "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
         "  ts TEXT NOT NULL,"
         "  slot INTEGER NOT NULL,"
         "  prev_count INTEGER,"
         "  new_count INTEGER,"
         "  delta INTEGER,"
         "  source TEXT)");                    // 'dispense' | 'admin' | 'boot' | 'scan'
    exec("CREATE INDEX IF NOT EXISTS idx_re_slot ON restock_events(slot)");
    exec("CREATE INDEX IF NOT EXISTS idx_re_ts   ON restock_events(ts)");

    // Per-dispense fault history. One row per failed DISPENSE reply from
    // the STM32 — the admin Faults page lists these and offers re-enable.
    exec("CREATE TABLE IF NOT EXISTS dispense_faults ("
         "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
         "  ts TEXT NOT NULL,"
         "  slot INTEGER NOT NULL,"
         "  reason TEXT NOT NULL,"            // STALL | NO_DROP | STEP_LOSS | TIMEOUT | BUSY
         "  weight_before INTEGER,"
         "  weight_after INTEGER,"
         "  drop_g INTEGER,"
         "  index_count INTEGER)");
    exec("CREATE INDEX IF NOT EXISTS idx_df_slot ON dispense_faults(slot)");
    exec("CREATE INDEX IF NOT EXISTS idx_df_ts   ON dispense_faults(ts)");
}

bool Database::exec(const QString &sql, const QVariantList &binds)
{
    QSqlQuery q(m_db);
    q.prepare(sql);
    for (const auto &b : binds) q.addBindValue(b);
    if (!q.exec()) {
        Logger::error("DB", "Query failed", { {"sql", sql},
                                              {"err", q.lastError().text()} });
        return false;
    }
    return true;
}

bool Database::setProductActive(int slot, bool active)
{
    const bool ok = exec("UPDATE products SET active=? WHERE slot=?",
                         { active ? 1 : 0, slot });
    if (ok) {
        emit productsChanged();
        Logger::audit("Vending",
                      active ? "Product re-enabled" : "Product disabled",
                      { {"slot", slot}, {"active", active} });
        /* Push to Mongo with a partial update — same kiosk + slot key. */
        if (m_mqtt && m_mqtt->isConnected()) {
            QJsonObject set { {"active",     active},
                              {"kiosk_id",   m_kioskId},
                              {"slot",       slot},
                              {"updated_at", QDateTime::currentDateTimeUtc()
                                                .toString(Qt::ISODate)} };
            m_mqtt->publishJson("products", set);
        }
    }
    return ok;
}

bool Database::upsertProduct(int slot, const QString &name, int pricePoints,
                             const QString &imagePath, bool active)
{
    const bool ok = exec(
        "INSERT INTO products(slot,name,price_points,image_path,active) "
        "VALUES(?,?,?,?,?) "
        "ON CONFLICT(slot) DO UPDATE SET "
        "  name=excluded.name, price_points=excluded.price_points,"
        "  image_path=excluded.image_path, active=excluded.active",
        { slot, name, pricePoints, imagePath, active ? 1 : 0 });
    if (ok) {
        emit productsChanged();
        Logger::audit("Admin", "Product upserted",
                      { {"slot",slot}, {"name",name}, {"price",pricePoints} });

        // Push to MongoDB — tagged with this kiosk's ID so each kiosk's
        // catalog stays separate. Uses upsert (insert-or-update on slot+kiosk).
        if (m_mqtt && m_mqtt->isConnected()) {
            QJsonObject filter { {"kiosk_id", m_kioskId}, {"slot", slot} };
            QJsonObject set    { {"name", name},
                                 {"price_points", pricePoints},
                                 {"image_path", imagePath},
                                 {"active",     active},
                                 {"kiosk_id",   m_kioskId},
                                 {"slot",       slot},
                                 {"updated_at", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)} };
            QJsonObject update { {"$set", set} };
            // Mongo Data API upsert via updateOne — uses "upsert":true.
            // Our minimal client doesn't expose upsert flag yet; fall back
            // to insertOne for a fresh row, ignore failure if exists.
            m_mqtt->publishJson("products", set);
        }
    }
    return ok;
}

bool Database::setProductCount(int slot, int count, int weightG)
{
    const bool ok = exec(
        "UPDATE products SET last_count=?, last_weight_g=? WHERE slot=?",
        { count, weightG, slot });
    if (ok) emit productsChanged();
    return ok;
}

// ─── Product catalog ──────────────────────────────────────────────────────

QVariantList Database::listCatalog(const QString &searchText) const
{
    QVariantList rows;
    QSqlQuery q(m_db);
    if (searchText.isEmpty()) {
        q.prepare("SELECT id,name,image_path,image_url,default_price_points,"
                  "typical_weight_g,barcode,source,updated_at "
                  "FROM product_catalog ORDER BY name ASC");
    } else {
        q.prepare("SELECT id,name,image_path,image_url,default_price_points,"
                  "typical_weight_g,barcode,source,updated_at "
                  "FROM product_catalog WHERE name LIKE ? ORDER BY name ASC");
        q.addBindValue("%" + searchText + "%");
    }
    if (!q.exec()) return rows;
    while (q.next()) {
        QVariantMap r;
        r["id"]              = q.value(0);
        r["name"]            = q.value(1);
        r["imagePath"]       = q.value(2);
        r["imageUrl"]        = q.value(3);
        r["defaultPrice"]    = q.value(4);
        r["typicalWeightG"]  = q.value(5);
        r["barcode"]         = q.value(6);
        r["source"]          = q.value(7);
        r["updatedAt"]       = q.value(8);
        rows.append(r);
    }
    return rows;
}

QVariantMap Database::getCatalogItem(const QString &id) const
{
    QSqlQuery q(m_db);
    q.prepare("SELECT id,name,image_path,image_url,default_price_points,"
              "typical_weight_g,barcode,source FROM product_catalog WHERE id=?");
    q.addBindValue(id);
    if (!q.exec() || !q.next()) return {};
    QVariantMap r;
    r["id"]              = q.value(0);
    r["name"]            = q.value(1);
    r["imagePath"]       = q.value(2);
    r["imageUrl"]        = q.value(3);
    r["defaultPrice"]    = q.value(4);
    r["typicalWeightG"]  = q.value(5);
    r["barcode"]         = q.value(6);
    r["source"]          = q.value(7);
    return r;
}

QString Database::upsertCatalog(const QVariantMap &row)
{
    QString id = row.value("id").toString();
    if (id.isEmpty()) {
        // Mint a fresh UUID. Same id used across kiosks (Mongo sync key).
        id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
    const QString ts = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    const bool ok = exec(
        "INSERT INTO product_catalog"
        "(id,name,image_path,image_url,default_price_points,typical_weight_g,"
        " barcode,source,created_kiosk,updated_at) "
        "VALUES(?,?,?,?,?,?,?,?,?,?) "
        "ON CONFLICT(id) DO UPDATE SET "
        "  name=excluded.name,"
        "  image_path=excluded.image_path,"
        "  image_url=excluded.image_url,"
        "  default_price_points=excluded.default_price_points,"
        "  typical_weight_g=excluded.typical_weight_g,"
        "  barcode=excluded.barcode,"
        "  source=excluded.source,"
        "  updated_at=excluded.updated_at",
        { id, row.value("name"),
          row.value("imagePath"), row.value("imageUrl"),
          row.value("defaultPrice", 0), row.value("typicalWeightG", 0),
          row.value("barcode"), row.value("source", "manual"),
          m_kioskId, ts });
    if (!ok) return {};
    Logger::audit("Catalog", "Product upserted",
                  { {"id", id}, {"name", row.value("name")} });

    // Push to Mongo so other kiosks pick it up on their next sync.
    if (m_mqtt && m_mqtt->isConnected()) {
        QJsonObject doc {
            {"id",                   id},
            {"name",                 row.value("name").toString()},
            {"image_path",           row.value("imagePath").toString()},
            {"image_url",            row.value("imageUrl").toString()},
            {"default_price_points", row.value("defaultPrice", 0).toInt()},
            {"typical_weight_g",     row.value("typicalWeightG", 0).toInt()},
            {"barcode",              row.value("barcode").toString()},
            {"source",               row.value("source", "manual").toString()},
            {"created_kiosk",        m_kioskId},
            {"updated_at",           ts}
        };
        m_mqtt->publishJson("product_catalog", doc);
    }
    return id;
}

bool Database::deleteCatalog(const QString &id)
{
    if (!exec("DELETE FROM product_catalog WHERE id=?", { id })) return false;
    Logger::audit("Catalog", "Product deleted", { {"id", id} });
    return true;
}

bool Database::assignSlotFromCatalog(int slot, const QString &catalogId)
{
    const QVariantMap cat = getCatalogItem(catalogId);
    if (cat.isEmpty()) return false;

    // Copy catalog values into the slot row. We DON'T overwrite the slot's
    // calibration constants (empty_shelf_raw / unit_weight_raw) — those are
    // physical properties of the load cell + product on this kiosk, not
    // attributes of the SKU.
    const bool ok = exec(
        "INSERT INTO products(slot,name,price_points,image_path,active,catalog_id) "
        "VALUES(?,?,?,?,1,?) "
        "ON CONFLICT(slot) DO UPDATE SET "
        "  name=excluded.name, price_points=excluded.price_points,"
        "  image_path=excluded.image_path, catalog_id=excluded.catalog_id,"
        "  active=1",
        { slot, cat.value("name"), cat.value("defaultPrice"),
          cat.value("imagePath"), catalogId });
    if (ok) {
        emit productsChanged();
        Logger::audit("Catalog", "Slot assigned from catalog",
                      { {"slot", slot}, {"catalog_id", catalogId},
                        {"name", cat.value("name")} });
    }
    return ok;
}

bool Database::setEmptyShelfRaw(int slot, int raw)
{
    const bool ok = exec("UPDATE products SET empty_shelf_raw=? WHERE slot=?",
                         { raw, slot });
    if (ok) {
        emit productsChanged();
        Logger::audit("Calibration", "Empty-shelf tare set",
                      { {"slot", slot}, {"raw", raw} });
    }
    return ok;
}

bool Database::setUnitWeightRaw(int slot, int rawPerItem)
{
    const bool ok = exec("UPDATE products SET unit_weight_raw=? WHERE slot=?",
                         { rawPerItem, slot });
    if (ok) {
        emit productsChanged();
        Logger::audit("Calibration", "Per-item weight set",
                      { {"slot", slot}, {"raw_per_item", rawPerItem} });
    }
    return ok;
}

bool Database::getCalibration(int slot, int *emptyShelfRaw, int *unitWeightRaw) const
{
    QSqlQuery q(m_db);
    q.prepare("SELECT empty_shelf_raw, unit_weight_raw "
              "FROM products WHERE slot=?");
    q.addBindValue(slot);
    if (!q.exec() || !q.next()) return false;
    if (emptyShelfRaw) *emptyShelfRaw = q.value(0).toInt();
    if (unitWeightRaw) *unitWeightRaw = q.value(1).toInt();
    return true;
}

bool Database::recordRestockEvent(int slot, int prevCount, int newCount,
                                  const QString &source)
{
    if (prevCount == newCount) return true;            // no-op, don't log noise
    const QString ts = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    const int delta  = newCount - prevCount;
    const bool ok = exec(
        "INSERT INTO restock_events(ts,slot,prev_count,new_count,delta,source) "
        "VALUES(?,?,?,?,?,?)",
        { ts, slot, prevCount, newCount, delta, source });
    if (ok) {
        Logger::info("Inventory",
                     QString("Slot %1: %2 → %3 (%4%5) [%6]")
                         .arg(slot).arg(prevCount).arg(newCount)
                         .arg(delta > 0 ? "+" : "").arg(delta).arg(source),
                     { {"slot", slot}, {"delta", delta}, {"source", source} });
    }
    return ok;
}

QVariantList Database::listRestockEvents(int limit) const
{
    QVariantList rows;
    QSqlQuery q(m_db);
    q.prepare("SELECT id,ts,slot,prev_count,new_count,delta,source "
              "FROM restock_events ORDER BY id DESC LIMIT ?");
    q.addBindValue(limit > 0 ? limit : 100);
    if (!q.exec()) return rows;
    while (q.next()) {
        QVariantMap r;
        r["id"]        = q.value(0);
        r["ts"]        = q.value(1);
        r["slot"]      = q.value(2);
        r["prevCount"] = q.value(3);
        r["newCount"]  = q.value(4);
        r["delta"]     = q.value(5);
        r["source"]    = q.value(6);
        rows.append(r);
    }
    return rows;
}

QVariantList Database::latestRestockBySlot() const
{
    QVariantList rows;
    QSqlQuery q(m_db);
    /* For each slot, return the most-recent restock_event. SQLite's
     * MAX(id) trick avoids a correlated subquery.                        */
    if (!q.exec(
        "SELECT r.slot, r.ts, r.delta, r.source "
        "FROM restock_events r "
        "JOIN (SELECT slot, MAX(id) AS mid FROM restock_events GROUP BY slot) m "
        "  ON r.slot = m.slot AND r.id = m.mid "
        "ORDER BY r.slot ASC")) {
        return rows;
    }
    while (q.next()) {
        QVariantMap r;
        r["slot"]   = q.value(0);
        r["ts"]     = q.value(1);
        r["delta"]  = q.value(2);
        r["source"] = q.value(3);
        rows.append(r);
    }
    return rows;
}

bool Database::recordDispenseFault(int slot, const QString &reasonCode,
                                   int weightBefore, int weightAfter,
                                   int drop, int indexCount)
{
    const QString ts = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    const bool ok = exec(
        "INSERT INTO dispense_faults"
        "(ts,slot,reason,weight_before,weight_after,drop_g,index_count) "
        "VALUES(?,?,?,?,?,?,?)",
        { ts, slot, reasonCode, weightBefore, weightAfter, drop, indexCount });
    if (ok) {
        Logger::audit("Vending", "Dispense fault",
                      { {"slot", slot}, {"reason", reasonCode},
                        {"before", weightBefore}, {"after", weightAfter},
                        {"drop", drop}, {"index", indexCount} });
        if (m_mqtt && m_mqtt->isConnected()) {
            QJsonObject doc {
                {"kiosk_id",       m_kioskId},
                {"slot",           slot},
                {"reason",         reasonCode},
                {"weight_before",  weightBefore},
                {"weight_after",   weightAfter},
                {"drop_g",         drop},
                {"index_count",    indexCount},
                {"ts",             ts}
            };
            m_mqtt->publishJson("dispense_faults", doc);
        }
    }
    return ok;
}

QVariantList Database::listDispenseFaults(int limit) const
{
    QVariantList rows;
    QSqlQuery q(m_db);
    q.prepare("SELECT id,ts,slot,reason,weight_before,weight_after,drop_g,index_count "
              "FROM dispense_faults ORDER BY id DESC LIMIT ?");
    q.addBindValue(limit > 0 ? limit : 100);
    if (!q.exec()) return rows;
    while (q.next()) {
        QVariantMap r;
        r["id"]     = q.value(0);
        r["ts"]     = q.value(1);
        r["slot"]   = q.value(2);
        r["reason"] = q.value(3);
        r["before"] = q.value(4);
        r["after"]  = q.value(5);
        r["drop"]   = q.value(6);
        r["index"]  = q.value(7);
        rows.append(r);
    }
    return rows;
}

QVariantList Database::faultsBySlot() const
{
    QVariantList rows;
    QSqlQuery q(m_db);
    if (!q.exec("SELECT slot, COUNT(*) AS n, MAX(ts) AS last "
                "FROM dispense_faults GROUP BY slot ORDER BY slot")) {
        return rows;
    }
    while (q.next()) {
        QVariantMap r;
        r["slot"]  = q.value(0);
        r["count"] = q.value(1);
        r["last"]  = q.value(2);
        rows.append(r);
    }
    return rows;
}

int Database::clearDispenseFaults()
{
    QSqlQuery q(m_db);
    if (!q.exec("DELETE FROM dispense_faults")) return 0;
    const int n = q.numRowsAffected();
    Logger::audit("Admin", "Dispense fault log cleared", { {"removed", n} });
    return n;
}

bool Database::recordTransaction(const QString &kind, const QString &userId,
                                 int slot, int amount, const QVariantMap &meta)
{
    QJsonObject obj;
    for (auto it = meta.constBegin(); it != meta.constEnd(); ++it)
        obj.insert(it.key(), QJsonValue::fromVariant(it.value()));
    const QString metaJson = QString::fromUtf8(
        QJsonDocument(obj).toJson(QJsonDocument::Compact));

    const QString ts = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    const bool ok = exec(
        "INSERT INTO transactions(ts,kind,user_id,slot,amount,meta) "
        "VALUES(?,?,?,?,?,?)",
        { ts, kind, userId, slot, amount, metaJson });
    if (ok) {
        emit transactionsChanged();
        Logger::audit("Tx", kind,
                      { {"user_id",userId}, {"slot",slot}, {"amount",amount} });

        // Push to MongoDB tagged with kiosk_id.
        if (m_mqtt && m_mqtt->isConnected()) {
            QJsonObject doc {
                {"kiosk_id", m_kioskId},
                {"ts",       ts},
                {"kind",     kind},
                {"user_id",  userId},
                {"slot",     slot},
                {"amount",   amount},
                {"meta",     QJsonDocument::fromJson(metaJson.toUtf8()).object()}
            };
            m_mqtt->publishJson("transactions", doc);
        }
    }
    return ok;
}

bool Database::ensureUser(const QString &userId, const QString &displayName)
{
    return exec(
        "INSERT INTO users(id,name,points,consent_at,last_seen) "
        "VALUES(?,?,0,?,?) ON CONFLICT(id) DO UPDATE SET "
        "  name=excluded.name, last_seen=excluded.last_seen",
        { userId, displayName,
          QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs),
          QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs) });
}

QVariantMap Database::getUser(const QString &userId) const
{
    QVariantMap result;
    QSqlQuery q(m_db);
    q.prepare("SELECT id,name,points,consent_at,last_seen FROM users WHERE id=?");
    q.addBindValue(userId);
    if (q.exec() && q.next()) {
        result["id"]         = q.value(0);
        result["name"]       = q.value(1);
        result["points"]     = q.value(2);
        result["consent_at"] = q.value(3);
        result["last_seen"]  = q.value(4);
    }
    return result;
}

bool Database::adjustUserPoints(const QString &userId, int delta)
{
    return exec("UPDATE users SET points = points + ? WHERE id = ?",
                { delta, userId });
}

int Database::deleteOldTransactions(int daysOlderThan)
{
    const QString cutoff = QDateTime::currentDateTimeUtc()
                            .addDays(-daysOlderThan)
                            .toString(Qt::ISODateWithMs);
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM transactions WHERE ts < ?");
    q.addBindValue(cutoff);
    if (!q.exec()) return -1;
    const int n = q.numRowsAffected();
    Logger::audit("Maintenance", "Old transactions deleted",
                  { {"count", n}, {"older_than_days", daysOlderThan} });
    return n;
}

int Database::pendingAuditCount() const
{
    QSqlQuery q(m_db);
    if (q.exec("SELECT COUNT(*) FROM audit_log WHERE synced=0") && q.next())
        return q.value(0).toInt();
    return -1;
}

void Database::onAuditEvent(const QString &category, const QString &msg,
                            const QVariantMap &meta, const QDateTime &when)
{
    QJsonObject obj;
    for (auto it = meta.constBegin(); it != meta.constEnd(); ++it)
        obj.insert(it.key(), QJsonValue::fromVariant(it.value()));
    exec("INSERT INTO audit_log(ts,category,msg,meta,synced) VALUES(?,?,?,?,0)",
         { when.toString(Qt::ISODateWithMs), category, msg,
           QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact)) });
}

void Database::setRemoteSync(const QString &endpointUrl, const QString &apiKey)
{
    m_remoteUrl = endpointUrl;
    m_apiKey    = apiKey;
    if (!endpointUrl.isEmpty()) m_syncTimer.start();
    else m_syncTimer.stop();
}

void Database::syncTick()
{
    if (m_remoteUrl.isEmpty()) return;
    // Pull up to 100 unsynced rows
    QSqlQuery q(m_db);
    q.prepare("SELECT id,ts,category,msg,meta FROM audit_log WHERE synced=0 LIMIT 100");
    if (!q.exec()) return;

    QJsonArray batch;
    QList<int> ids;
    while (q.next()) {
        QJsonObject row;
        row["ts"]       = q.value(1).toString();
        row["category"] = q.value(2).toString();
        row["msg"]      = q.value(3).toString();
        row["meta"]     = QJsonDocument::fromJson(q.value(4).toByteArray()).object();
        batch.append(row);
        ids << q.value(0).toInt();
    }
    if (batch.isEmpty()) return;

    QNetworkRequest req(QUrl{m_remoteUrl});
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    if (!m_apiKey.isEmpty())
        req.setRawHeader("Authorization", "Bearer " + m_apiKey.toUtf8());

    QNetworkReply *reply = netMgr()->post(req, QJsonDocument(batch).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply, ids]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            Logger::warn("DB", "Remote sync failed",
                         { {"err", reply->errorString()} });
            return;
        }
        // Mark synced
        QSqlQuery upd(m_db);
        upd.prepare("UPDATE audit_log SET synced=1 WHERE id=?");
        for (int id : ids) { upd.addBindValue(id); upd.exec(); upd.finish(); }
        Logger::info("DB", "Remote sync OK", { {"count", ids.size()} });
    });
}
