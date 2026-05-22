#ifndef PRODUCT_IMAGE_CATALOG_H
#define PRODUCT_IMAGE_CATALOG_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QQmlEngine>
#include <QVariantList>

/**
 * ProductImageCatalog — the admin's product-image picker source.
 *
 * Three sources of images, in order:
 *   1. Built-in presets   (qrc:/.../resources/assets/products/*.png)
 *   2. Custom images on disk that the mobile app uploaded to a known dir
 *   3. URLs (the admin pastes an http(s) URL)
 *
 * QML calls images to render the picker grid. The path stored on a
 * Product row is one of:
 *   - "qrc:/qt/qml/.../products/cola.png"     (preset)
 *   - "file:///home/pi/.../uploads/x.png"     (uploaded via app)
 *   - "https://...png"                        (URL)
 *
 * The picker shows all three together, deduplicated.
 */
class ProductImageCatalog : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(ProductImages)
    QML_SINGLETON

    Q_PROPERTY(QVariantList images READ images NOTIFY changed)
    Q_PROPERTY(QString uploadDir READ uploadDir CONSTANT)

public:
    explicit ProductImageCatalog(QObject *parent = nullptr);
    static ProductImageCatalog *create(QQmlEngine *, QJSEngine *) { return s_instance; }
    static ProductImageCatalog *s_instance;

    /** Returns the list of available images.
     *  Each entry: { "path": "...", "name": "Cola", "source": "preset|upload|url" }. */
    QVariantList images() const { return m_images; }

    /** Path where the mobile app uploads custom product images on the Pi.
     *  Exposed so the upload service knows where to drop files. */
    QString uploadDir() const { return m_uploadDir; }

public slots:
    /** Re-scan presets + upload dir. Call after any upload. */
    Q_INVOKABLE void refresh();

    /** Validate a URL (must end in .png/.jpg/.jpeg/.webp). */
    Q_INVOKABLE bool isValidImageUrl(const QString &url) const;

    /** Add a URL to the catalog so it appears in the picker on next refresh.
     *  URLs are stored in a tiny JSON list on disk so they persist.    */
    Q_INVOKABLE void addUrl(const QString &url, const QString &name);

    /** Remove a custom entry (preset images can't be removed). */
    Q_INVOKABLE void remove(const QString &path);

signals:
    void changed();

private:
    void loadPresets();
    void loadUploads();
    void loadUrls();
    void saveUrls();

    QVariantList m_images;
    QString      m_uploadDir;
    QString      m_urlsJsonPath;
};

#endif // PRODUCT_IMAGE_CATALOG_H
