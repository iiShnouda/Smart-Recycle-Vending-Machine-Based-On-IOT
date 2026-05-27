#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QByteArray>
#include <atomic>

struct mosquitto;          // forward decl from libmosquitto

/**
 * MqttClient — the kiosk's IoT pipe.
 *
 * Thin Qt wrapper around libmosquitto. The kiosk publishes events
 * (transactions, faults, inventory changes, heartbeat) to a public
 * broker — HiveMQ Cloud, EMQX Serverless, or a self-hosted Mosquitto
 * on the Pi. A separate subscriber on your laptop / phone / dashboard
 * can listen in for telemetry.
 *
 *   Topic layout
 *   ────────────
 *     rewingo/<kiosk_id>/status         "online" / "offline"  (retained, LWT)
 *     rewingo/<kiosk_id>/heartbeat      "{\"uptime_ms\":1234,\"version\":\"0.2.0\"}"
 *     rewingo/<kiosk_id>/transaction    "{\"kind\":\"vending\",...}"
 *     rewingo/<kiosk_id>/fault          "{\"slot\":3,\"reason\":\"STALL\",...}"
 *     rewingo/<kiosk_id>/inventory      "{\"slot\":3,\"delta\":+20,...}"
 *     rewingo/<kiosk_id>/cmd            (subscribe — server pushes commands)
 *
 * Thread model
 * ────────────
 *   libmosquitto runs its own background thread via mosquitto_loop_start.
 *   Its callbacks fire on that thread. Anything that has to touch Qt
 *   objects gets hopped back to the Qt main thread via
 *   QMetaObject::invokeMethod(..., Qt::QueuedConnection).
 *
 * Lifecycle
 * ─────────
 *   configure() once with broker info from /etc/rewingo/.env.
 *   connectToBroker() returns immediately; `connected` signal fires
 *   when the TLS handshake + CONNACK finishes.
 *   On unexpected disconnect, libmosquitto auto-reconnects with the
 *   exponential backoff we configure in connectToBroker().
 */
class MqttClient : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(Mqtt)
    QML_SINGLETON

    Q_PROPERTY(bool connected READ isConnected NOTIFY connectionChanged)

public:
    explicit MqttClient(QObject *parent = nullptr);
    ~MqttClient() override;

    static MqttClient *create(QQmlEngine *, QJSEngine *) { return s_instance; }
    static MqttClient *s_instance;

    /** Set broker host/port/credentials. host is the only required field.
     *  Pass clientId to identify the kiosk uniquely (typically the
     *  Settings("kiosk/id") value). useTLS=true uses port 8883 + TLS. */
    void configure(const QString &host, int port,
                   const QString &username, const QString &password,
                   const QString &clientId, bool useTLS = true);

    /** Set the topic prefix all publishes go through. Typically
     *  "rewingo/<kiosk-id>". The /status, /transaction, /fault, … suffixes
     *  are appended automatically. */
    void setTopicBase(const QString &base);

    /** Open the connection. Idempotent; returns immediately even if
     *  the TCP/TLS handshake is still pending. Watch the
     *  `connectionChanged(true)` signal for completion.            */
    Q_INVOKABLE void connectToBroker();

    /** Tear down cleanly + send the LWT message manually.          */
    Q_INVOKABLE void disconnectFromBroker();

    bool isConnected() const { return m_connected.load(); }

public slots:
    /** Publish a payload to <topicBase>/<suffix>. qos: 0 fire-and-forget,
     *  1 at-least-once (default), 2 exactly-once. retain: broker keeps
     *  the last message so new subscribers see it on connect.       */
    Q_INVOKABLE void publish(const QString &suffix, const QString &payload,
                             int qos = 1, bool retain = false);

    /** Shorthand: publish a JSON object (serialised) to <topicBase>/<suffix>. */
    void publishJson(const QString &suffix, const QJsonObject &doc,
                     int qos = 1, bool retain = false);

signals:
    void connectionChanged(bool connected);
    /** A message arrived on a subscribed topic. Use for /cmd handling. */
    void messageReceived(const QString &topic, const QString &payload);
    void error(const QString &reason);

private:
    /* libmosquitto callback trampolines — must be plain C functions. */
    static void onConnectCb   (struct mosquitto *m, void *self, int rc);
    static void onDisconnectCb(struct mosquitto *m, void *self, int rc);
    static void onMessageCb   (struct mosquitto *m, void *self,
                               const struct mosquitto_message *msg);

    /* Marshalled-to-Qt-thread targets the callbacks fire. */
    Q_INVOKABLE void emitConnected(bool ok);
    Q_INVOKABLE void emitMessage(const QString &topic, const QString &payload);

    struct mosquitto    *m_handle = nullptr;
    QString              m_host;
    int                  m_port = 8883;
    QString              m_username;
    QString              m_password;
    QString              m_clientId;
    bool                 m_useTLS = true;
    QString              m_topicBase;
    std::atomic<bool>    m_connected { false };
};

#endif // MQTT_CLIENT_H
