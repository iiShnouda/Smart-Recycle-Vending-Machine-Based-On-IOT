#ifndef DATABASE_H
#define DATABASE_H

#include <QObject>
#include <QString>
#include <QSqlDatabase>
#include <QVariant>
#include <QVariantMap>
#include <QDateTime>
#include <QTimer>

/**
 * Database — SQLite locally, periodic sync to a remote REST endpoint.
 *
 * Tables:
 *   users         (id TEXT PK, name TEXT, points INT, consent_at TEXT, last_seen TEXT)
 *   products      (slot INT PK, name TEXT, price_points INT, image_path TEXT,
 *                  active INT, last_count INT, last_weight_g INT)
 *   transactions  (id INTEGER PK, ts TEXT, kind TEXT, user_id TEXT, slot INT,
 *                  amount INT, meta TEXT)         // kind = "recycle" / "vending"
 *   audit_log     (id INTEGER PK, ts TEXT, category TEXT, msg TEXT, meta TEXT,
 *                  synced INT)
 *
 * Privacy: transactions table never stores names — only `user_id`.
 *          PII (name) lives only in `users`, viewable by admin.
 *          Configurable retention: deleteOldTransactions(daysOlderThan).
 */
class Database : public QObject {
    Q_OBJECT
public:
    explicit Database(QObject *parent = nullptr);
    ~Database() override;

    bool open(const QString &dbPath);
    void close();
    bool isOpen() const { return m_db.isOpen(); }

    /** Set the kiosk identity (used to tag remote-sync rows). */
    void setKioskId(const QString &id) { m_kioskId = id; }
    QString kioskId() const { return m_kioskId; }

    /** Wire the MongoDB client so writes also push to the cloud. */
    void setMongoClient(class MongoClient *m) { m_mongo = m; }

    /** Configure REST endpoint for sync. Empty URL disables sync. */
    void setRemoteSync(const QString &endpointUrl, const QString &apiKey = {});

    // ---- products ----
    bool upsertProduct(int slot, const QString &name, int pricePoints,
                       const QString &imagePath, bool active);
    bool setProductCount(int slot, int count, int weightG);

    // ---- transactions ----
    bool recordTransaction(const QString &kind, const QString &userId,
                           int slot, int amount, const QVariantMap &meta = {});

    // ---- users ----
    bool ensureUser(const QString &userId, const QString &displayName);
    QVariantMap getUser(const QString &userId) const;
    bool adjustUserPoints(const QString &userId, int delta);

    // ---- maintenance ----
    int  deleteOldTransactions(int daysOlderThan);
    int  pendingAuditCount() const;

signals:
    void productsChanged();
    void transactionsChanged();

public slots:
    /** Wire to Logger::auditEvent — persists every audit to DB for sync. */
    void onAuditEvent(const QString &category, const QString &msg,
                      const QVariantMap &meta, const QDateTime &when);

private slots:
    void syncTick();

private:
    void createTables();
    bool exec(const QString &sql, const QVariantList &binds = {});

    QSqlDatabase m_db;
    QString      m_remoteUrl;
    QString      m_apiKey;
    QTimer       m_syncTimer;
    QString      m_kioskId;
    class MongoClient *m_mongo = nullptr;
};

#endif // DATABASE_H
