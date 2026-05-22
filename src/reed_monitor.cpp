#include "../include/reed_monitor.h"
#include "../include/logger.h"

#include <QFile>
#include <QTextStream>

ReedMonitor::ReedMonitor(QObject *parent) : QObject(parent)
{
    m_pollTimer.setInterval(50);                      // 20 Hz poll
    connect(&m_pollTimer, &QTimer::timeout, this, &ReedMonitor::poll);

    m_debounceTimer.setSingleShot(true);
    m_debounceTimer.setInterval(200);                 // 200 ms debounce
    connect(&m_debounceTimer, &QTimer::timeout, this, &ReedMonitor::onDebounceTimeout);
}

ReedMonitor::~ReedMonitor() { stop(); }

bool ReedMonitor::start(int gpioNumber, bool activeLow)
{
    m_gpio      = gpioNumber;
    m_activeLow = activeLow;

#ifdef Q_OS_LINUX
    if (!exportGpio(gpioNumber)) {
        Logger::warn("Reed", "GPIO export failed — running in dev mode",
                     { {"gpio", gpioNumber} });
    }
#else
    Logger::info("Reed", "Dev mode (no GPIO on this OS)");
#endif

    m_pollTimer.start();
    return true;
}

void ReedMonitor::stop()
{
    m_pollTimer.stop();
    m_debounceTimer.stop();
}

void ReedMonitor::rearm()
{
    m_armed = true;
    Logger::info("Reed", "Re-armed for next magnet-away");
}

void ReedMonitor::triggerForDev()
{
    // Force a magnet-away signal for desktop testing.
    if (!m_armed) return;
    m_armed = false;
    Logger::audit("Reed", "Admin requested (dev trigger)");
    emit adminRequested();
}

bool ReedMonitor::exportGpio(int gpio)
{
    QFile exportFile("/sys/class/gpio/export");
    if (exportFile.open(QIODevice::WriteOnly)) {
        QTextStream s(&exportFile);
        s << gpio;
        exportFile.close();
    } // else may already be exported — ignore

    const QString dirPath = QString("/sys/class/gpio/gpio%1/direction").arg(gpio);
    QFile dir(dirPath);
    if (!dir.open(QIODevice::WriteOnly)) return false;
    QTextStream s(&dir);
    s << "in";
    return true;
}

bool ReedMonitor::readGpio(int gpio, bool &outValue)
{
#ifdef Q_OS_LINUX
    const QString valPath = QString("/sys/class/gpio/gpio%1/value").arg(gpio);
    QFile f(valPath);
    if (!f.open(QIODevice::ReadOnly)) return false;
    const QByteArray b = f.readAll().trimmed();
    outValue = (b != "0");
    return true;
#else
    (void)gpio; (void)outValue;
    return false;
#endif
}

void ReedMonitor::poll()
{
    if (m_gpio < 0) return;

    bool raw;
    if (!readGpio(m_gpio, raw)) return;

    // Normalize: m_magnetPresent should be true when magnet is at the switch.
    const bool present = m_activeLow ? !raw : raw;

    if (present == m_pendingState) return;     // no change since last debounce

    m_pendingState = present;
    m_debounceTimer.start();                   // wait for stable state
}

void ReedMonitor::onDebounceTimeout()
{
    if (m_pendingState == m_magnetPresent) return;

    m_magnetPresent = m_pendingState;
    emit magnetStateChanged(m_magnetPresent);
    Logger::info("Reed", m_magnetPresent ? "magnet present" : "magnet away");

    if (!m_magnetPresent && m_armed) {
        m_armed = false;
        Logger::audit("Reed", "Admin requested (magnet removed)");
        emit adminRequested();
    }
}
