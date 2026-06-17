#ifndef ADMIN_AUTH_H
#define ADMIN_AUTH_H

#include <QObject>
#include <QString>
#include <QQmlEngine>
#include <QTimer>

class FaceRecSidecar;

/**
 * AdminAuth — gate that decides whether to unlock the admin panel.
 *
 * Flow:
 *   1. QML calls startScan() when the user enters AdminGatePage.
 *   2. State goes SCANNING → the REAL face recogniser (FaceRecSidecar) runs.
 *   3. On a match we read the face's ROLE from faces.db: only role=="admin"
 *      unlocks (so admin login == face login, restricted to admins). As a
 *      no-lock-out bootstrap, while NO admin exists yet any recognised user
 *      may enter (so the first person can promote themselves).
 *   4. Unknown/timeout/non-admin → REJECTED; 3 fails → LOCKED for kLockoutMs.
 *   5. QML watches `state` and `attemptsRemaining` to animate.
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
    // True when the current session got in via the no-admin bootstrap (this
    // face is NOT yet role=admin). The panel uses it to offer "make me admin".
    Q_PROPERTY(bool    isBootstrapAdmin    READ isBootstrapAdmin     NOTIFY adminIdChanged)

    explicit AdminAuth(QObject *parent = nullptr);

    static AdminAuth *create(QQmlEngine *, QJSEngine *) { return s_instance; }
    static AdminAuth *s_instance;

    State   state()             const { return m_state; }
    int     attemptsRemaining() const { return m_attemptsRemaining; }
    QString adminName()         const { return m_adminName; }
    QString adminId()           const { return m_adminId; }
    bool    isBootstrapAdmin()  const { return m_bootstrapEntry; }

    /** Wire the real face recogniser (called once from ApplicationManager). */
    void bindFace(FaceRecSidecar *face);

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
    void onScanTimeout();
    void onFaceIdentified(const QString &name, double score);
    void onFaceUnknown(double bestScore);
    void onFaceFailed(const QString &reason);

private:
    void accept(const QString &id, const QString &name);
    void reject(const char *why);

    void setState(State s);
    void setAdmin(const QString &id, const QString &name);

    FaceRecSidecar *m_face   = nullptr;
    State   m_state             = IDLE;
    int     m_attemptsRemaining = kMaxAttempts;
    bool    m_bootstrapEntry    = false;
    QString m_adminName;
    QString m_adminId;

    QTimer  m_scanTimer;     // now a TIMEOUT for the face sidecar
    QTimer  m_lockoutTimer;

    static constexpr int kMaxAttempts = 3;
    static constexpr int kScanMs      = 22000;    // face-sidecar timeout (ms)
    static constexpr int kLockoutMs   = 30000;    // 30 s lockout after 3 fails
};

#endif // ADMIN_AUTH_H
