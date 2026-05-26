#ifndef PRODUCT_CATALOG_H
#define PRODUCT_CATALOG_H

#include <QAbstractListModel>
#include <QQmlEngine>
#include <QVariantMap>
#include <QString>

class Database;
class QNetworkAccessManager;

/**
 * ProductCatalog — Qt model wrapping the shared SKU catalog.
 *
 * Why this exists: products are not per-slot. An admin builds the catalog
 * once (Cola, Chips, Water…) and every slot just picks from it. Other
 * kiosks see the same catalog because each row is also written to
 * MongoDB's `product_catalog` collection.
 *
 * Image cache: catalog rows may reference remote URLs (from Open Food
 * Facts). On first save we download to AppDataLocation/catalog_images/
 * so the kiosk works offline forever after.
 *
 * QML access (singleton):
 *     CatalogModel.list()
 *     CatalogModel.addOrUpdate({name:"Cola", defaultPrice:50, typicalWeightG:330})
 *     CatalogModel.assignToSlot(slot, catalogId)
 *     CatalogModel.cacheImageFromUrl(url, callback)
 */
class ProductCatalog : public QAbstractListModel {
    Q_OBJECT
    QML_NAMED_ELEMENT(CatalogModel)
    QML_SINGLETON
public:
    enum Roles {
        RoleId = Qt::UserRole + 1,
        RoleName, RoleImagePath, RoleImageUrl, RoleDefaultPrice,
        RoleTypicalWeightG, RoleBarcode, RoleSource, RoleUpdatedAt
    };

    explicit ProductCatalog(QObject *parent = nullptr);
    static ProductCatalog *create(QQmlEngine *, QJSEngine *) { return s_instance; }
    static ProductCatalog *s_instance;

    void setDatabase(Database *db);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

public slots:
    /** Reload from SQLite. Call after upserts or Mongo pulls. */
    Q_INVOKABLE void reload(const QString &filter = QString());

    /** Insert or update one catalog row. If `row.imageUrl` is set we
     *  download the image asynchronously and rewrite `imagePath` once the
     *  cache file lands.                                                  */
    Q_INVOKABLE QString addOrUpdate(const QVariantMap &row);

    /** Remove one row by id. */
    Q_INVOKABLE bool remove(const QString &id);

    /** Assign one slot to a catalog item — name/image/price are copied
     *  into the slot row. Calibration constants are NOT touched (those
     *  belong to this kiosk's load cell, not the SKU).                   */
    Q_INVOKABLE bool assignToSlot(int slot, const QString &catalogId);

    /** Find by exact (case-insensitive) name. Returns {} if not found. */
    Q_INVOKABLE QVariantMap findByName(const QString &name) const;

    /** Pull every catalog row from Mongo and merge into local SQLite.
     *  Called on app start + when admin enters the Products page.       */
    Q_INVOKABLE void pullFromCloud();

signals:
    void cloudSyncFinished(int rowsMerged);

private:
    QString cacheImageFromUrl(const QString &url);

    Database              *m_db      = nullptr;
    QNetworkAccessManager *m_net     = nullptr;
    QString                m_cacheDir;
    QList<QVariantMap>     m_rows;
};

#endif // PRODUCT_CATALOG_H
