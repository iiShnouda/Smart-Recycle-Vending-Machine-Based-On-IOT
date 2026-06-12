#ifndef MACHINE_LINK_H
#define MACHINE_LINK_H

#include <QObject>
#include <QQmlEngine>
#include <QString>

class QProcess;

/**
 * MachineLink — QR sign-in for the kiosk.
 *
 * Shows a FIXED, machine-specific QR ("REWINGO:<machineId>") that the
 * ReWinGo phone app scans. The app tells the backend, which publishes the
 * linked user to rewingo/<machineId>/login over MQTT (HiveMQ Cloud). We
 * listen for that on the shared MqttClient and log the user in.
 *
 *   QML: MachineLink.beginQrSession(); show MachineLink.qrImagePath;
 *        onLoginReceived(userId,name,points) -> log in.
 */
class MachineLink : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(MachineLink)
    QML_SINGLETON

    Q_PROPERTY(bool    connected   READ connected   NOTIFY connectedChanged)
    Q_PROPERTY(QString qrImagePath READ qrImagePath NOTIFY sessionChanged)
    Q_PROPERTY(QString state       READ state       NOTIFY stateChanged)

public:
    explicit MachineLink(QObject *parent = nullptr);
    ~MachineLink() override;
    static MachineLink *create(QQmlEngine *, QJSEngine *) { return s_instance; }
    static MachineLink *s_instance;

    /** machineId = this kiosk's id. Hooks onto the shared MqttClient so the
     *  login event on rewingo/<machineId>/login drives loginReceived(). */
    void configure(const QString &machineId);

    bool    connected()   const { return m_connected; }
    QString qrImagePath() const { return m_qrImagePath; }
    QString state()       const { return m_state; }

public slots:
    /** Render the fixed QR + arm the login listener. */
    Q_INVOKABLE void beginQrSession();
    Q_INVOKABLE void cancel();

signals:
    void connectedChanged();
    void sessionChanged();
    void stateChanged();
    /** The backend relayed the user the phone scanned us with. */
    void loginReceived(const QString &userId, const QString &name, int points);

private:
    void setState(const QString &s);
    void setConnected(bool c);
    void renderQr(const QString &payload);
    void handleLogin(const QString &payload);

    QString m_machineId;
    QString m_token;            // per-session, single-use (Discord-style)
    QString m_qrImagePath;
    QString m_state         = QStringLiteral("idle");
    bool    m_connected     = false;
    bool    m_sessionActive = false;
};

#endif // MACHINE_LINK_H
