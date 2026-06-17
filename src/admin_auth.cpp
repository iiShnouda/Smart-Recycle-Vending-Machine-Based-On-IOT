#include "../include/admin_auth.h"
#include "../include/face_rec_sidecar.h"
#include "../include/logger.h"

AdminAuth *AdminAuth::s_instance = nullptr;

AdminAuth::AdminAuth(QObject *parent) : QObject(parent)
{
    if (!s_instance) s_instance = this;

    // m_scanTimer is now a TIMEOUT: if the face sidecar doesn't answer in
    // kScanMs, treat the attempt as a failure so the UI never hangs.
    m_scanTimer.setSingleShot(true);
    m_scanTimer.setInterval(kScanMs);
    connect(&m_scanTimer, &QTimer::timeout, this, &AdminAuth::onScanTimeout);

    m_lockoutTimer.setSingleShot(true);
    m_lockoutTimer.setInterval(kLockoutMs);
    connect(&m_lockoutTimer, &QTimer::timeout, this, [this]() {
        m_attemptsRemaining = kMaxAttempts;
        emit attemptsChanged();
        setState(IDLE);
    });
}

void AdminAuth::bindFace(FaceRecSidecar *face)
{
    if (!face || m_face) return;
    m_face = face;
    connect(face, &FaceRecSidecar::identified, this, &AdminAuth::onFaceIdentified);
    connect(face, &FaceRecSidecar::unknown,    this, &AdminAuth::onFaceUnknown);
    connect(face, &FaceRecSidecar::failed,     this, &AdminAuth::onFaceFailed);
}

void AdminAuth::reset()
{
    m_scanTimer.stop();
    m_lockoutTimer.stop();
    m_attemptsRemaining = kMaxAttempts;
    emit attemptsChanged();
    setAdmin({}, {});
    setState(IDLE);
}

void AdminAuth::startScan()
{
    if (m_state == LOCKED) return;
    setState(SCANNING);
    Logger::info("AdminAuth", "Scan started",
                 { {"attempts_left", m_attemptsRemaining} });

    if (m_face) {
        m_face->identify();          // real recognition; result via signals
        m_scanTimer.start();         // safety timeout
    } else {
        // No recogniser wired — fail closed rather than open.
        Logger::error("AdminAuth", "No FaceRec bound — cannot verify admin");
        reject("face recogniser unavailable");
    }
}

void AdminAuth::cancelScan()
{
    m_scanTimer.stop();
    if (m_face) m_face->cancel();
    if (m_state == SCANNING) setState(IDLE);
}

void AdminAuth::logout()
{
    Logger::audit("AdminAuth", "Logout", { {"admin_id", m_adminId} });
    setAdmin({}, {});
    setState(IDLE);
}

void AdminAuth::onScanTimeout()
{
    if (m_state != SCANNING) return;
    if (m_face) m_face->cancel();
    reject("timeout — no face matched");
}

void AdminAuth::onFaceIdentified(const QString &name, double score)
{
    Q_UNUSED(score)
    if (m_state != SCANNING) return;     // ignore login-screen scans
    m_scanTimer.stop();

    // Admin login == face login, restricted to role=="admin". Bootstrap:
    // while NO admin exists yet, any recognised user may enter (so the first
    // person can promote themselves on the Admins screen).
    const QString role  = m_face ? m_face->lastRole()    : QStringLiteral("user");
    const int     uid   = m_face ? m_face->lastUserId()  : 0;
    const bool    boot  = m_face ? !m_face->adminsExist() : false;
    const bool    admin = (role == QLatin1String("admin")) || boot;

    if (admin) {
        // They got in, but flag if it was the bootstrap (not yet role=admin)
        // so the panel can offer to make them the permanent admin.
        m_bootstrapEntry = (role != QLatin1String("admin"));
        accept(QString::number(uid), name);
    } else {
        Logger::warn("AdminAuth", "Recognised but not an admin",
                     { {"user", name}, {"role", role} });
        reject("not an admin");
    }
}

void AdminAuth::onFaceUnknown(double bestScore)
{
    Q_UNUSED(bestScore)
    if (m_state != SCANNING) return;
    m_scanTimer.stop();
    reject("face not recognised");
}

void AdminAuth::onFaceFailed(const QString &reason)
{
    if (m_state != SCANNING) return;
    m_scanTimer.stop();
    reject(reason.toUtf8().constData());
}

void AdminAuth::accept(const QString &id, const QString &name)
{
    setAdmin(id, name);
    setState(ACCEPTED);
    Logger::audit("AdminAuth", "Admin unlocked",
                  { {"admin_id", m_adminId}, {"admin_name", m_adminName} });
    emit unlocked();
}

void AdminAuth::reject(const char *why)
{
    m_attemptsRemaining--;
    emit attemptsChanged();
    Logger::warn("AdminAuth", QString("Admin scan rejected: %1").arg(why),
                 { {"attempts_left", m_attemptsRemaining} });
    if (m_attemptsRemaining <= 0) {
        setState(LOCKED);
        Logger::error("AdminAuth", "Locked out — too many failed attempts");
        m_lockoutTimer.start();
    } else {
        setState(REJECTED);
    }
}

void AdminAuth::setState(State s)
{
    if (m_state == s) return;
    m_state = s;
    emit stateChanged();
}

void AdminAuth::setAdmin(const QString &id, const QString &name)
{
    if (m_adminId != id)    { m_adminId   = id;   emit adminIdChanged();   }
    if (m_adminName != name){ m_adminName = name; emit adminNameChanged(); }
}
