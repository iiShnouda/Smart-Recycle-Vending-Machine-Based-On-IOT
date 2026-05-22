#include "../include/diagnostics_runner.h"
#include "../include/applicationmanager.h"
#include "../include/logger.h"

#include <QDateTime>
#include <QVariantMap>

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
    record(command, true, reply);
}

void DiagnosticsRunner::onCommandFailed(const QString &command, const QString &reason)
{
    record(command, false, reason);
}

void DiagnosticsRunner::testPing()           { send("PING",   500); }
void DiagnosticsRunner::testStatus()         { send("STATUS", 500); }
void DiagnosticsRunner::testMotor(int slot)
{
    // 200 microsteps × 8 = 1600 (1 rev at 1/8 microstep). Conservative.
    send(QString("STEP:%1:1600:0").arg(slot), 15000);
}
void DiagnosticsRunner::testCell(int slot)
{
    send(QString("WEIGH:%1").arg(slot), 1500);
}
void DiagnosticsRunner::testServo(int us)
{
    if (us < 500)  us = 500;
    if (us > 2500) us = 2500;
    send(QString("SERVO:%1").arg(us), 800);
}

void DiagnosticsRunner::testRunAll()
{
    testPing();
    testStatus();
    for (int s = 1; s <= 8; ++s) testCell(s);
    // Skip motors in "run all" — they take too long & need motor power.
    // Admin can fire them individually.
}

void DiagnosticsRunner::clearResults()
{
    m_results.clear();
    emit resultsChanged();
}
