#include "Serial_Connection.h"
#include <QDebug>

// =========================================================================
// Serial_Connection
//
// Lives on its own QThread. All public slots are invoked via queued
// connections from the GUI thread, so they execute on the worker thread.
// =========================================================================

Serial_Connection::Serial_Connection(QObject *parent)
    : QObject{parent}
{}

Serial_Connection::~Serial_Connection()
{
    closePort();
}

// ─── Public slots ──────────────────────────────────────────────────────────

void Serial_Connection::start(const QString &portName, int baud)
{
    m_isStarted = true;
    m_portName  = portName;
    m_baud      = (baud > 0) ? baud : 115200;

    // Resolve which VID:PID to scan for if this is an AUTO spec. Leaves
    // m_targetVid/Pid at the STM32 defaults for bare "AUTO" or a literal
    // device path (parseAutoSpec only overrides them for "AUTO:<vid>:<pid>").
    parseAutoSpec(m_portName);

    // Lazily create the three timers (only on first start). All three live on
    // this object → they run on the worker thread's event loop, not the GUI's.
    if (!m_ackTimer) {
        m_ackTimer = new QTimer(this);
        m_ackTimer->setSingleShot(true);
        connect(m_ackTimer, &QTimer::timeout,
                this,       &Serial_Connection::onAckTimeout);
    }
    if (!m_watchdogTimer) {
        m_watchdogTimer = new QTimer(this);
        m_watchdogTimer->setInterval(kWatchdogPeriod);
        connect(m_watchdogTimer, &QTimer::timeout,
                this,            &Serial_Connection::onWatchdogTick);
    }
    if (!m_reconnectTimer) {
        m_reconnectTimer = new QTimer(this);
        m_reconnectTimer->setInterval(kReconnectPeriod);
        connect(m_reconnectTimer, &QTimer::timeout,
                this,             &Serial_Connection::onReconnectTick);
    }

    openPort();
}

void Serial_Connection::stop()
{
    m_isStarted = false;
    if (m_ackTimer)       m_ackTimer->stop();
    if (m_watchdogTimer)  m_watchdogTimer->stop();
    if (m_reconnectTimer) m_reconnectTimer->stop();
    m_outQueue.clear();
    m_pending = {};
    closePort();
}

void Serial_Connection::sendCommand(const QString &command, int timeoutMs)
{
    if (command.isEmpty()) return;
    if (timeoutMs < 50)  timeoutMs = 50;       // clamp to a sane minimum
    m_outQueue.enqueue({command, timeoutMs});

    // If nothing is in flight, kick off the next one immediately.
    if (m_pending.command.isEmpty()) {
        sendNextFromQueue();
    }
}

// ─── Connection management ─────────────────────────────────────────────────

bool Serial_Connection::parseAutoSpec(const QString &portName)
{
    // Reset the exclude-mode flag; re-set below only for "AUTO:OTHER".
    m_matchOther = false;

    // Bare "AUTO" (or a literal path, or empty) → keep current m_targetVid/Pid
    // (which start out as the STM32 defaults from the in-class initializer,
    // and otherwise just retain whatever was set on a previous start() call).
    if (!portName.startsWith("AUTO:", Qt::CaseInsensitive))
        return true;

    // "AUTO:OTHER" → match the first serial port that is NOT the STM32.
    // Robust when the second board's exact VID:PID is unknown (clone Arduinos
    // vary). findPortByVidPid() honours m_matchOther.
    if (portName.compare("AUTO:OTHER", Qt::CaseInsensitive) == 0) {
        m_matchOther = true;
        return true;
    }

    const QStringList parts = portName.split(':');
    if (parts.size() != 3) {
        qWarning() << "[Serial] Malformed AUTO spec" << portName
                   << "— expected AUTO:<VID>:<PID> in hex (e.g. AUTO:2341:0043)."
                      " Falling back to STM32 defaults" << STM32_VID << STM32_PID;
        m_targetVid = STM32_VID;
        m_targetPid = STM32_PID;
        return false;
    }

    bool vidOk = false, pidOk = false;
    const quint16 vid = parts.at(1).toUShort(&vidOk, 16);
    const quint16 pid = parts.at(2).toUShort(&pidOk, 16);
    if (!vidOk || !pidOk) {
        qWarning() << "[Serial] Could not parse hex VID:PID from" << portName
                   << "— falling back to STM32 defaults" << STM32_VID << STM32_PID;
        m_targetVid = STM32_VID;
        m_targetPid = STM32_PID;
        return false;
    }

    m_targetVid = vid;
    m_targetPid = pid;
    return true;
}

void Serial_Connection::openPort()
{
    if (m_serial && m_serial->isOpen()) return;

    // Logged-once guard: a missing board used to print the "retrying" line
    // every reconnect tick and flood the log. Now it's said once, then reset
    // when we actually connect.
    static bool s_warnedNoPort = false;

    // Lazily construct the QSerialPort. Wired only once per object lifetime.
    if (!m_serial) {
        m_serial = new QSerialPort(this);
        connect(m_serial, &QSerialPort::readyRead,
                this,     &Serial_Connection::onReadyRead);
        connect(m_serial, &QSerialPort::errorOccurred,
                this,     &Serial_Connection::onSerialError);
    }

    // Resolve which port to open: explicit name, or auto-detect by VID:PID
    // (m_targetVid/Pid — set in start()/parseAutoSpec(), defaults to the
    // STM32's IDs for bare "AUTO").
    QString port = m_portName;
    if (port.isEmpty() || port == "AUTO" || port.startsWith("AUTO:", Qt::CaseInsensitive)) {
        port = findPortByVidPid();
    }

    if (port.isEmpty()) {
        if (!s_warnedNoPort) {
            if (m_matchOther)
                qWarning() << "[Serial] No non-STM32 serial device detected "
                              "(AUTO:OTHER) — recycle Arduino not plugged in? "
                              "Retrying quietly until it appears.";
            else
                qWarning() << "[Serial] No device detected for VID:PID"
                           << Qt::hex << m_targetVid << ":" << m_targetPid << Qt::dec
                           << "— retrying quietly until it appears.";
            s_warnedNoPort = true;
        }
        m_reconnectTimer->start();
        return;
    }

    // Configure 115200 8N1 — the universal default.
    m_serial->setPortName(port);
    m_serial->setBaudRate(m_baud);
    m_serial->setDataBits(QSerialPort::Data8);
    m_serial->setParity(QSerialPort::NoParity);
    m_serial->setStopBits(QSerialPort::OneStop);
    m_serial->setFlowControl(QSerialPort::NoFlowControl);

    if (!m_serial->open(QIODevice::ReadWrite)) {
        qWarning() << "[Serial] Open failed:" << m_serial->errorString();
        emit errorOccurred(m_serial->errorString());
        m_reconnectTimer->start();
        return;
    }

    s_warnedNoPort = false;          // re-arm the once-only "no device" log
    qInfo() << "[Serial] Connected on" << port << "@" << m_baud;
    m_serial->clear();          // discard anything sitting in OS buffers
    m_rxBuffer.clear();
    m_pingsMissed = 0;
    m_reconnectTimer->stop();
    m_watchdogTimer->start();
    emit connected();

    // If commands queued up while disconnected, drain them now.
    if (m_pending.command.isEmpty() && !m_outQueue.isEmpty()) {
        sendNextFromQueue();
    }
}

void Serial_Connection::closePort()
{
    // Note: m_serial is a pointer — `m_serial && ...` checks "not null AND ..."
    if (m_serial && m_serial->isOpen()) {
        m_serial->close();
    }
    if (m_watchdogTimer) m_watchdogTimer->stop();
    if (m_ackTimer)      m_ackTimer->stop();
}

void Serial_Connection::teardownAfterFailure()
{
    // Hard reset of the port object. Used when the link is dead beyond repair
    // (cable yanked, watchdog timeout, ResourceError). After this, the
    // reconnect timer keeps trying every kReconnectPeriod ms until it works.
    closePort();
    if (m_serial) {
        m_serial->deleteLater();
        m_serial = nullptr;
    }
    m_pending = {};                          // drop the in-flight command
    emit disconnected();
    if (m_isStarted) m_reconnectTimer->start();
}

QString Serial_Connection::findPortByVidPid() const
{
    // Walk all available ports and pick the one whose USB descriptor matches
    // m_targetVid/m_targetPid (STM32 by default, or whatever board start()
    // was given via "AUTO:<vid>:<pid>"). Lets the user move the cable around
    // without hard-coding "COM5" or "/dev/ttyACM0".
    //
    // "AUTO:OTHER" mode (m_matchOther): pick the first port that HAS a USB
    // descriptor and is NOT the STM32. This is how the recycle Arduino is
    // found — its exact VID:PID is unknown (clones differ), but "the serial
    // device that isn't the STM32" uniquely identifies it on this Pi, which
    // only ever has the two boards. Ports with no descriptor (e.g. the Pi's
    // own GPIO UART /dev/ttyAMA0, or a phantom ttyACM with no enumeration)
    // are skipped so we never grab a non-board tty by accident.
    for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts()) {
        if (!info.hasVendorIdentifier() || !info.hasProductIdentifier())
            continue;
        const quint16 vid = info.vendorIdentifier();
        const quint16 pid = info.productIdentifier();

        if (m_matchOther) {
            // Skip the STM32; the first remaining USB-CDC board wins.
            if (vid == STM32_VID && pid == STM32_PID)
                continue;
            qInfo() << "[Serial] AUTO:OTHER matched" << info.portName()
                    << "VID:PID" << Qt::hex << vid << ":" << pid << Qt::dec;
            return info.portName();
        }

        if (vid == m_targetVid && pid == m_targetPid)
            return info.portName();
    }
    return {};
}

// ─── Send queue ────────────────────────────────────────────────────────────

void Serial_Connection::sendNextFromQueue()
{
    if (m_outQueue.isEmpty()) return;
    if (!m_serial || !m_serial->isOpen()) {
        qWarning() << "[Serial] Cannot send, port not open. Will retry after reconnect.";
        return;
    }
    const OutCommand next = m_outQueue.dequeue();
    m_pending.command   = next.text;
    m_pending.attempts  = 1;
    m_pending.timeoutMs = next.timeoutMs;

    writeRaw(m_pending.command.toUtf8() + '\n');

    // Arm the per-command ACK timer with this command's timeout.
    m_ackTimer->start(m_pending.timeoutMs);
}

void Serial_Connection::writeRaw(const QByteArray &bytes)
{
    qDebug() << "[Serial] TX:" << bytes.trimmed();
    const qint64 n = m_serial->write(bytes);
    if (n != bytes.size()) {
        qWarning() << "[Serial] Short write:" << n << "of" << bytes.size()
        << m_serial->errorString();
    }
    m_serial->flush();         // push to OS now (don't wait for event loop)
}

// ─── Receive ───────────────────────────────────────────────────────────────

void Serial_Connection::onReadyRead()
{
    // Append everything new to the rolling buffer.
    m_rxBuffer.append(m_serial->readAll());

    // Pull out as many complete lines as are buffered.
    while (m_rxBuffer.contains('\n')) {
        const int idx = m_rxBuffer.indexOf('\n');
        const QByteArray line = m_rxBuffer.left(idx).trimmed();
        m_rxBuffer.remove(0, idx + 1);
        if (line.isEmpty()) continue;

        const QString reply = QString::fromUtf8(line);
        qDebug() << "[Serial] RX:" << reply;

        // Any traffic from the board means the link is healthy → reset
        // the missed-ping counter so the watchdog won't trigger.
        m_pingsMissed = 0;

        // Try to resolve the in-flight command if this looks like an ACK.
        if (!m_pending.command.isEmpty()) {
            const bool isDispense = m_pending.command.startsWith("DISPENSE", Qt::CaseInsensitive);
            const bool isStepOk   = reply.startsWith("Done STEP", Qt::CaseInsensitive);
            
            const bool ok  = (reply.startsWith("Done", Qt::CaseInsensitive) && !(isDispense && isStepOk))
                          || reply.startsWith("OK",   Qt::CaseInsensitive)
                          || reply.startsWith("PONG", Qt::CaseInsensitive);
            const bool err = reply.startsWith("Error", Qt::CaseInsensitive);

            if (ok || err) {
                m_ackTimer->stop();
                if (ok) emit commandSucceeded(m_pending.command, reply);
                else    emit commandFailed   (m_pending.command, reply);
                m_pending = {};
                sendNextFromQueue();
            }
            // Anything else is treated as an unsolicited message (sensor
            // event, log line, etc.) and only emitted via replyReceived().
        }

        emit replyReceived(reply);
    }
}

// ─── Timeouts & watchdog ───────────────────────────────────────────────────

void Serial_Connection::onAckTimeout()
{
    if (m_pending.command.isEmpty()) return;

    // Try again, up to kMaxRetries total attempts.
    if (m_pending.attempts < kMaxRetries) {
        m_pending.attempts++;
        qWarning() << "[Serial] No ack — retry" << m_pending.attempts
                   << "of" << kMaxRetries << ":" << m_pending.command;
        if (m_serial && m_serial->isOpen()) {
            writeRaw(m_pending.command.toUtf8() + '\n');
            m_ackTimer->start(m_pending.timeoutMs);
        } else {
            // Port died mid-retry → bail out, watchdog/reconnect handles recovery
            emit commandFailed(m_pending.command, "port closed");
            m_pending = {};
        }
    } else {
        qWarning() << "[Serial] Command failed after" << kMaxRetries
                   << "tries:" << m_pending.command;
        emit commandFailed(m_pending.command, "timeout");
        m_pending = {};
        sendNextFromQueue();
    }
}

void Serial_Connection::onWatchdogTick()
{
    if (!m_serial || !m_serial->isOpen()) return;

    // If we've sent kMaxMissedPings PINGs without any reply, the link is dead.
    if (m_pingsMissed >= kMaxMissedPings) {
        qWarning() << "[Serial] Watchdog: link dead, reconnecting...";
        emit errorOccurred("watchdog timeout");
        teardownAfterFailure();
        return;
    }

    m_pingsMissed++;
    // Queue PING with the default (fast) timeout. Any reply from the board
    // resets m_pingsMissed in onReadyRead(), so even unsolicited traffic
    // proves the link is alive.
    m_outQueue.enqueue({QStringLiteral("PING"), kDefaultAckMs});
    if (m_pending.command.isEmpty()) sendNextFromQueue();
}

void Serial_Connection::onReconnectTick()
{
    if (!m_isStarted) {
        m_reconnectTimer->stop();
        return;
    }
    qInfo() << "[Serial] Trying to reconnect...";
    openPort();
}

void Serial_Connection::onSerialError(QSerialPort::SerialPortError err)
{
    if (err == QSerialPort::NoError) return;
    qWarning() << "[Serial] Error:" << err
               << (m_serial ? m_serial->errorString() : QString());
    if (m_serial) emit errorOccurred(m_serial->errorString());

    // ResourceError = the device disappeared (cable yank). Fully tear down.
    if (err == QSerialPort::ResourceError) {
        teardownAfterFailure();
    }
}