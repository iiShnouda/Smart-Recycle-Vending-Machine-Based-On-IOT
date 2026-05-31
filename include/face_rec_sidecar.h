#ifndef FACE_REC_SIDECAR_H
#define FACE_REC_SIDECAR_H

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QPointer>
#include <QProcess>

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

public:
    explicit FaceRecSidecar(QObject *parent = nullptr);
    static FaceRecSidecar *create(QQmlEngine *, QJSEngine *) { return s_instance; }
    static FaceRecSidecar *s_instance;

    bool    isRunning() const;
    QString status()    const { return m_status; }

public slots:
    Q_INVOKABLE void identify();
    Q_INVOKABLE void enroll(const QString &name);
    Q_INVOKABLE void cancel();

signals:
    void runningChanged();
    void statusChanged();
    void identified(const QString &name, double score);
    void unknown(double bestScore);
    void enrolled(const QString &name);
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
};

#endif // FACE_REC_SIDECAR_H
