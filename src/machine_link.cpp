#include "../include/machine_link.h"
#include "../include/logger.h"

#include <QProcess>
#include <QUuid>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>

MachineLink *MachineLink::s_instance = nullptr;

MachineLink::MachineLink(QObject *parent) : QObject(parent)
{
    if (!s_instance) s_instance = this;
}

MachineLink::~MachineLink()
{
    if (m_proc) { m_proc->kill(); m_proc->waitForFinished(300); }
    if (s_instance == this) s_instance = nullptr;
}

void MachineLink::configure(const QString &machineId, const QString &wsBase)
{
    m_machineId = machineId;
    m_wsBase    = wsBase;
}

QString MachineLink::resolvePython() const
{
#ifdef Q_OS_WIN
    return QStringLiteral("python");
#else
    if (QFileInfo::exists("/opt/face_rec/.venv/bin/python"))
        return QStringLiteral("/opt/face_rec/.venv/bin/python");
    return QStringLiteral("/usr/bin/python3");
#endif
}

QString MachineLink::resolveScriptDir() const
{
#ifdef Q_OS_WIN
    return QStringLiteral(".");
#else
    return QStringLiteral("/opt/face_rec/scripts");
#endif
}

void MachineLink::start()
{
    if (m_wsBase.isEmpty() || m_machineId.isEmpty()) return;
    if (m_proc && m_proc->state() != QProcess::NotRunning) return;

    const QString py     = resolvePython();
    const QString script = resolveScriptDir() + "/machine_link_sidecar.py";
    if (!QFileInfo::exists(py) || !QFileInfo::exists(script)) {
        Logger::warn("MachineLink",
                     QString("sidecar/python missing (py=%1 script=%2)").arg(py, script));
        return;
    }

    m_proc = new QProcess(this);
    m_proc->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_proc.data(), &QProcess::readyReadStandardOutput,
            this, &MachineLink::onStdout);
    connect(m_proc.data(), &QProcess::finished, this,
            [this](int, QProcess::ExitStatus) { setConnected(false); });

    m_proc->start(py, { QStringLiteral("-u"), script, m_wsBase, m_machineId });
    Logger::audit("MachineLink", "Started link sidecar",
                  { {"machineId", m_machineId}, {"ws", m_wsBase} });
}

void MachineLink::onStdout()
{
    if (!m_proc) return;
    m_stdoutBuf += QString::fromUtf8(m_proc->readAllStandardOutput());

    int nl;
    while ((nl = m_stdoutBuf.indexOf('\n')) >= 0) {
        const QString line = m_stdoutBuf.left(nl).trimmed();
        m_stdoutBuf.remove(0, nl + 1);
        if (line.isEmpty()) continue;

        const QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8());
        if (!doc.isObject()) { Logger::info("MachineLink", line); continue; }
        const QJsonObject o = doc.object();
        const QString     t = o.value("type").toString();

        if      (t == "connected")    setConnected(true);
        else if (t == "disconnected") setConnected(false);
        else if (t == "login") {
            if (m_token.isEmpty() || o.value("token").toString() != m_token)
                continue;                       // not our QR session
            const QJsonObject u = o.value("user").toObject();
            setState(QStringLiteral("linked"));
            emit loginReceived(u.value("id").toString(),
                               u.value("name").toString(),
                               u.value("points").toInt());
        }
        else if (t == "error" || t == "ws_error") {
            Logger::warn("MachineLink", o.value("msg").toString());
        }
    }
}

void MachineLink::beginQrSession()
{
    m_token = QUuid::createUuid().toString(QUuid::WithoutBraces);
    renderQr(QStringLiteral("REWINGO:") + m_machineId + ":" + m_token);
    setState(QStringLiteral("waiting"));
    start();                                    // make sure the sidecar is up
    emit sessionChanged();
}

void MachineLink::cancel()
{
    m_token.clear();
    setState(QStringLiteral("idle"));
}

void MachineLink::renderQr(const QString &payload)
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QDir().mkpath(dir);
    m_qrImagePath = dir + "/rewingo_qr.png";

    // Qt's network can't resolve hosts on some kiosk networks, so fetch the QR
    // PNG with curl (which uses the OS resolver) into a temp file; QML shows it.
    const QString url =
        QStringLiteral("https://api.qrserver.com/v1/create-qr-code/?size=360x360&margin=10&data=")
        + QString::fromUtf8(QUrl::toPercentEncoding(payload));

    auto *p = new QProcess(this);
    const QString out = m_qrImagePath;
    connect(p, &QProcess::finished, this,
            [this, p](int, QProcess::ExitStatus) {
        p->deleteLater();
        emit sessionChanged();                  // PNG ready → QML reloads it
    });
    p->start(QStringLiteral("curl"),
             { QStringLiteral("-fsSL"), QStringLiteral("--max-time"),
               QStringLiteral("15"), QStringLiteral("-o"), out, url });
}

void MachineLink::setState(const QString &s)
{
    if (m_state == s) return;
    m_state = s;
    emit stateChanged();
}

void MachineLink::setConnected(bool c)
{
    if (m_connected == c) return;
    m_connected = c;
    emit connectedChanged();
}
