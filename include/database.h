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

    /** Wire the MQTT client so every write is also published to the
     *  broker (one MQTT topic per logical "collection").              */
    void setMqttClient(class MqttClient *m) { m_mqtt = m; }

    /** Configure REST endpoint for sync. Empty URL disables sync. */
    void setRemoteSync(const QString &endpointUrl, const QString &apiKey = {});

    // ---- products ----
    bool upsertProduct(int slot, const QString &name, int pricePoints,
                       const QString &imagePath, bool active);
    bool setProductCount(int slot, int count, int weightG);
    /** Flip just the `active` flag for one slot (used by dispense fault
     *  handling to disable a product the auger couldn't drop).             */
    bool setProductActive(int slot, bool active);

    // ---- product catalog (shared across kiosks via Mongo) ----
    /** All catalog rows, newest first. */
    QVariantList listCatalog(const QString &searchText = QString()) const;
    QVariantMap  getCatalogItem(const QString &id) const;
    /** Insert-or-update one row. `id` may be empty → a UUID is generated.
     *  Returns the resolved id (existing or new). Pushes to Mongo on
     *  success.                                                            */
    QString      upsertCatalog(const QVariantMap &row);
    bool         deleteCatalog(const QString &id);
    /** Link an existing slot to a catalog entry — copies the catalog's
     *  name/image/price into the slot and stores catalog_id for lookup.  */
    bool         assignSlotFromCatalog(int slot, const QString &catalogId);

    // ---- per-slot load-cell calibration ----
    /** Save the HX711 reading taken with an empty shelf. */
    bool setEmptyShelfRaw(int slot, int raw);
    /** Save the per-item raw weight (raw_with_N_items − empty_shelf) / N. */
    bool setUnitWeightRaw(int slot, int rawPerItem);
    /** Read calibration constants for a slot. */
    bool getCalibration(int slot, int *emptyShelfRaw, int *unitWeightRaw) const;

    /** Append a row to restock_events. `source` is one of
     *  'dispense', 'admin', 'boot', 'scan'. Used by the admin UI to see
     *  who/what touched a slot.                                          */
    bool recordRestockEvent(int slot, int prevCount, int newCount,
                            const QString &source);

    /** Recent inventory events, newest first. */
    QVariantList listRestockEvents(int limit = 100) const;
    /** Most recent event per slot, for the inventory overview page. */
    QVariantList latestRestockBySlot() const;

    // ---- transactions ----
    bool recordTransaction(const QString &kind, const QString &userId,
                           int slot, int amount, const QVariantMap &meta = {});

    // ---- dispense faults ----
    /** Append one row to the dispense_faults table.
     *  reasonCode is one of: STALL, NO_DROP, STEP_LOSS, TIMEOUT, BUSY.   */
    bool recordDispenseFault(int slot, const QString &reasonCode,
                             int weightBefore, int weightAfter,
                             int drop, int indexCount);

    /** Most-recent faults across all slots, newest first.                */
    QVariantList listDispenseFaults(int limit = 100) const;

    /** Per-slot fault counters since boot / since `since`. Used by the
     *  admin "X" badges on each slot card.                              */
    QVariantList faultsBySlot() const;

    /** Wipe history (admin housekeeping). Returns rows deleted.        */
    int clearDispenseFaults();

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
    class MqttClient *m_mqtt = nullptr;
};

#endif // DATABASE_H
