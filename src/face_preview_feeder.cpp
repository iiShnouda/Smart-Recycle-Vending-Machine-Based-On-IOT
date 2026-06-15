#include "../include/face_preview_feeder.h"

#include <QFile>
#include <QDir>

FacePreviewFeeder *FacePreviewFeeder::s_instance = nullptr;

FacePreviewFeeder::FacePreviewFeeder(QObject *parent) : QObject(parent)
{
    if (!s_instance) s_instance = this;
    m_timer.setInterval(120);                 // ~8 fps
    connect(&m_timer, &QTimer::timeout, this, &FacePreviewFeeder::tick);
}

void FacePreviewFeeder::start()
{
    tick();
    m_timer.start();
}

void FacePreviewFeeder::stop()
{
    m_timer.stop();
    if (!m_frame.isEmpty()) { m_frame.clear(); emit frameChanged(); }
}

void FacePreviewFeeder::tick()
{
    // The sidecars write to literal /tmp/rewingo_face.jpg.
    QFile f(QStringLiteral("/tmp/rewingo_face.jpg"));
    if (!f.open(QIODevice::ReadOnly)) return;
    const QByteArray jpg = f.readAll();
    f.close();
    if (jpg.size() < 100) return;             // skip an empty/half-written frame
    const QString next = QStringLiteral("data:image/jpeg;base64,")
                       + QString::fromLatin1(jpg.toBase64());
    if (next == m_frame) return;              // unchanged → no repaint
    m_frame = next;
    emit frameChanged();
}
