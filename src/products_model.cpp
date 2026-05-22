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
        case RoleSlot:    return r.slot;
        case RoleName:    return r.name;
        case RolePrice:   return r.price;
        case RoleImage:   return r.imagePath;
        case RoleActive:  return r.active;
        case RoleCount:   return r.count;
        case RoleWeight:  return r.weightG;
        default:          return {};
    }
}

QHash<int, QByteArray> ProductsModel::roleNames() const
{
    return {
        { RoleSlot,   "slot"        },
        { RoleName,   "name"        },
        { RolePrice,  "pricePoints" },
        { RoleImage,  "imagePath"   },
        { RoleActive, "active"      },
        { RoleCount,  "count"       },
        { RoleWeight, "weightG"     },
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
    if (q.exec("SELECT slot,name,price_points,image_path,active,last_count,last_weight_g "
               "FROM products ORDER BY slot ASC")) {
        while (q.next()) {
            const int slot = q.value(0).toInt();
            if (slot < 1 || slot > 8) continue;
            Row &r = m_rows[slot - 1];
            r.name      = q.value(1).toString();
            r.price     = q.value(2).toInt();
            r.imagePath = q.value(3).toString();
            r.active    = q.value(4).toInt() != 0;
            r.count     = q.value(5).toInt();
            r.weightG   = q.value(6).toInt();
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
