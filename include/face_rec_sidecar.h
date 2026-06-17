#ifndef FACE_REC_SIDECAR_H
#define FACE_REC_SIDECAR_H

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QPointer>
#include <QProcess>
#include <QImage>

/*
 * FaceRecSidecar — drives the FaceRec_project's Python pipeline.
 *
 * Liveness (MediaPipe FaceMesh) is python-only in practice, so the kiosk
 * spawns the existing scripts (scripts.recognize_user / scripts.enroll_user)
 * as a child process. The Python opens its own cv2 window for the camera +
 * liveness prompts; the kiosk reads stdout for the final result line and
 * emits Qt signals.
 *
 * Paths come from QSettings (faceRec/pythonExe + faceRec/projectDir) with
 * Windows defaults that point at the user's existing FaceRec_project setup;
 * on the Pi, fill these in once via `/etc/rewingo/.env` or the admin panel.
 *
 * QML use:
 *     import Recycle_Vending_Machine_LCD
 *     FaceRec.identify()
 *     onIdentified: (name, score) => ...
 *     onUnknown:    (score)       => ...
 *     onFailed:     (reason)      => ...
 */
class FaceRecSidecar : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(FaceRec)
    QML_SINGLETON

    Q_PROPERTY(bool    running READ isRunning NOTIFY runningChanged)
    Q_PROPERTY(QString status  READ status    NOTIFY statusChanged)
    // Liveness stage drives the on-screen prompt in FaceDetectionPage.
    //   ""           idle / not started
    //   "BLINK"      blink twice
    //   "TURN_RIGHT" turn head right
    //   "TURN_LEFT"  turn head left
    //   "RECOGNIZE"  hold still, ArcFace match running
    Q_PROPERTY(QString stage         READ stage         NOTIFY stageChanged)
    Q_PROPERTY(int     blinkCount    READ blinkCount    NOTIFY stageChanged)
    Q_PROPERTY(int     blinkRequired READ blinkRequired NOTIFY stageChanged)
    // Enrollment progress for the registration page: poses captured so far
    // (enrollCount) out of enrollTotal (3). Drives the "N / 3" label + arc.
    Q_PROPERTY(int     enrollCount   READ enrollCount   NOTIFY enrollProgressChanged)
    Q_PROPERTY(int     enrollTotal   READ enrollTotal   NOTIFY enrollProgressChanged)

public:
    explicit FaceRecSidecar(QObject *parent = nullptr);
    ~FaceRecSidecar() override;
    static FaceRecSidecar *create(QQmlEngine *, QJSEngine *) { return s_instance; }
    static FaceRecSidecar *s_instance;

    bool    isRunning()     const;
    QString status()        const { return m_status; }
    QString stage()         const { return m_stage; }
    int     blinkCount()    const { return m_blinkCount; }
    int     blinkRequired() const { return m_blinkRequired; }
    int     enrollCount()   const { return m_enrollCount; }
    int     enrollTotal()   const { return m_enrollTotal; }
    // Set by the last "identified" event — the matched user's role
    // ("admin"/"user"), their user_id, and whether ANY admin exists yet.
    // The admin gate reads these to decide whether to unlock.
    QString lastRole()      const { return m_lastRole; }
    int     lastUserId()    const { return m_lastUserId; }
    bool    adminsExist()   const { return m_adminsExist; }

public slots:
    Q_INVOKABLE void identify();
    Q_INVOKABLE void enroll(const QString &name);
    /** Name-after-face: set the real name/mobile on a just-enrolled face. */
    Q_INVOKABLE void finalizeUser(int userId, const QString &name,
                                  const QString &mobile);
    /** Set a user's role ("admin"/"user") in faces.db (one-shot sidecar). */
    Q_INVOKABLE void setRole(int userId, const QString &role);
    Q_INVOKABLE void cancel();
    /** Pump one BGR/RGB camera frame into the sidecar. The kiosk's
     *  Camera owns the device; this just JPEG-encodes the frame and
     *  pipes it to the running Python process. No-op if not running. */
    Q_INVOKABLE void feedFrame(const QImage &img);

signals:
    void runningChanged();
    void statusChanged();
    void stageChanged();
    void enrollProgressChanged();
    void identified(const QString &name, double score);
    void unknown(double bestScore);
    void enrolled(const QString &name, int userId);
    void finalized(int userId);
    void roleSet(int userId, const QString &role, bool ok);
    void failed(const QString &reason);

private slots:
    void onStdout();
    void onFinished(int exitCode, QProcess::ExitStatus status);
    void onErrorOccurred(QProcess::ProcessError err);

private:
    QString resolvePython() const;
    QString resolveProjectDir() const;
    void    startSidecar(const QStringList &args);
    void    setStatus(const QString &s);

    QPointer<QProcess> m_proc;
    QString            m_status;
    QString            m_currentMode;     // "identify" | "enroll"
    QString            m_pendingName;
    QString            m_stdoutBuf;
    QString            m_stage;           // current liveness stage
    int                m_blinkCount    = 0;
    int                m_blinkRequired = 2;
    int                m_enrollCount   = 0;   // poses captured during enrollment
    int                m_enrollTotal   = 3;   // poses required
    quint64            m_framesSent    = 0;   // diagnostic — frames pushed to Python
    QString            m_lastRole      = QStringLiteral("user"); // from "identified"
    int                m_lastUserId    = 0;
    bool               m_adminsExist   = false;
};

#endif // FACE_REC_SIDECAR_H
