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
        RoleName, RolePrice, RoleImage, RoleActive, RoleCount, RoleWeight,
        RoleEmptyShelfRaw,     // HX711 reading when shelf is empty
        RoleUnitWeightRaw,     // HX711 raw counts per single item
        RoleLastRaw,           // most recent raw reading from the scanner
        RoleCalibrated         // true if both empty + unit are set
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

    /** Enable / disable a single slot. Used by the vending dispense flow
     *  to auto-disable a product after a fault.                          */
    Q_INVOKABLE bool setActive(int slot, bool active);

    /** Admin "in stock" toggle. Sets the stock count directly (no load cell):
     *  in stock -> a positive count, out -> 0. Vending shows a slot as
     *  buyable when count > 0.                                            */
    Q_INVOKABLE bool setInStock(int slot, bool inStock);

    /** Apply a raw HX711 reading from the InventoryScanner. Re-computes
     *  the count using stored calibration constants; logs a restock_events
     *  row if the count changed since last scan. Returns the new count.  */
    int ingestRawReading(int slot, int raw, const QString &source);

    /* ── Calibration (callable from QML) ──────────────────────────────── */

    /** Capture the *current* raw reading as the empty-shelf tare for `slot`.
     *  Admin workflow: empty the shelf → press button → call this with the
     *  latest scanner value.                                              */
    Q_INVOKABLE bool calibrateEmptyShelf(int slot, int currentRaw);

    /** Given the current raw reading and how many items are sitting on the
     *  shelf, derive `unit_weight_raw = (current - empty_shelf_raw) / N`.
     *  Returns false if N is zero or the shelf is below the tare.        */
    Q_INVOKABLE bool calibrateUnitWeight(int slot, int currentRaw, int knownCount);

    /** First slot with no product configured (name still "Slot N"), or -1 if
     *  all 8 are taken. Used to suggest a slot when scanning a new product. */
    Q_INVOKABLE int firstEmptySlot() const;

    /** Snapshot helpers for QML diagnostics screens. */
    Q_INVOKABLE int emptyShelfRaw(int slot) const;
    Q_INVOKABLE int unitWeightRaw(int slot) const;
    Q_INVOKABLE int lastRaw     (int slot) const;

private:
    struct Row {
        int     slot           = 0;
        QString name;
        int     price          = 0;
        QString imagePath;
        bool    active         = false;
        int     count          = 0;
        int     weightG        = 0;
        int     emptyShelfRaw  = 0;
        int     unitWeightRaw  = 0;
        int     lastRaw        = 0;     // not persisted — runtime only
    };

    QList<Row> m_rows;
    Database  *m_db = nullptr;
};

#endif // PRODUCTS_MODEL_H
