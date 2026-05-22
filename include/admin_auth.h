#ifndef ADMIN_AUTH_H
#define ADMIN_AUTH_H

#include <QObject>
#include <QString>
#include <QQmlEngine>
#include <QTimer>

/**
 * AdminAuth — gate that decides whether to unlock the admin panel.
 *
 * Flow:
 *   1. QML calls startScan() when the user enters AdminGatePage.
 *   2. State goes SCANNING → after 2 s (or face-rec callback), goes to
 *      ACCEPTED or REJECTED.
 *   3. On REJECTED, attemptsRemaining decreases. After 3 fails → LOCKED
 *      for kLockoutMs ms.
 *   4. QML watches `state` and `attemptsRemaining` properties to animate.
 *
 * Face recognition is stubbed — replace verifyFromCamera() with your
 * real OpenCV / dlib / face_recognition pipeline. The state machine is
 * already correct; just plug a real verdict into the stub.
 */
class AdminAuth : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(AdminAuth)
    QML_SINGLETON

public:
    enum State { IDLE, SCANNING, ACCEPTED, REJECTED, LOCKED };
    Q_ENUM(State)

    Q_PROPERTY(State   state               READ state               NOTIFY stateChanged)
    Q_PROPERTY(int     attemptsRemaining   READ attemptsRemaining   NOTIFY attemptsChanged)
    Q_PROPERTY(QString adminName           READ adminName           NOTIFY adminNameChanged)
    Q_PROPERTY(QString adminId             READ adminId             NOTIFY adminIdChanged)

    explicit AdminAuth(QObject *parent = nullptr);

    static AdminAuth *create(QQmlEngine *, QJSEngine *) { return s_instance; }
    static AdminAuth *s_instance;

    State   state()             const { return m_state; }
    int     attemptsRemaining() const { return m_attemptsRemaining; }
    QString adminName()         const { return m_adminName; }
    QString adminId()           const { return m_adminId; }

public slots:
    /** Reset state to IDLE, ready for a new gate visit. */
    Q_INVOKABLE void reset();

    /** Begin one scan attempt. Auto-completes after ~2 s or as soon as
     *  verifyFromCamera() returns. */
    Q_INVOKABLE void startScan();

    /** Abandon the current scan and go IDLE. */
    Q_INVOKABLE void cancelScan();

    /** Called from the admin panel "Logout" button. Returns to IDLE.    */
    Q_INVOKABLE void logout();

signals:
    void stateChanged();
    void attemptsChanged();
    void adminNameChanged();
    void adminIdChanged();
    void unlocked();      // ACCEPTED transition — QML pushes AdminMainPage

private slots:
    void onScanComplete();

private:
    /** Stub. Replace with real recognition that returns:
     *   - true  → admin verified
     *   - false → no/unknown face
     *  Also sets m_adminId / m_adminName when matched. */
    bool verifyFromCamera();

    void setState(State s);
    void setAdmin(const QString &id, const QString &name);

    State   m_state             = IDLE;
    int     m_attemptsRemaining = kMaxAttempts;
    QString m_adminName;
    QString m_adminId;

    QTimer  m_scanTimer;
    QTimer  m_lockoutTimer;

    static constexpr int kMaxAttempts = 3;
    static constexpr int kScanMs      = 2000;     // simulated scan duration
    static constexpr int kLockoutMs   = 30000;    // 30 s lockout after 3 fails
};

#endif // ADMIN_AUTH_H
