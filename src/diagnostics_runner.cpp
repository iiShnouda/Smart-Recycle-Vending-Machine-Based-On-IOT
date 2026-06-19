#include "../include/diagnostics_runner.h"
#include "../include/applicationmanager.h"
#include "../include/logger.h"
#include <QDateTime>
#include <QVariantMap>
#include <QStringList>
DiagnosticsRunner *DiagnosticsRunner::s_instance = nullptr;
DiagnosticsRunner::DiagnosticsRunner(QObject *parent) : QObject(parent)
{
    if (!s_instance) s_instance = this;
}
void DiagnosticsRunner::send(const QString &cmd, int timeoutMs)
{
    auto *app = ApplicationManager::s_instance;
    if (!app) return;
    app->sendSerial(cmd, timeoutMs);
}
void DiagnosticsRunner::record(const QString &cmd, bool ok, const QString &reply)
{
    QVariantMap entry;
    entry["ok"]    = ok;
    entry["reply"] = reply;
    entry["time"]  = QDateTime::currentDateTime().toString("HH:mm:ss");
    m_results[cmd] = entry;
    emit resultsChanged();
    Logger::audit("Diag", cmd, { {"ok", ok}, {"reply", reply} });
}
void DiagnosticsRunner::onCommandSucceeded(const QString &command, const QString &reply)
{
    // WEIGH_ALL returns all 8 cells at once
    // ("Done WEIGH_ALL:c0,c1,...,c7"); fan it out so each per-cell row
    // ("WEIGH:1".."WEIGH:8") shows its own value.
    if (command == QLatin1String("WEIGH_ALL")) {
        // reply arrives as the part after "Done WEIGH_ALL:" — comma-separated
        // cell values, per h_weigh_all() in protocol.c.
        const QStringList parts = reply.split(',');
        for (int i = 0; i < parts.size() && i < 8; ++i)
            record(QStringLiteral("WEIGH:%1").arg(i + 1), true, parts.at(i).trimmed());
        return;
    }
    record(command, true, reply);
}
void DiagnosticsRunner::onCommandFailed(const QString &command, const QString &reason)
{
    record(command, false, reason);
}
void DiagnosticsRunner::testPing()           { send("PING", 500); }
void DiagnosticsRunner::testStatus()         { send("IR_STATE", 500); }   // IR sensor mask
void DiagnosticsRunner::testMotor(int slot)
{
    // Spin motor `slot` (1..8) one revolution via the real vend path: the
    // firmware selects that slot on the 595 mux and rotates the TMC2209.
    // protocol.c's dispatcher splits on ':' (Protocol_Dispatch / h_dispense),
    // not a space, so the command name and argument MUST be colon-joined.
    send(QString("DISPENSE:%1").arg(slot - 1), 15000);   // firmware slot is 0-based
}
void DiagnosticsRunner::testCell(int slot)
{
    Q_UNUSED(slot);
    // Firmware weighs all 8 cells at once via WEIGH_ALL (h_weigh_all in
    // protocol.c), replying "Done WEIGH_ALL:c0,...,c7". onCommandSucceeded
    // fans that reply out to each cell row. (Plain WEIGH only returns a
    // single cell and uses a different reply format — not what the fan-out
    // parser expects.)
    send(QStringLiteral("WEIGH_ALL"), 1500);
}
void DiagnosticsRunner::testServo(int deg)
{
    if (deg < 0)   deg = 0;
    if (deg > 180) deg = 180;
    // Degrees go through ANGLE (h_servo_angle, 0-180), not SERVO
    // (h_servo_pulse, which expects a 500-2500us pulse width and would
    // reject any value below 500 as out of range). Colon-joined to match
    // the dispatcher's ':' split.
    send(QString("ANGLE:%1").arg(deg), 800);
}
void DiagnosticsRunner::testRunAll()
{
    testPing();
    testStatus();          // IR mask
    testCell(1);           // a single WEIGH_ALL fills all 8 cell rows
    // Motors skipped here (slow + need 12 V); fire them individually.
}
void DiagnosticsRunner::clearResults()
{
    m_results.clear();
    emit resultsChanged();
}