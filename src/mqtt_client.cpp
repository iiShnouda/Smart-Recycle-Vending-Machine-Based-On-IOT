#include "../include/mqtt_client.h"
#include "../include/logger.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>

// HAVE_MOSQUITTO is defined by CMake only when libmosquitto-dev was found.
// Without it (e.g. a Windows dev build to run the UI) every method below
// becomes a no-op stub and telemetry is silently dropped — the kiosk
// still launches and works offline.
#ifdef HAVE_MOSQUITTO
#include <mosquitto.h>
#endif

MqttClient *MqttClient::s_instance = nullptr;

MqttClient::MqttClient(QObject *parent) : QObject(parent)
{
    if (!s_instance) s_instance = this;

#ifdef HAVE_MOSQUITTO
    // libmosquitto's process-wide init. Safe to call once at startup.
    mosquitto_lib_init();
#else
    Logger::warn("Mqtt",
                 "Built without libmosquitto — MQTT disabled, telemetry dropped");
#endif
}

MqttClient::~MqttClient()
{
#ifdef HAVE_MOSQUITTO
    if (m_handle) {
        mosquitto_loop_stop(m_handle, /*force*/ true);
        mosquitto_destroy(m_handle);
        m_handle = nullptr;
    }
    mosquitto_lib_cleanup();
#endif
}

// ── Configuration ─────────────────────────────────────────────────────────

void MqttClient::configure(const QString &host, int port,
                           const QString &username, const QString &password,
                           const QString &clientId, bool useTLS)
{
    m_host     = host;
    m_port     = port > 0 ? port : (useTLS ? 8883 : 1883);
    m_username = username;
    m_password = password;
    m_clientId = clientId;
    m_useTLS   = useTLS;
}

void MqttClient::setTopicBase(const QString &base)
{
    m_topicBase = base;
    while (m_topicBase.endsWith('/'))
        m_topicBase.chop(1);
}

// ── Connect / disconnect ──────────────────────────────────────────────────

void MqttClient::connectToBroker()
{
#ifndef HAVE_MOSQUITTO
    Logger::warn("Mqtt", "connectToBroker: built without libmosquitto — no-op");
    return;
#else
    if (m_host.isEmpty()) {
        Logger::warn("Mqtt", "No host configured; refusing to connect");
        return;
    }
    if (m_handle) {
        // Already connected (or in mid-flight). Disconnect first.
        mosquitto_loop_stop(m_handle, true);
        mosquitto_destroy(m_handle);
        m_handle = nullptr;
        m_connected.store(false);
    }

    // clean_session = true → no stored subs from a previous run.
    m_handle = mosquitto_new(
        m_clientId.isEmpty() ? nullptr : m_clientId.toUtf8().constData(),
        /*clean_session*/ true,
        this);
    if (!m_handle) {
        Logger::warn("Mqtt", "mosquitto_new failed");
        emit error("mosquitto_new failed");
        return;
    }

    // Trampoline callbacks — receive `self` as the user-data pointer.
    mosquitto_connect_callback_set   (m_handle, &MqttClient::onConnectCb);
    mosquitto_disconnect_callback_set(m_handle, &MqttClient::onDisconnectCb);
    mosquitto_message_callback_set   (m_handle, &MqttClient::onMessageCb);

    if (!m_username.isEmpty()) {
        mosquitto_username_pw_set(m_handle,
                                  m_username.toUtf8().constData(),
                                  m_password.toUtf8().constData());
    }

    // Last Will and Testament: if we vanish, broker publishes "offline" on
    // <base>/status retained. New subscribers immediately see the kiosk is
    // down.
    if (!m_topicBase.isEmpty()) {
        const QByteArray willTopic = (m_topicBase + "/status").toUtf8();
        const QByteArray willMsg   = "offline";
        mosquitto_will_set(m_handle, willTopic.constData(),
                           willMsg.size(), willMsg.constData(),
                           /*qos*/ 1, /*retain*/ true);
    }

    if (m_useTLS) {
        // System CA bundle (the same one curl uses). Good enough for
        // public brokers; for private CA, point this at your cert.
        mosquitto_tls_set(m_handle,
                          /*cafile*/ "/etc/ssl/certs/ca-certificates.crt",
                          /*capath*/ nullptr,
                          /*certfile*/ nullptr,
                          /*keyfile*/ nullptr,
                          /*pw_callback*/ nullptr);
        // Set the default TLS version negotiation — mosquitto 2.x default
        // already picks TLS 1.2+, no need to lock it down.
    }

    // Auto-reconnect on socket loss: between 1 s and 30 s, exponential.
    mosquitto_reconnect_delay_set(m_handle, /*min*/ 1, /*max*/ 30,
                                  /*exponential*/ true);

    Logger::info("Mqtt", QString("Connecting to %1:%2 (tls=%3) as %4")
                 .arg(m_host).arg(m_port).arg(m_useTLS).arg(m_clientId));

    const int rc = mosquitto_connect_async(
        m_handle, m_host.toUtf8().constData(), m_port,
        /*keepalive*/ 60);
    if (rc != MOSQ_ERR_SUCCESS) {
        Logger::warn("Mqtt", QString("connect_async failed: %1")
                     .arg(mosquitto_strerror(rc)));
        emit error(mosquitto_strerror(rc));
        mosquitto_destroy(m_handle);
        m_handle = nullptr;
        return;
    }

    // Spin up libmosquitto's own thread that handles socket I/O +
    // keepalives + reconnects. Saves us from running mosquitto_loop()
    // on a QTimer.
    mosquitto_loop_start(m_handle);
#endif // HAVE_MOSQUITTO
}

void MqttClient::disconnectFromBroker()
{
#ifdef HAVE_MOSQUITTO
    if (!m_handle) return;
    // Publish a clean "offline" before tearing down (overrides the LWT
    // which would fire only on dirty disconnect).
    if (!m_topicBase.isEmpty()) {
        const QByteArray topic = (m_topicBase + "/status").toUtf8();
        const QByteArray msg   = "offline";
        mosquitto_publish(m_handle, nullptr, topic.constData(),
                          msg.size(), msg.constData(), 1, /*retain*/ true);
    }
    mosquitto_disconnect(m_handle);
    mosquitto_loop_stop(m_handle, /*force*/ false);
    mosquitto_destroy(m_handle);
    m_handle = nullptr;
    m_connected.store(false);
#endif // HAVE_MOSQUITTO
}

// ── Publish ───────────────────────────────────────────────────────────────

void MqttClient::publish(const QString &suffix, const QString &payload,
                         int qos, bool retain)
{
#ifndef HAVE_MOSQUITTO
    Q_UNUSED(suffix); Q_UNUSED(payload); Q_UNUSED(qos); Q_UNUSED(retain);
    return;
#else
    if (!m_handle || !m_connected.load()) {
        // Drop messages while disconnected. We could buffer them, but
        // most telemetry is better lost than replayed minutes later.
        return;
    }
    const QString topic = m_topicBase + "/" + suffix;
    const QByteArray t  = topic.toUtf8();
    const QByteArray p  = payload.toUtf8();
    const int rc = mosquitto_publish(m_handle, nullptr, t.constData(),
                                     p.size(), p.constData(), qos, retain);
    if (rc != MOSQ_ERR_SUCCESS) {
        Logger::warn("Mqtt", QString("publish %1 failed: %2")
                     .arg(topic, mosquitto_strerror(rc)));
    }
#endif // HAVE_MOSQUITTO
}

void MqttClient::publishJson(const QString &suffix, const QJsonObject &doc,
                             int qos, bool retain)
{
    publish(suffix,
            QString::fromUtf8(QJsonDocument(doc).toJson(QJsonDocument::Compact)),
            qos, retain);
}

// ── libmosquitto → Qt callback trampolines ────────────────────────────────

void MqttClient::onConnectCb(struct mosquitto *m, void *self, int rc)
{
#ifndef HAVE_MOSQUITTO
    Q_UNUSED(m); Q_UNUSED(self); Q_UNUSED(rc);
#else
    auto *c = static_cast<MqttClient *>(self);
    const bool ok = (rc == 0);
    if (ok) {
        Logger::info("Mqtt", "Connected to broker");

        // On (re)connect: publish "online" retained on /status, subscribe
        // to /cmd. Both happen on the libmosquitto thread but only touch
        // libmosquitto state, so no Qt thread hop needed here.
        if (!c->m_topicBase.isEmpty()) {
            const QByteArray topic = (c->m_topicBase + "/status").toUtf8();
            const QByteArray msg   = "online";
            mosquitto_publish(m, nullptr, topic.constData(),
                              msg.size(), msg.constData(), 1, /*retain*/ true);

            const QByteArray cmdTopic = (c->m_topicBase + "/cmd").toUtf8();
            mosquitto_subscribe(m, nullptr, cmdTopic.constData(), 1);
        }
    } else {
        Logger::warn("Mqtt", QString("Connect failed (rc=%1: %2)")
                     .arg(rc).arg(mosquitto_connack_string(rc)));
    }
    c->m_connected.store(ok);
    // Hop back to Qt thread to emit the signal safely.
    QMetaObject::invokeMethod(c, "emitConnected", Qt::QueuedConnection,
                              Q_ARG(bool, ok));
#endif // HAVE_MOSQUITTO
}

void MqttClient::onDisconnectCb(struct mosquitto *, void *self, int rc)
{
#ifndef HAVE_MOSQUITTO
    Q_UNUSED(self); Q_UNUSED(rc);
#else
    auto *c = static_cast<MqttClient *>(self);
    Logger::info("Mqtt", QString("Disconnected (rc=%1)").arg(rc));
    c->m_connected.store(false);
    QMetaObject::invokeMethod(c, "emitConnected", Qt::QueuedConnection,
                              Q_ARG(bool, false));
    // libmosquitto auto-reconnects (mosquitto_reconnect_delay_set above);
    // we don't have to do anything here.
#endif
}

void MqttClient::onMessageCb(struct mosquitto *, void *self,
                             const struct mosquitto_message *msg)
{
#ifndef HAVE_MOSQUITTO
    Q_UNUSED(self); Q_UNUSED(msg);
#else
    if (!msg || !msg->topic) return;
    auto *c = static_cast<MqttClient *>(self);
    const QString topic = QString::fromUtf8(msg->topic);
    const QString payload = QString::fromUtf8(
        reinterpret_cast<const char *>(msg->payload),
        msg->payloadlen);
    QMetaObject::invokeMethod(c, "emitMessage", Qt::QueuedConnection,
                              Q_ARG(QString, topic),
                              Q_ARG(QString, payload));
#endif
}

// ── Qt-thread emitters ────────────────────────────────────────────────────

void MqttClient::emitConnected(bool ok)
{
    emit connectionChanged(ok);
}

void MqttClient::emitMessage(const QString &topic, const QString &payload)
{
    emit messageReceived(topic, payload);
}
