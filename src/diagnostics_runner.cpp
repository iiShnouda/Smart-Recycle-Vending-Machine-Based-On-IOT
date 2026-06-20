#include "../include/diagnostics_runner.h"
#include "../include/applicationmanager.h"
#include "../include/logger.h"
#include <QDateTime>
#include <QVariantMap>
#include <QStringList>

DiagnosticsRunner *DiagnosticsRunner::s_instance = nullptr;

// One mechanical revolution of the TMC2209 in standalone 1/8 microstepping
// (MS1=MS2=GND): 200 full steps × 8 = 1600 step pulses (see
// STEPPER_STEPS_PER_REV, firmware). Must match the firmware or the test spins
// the wrong number of turns.
static constexpr int kStepsPerRev = 1600;

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
    // WEIGH_ALL returns all 8 cells in one reply ("Done WEIGH_ALL:c0,c1,...,c7").
    // Fan it out so each per-cell row ("WEIGH:1".."WEIGH:8") shows its own value.
    if (command == QLatin1String("WEIGH_ALL")) {
        QString body = reply;
        const int c = body.indexOf(QLatin1String("WEIGH_ALL:"));
        if (c >= 0) body = body.mid(c + 10);          // strip "Done WEIGH_ALL:"
        const QStringList parts = body.split(',');
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

// ── Wire-command builders (kept in one place so QML row keys match exactly) ──
QString DiagnosticsRunner::motorCmd(int slot)
{
    // STEP:<count>:<dir>:<motor> — the firmware selects the motor on the 595
    // mux, spins exactly one revolution, then de-energises (h_step + stepper
    // task). Replies "Done STEP". 1-based motor, matching the vend path.
    return QStringLiteral("STEP:%1:1:%2").arg(kStepsPerRev).arg(slot);
}

QString DiagnosticsRunner::relayCmd(int idx, bool on)
{
    return QStringLiteral("RELAY:%1:%2").arg(idx).arg(on ? 1 : 0);
}

// ── Tests ────────────────────────────────────────────────────────────────────
void DiagnosticsRunner::testPing() { send(QStringLiteral("PING"), 500); }

void DiagnosticsRunner::testIr()   { send(QStringLiteral("IR"), 800); }   // → "OK IR:0x1F"

void DiagnosticsRunner::testMotor(int slot)
{
    if (slot < 1 || slot > 8) return;
    send(motorCmd(slot), 15000);          // a full revolution takes a few seconds
}

void DiagnosticsRunner::testConveyor()
{
    // Recycle belt self-test: the firmware runs it ~2 s then replies
    // "Done CONVEYOR" (h_conveyor → Recycle_RequestConveyorTest).
    send(QStringLiteral("CONVEYOR"), 15000);
}

void DiagnosticsRunner::testCell(int slot)
{
    Q_UNUSED(slot);
    // One WEIGH_ALL fills all 8 cell rows (fanned out in onCommandSucceeded).
    send(QStringLiteral("WEIGH_ALL"), 1500);
}

void DiagnosticsRunner::testServo(int deg)
{
    if (deg < 0)   deg = 0;
    if (deg > 180) deg = 180;
    send(QStringLiteral("ANGLE:%1").arg(deg), 800);   // → "OK ANGLE:<deg>"
}

void DiagnosticsRunner::testRelay(int idx, bool on)
{
    if (idx < 1 || idx > 3) return;
    send(relayCmd(idx, on), 800);                     // → "OK RELAY:<idx>:<0|1>"
}

void DiagnosticsRunner::testDoor() { send(QStringLiteral("DOOR"), 500); }  // → "OK DOOR:open|closed"

void DiagnosticsRunner::testRunAll()
{
    // Only the quick, side-effect-free checks. Motors, conveyor, relays and
    // the servo are fired individually so the operator watches each one move.
    testPing();
    testIr();
    testCell(1);     // one WEIGH_ALL fills all 8 cell rows
    testDoor();
}

void DiagnosticsRunner::clearResults()
{
    m_results.clear();
    emit resultsChanged();
}
