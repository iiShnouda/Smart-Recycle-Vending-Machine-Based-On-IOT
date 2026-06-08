#ifndef MACHINE_LINK_H
#define MACHINE_LINK_H

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QPointer>

class QProcess;

/**
 * MachineLink — Discord-style QR login.
 *
 * A small Python sidecar holds a WebSocket to the backend's /iot endpoint
 * (the OS resolver works there where Qt's network stack can't resolve the
 * host). We read the sidecar's stdout for the user the phone app linked, and
 * render the QR via curl. Exposed to QML as the `MachineLink` singleton.
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

    /** machineId = this kiosk's id; wsBase e.g. wss://host/iot (query added). */
    void configure(const QString &machineId, const QString &wsBase);
    void start();                       // launch the link sidecar

    bool    connected()   const { return m_connected; }
    QString qrImagePath() const { return m_qrImagePath; }
    QString state()       const { return m_state; }

public slots:
    /** Mint a new login token, render its QR, and ensure the sidecar is up. */
    Q_INVOKABLE void beginQrSession();
    Q_INVOKABLE void cancel();

signals:
    void connectedChanged();
    void sessionChanged();
    void stateChanged();
    /** The backend relayed the user the phone scanned us with. */
    void loginReceived(const QString &userId, const QString &name, int points);

private slots:
    void onStdout();

private:
    void setState(const QString &s);
    void setConnected(bool c);
    void renderQr(const QString &payload);
    QString resolvePython() const;
    QString resolveScriptDir() const;

    QPointer<QProcess> m_proc;
    QString  m_stdoutBuf;
    QString  m_machineId;
    QString  m_wsBase;
    QString  m_token;
    QString  m_qrImagePath;
    QString  m_state = QStringLiteral("idle");
    bool     m_connected = false;
};

#endif // MACHINE_LINK_H
