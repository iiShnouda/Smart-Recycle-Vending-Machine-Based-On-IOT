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
    // WEIGH returns all 8 cells at once ("W,c0,...,c7"); fan it out so each
    // per-cell row ("WEIGH:1".."WEIGH:8") shows its own value.
    if (command == QLatin1String("WEIGH") && reply.startsWith('W')) {
        const QStringList parts = reply.split(',');
        for (int i = 1; i < parts.size() && i <= 8; ++i)
            record(QStringLiteral("WEIGH:%1").arg(i), true, parts.at(i).trimmed());
        return;
    }
    record(command, true, reply);
}

void DiagnosticsRunner::onCommandFailed(const QString &command, const QString &reason)
{
    record(command, false, reason);
}

void DiagnosticsRunner::testPing()           { send("PING", 500); }
void DiagnosticsRunner::testStatus()         { send("IR",   500); }   // IR sensor mask
void DiagnosticsRunner::testMotor(int slot)
{
    // Spin motor `slot` (1..8) one revolution via the real vend path: the
    // firmware selects that slot on the 595 mux and rotates the TMC2209.
    send(QString("DISPENSE %1").arg(slot - 1), 15000);   // firmware slot is 0-based
}
void DiagnosticsRunner::testCell(int slot)
{
    Q_UNUSED(slot);
    // Firmware weighs all 8 cells at once; onCommandSucceeded fans the reply
    // out to each cell row.
    send(QStringLiteral("WEIGH"), 1500);
}
void DiagnosticsRunner::testServo(int deg)
{
    if (deg < 0)   deg = 0;
    if (deg > 180) deg = 180;
    send(QString("SERVO %1").arg(deg), 800);   // firmware: SERVO <degrees>
}

void DiagnosticsRunner::testRunAll()
{
    testPing();
    testStatus();          // IR mask
    testCell(1);           // a single WEIGH fills all 8 cell rows
    // Motors skipped here (slow + need 12 V); fire them individually.
}

void DiagnosticsRunner::clearResults()
{
    m_results.clear();
    emit resultsChanged();
}
