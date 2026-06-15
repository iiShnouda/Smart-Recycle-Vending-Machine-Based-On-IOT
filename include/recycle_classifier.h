#ifndef RECYCLE_CLASSIFIER_H
#define RECYCLE_CLASSIFIER_H

#include <QObject>
#include <QProcess>
#include <QPointer>
#include <QTimer>

/*
 * RecycleClassifier — the camera "brain" for the recycle lane.
 *
 * When the STM32 reports EVT,CAMERA (an item is sitting at the camera),
 * RecycleSession emits cameraRequested(); we launch a HEADLESS Python
 * sidecar (recycle_classify.py) that opens the CSI camera, runs the recycle
 * YOLO model, and prints a JSON verdict. We forward that verdict back to the
 * STM32 (via RecycleSession::sendVerdict) as VERDICT BOTTLE|CAN|REJECT.
 *
 * No cv2 window — the kiosk owns the screen. If the sidecar can't classify
 * (no camera, crash, timeout) we default to REJECT so the lane always clears
 * and the item is handed back to the user instead of jamming.
 *
 * Paths (override via QSettings):
 *   recycle/pythonExe   python interpreter   (Pi default: ~/recycle_venv/bin/python)
 *   recycle/scriptPath  recycle_classify.py  (Pi default: ~/recycle_classify.py)
 */
class RecycleClassifier : public QObject {
    Q_OBJECT
public:
    explicit RecycleClassifier(QObject *parent = nullptr);
    ~RecycleClassifier() override;

    bool isRunning() const;

public slots:
    void classify();   // launch the sidecar; emits verdict() exactly once
    void cancel();

signals:
    void verdict(const QString &v);     // "BOTTLE" | "CAN" | "REJECT"
    void failed(const QString &msg);

private slots:
    void onStdout();
    void onFinished(int exitCode, QProcess::ExitStatus status);
    void onErrorOccurred(QProcess::ProcessError err);

private:
    QString resolvePython() const;
    QString resolveScript() const;
    void    emitVerdictOnce(const QString &v);

    QPointer<QProcess> m_proc;
    QString            m_buf;
    QTimer             m_timeout;
    bool               m_emitted = false;
};

#endif // RECYCLE_CLASSIFIER_H
