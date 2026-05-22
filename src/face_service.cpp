#include "../include/face_service.h"
#include "../include/yolo_runner.h"
#include "../include/database.h"
#include "../include/logger.h"

#include <QSqlQuery>
#include <QSqlDatabase>
#include <QSqlError>
#include <QByteArray>
#include <QDataStream>

FaceService *FaceService::s_instance = nullptr;

FaceService::FaceService(QObject *parent) : QObject(parent)
{
    if (!s_instance) s_instance = this;
    m_timeoutTimer.setSingleShot(true);
    m_timeoutTimer.setInterval(kIdentifyTimeoutMs);
    connect(&m_timeoutTimer, &QTimer::timeout, this, [this]() {
        if (m_mode == Identifying) { m_mode = Idle; emit busyChanged(); emit noMatch(); }
    });
}

void FaceService::startEnrollment(const QString &userId, const QString &name)
{
    if (m_mode != Idle) return;
    m_enrollUserId = userId;
    m_enrollName   = name;
    m_collected.clear();
    m_progress = 0;
    m_mode     = Enrolling;
    emit progressChanged();
    emit busyChanged();
    Logger::info("FaceService", "Enrollment started", { {"user_id", userId} });
}

void FaceService::startIdentify()
{
    if (m_mode != Idle) return;
    m_mode = Identifying;
    emit busyChanged();
    m_timeoutTimer.start();
}

void FaceService::cancel()
{
    m_mode = Idle;
    m_progress = 0;
    m_collected.clear();
    m_timeoutTimer.stop();
    emit progressChanged();
    emit busyChanged();
}

bool FaceService::feedFrame(const QImage &img)
{
    if (m_mode == Idle || !m_yolo || img.isNull()) return false;

    const auto faces = m_yolo->detect(img, 0.55f);
    if (faces.isEmpty()) return false;          // wait for next frame
    const QVector<float> emb = m_yolo->extractFaceEmbedding(img);
    if (emb.isEmpty()) return false;

    if (m_mode == Enrolling) {
        m_collected.append(emb);
        m_progress = m_collected.size();
        emit progressChanged();
        if (m_progress >= kEnrollFrames) finishEnrollment();
        return true;
    }

    if (m_mode == Identifying) {
        float score = 0;
        const QString id = findBestMatch(emb, score);
        m_timeoutTimer.stop();
        m_mode = Idle;
        emit busyChanged();
        if (!id.isEmpty() && score >= kMatchThreshold) {
            QVariantMap u = m_db ? m_db->getUser(id) : QVariantMap{};
            const QString name = u.value("name").toString();
            Logger::audit("FaceService", "Identify match",
                          { {"user_id", id}, {"score", score} });
            emit matched(id, name);
        } else {
            Logger::info("FaceService", "Identify: no match",
                         { {"best_score", score} });
            emit noMatch();
        }
        return true;
    }
    return false;
}

void FaceService::finishEnrollment()
{
    // Average the collected embeddings to one stable vector.
    QVector<float> avg(m_collected.first().size(), 0.0f);
    for (const auto &v : m_collected)
        for (int i = 0; i < avg.size(); ++i) avg[i] += v[i];
    for (float &v : avg) v /= m_collected.size();

    // L2 normalise the average
    float n = 0; for (float v : avg) n += v*v;
    n = (n > 0) ? std::sqrt(n) : 1;
    for (float &v : avg) v /= n;

    // Serialise + store in DB.
    QByteArray blob;
    QDataStream s(&blob, QIODevice::WriteOnly);
    s.setVersion(QDataStream::Qt_6_0);
    s.setByteOrder(QDataStream::LittleEndian);
    for (float v : avg) s << v;

    if (m_db && m_db->isOpen()) {
        m_db->ensureUser(m_enrollUserId, m_enrollName);
        QSqlQuery q(QSqlDatabase::database("rewingo"));
        q.prepare("UPDATE users SET face_embedding=? WHERE id=?");
        q.addBindValue(blob);
        q.addBindValue(m_enrollUserId);
        if (!q.exec()) {
            Logger::error("FaceService", QStringLiteral("Save embedding failed"),
                          QVariantMap{ {"err", q.lastError().text()} });
            m_mode = Idle; m_collected.clear();
            emit busyChanged(); emit enrollFailed("DB save failed");
            return;
        }
    }
    Logger::audit("FaceService", "Enrollment complete",
                  { {"user_id", m_enrollUserId} });
    const QString id = m_enrollUserId;
    m_mode = Idle;
    m_collected.clear();
    emit busyChanged();
    emit enrollSucceeded(id);
}

QString FaceService::findBestMatch(const QVector<float> &emb, float &outScore) const
{
    outScore = 0.0f;
    QString bestId;
    if (!m_db || !m_db->isOpen()) return {};

    QSqlQuery q(QSqlDatabase::database("rewingo"));
    if (!q.exec("SELECT id, face_embedding FROM users WHERE face_embedding IS NOT NULL"))
        return {};

    while (q.next()) {
        const QString id = q.value(0).toString();
        const QByteArray blob = q.value(1).toByteArray();
        QVector<float> ref(emb.size());
        QDataStream s(blob);
        s.setVersion(QDataStream::Qt_6_0);
        s.setByteOrder(QDataStream::LittleEndian);
        for (int i = 0; i < ref.size() && !s.atEnd(); ++i) s >> ref[i];

        const float score = YoloRunner::similarity(emb, ref);
        if (score > outScore) { outScore = score; bestId = id; }
    }
    return bestId;
}
