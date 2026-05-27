#include "../include/product_image_catalog.h"
#include "../include/logger.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QUrl>

ProductImageCatalog *ProductImageCatalog::s_instance = nullptr;

ProductImageCatalog::ProductImageCatalog(QObject *parent) : QObject(parent)
{
    if (!s_instance) s_instance = this;

    const QString data = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation);
    m_uploadDir    = data + "/uploads/products";
    m_urlsJsonPath = data + "/product_urls.json";
    QDir().mkpath(m_uploadDir);

    refresh();
}

bool ProductImageCatalog::isValidImageUrl(const QString &url) const
{
    if (!url.startsWith("http://") && !url.startsWith("https://")) return false;
    const QString lower = url.toLower();
    return lower.endsWith(".png")  || lower.endsWith(".jpg")
        || lower.endsWith(".jpeg") || lower.endsWith(".webp")
        || lower.endsWith(".gif");
}

void ProductImageCatalog::refresh()
{
    m_images.clear();
    loadPresets();
    loadUploads();
    loadUrls();
    emit changed();
    Logger::info("ProductImages", "Catalog refreshed",
                 { {"count", int(m_images.size())} });
}

void ProductImageCatalog::loadPresets()
{
    // Walk the qrc:/ presets directory. Filenames become display names.
    const QString prefix = "qrc:/Recycle_Vending_Machine_LCD/"
                           "resources/assets/products";
    QDir d(":/Recycle_Vending_Machine_LCD/resources/assets/products");
    if (!d.exists()) return;

    const auto files = d.entryInfoList(
        { "*.png", "*.jpg", "*.jpeg", "*.webp" }, QDir::Files, QDir::Name);
    for (const auto &fi : files) {
        QVariantMap row;
        row["path"]   = prefix + "/" + fi.fileName();
        row["name"]   = fi.completeBaseName();    // "cola" → display
        row["source"] = "preset";
        m_images.append(row);
    }
}

void ProductImageCatalog::loadUploads()
{
    QDir d(m_uploadDir);
    if (!d.exists()) return;
    const auto files = d.entryInfoList(
        { "*.png", "*.jpg", "*.jpeg", "*.webp" }, QDir::Files, QDir::Time);
    for (const auto &fi : files) {
        QVariantMap row;
        row["path"]   = QUrl::fromLocalFile(fi.absoluteFilePath()).toString();
        row["name"]   = fi.completeBaseName();
        row["source"] = "upload";
        m_images.append(row);
    }
}

void ProductImageCatalog::loadUrls()
{
    QFile f(m_urlsJsonPath);
    if (!f.open(QIODevice::ReadOnly)) return;
    const auto arr = QJsonDocument::fromJson(f.readAll()).array();
    for (const auto &v : arr) {
        const auto obj = v.toObject();
        QVariantMap row;
        row["path"]   = obj.value("url").toString();
        row["name"]   = obj.value("name").toString();
        row["source"] = "url";
        m_images.append(row);
    }
}

void ProductImageCatalog::saveUrls()
{
    QJsonArray arr;
    for (const auto &v : m_images) {
        const auto m = v.toMap();
        if (m.value("source").toString() != "url") continue;
        QJsonObject o;
        o["url"]  = m.value("path").toString();
        o["name"] = m.value("name").toString();
        arr.append(o);
    }
    QFile f(m_urlsJsonPath);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(QJsonDocument(arr).toJson());
}

void ProductImageCatalog::addUrl(const QString &url, const QString &name)
{
    if (!isValidImageUrl(url)) {
        Logger::warn("ProductImages", "Invalid URL rejected", { {"url", url} });
        return;
    }
    QVariantMap row;
    row["path"]   = url;
    row["name"]   = name.isEmpty() ? QUrl(url).fileName() : name;
    row["source"] = "url";
    m_images.append(row);
    saveUrls();
    emit changed();
    Logger::audit("ProductImages", "URL added", { {"url", url}, {"name", name} });
}

void ProductImageCatalog::remove(const QString &path)
{
    for (int i = m_images.size() - 1; i >= 0; --i) {
        const auto m = m_images[i].toMap();
        if (m.value("path").toString() == path
            && m.value("source").toString() != "preset") {
            // Delete underlying file if it was an upload
            if (m.value("source").toString() == "upload") {
                const QString localPath = QUrl(path).toLocalFile();
                QFile::remove(localPath);
            }
            m_images.removeAt(i);
            Logger::audit("ProductImages", "Removed", { {"path", path} });
        }
    }
    saveUrls();
    emit changed();
}
