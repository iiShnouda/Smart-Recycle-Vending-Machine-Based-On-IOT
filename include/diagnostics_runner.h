#ifndef DIAGNOSTICS_RUNNER_H
#define DIAGNOSTICS_RUNNER_H

#include <QObject>
#include <QQmlEngine>
#include <QHash>
#include <QString>

/**
 * DiagnosticsRunner — fires STM32 commands and tracks pass/fail per channel.
 *
 * Exposed to QML as a singleton. Three test groups:
 *   1. PING/STATUS                           (link sanity)
 *   2. STEP:N:STEPS:DIR  for N in 1..8       (motors)
 *   3. WEIGH:N           for N in 1..8       (load cells)
 *   4. IR / RCWL / REED  (sensors)
 *
 * The page calls testMotor(n) / testCell(n) etc. as the user clicks.
 * Replies come back through ApplicationManager's serialCommandSucceeded /
 * serialCommandFailed signals — we listen and update result map.
 */
class DiagnosticsRunner : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(Diagnostics)
    QML_SINGLETON

    Q_PROPERTY(QVariantMap results READ results NOTIFY resultsChanged)
public:
    explicit DiagnosticsRunner(QObject *parent = nullptr);

    static DiagnosticsRunner *create(QQmlEngine *, QJSEngine *) { return s_instance; }
    static DiagnosticsRunner *s_instance;

    QVariantMap results() const { return m_results; }

public slots:
    Q_INVOKABLE void testPing();
    Q_INVOKABLE void testStatus();
    Q_INVOKABLE void testMotor(int slot);            // 1..8
    Q_INVOKABLE void testCell(int slot);             // 1..8
    Q_INVOKABLE void testServo(int us);              // 500..2500
    Q_INVOKABLE void testRunAll();                   // sweeps all of the above
    Q_INVOKABLE void clearResults();

    // Hooks from ApplicationManager
    void onCommandSucceeded(const QString &command, const QString &reply);
    void onCommandFailed   (const QString &command, const QString &reason);

signals:
    void resultsChanged();
    void runProgress(int done, int total);

private:
    void send(const QString &cmd, int timeoutMs);
    void record(const QString &cmd, bool ok, const QString &reply);

    QVariantMap m_results;   // command string → { ok: bool, reply: str, time: str }
};

#endif // DIAGNOSTICS_RUNNER_H
