#ifndef FACE_PREVIEW_FEEDER_H
#define FACE_PREVIEW_FEEDER_H

#include <QObject>
#include <QQmlEngine>
#include <QTimer>

/*
 * FacePreviewFeeder — pushes the live face-rec frame to QML as a data URL.
 *
 * The Python face sidecars write the current camera frame to
 * /tmp/rewingo_face.jpg. Earlier preview attempts (file:///...?t= and an
 * image:// provider) rendered BLANK on the Pi. This feeder sidesteps all of
 * that: a timer reads the JPEG every ~120ms, base64-encodes it, and exposes
 * it as a `frame` property holding a "data:image/jpeg;base64,…" string —
 * which QML's Image decodes synchronously and reliably:
 *
 *     Component.onCompleted: FaceFrame.start()
 *     Component.onDestruction: FaceFrame.stop()
 *     Image { source: FaceFrame.frame }
 */
class FacePreviewFeeder : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(FaceFrame)
    QML_SINGLETON
    Q_PROPERTY(QString frame READ frame NOTIFY frameChanged)
public:
    explicit FacePreviewFeeder(QObject *parent = nullptr);
    static FacePreviewFeeder *create(QQmlEngine *, QJSEngine *) { return s_instance; }
    static FacePreviewFeeder *s_instance;

    QString frame() const { return m_frame; }

public slots:
    Q_INVOKABLE void start();   // begin polling the preview file
    Q_INVOKABLE void stop();    // stop polling (clears the frame)

signals:
    void frameChanged();

private:
    void tick();

    QTimer  m_timer;
    QString m_frame;
};

#endif // FACE_PREVIEW_FEEDER_H
