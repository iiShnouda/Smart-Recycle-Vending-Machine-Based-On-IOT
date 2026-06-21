#include "../include/machine_link.h"
#include "../include/mqtt_client.h"
#include "../include/logger.h"

#include <QProcess>
#include <QSettings>
#include <QUrl>
#include <QUuid>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTimer>

#include <functional>
#include <memory>

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
    if (!machineId.isEmpty())
        m_machineId = machineId;
    m_configured = true;

    // Hook onto the shared MQTT client. The backend publishes the linked
    // user to rewingo/<machineId>/login, which MqttClient subscribes to on
    // connect. libmosquitto uses the OS resolver, so it works on the kiosk
    // networks where Qt's own network stack can't resolve the broker host.
    if (MqttClient *mq = MqttClient::s_instance) {
        connect(mq, &MqttClient::messageReceived, this,
                [this](const QString &topic, const QString &payload) {
            if (topic.endsWith(QStringLiteral("/login")))
                handleLogin(payload);
            else if (topic.endsWith(QStringLiteral("/verify_result")))
                handleVerifyResult(payload);
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
    // Resilience: if configure() never reached THIS (QML-singleton) instance,
    // the QR id is empty ("REWINGO::<token>" → the phone rejects it as
    // "invalid / out of date") AND the MQTT login hookup is missing (so the
    // phone's sign-in never reaches us). Configure now from the persisted id
    // so the instance QML actually drives is fully wired.
    if (!m_configured) {
        QSettings s;
        configure(s.value(QStringLiteral("kiosk/id")).toString());
        Logger::warn("MachineLink",
                     "self-configured at QR time (configure() never reached "
                     "this instance) — machineId=" + m_machineId);
    }

    m_sessionActive = true;

    // Final guard: the QR MUST carry the kiosk id.
    if (m_machineId.isEmpty()) {
        QSettings s;
        m_machineId = s.value(QStringLiteral("kiosk/id")).toString();
    }

    // Discord-style: a fresh random token every time the QR screen opens, so
    // each QR is unique and single-use. The phone scans
    // "REWINGO:<machineId>:<token>" and echoes the token back; we only sign in
    // if it matches this session (a stale/old QR can't log anyone in).
    m_token = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
    renderQr(QStringLiteral("REWINGO:") + m_machineId + ":" + m_token);
    setState(m_connected ? QStringLiteral("waiting") : QStringLiteral("idle"));
    emit sessionChanged();
}

void MachineLink::beginClaim(const QString &name, const QString &phone)
{
    // One-time claim token. The phone app scans "REWINGO-CLAIM:<token>" and the
    // backend (which we tell about the pending account now) auto-links it to the
    // logged-in app user — no password.
    m_claimToken = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);

    if (MqttClient *mq = MqttClient::s_instance) {
        QJsonObject o;
        o.insert(QStringLiteral("name"),  name);
        o.insert(QStringLiteral("phone"), phone);
        o.insert(QStringLiteral("token"), m_claimToken);
        mq->publishJson(QStringLiteral("register"), o, /*qos*/ 1, /*retain*/ false);
        Logger::audit("MachineLink", "pending account published for claim",
                      { {"machineId", m_machineId}, {"token", m_claimToken} });
    } else {
        Logger::warn("MachineLink", "beginClaim: no MqttClient — can't register pending account");
    }

    // Reuse the QR renderer (writes qrImagePath). The claim screen and the login
    // screen are never shown at the same time, so sharing the path is fine.
    renderQr(QStringLiteral("REWINGO-CLAIM:") + m_claimToken);
    emit claimChanged();
}

void MachineLink::verifyMobile(const QString &phone)
{
    if (!m_configured) {
        QSettings s;
        configure(s.value(QStringLiteral("kiosk/id")).toString());
    }
    MqttClient *mq = MqttClient::s_instance;
    if (!mq || !mq->isConnected()) {
        // Offline → can't confirm the account exists. Fail closed (treat as not
        // verified) so we never enrol a face against an unverifiable number.
        Logger::warn("MachineLink", "verifyMobile: MQTT offline — cannot verify");
        emit mobileVerified(phone, false);
        return;
    }
    QJsonObject o;
    o.insert(QStringLiteral("phone"), phone);
    mq->publishJson(QStringLiteral("verify"), o, /*qos*/ 1, /*retain*/ false);
    Logger::audit("MachineLink", "mobile verify requested", { {"phone", phone} });
}

void MachineLink::handleVerifyResult(const QString &payload)
{
    const QJsonObject o = QJsonDocument::fromJson(payload.toUtf8()).object();
    const QString phone = o.value(QStringLiteral("phone")).toString();
    const bool exists   = o.value(QStringLiteral("exists")).toBool(false);
    Logger::audit("MachineLink", "mobile verify result",
                  { {"phone", phone}, {"exists", exists} });
    emit mobileVerified(phone, exists);
}

void MachineLink::cancel()
{
    m_sessionActive = false;
    m_token.clear();
    setState(QStringLiteral("idle"));
}

void MachineLink::handleLogin(const QString &payload)
{
    // Log every login the broker delivers, with the gate state, so a failed
    // sign-in is diagnosable from the kiosk log instead of failing silently.
    Logger::info("MachineLink",
                 QString("login received (sessionActive=%1, haveToken=%2): %3")
                     .arg(m_sessionActive)
                     .arg(!m_token.isEmpty())
                     .arg(payload));

    if (!m_sessionActive) {                // only while the QR screen is up
        Logger::warn("MachineLink",
                     "login ignored — no active QR session (screen closed/timed out)");
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(payload.toUtf8());
    if (!doc.isObject()) {
        Logger::warn("MachineLink", "login ignored — payload is not JSON");
        return;
    }
    const QJsonObject o = doc.object();

    // Discord-style single-use token: ignore any login that doesn't carry
    // THIS session's token (an old/stale QR can't sign anyone in).
    const QString gotToken = o.value("token").toString();
    if (m_token.isEmpty() || gotToken != m_token) {
        Logger::warn("MachineLink",
                     QString("login ignored — token mismatch (got='%1' want='%2'). "
                             "Likely a stale QR was scanned.")
                         .arg(gotToken, m_token));
        return;
    }

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
    const QString out = m_qrImagePath;

    // Delete the previous session's QR FIRST. If the network hiccups and curl
    // can't fetch the new code, the screen must show a spinner — NEVER a stale
    // QR that still encodes an OLD token. A stale code makes the phone echo the
    // wrong token, which the kiosk silently rejects, so the user never lands on
    // the LCD. Removing it up front guarantees the displayed QR always matches
    // the current m_token.
    QFile::remove(out);
    emit sessionChanged();                  // QML shows the spinner meanwhile

    // Qt's network can't resolve hosts on some kiosk networks, so fetch the QR
    // PNG with curl (OS resolver) into a temp file; QML shows it.
    const QString url =
        QStringLiteral("https://api.qrserver.com/v1/create-qr-code/?size=360x360&margin=10&data=")
        + QString::fromUtf8(QUrl::toPercentEncoding(payload));

    // Retry a few times so a transient blip doesn't leave the QR screen blank.
    auto attempt = std::make_shared<std::function<void(int)>>();
    *attempt = [this, url, out, attempt](int tries) {
        auto *p = new QProcess(this);
        connect(p, &QProcess::finished, this,
                [this, p, out, attempt, tries](int code, QProcess::ExitStatus) {
            p->deleteLater();
            const QFileInfo fi(out);
            const bool ok = (code == 0 && fi.exists() && fi.size() > 100);
            if (ok) {
                emit sessionChanged();      // PNG ready → QML loads the new code
                *attempt = nullptr;         // break the self-referential cycle
            } else if (tries > 1) {
                QTimer::singleShot(700, this,
                                   [attempt, tries] { (*attempt)(tries - 1); });
            } else {
                Logger::warn("MachineLink",
                             "QR render failed after retries — screen shows a spinner");
                *attempt = nullptr;         // break the self-referential cycle
            }
        });
        p->start(QStringLiteral("curl"),
                 { QStringLiteral("-fsSL"), QStringLiteral("--max-time"),
                   QStringLiteral("15"), QStringLiteral("-o"), out, url });
    };
    (*attempt)(3);
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
