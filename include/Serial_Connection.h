#ifndef SERIAL_CONNECTION_H
#define SERIAL_CONNECTION_H

#include <QObject>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QByteArray>
#include <QQueue>
#include <QString>
#include <QTimer>

/**
 * Serial_Connection — talks to the STM32 over USB-CDC (virtual COM port).
 *
 * Lives on its own QThread so blocking calls (waitForReadyRead, etc.) NEVER
 * freeze the GUI. Communicates with the rest of the app via signals/slots.
 *
 * Reliability features:
 *   - Auto-reconnect every 2 s if the port is missing or dies.
 *   - Per-command ACK timeout (configurable: fast for sensors, long for motors).
 *   - Up to 3 retransmissions of a command if no reply.
 *   - Watchdog ping every 5 s; declares the link dead after 3 missed pings.
 *
 * Wire protocol (text, line-based, '\n' terminated):
 *   Pi  → STM32 :  "MOTOR_ON\n"      "STEP:1600:1\n"      "PING\n"
 *   STM32 → Pi  :  "Done\n"          "Error bad_param\n"  "PONG\n"
 *
 * Replies that start (case-insensitive) with "Done", "OK" or "PONG"
 * resolve the pending command as success.
 * Replies starting with "Error" resolve it as failure.
 * Anything else (e.g. unsolicited "PLASTIC", "CAN") is forwarded
 * via replyReceived() but does not consume the pending command.
 */
class Serial_Connection : public QObject
{
    Q_OBJECT
public:
    explicit Serial_Connection(QObject *parent = nullptr);
    ~Serial_Connection() override;

    // STM32 USB-CDC default IDs (STMicroelectronics / Virtual COM Port)
    static constexpr quint16 STM32_VID = 0x0483;
    static constexpr quint16 STM32_PID = 0x5740;

    // Default ACK window for "fast" commands (sensor reads, ping, LED, etc.)
    static constexpr int kDefaultAckMs = 500;
    // Comfortable window for slow commands (motor moves, weighing, etc.)
    static constexpr int kSlowAckMs    = 5000;

public slots:
    /** Open the port and start watchdog / reconnect timers.
     *  portName: a real port name ("COM5", "/dev/ttyACM0") or "AUTO" to
     *  auto-detect by USB VID:PID. */
    void start(const QString &portName, int baud);

    /** Stop everything (timers + port). Queued commands are dropped. */
    void stop();

    /** Queue a command for sending.
     *  Omit timeoutMs → uses kDefaultAckMs (500 ms) for fast commands.
     *  Pass a larger value for slow commands (motors, weighing). */
    void sendCommand(const QString &command, int timeoutMs = kDefaultAckMs);

signals:
    void connected();                                                   // port opened
    void disconnected();                                                // port closed (cable yank, watchdog, stop())
    void replyReceived   (const QString &reply);                        // every line received, ack or not
    void commandSucceeded(const QString &command, const QString &reply); // got Done/OK/PONG
    void commandFailed   (const QString &command, const QString &reason);// got Error / timed out / port died
    void errorOccurred   (const QString &message);                      // low-level QSerialPort error

private slots:
    void onReadyRead();
    void onSerialError(QSerialPort::SerialPortError err);
    void onAckTimeout();          // pending command got no reply in time
    void onWatchdogTick();        // periodic PING + dead-link detection
    void onReconnectTick();       // periodic re-open attempts when port is down

private:
    void openPort();
    void closePort();
    QString findPortByVidPid() const;
    void writeRaw(const QByteArray &bytes);
    void sendNextFromQueue();
    void teardownAfterFailure();

    // ---- runtime state ----
    QSerialPort *m_serial = nullptr;     // owned; null between failures and reconnects
    QByteArray   m_rxBuffer;             // accumulates partial lines until '\n'
    QString      m_portName;
    int          m_baud      = 115200;
    bool         m_isStarted = false;    // stop()/start() flag

    // One command in-flight at a time (waiting for STM32 reply)
    struct PendingCommand {
        QString command;
        int     attempts  = 0;
        int     timeoutMs = kDefaultAckMs;   // copied into m_ackTimer when sent
    };
    PendingCommand   m_pending;

    // FIFO of commands to send. Each entry is (text, perCommandTimeoutMs).
    struct OutCommand {
        QString text;
        int     timeoutMs;
    };
    QQueue<OutCommand> m_outQueue;

    QTimer *m_ackTimer       = nullptr;   // single-shot, per pending command
    QTimer *m_watchdogTimer  = nullptr;   // periodic PING
    QTimer *m_reconnectTimer = nullptr;   // periodic reopen attempts
    int     m_pingsMissed    = 0;

    // ---- tunables ----
    static constexpr int kMaxRetries      = 3;      // total send attempts per command
    static constexpr int kWatchdogPeriod  = 5000;   // ping every 5 s
    static constexpr int kMaxMissedPings  = 3;      // dead after 3 missed
    static constexpr int kReconnectPeriod = 2000;   // try to reopen every 2 s
};

#endif // SERIAL_CONNECTION_H
