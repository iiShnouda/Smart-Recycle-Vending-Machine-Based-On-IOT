#include "../include/products_model.h"
#include "../include/database.h"
#include "../include/logger.h"

#include <QSqlQuery>
#include <QSqlError>

ProductsModel *ProductsModel::s_instance = nullptr;

ProductsModel::ProductsModel(QObject *parent) : QAbstractListModel(parent)
{
    if (!s_instance) s_instance = this;
    // Initialize 8 empty rows; admin can fill them in.
    for (int slot = 1; slot <= 8; ++slot) {
        m_rows.append({ slot, QString("Slot %1").arg(slot), 0, {}, false, 0, 0 });
    }
}

void ProductsModel::setDatabase(Database *db)
{
    m_db = db;
    if (m_db) {
        connect(m_db, &Database::productsChanged, this, &ProductsModel::reload);
        reload();
    }
}

int ProductsModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return m_rows.size();
}

QVariant ProductsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};
    const Row &r = m_rows.at(index.row());
    switch (role) {
        case RoleSlot:           return r.slot;
        case RoleName:           return r.name;
        case RolePrice:          return r.price;
        case RoleImage:          return r.imagePath;
        case RoleActive:         return r.active;
        case RoleCount:          return r.count;
        case RoleWeight:         return r.weightG;
        case RoleEmptyShelfRaw:  return r.emptyShelfRaw;
        case RoleUnitWeightRaw:  return r.unitWeightRaw;
        case RoleLastRaw:        return r.lastRaw;
        case RoleCalibrated:     return r.unitWeightRaw > 0;
        default:                 return {};
    }
}

QHash<int, QByteArray> ProductsModel::roleNames() const
{
    return {
        { RoleSlot,            "slot"           },
        { RoleName,            "name"           },
        { RolePrice,           "pricePoints"    },
        { RoleImage,           "imagePath"      },
        { RoleActive,          "active"         },
        { RoleCount,           "count"          },
        { RoleWeight,          "weightG"        },
        { RoleEmptyShelfRaw,   "emptyShelfRaw"  },
        { RoleUnitWeightRaw,   "unitWeightRaw"  },
        { RoleLastRaw,         "lastRaw"        },
        { RoleCalibrated,      "calibrated"     },
    };
}

void ProductsModel::reload()
{
    if (!m_db || !m_db->isOpen()) return;

    beginResetModel();
    m_rows.clear();
    // Ensure 8 slots exist in model even if DB is empty.
    for (int slot = 1; slot <= 8; ++slot)
        m_rows.append({ slot, QString("Slot %1").arg(slot), 0, {}, false, 0, 0 });

    QSqlQuery q(QSqlDatabase::database("rewingo"));
    if (q.exec("SELECT slot,name,price_points,image_path,active,last_count,"
               "last_weight_g,empty_shelf_raw,unit_weight_raw "
               "FROM products ORDER BY slot ASC")) {
        while (q.next()) {
            const int slot = q.value(0).toInt();
            if (slot < 1 || slot > 8) continue;
            Row &r = m_rows[slot - 1];
            r.name           = q.value(1).toString();
            r.price          = q.value(2).toInt();
            r.imagePath      = q.value(3).toString();
            r.active         = q.value(4).toInt() != 0;
            r.count          = q.value(5).toInt();
            r.weightG        = q.value(6).toInt();
            r.emptyShelfRaw  = q.value(7).toInt();
            r.unitWeightRaw  = q.value(8).toInt();
        }
    }
    endResetModel();
}

bool ProductsModel::setProduct(int slot, const QString &name, int pricePoints,
                               const QString &imagePath, bool active)
{
    if (!m_db || slot < 1 || slot > 8) return false;
    const bool ok = m_db->upsertProduct(slot, name, pricePoints, imagePath, active);
    if (ok) reload();
    return ok;
}

bool ProductsModel::updateInventory(int slot, int count, int weightG)
{
    if (!m_db || slot < 1 || slot > 8) return false;
    const bool ok = m_db->setProductCount(slot, count, weightG);
    if (ok) {
        Row &r = m_rows[slot - 1];
        r.count = count;
        r.weightG = weightG;
        const QModelIndex idx = index(slot - 1);
        emit dataChanged(idx, idx, { RoleCount, RoleWeight });
    }
    return ok;
}

bool ProductsModel::setActive(int slot, bool active)
{
    if (!m_db || slot < 1 || slot > 8) return false;
    const bool ok = m_db->setProductActive(slot, active);
    if (ok) {
        Row &r = m_rows[slot - 1];
        r.active = active;
        const QModelIndex idx = index(slot - 1);
        emit dataChanged(idx, idx, { RoleActive });
    }
    return ok;
}

int ProductsModel::ingestRawReading(int slot, int raw, const QString &source)
{
    if (slot < 1 || slot > 8) return -1;
    Row &r = m_rows[slot - 1];

    r.lastRaw = raw;

    // No calibration yet → we can record the raw value but can't infer a
    // count. UI will show "Not calibrated".
    int newCount = r.count;
    if (r.unitWeightRaw > 0) {
        const int load = raw - r.emptyShelfRaw;
        // round(load / unit) — symmetric for negative loads.
        if (load >= 0) {
            newCount = (load + r.unitWeightRaw / 2) / r.unitWeightRaw;
        } else {
            // Negative load means we read lighter than the empty tare — most
            // likely a calibration drift or someone leaning on the shelf.
            // Clamp to 0 so the count never goes negative.
            newCount = 0;
        }
        if (newCount < 0) newCount = 0;
    }

    // Approximate grams for legacy UI: hardcoded HX711 scale of ~22 raw / g
    // for a 1 kg cell at gain 128. Tune by patching this constant once.
    constexpr int RAW_PER_GRAM = 22;
    const int approxGrams = (raw - r.emptyShelfRaw) / RAW_PER_GRAM;

    const int prevCount = r.count;
    if (newCount != prevCount && m_db && r.unitWeightRaw > 0) {
        m_db->recordRestockEvent(slot, prevCount, newCount, source);
    }

    r.count   = newCount;
    r.weightG = approxGrams;

    if (m_db) {
        // Persist count + weight so a reboot starts from the right place
        // (we re-measure on boot anyway, but this keeps the SELECT fresh
        // for any UI that reads before the first scan completes).
        m_db->setProductCount(slot, r.count, r.weightG);
    }

    const QModelIndex idx = index(slot - 1);
    emit dataChanged(idx, idx,
                     { RoleCount, RoleWeight, RoleLastRaw });
    return newCount;
}

bool ProductsModel::calibrateEmptyShelf(int slot, int currentRaw)
{
    if (!m_db || slot < 1 || slot > 8) return false;
    if (!m_db->setEmptyShelfRaw(slot, currentRaw)) return false;
    m_rows[slot - 1].emptyShelfRaw = currentRaw;
    const QModelIndex idx = index(slot - 1);
    emit dataChanged(idx, idx, { RoleEmptyShelfRaw, RoleCalibrated });
    return true;
}

bool ProductsModel::calibrateUnitWeight(int slot, int currentRaw, int knownCount)
{
    if (!m_db || slot < 1 || slot > 8 || knownCount <= 0) return false;
    Row &r = m_rows[slot - 1];
    const int delta = currentRaw - r.emptyShelfRaw;
    if (delta <= 0) return false;          // shelf reads lighter than empty?
    const int perItem = delta / knownCount;
    if (!m_db->setUnitWeightRaw(slot, perItem)) return false;
    r.unitWeightRaw = perItem;
    r.count = knownCount;
    const QModelIndex idx = index(slot - 1);
    emit dataChanged(idx, idx,
                     { RoleUnitWeightRaw, RoleCount, RoleCalibrated });
    return true;
}

int ProductsModel::emptyShelfRaw(int slot) const
{
    if (slot < 1 || slot > 8) return 0;
    return m_rows[slot - 1].emptyShelfRaw;
}

int ProductsModel::unitWeightRaw(int slot) const
{
    if (slot < 1 || slot > 8) return 0;
    return m_rows[slot - 1].unitWeightRaw;
}

int ProductsModel::lastRaw(int slot) const
{
    if (slot < 1 || slot > 8) return 0;
    return m_rows[slot - 1].lastRaw;
}
