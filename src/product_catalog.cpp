#include "../include/product_catalog.h"
#include "../include/database.h"
#include "../include/logger.h"
#include "../include/mqtt_client.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>

ProductCatalog *ProductCatalog::s_instance = nullptr;

ProductCatalog::ProductCatalog(QObject *parent) : QAbstractListModel(parent)
{
    if (!s_instance) s_instance = this;

    m_net = new QNetworkAccessManager(this);

    // Cache dir for downloaded images: AppDataLocation/catalog_images/
    m_cacheDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                 + "/catalog_images";
    QDir().mkpath(m_cacheDir);
}

void ProductCatalog::setDatabase(Database *db)
{
    m_db = db;
    if (m_db) reload();
}

// ─── Model ────────────────────────────────────────────────────────────────

int ProductCatalog::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

QVariant ProductCatalog::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};
    const QVariantMap &r = m_rows.at(index.row());
    switch (role) {
        case RoleId:             return r.value("id");
        case RoleName:           return r.value("name");
        case RoleImagePath:      return r.value("imagePath");
        case RoleImageUrl:       return r.value("imageUrl");
        case RoleDefaultPrice:   return r.value("defaultPrice");
        case RoleTypicalWeightG: return r.value("typicalWeightG");
        case RoleBarcode:        return r.value("barcode");
        case RoleSource:         return r.value("source");
        case RoleUpdatedAt:      return r.value("updatedAt");
        default:                 return {};
    }
}

QHash<int, QByteArray> ProductCatalog::roleNames() const
{
    return {
        { RoleId,             "id"             },
        { RoleName,           "name"           },
        { RoleImagePath,      "imagePath"      },
        { RoleImageUrl,       "imageUrl"       },
        { RoleDefaultPrice,   "defaultPrice"   },
        { RoleTypicalWeightG, "typicalWeightG" },
        { RoleBarcode,        "barcode"        },
        { RoleSource,         "source"         },
        { RoleUpdatedAt,      "updatedAt"      },
    };
}

// ─── Public API ───────────────────────────────────────────────────────────

void ProductCatalog::reload(const QString &filter)
{
    if (!m_db) return;
    beginResetModel();
    m_rows.clear();
    const QVariantList rows = m_db->listCatalog(filter);
    for (const QVariant &v : rows) m_rows.append(v.toMap());
    endResetModel();
}

QString ProductCatalog::addOrUpdate(const QVariantMap &row)
{
    if (!m_db) return {};

    QVariantMap r = row;

    // If we got a remote URL, kick off a download into the cache dir.
    // The local file path takes precedence in `imagePath`; we keep the
    // original URL so other kiosks (or a fresh cache) can re-fetch.
    const QString url = r.value("imageUrl").toString();
    if (!url.isEmpty() && r.value("imagePath").toString().isEmpty()) {
        const QString cached = cacheImageFromUrl(url);
        if (!cached.isEmpty()) r["imagePath"] = "file:///" + cached;
    }

    const QString id = m_db->upsertCatalog(r);
    reload();
    return id;
}

bool ProductCatalog::remove(const QString &id)
{
    if (!m_db) return false;
    const bool ok = m_db->deleteCatalog(id);
    if (ok) reload();
    return ok;
}

bool ProductCatalog::assignToSlot(int slot, const QString &catalogId)
{
    return m_db && m_db->assignSlotFromCatalog(slot, catalogId);
}

QVariantMap ProductCatalog::findByName(const QString &name) const
{
    for (const auto &r : m_rows) {
        if (r.value("name").toString().compare(name, Qt::CaseInsensitive) == 0)
            return r;
    }
    return {};
}

void ProductCatalog::pullFromCloud()
{
    // TODO: subscribe to rewingo/+/product_catalog from a fleet-wide
    // listener server and feed catalog rows back via /cmd. For now,
    // catalog rows reach other kiosks via the Mongo `insertOne` we do in
    // upsertCatalog — a fresh kiosk picks them up by querying its Mongo
    // products view directly. Stub this so QML can still call it.
    emit cloudSyncFinished(0);
}

// ─── Image caching ────────────────────────────────────────────────────────

QString ProductCatalog::cacheImageFromUrl(const QString &url)
{
    // We use a hash of the URL as the filename so multiple kiosks that
    // download the same Open Food Facts image end up with the same local
    // path — useful for the Mongo sync round-trip.
    const QByteArray hash =
        QCryptographicHash::hash(url.toUtf8(), QCryptographicHash::Sha1).toHex();
    // Preserve the file extension if present in the URL.
    QString ext = QUrl(url).fileName().section('.', -1).toLower();
    if (ext.size() > 5 || ext.isEmpty()) ext = "jpg";
    const QString path = m_cacheDir + "/" + QString::fromUtf8(hash) + "." + ext;

    if (QFile::exists(path)) return path;     // already cached

    // Fire the download — but we can't block here. Save into a temp file
    // and rename when the reply finishes. The catalog model row gets
    // updated by a follow-up reload() call inside the handler.
    const QString tmpPath = path + ".part";
    auto *reply = m_net->get(QNetworkRequest(QUrl(url)));
    QObject::connect(reply, &QNetworkReply::finished, this,
                     [this, reply, tmpPath, path]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            Logger::warn("Catalog", "Image download failed",
                         { {"url", reply->url().toString()},
                           {"err", reply->errorString()} });
            return;
        }
        QFile f(tmpPath);
        if (!f.open(QIODevice::WriteOnly)) return;
        f.write(reply->readAll());
        f.close();
        QFile::rename(tmpPath, path);
        // Refresh the model so the new imagePath is reflected.
        reload();
    });

    // Return the eventual path. The UI will see a missing file briefly
    // and the image element will then refresh once it lands.
    return path;
}
