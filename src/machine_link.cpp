#include "../include/machine_link.h"
#include "../include/mqtt_client.h"
#include "../include/logger.h"

#include <QProcess>
#include <QUrl>
#include <QUuid>
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
    if (s_instance == this) s_instance = nullptr;
}

void MachineLink::configure(const QString &machineId)
{
    m_machineId = machineId;

    // Hook onto the shared MQTT client. The backend publishes the linked
    // user to rewingo/<machineId>/login, which MqttClient subscribes to on
    // connect. libmosquitto uses the OS resolver, so it works on the kiosk
    // networks where Qt's own network stack can't resolve the broker host.
    if (MqttClient *mq = MqttClient::s_instance) {
        connect(mq, &MqttClient::messageReceived, this,
                [this](const QString &topic, const QString &payload) {
            if (topic.endsWith(QStringLiteral("/login")))
                handleLogin(payload);
        });
        connect(mq, &MqttClient::connectionChanged, this,
                [this](bool ok) { setConnected(ok); });
        setConnected(mq->isConnected());
    } else {
        Logger::warn("MachineLink", "No MqttClient instance — QR login disabled");
    }
}

void MachineLink::beginQrSession()
{
    m_sessionActive = true;

    // Discord-style: a fresh random token every time the QR screen opens, so
    // each QR is unique and single-use. The phone scans
    // "REWINGO:<machineId>:<token>" and echoes the token back; we only sign in
    // if it matches this session (a stale/old QR can't log anyone in).
    m_token = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
    renderQr(QStringLiteral("REWINGO:") + m_machineId + ":" + m_token);
    setState(m_connected ? QStringLiteral("waiting") : QStringLiteral("idle"));
    emit sessionChanged();
}

void MachineLink::cancel()
{
    m_sessionActive = false;
    m_token.clear();
    setState(QStringLiteral("idle"));
}

void MachineLink::handleLogin(const QString &payload)
{
    if (!m_sessionActive) return;          // only while the QR screen is up

    const QJsonDocument doc = QJsonDocument::fromJson(payload.toUtf8());
    if (!doc.isObject()) return;
    const QJsonObject o = doc.object();

    // Discord-style single-use token: ignore any login that doesn't carry
    // THIS session's token (an old/stale QR can't sign anyone in).
    if (m_token.isEmpty() || o.value("token").toString() != m_token) return;

    // Accept either {"user":{...}} or a flat {id,name,points} payload.
    const QJsonObject u = o.contains(QStringLiteral("user"))
                              ? o.value("user").toObject() : o;
    const QString id    = u.value("id").toString(u.value("userId").toString());
    const QString name  = u.value("name").toString();
    const int     points = u.value("points").toInt();
    if (id.isEmpty() && name.isEmpty()) return;

    setState(QStringLiteral("linked"));
    Logger::audit("MachineLink", "QR login received",
                  { {"machineId", m_machineId}, {"user", id} });
    emit loginReceived(id, name, points);
}

void MachineLink::renderQr(const QString &payload)
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QDir().mkpath(dir);
    m_qrImagePath = dir + "/rewingo_qr.png";

    // Qt's network can't resolve hosts on some kiosk networks, so fetch the QR
    // PNG with curl (OS resolver) into a temp file; QML shows it. The payload
    // is fixed, so this renders the same code every time.
    const QString url =
        QStringLiteral("https://api.qrserver.com/v1/create-qr-code/?size=360x360&margin=10&data=")
        + QString::fromUtf8(QUrl::toPercentEncoding(payload));

    auto *p = new QProcess(this);
    const QString out = m_qrImagePath;
    connect(p, &QProcess::finished, this,
            [this, p](int, QProcess::ExitStatus) {
        p->deleteLater();
        emit sessionChanged();             // PNG ready → QML reloads it
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
