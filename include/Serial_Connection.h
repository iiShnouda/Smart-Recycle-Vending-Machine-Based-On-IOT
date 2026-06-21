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
 * Serial_Connection — talks to a board over USB-CDC (virtual COM port).
 * Used for both the STM32 (vending) and the recycle Arduino — each gets
 * its own instance, on its own QThread, with its own target VID:PID.
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
 *   Pi  → board :  "MOTOR_ON\n"      "STEP:1600:1\n"      "PING\n"
 *   board → Pi  :  "Done\n"          "Error bad_param\n"  "PONG\n"
 *
 * Replies that start (case-insensitive) with "Done", "OK" or "PONG"
 * resolve the pending command as success.
 * Replies starting with "Error" resolve it as failure.
 * Anything else (e.g. unsolicited "PLASTIC", "CAN", "EVT,...") is forwarded
 * via replyReceived() but does not consume the pending command.
 */
class Serial_Connection : public QObject
{
    Q_OBJECT
public:
    explicit Serial_Connection(QObject *parent = nullptr);
    ~Serial_Connection() override;

    // STM32 USB-CDC default IDs (STMicroelectronics / Virtual COM Port).
    // Kept for back-compat with callers that still reference these directly;
    // start("AUTO", baud) below defaults to this same pair.
    static constexpr quint16 STM32_VID = 0x0483;
    static constexpr quint16 STM32_PID = 0x5740;

    // Default ACK window for "fast" commands (sensor reads, ping, LED, etc.)
    static constexpr int kDefaultAckMs = 500;
    // Comfortable window for slow commands (motor moves, weighing, etc.)
    static constexpr int kSlowAckMs    = 5000;

public slots:
    /** Open the port and start watchdog / reconnect timers.
     *  portName: a real port name ("COM5", "/dev/ttyACM0"), "AUTO" to
     *  auto-detect the STM32 by its default VID:PID (0483:5740), or
     *  "AUTO:<vid>:<pid>" (4-digit hex, e.g. "AUTO:2341:0043") to
     *  auto-detect a different USB-CDC device by its own VID:PID — used
     *  for the recycle Arduino, which is a separate physical board with
     *  its own descriptor. */
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
    /** Parses "AUTO:<vid>:<pid>" (hex) out of m_portName into m_targetVid/
     *  m_targetPid. Called once from start(); leaves m_targetVid/Pid at
     *  the STM32 defaults if portName is bare "AUTO" or isn't an
     *  "AUTO:..." spec at all (i.e. a literal device path). Returns false
     *  on a malformed "AUTO:..." spec (logs a warning, falls back to the
     *  STM32 defaults rather than refusing to start). */
    bool parseAutoSpec(const QString &portName);
    void writeRaw(const QByteArray &bytes);
    void sendNextFromQueue();
    void teardownAfterFailure();

    // ---- runtime state ----
    QSerialPort *m_serial = nullptr;     // owned; null between failures and reconnects
    QByteArray   m_rxBuffer;             // accumulates partial lines until '\n'
    QString      m_portName;
    int          m_baud      = 115200;
    bool         m_isStarted = false;    // stop()/start() flag

    // Which VID:PID findPortByVidPid() scans for when m_portName is an
    // "AUTO" spec. Defaults to the STM32's IDs; overridden by parseAutoSpec()
    // when m_portName is "AUTO:<vid>:<pid>" — e.g. the recycle Arduino
    // (2341:0043 for a genuine Uno R3; differs for clones — see ApplicationManager).
    quint16 m_targetVid = STM32_VID;
    quint16 m_targetPid = STM32_PID;

    // "AUTO:OTHER" mode — match the first USB-CDC port whose VID:PID is NOT
    // the STM32's. Used for the recycle Arduino when its exact VID:PID is
    // unknown (clone boards use CH340 1a86:7523, FTDI 0403:6001, etc.), so we
    // can't hard-code it: "the serial device that isn't the STM32" is robust
    // regardless of which Arduino/clone is plugged in. Set by parseAutoSpec().
    bool m_matchOther = false;

    // One command in-flight at a time (waiting for the board's reply)
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