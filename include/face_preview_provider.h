#ifndef FACE_PREVIEW_PROVIDER_H
#define FACE_PREVIEW_PROVIDER_H

#include <QQuickImageProvider>
#include <QImage>
#include <QDir>

/*
 * FacePreviewProvider — serves the live face-rec preview frame to QML.
 *
 * The Python face sidecars write the current camera frame to
 * <tmp>/rewingo_face.jpg (~8 fps). QML can't reliably hot-reload a local
 * file:// URL — the "?t=" query cache-buster + asynchronous loading is
 * fragile on the Pi and left the preview disc BLANK even though the file on
 * disk is a perfect frame. This provider reads the file FRESH on every
 * request, so QML just bumps a counter in the id to pull the latest frame:
 *
 *     Image { cache: false; source: "image://facepreview/" + tick }
 */
class FacePreviewProvider : public QQuickImageProvider {
public:
    FacePreviewProvider()
        : QQuickImageProvider(QQuickImageProvider::Image) {}

    QImage requestImage(const QString &id, QSize *size,
                        const QSize &requestedSize) override
    {
        Q_UNUSED(id)   // id is only a cache-buster (the frame tick)
        QImage img(QDir::tempPath() + QStringLiteral("/rewingo_face.jpg"));
        if (img.isNull()) {
            img = QImage(2, 2, QImage::Format_RGB888);
            img.fill(Qt::black);
        }
        if (requestedSize.isValid() && !requestedSize.isEmpty())
            img = img.scaled(requestedSize, Qt::KeepAspectRatioByExpanding,
                             Qt::SmoothTransformation);
        if (size) *size = img.size();
        return img;
    }
};

#endif // FACE_PREVIEW_PROVIDER_H
