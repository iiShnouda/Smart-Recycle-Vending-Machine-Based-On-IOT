#ifndef FACE_SERVICE_H
#define FACE_SERVICE_H

#include <QObject>
#include <QString>
#include <QQmlEngine>
#include <QTimer>
#include <QImage>
#include <QVector>

class YoloRunner;
class Database;

/**
 * FaceService — orchestrates face recognition for both users and admins.
 *
 * Two flows:
 *   1. enroll(userId, name)
 *        Captures 5 frames, averages embeddings, stores via Database.
 *        Emits enrollProgress(0..5), then enrollSucceeded/Failed.
 *
 *   2. identify()
 *        Captures 1 frame, compares embedding to stored users.
 *        Emits matched(userId, name) or noMatch().
 *
 * QML drives this — capture happens inside, you only feed it QImages from
 * a QML VideoOutput (via VideoFrame::toImage()).
 */
class FaceService : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(FaceService)
    QML_SINGLETON

    Q_PROPERTY(int  enrollProgress READ enrollProgress NOTIFY progressChanged)
    Q_PROPERTY(int  enrollTotal    READ enrollTotal    CONSTANT)
    Q_PROPERTY(bool busy           READ busy           NOTIFY busyChanged)

public:
    explicit FaceService(QObject *parent = nullptr);
    static FaceService *create(QQmlEngine *, QJSEngine *) { return s_instance; }
    static FaceService *s_instance;

    void setRunner(YoloRunner *r)  { m_yolo = r; }
    void setDatabase(Database *db) { m_db = db; }

    int  enrollProgress() const { return m_progress; }
    int  enrollTotal()    const { return kEnrollFrames; }
    bool busy()           const { return m_mode != Idle; }

public slots:
    /** Start an enrollment session. Frames must be supplied via feedFrame(). */
    Q_INVOKABLE void startEnrollment(const QString &userId, const QString &name);

    /** Start a 1-shot identify session. */
    Q_INVOKABLE void startIdentify();

    /** Abort whatever is in progress. */
    Q_INVOKABLE void cancel();

    /** Push a camera frame in. QML calls this on every video frame.
     *  Returns true if the frame was used. */
    Q_INVOKABLE bool feedFrame(const QImage &img);

signals:
    void progressChanged();
    void busyChanged();

    void enrollSucceeded(const QString &userId);
    void enrollFailed   (const QString &reason);

    void matched(const QString &userId, const QString &name);
    void noMatch();

private:
    enum Mode { Idle, Enrolling, Identifying };

    void finishEnrollment();
    QString findBestMatch(const QVector<float> &emb, float &outScore) const;

    YoloRunner *m_yolo = nullptr;
    Database   *m_db   = nullptr;

    Mode    m_mode      = Idle;
    int     m_progress  = 0;
    QString m_enrollUserId;
    QString m_enrollName;
    QVector<QVector<float>> m_collected;

    static constexpr int   kEnrollFrames        = 5;
    static constexpr float kMatchThreshold      = 0.60f;     // cosine sim
    static constexpr int   kIdentifyTimeoutMs   = 3000;
    QTimer m_timeoutTimer;
};

#endif // FACE_SERVICE_H
