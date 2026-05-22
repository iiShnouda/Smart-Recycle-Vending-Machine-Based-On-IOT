#ifndef PRODUCTS_MODEL_H
#define PRODUCTS_MODEL_H

#include <QAbstractListModel>
#include <QQmlEngine>

class Database;

/**
 * ProductsModel — exposes the 8 vending slots to QML.
 *
 * Roles:
 *   slot           : int        (1..8)
 *   name           : QString
 *   pricePoints    : int
 *   imagePath      : QString    (file:// or qrc:/ url)
 *   active         : bool
 *   count          : int        (live, from load cell scan)
 *   weightG        : int        (grams, from load cell)
 */
class ProductsModel : public QAbstractListModel {
    Q_OBJECT
    QML_NAMED_ELEMENT(ProductsModel)
    QML_SINGLETON
public:
    enum Roles {
        RoleSlot = Qt::UserRole + 1,
        RoleName, RolePrice, RoleImage, RoleActive, RoleCount, RoleWeight
    };

    explicit ProductsModel(QObject *parent = nullptr);
    static ProductsModel *create(QQmlEngine *, QJSEngine *) { return s_instance; }
    static ProductsModel *s_instance;

    void setDatabase(Database *db);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

public slots:
    /** Reload all rows from DB. Call after edits or initial setup. */
    Q_INVOKABLE void reload();

    /** Add or update a single slot's metadata. */
    Q_INVOKABLE bool setProduct(int slot, const QString &name,
                                int pricePoints, const QString &imagePath,
                                bool active);

    /** Update live count + weight (called by load cell scanner). */
    Q_INVOKABLE bool updateInventory(int slot, int count, int weightG);

private:
    struct Row {
        int     slot      = 0;
        QString name;
        int     price     = 0;
        QString imagePath;
        bool    active    = false;
        int     count     = 0;
        int     weightG   = 0;
    };

    QList<Row> m_rows;
    Database  *m_db = nullptr;
};

#endif // PRODUCTS_MODEL_H
