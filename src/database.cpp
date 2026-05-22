#include "../include/database.h"
#include "../include/logger.h"
#include "../include/mongo_client.h"

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
         "  last_weight_g INTEGER DEFAULT 0)");

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
        if (m_mongo && m_mongo->isConfigured()) {
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
            m_mongo->insertOne("products", set, [](bool, const QJsonObject&){});
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
        if (m_mongo && m_mongo->isConfigured()) {
            QJsonObject doc {
                {"kiosk_id", m_kioskId},
                {"ts",       ts},
                {"kind",     kind},
                {"user_id",  userId},
                {"slot",     slot},
                {"amount",   amount},
                {"meta",     QJsonDocument::fromJson(metaJson.toUtf8()).object()}
            };
            m_mongo->insertOne("transactions", doc, [](bool, const QJsonObject&){});
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
