#include "../include/idle_manager.h"
#include "../include/logger.h"

IdleManager *IdleManager::s_instance = nullptr;

IdleManager::IdleManager(QObject *parent) : QObject(parent)
{
    if (!s_instance) s_instance = this;
    m_timer.setSingleShot(true);
    m_timer.setInterval(m_timeoutMs);
    connect(&m_timer, &QTimer::timeout, this, [this]() {
        Logger::info("Idle", "Timeout — returning to sleep");
        emit timedOut();
    });
}

void IdleManager::setTimeoutMs(int ms)
{
    if (m_timeoutMs == ms || ms < 1000) return;
    m_timeoutMs = ms;
    m_timer.setInterval(ms);
    emit timeoutMsChanged();
}

void IdleManager::touch()
{
    emit touched();              // fire even in admin (idle disabled) so the
                                 // LED idle timer still sees activity
    if (!m_enabled) return;
    m_timer.start();
}

void IdleManager::disable()
{
    if (!m_enabled) return;
    m_enabled = false;
    m_timer.stop();
    emit enabledChanged();
}

void IdleManager::enable()
{
    if (m_enabled) return;
    m_enabled = true;
    m_timer.start();
    emit enabledChanged();
}
