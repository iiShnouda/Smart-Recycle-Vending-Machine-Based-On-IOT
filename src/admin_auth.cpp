#include "../include/admin_auth.h"
#include "../include/logger.h"

AdminAuth *AdminAuth::s_instance = nullptr;

AdminAuth::AdminAuth(QObject *parent) : QObject(parent)
{
    if (!s_instance) s_instance = this;

    m_scanTimer.setSingleShot(true);
    m_scanTimer.setInterval(kScanMs);
    connect(&m_scanTimer, &QTimer::timeout, this, &AdminAuth::onScanComplete);

    m_lockoutTimer.setSingleShot(true);
    m_lockoutTimer.setInterval(kLockoutMs);
    connect(&m_lockoutTimer, &QTimer::timeout, this, [this]() {
        m_attemptsRemaining = kMaxAttempts;
        emit attemptsChanged();
        setState(IDLE);
    });
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
    m_scanTimer.start();
    Logger::info("AdminAuth", "Scan started",
                 { {"attempts_left", m_attemptsRemaining} });
}

void AdminAuth::cancelScan()
{
    m_scanTimer.stop();
    if (m_state == SCANNING) setState(IDLE);
}

void AdminAuth::logout()
{
    Logger::audit("AdminAuth", "Logout", { {"admin_id", m_adminId} });
    setAdmin({}, {});
    setState(IDLE);
}

void AdminAuth::onScanComplete()
{
    const bool ok = verifyFromCamera();
    if (ok) {
        setState(ACCEPTED);
        Logger::audit("AdminAuth", "Admin unlocked",
                      { {"admin_id", m_adminId},
                        {"admin_name", m_adminName} });
        emit unlocked();
    } else {
        m_attemptsRemaining--;
        emit attemptsChanged();
        Logger::warn("AdminAuth", "Face mismatch",
                     { {"attempts_left", m_attemptsRemaining} });
        if (m_attemptsRemaining <= 0) {
            setState(LOCKED);
            Logger::error("AdminAuth", "Locked out — too many failed attempts");
            m_lockoutTimer.start();
        } else {
            setState(REJECTED);
        }
    }
}

bool AdminAuth::verifyFromCamera()
{
    // -------------------------------------------------------------------
    // STUB. Replace with your actual face-recognition pipeline.
    //
    // Recommended path (Linux/Pi):
    //   1. Capture a frame from /dev/video0 (libcamera + Qt 6.5+ MediaSource
    //      or `cv::VideoCapture`).
    //   2. Run a detector (Haar cascade or MTCNN) to find the face.
    //   3. Compute embedding (face_recognition / dlib / Insightface).
    //   4. Compare to stored admin embeddings (cosine similarity < 0.6).
    //   5. Return true with the matched admin's id+name; false otherwise.
    //
    // For now: in development mode we always return true with a fake admin.
    // -------------------------------------------------------------------
    setAdmin("admin-001", "Shnouda");
    return true;
}

void AdminAuth::setState(State s)
{
    if (m_state == s) return;
    m_state = s;
    emit stateChanged();
}

void AdminAuth::setAdmin(const QString &id, const QString &name)
{
    if (m_adminId != id)   { m_adminId   = id;   emit adminIdChanged();   }
    if (m_adminName != name){m_adminName = name; emit adminNameChanged(); }
}
