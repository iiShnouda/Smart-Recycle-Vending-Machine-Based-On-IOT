#include "../include/recycle_session.h"
#include "../include/logger.h"

#include <QSettings>
#include <QTimer>
#include <QStringList>

RecycleSession *RecycleSession::s_instance = nullptr;

RecycleSession::RecycleSession(QObject *parent) : QObject(parent)
{
    if (!s_instance) s_instance = this;

    // Point values are admin-configurable + persisted.
    //   small bottle = 1,  large bottle = 2,  can = 2.
    QSettings s;
    m_smallPts = s.value("recycle/smallBottlePoints", 1).toInt();
    m_largePts = s.value("recycle/largeBottlePoints", 2).toInt();
    m_canPts   = s.value("recycle/canPoints",         2).toInt();
    m_pointEGP = s.value("recycle/pointValueEGP",   0.4).toDouble();
    // Cumulative bin fill survives restarts (reset by the admin "empty bin").
    m_aluBin     = s.value("recycle/aluBinCount",     0).toInt();
    m_plasticBin = s.value("recycle/plasticBinCount", 0).toInt();

    // Belt-run safety watchdog: if any phase runs too long (item jammed, a
    // sensor never trips), stop the belt and reset so it never runs forever.
    m_phaseTimer = new QTimer(this);
    m_phaseTimer->setSingleShot(true);
    connect(m_phaseTimer, &QTimer::timeout, this, [this]() {
        Logger::audit("Recycle", "Phase timeout — belt stopped",
                      { {"phase", int(m_phase)} });
        beltStop();
        servo(QStringLiteral("NEUTRAL"));
        m_phase = PhIdle;
        setLast(tr("Timed out — try again"));
        maybeFinish();
    });

    // Reject eject: reverse the belt for a moment to spit the item back out.
    m_ejectTimer = new QTimer(this);
    m_ejectTimer->setSingleShot(true);
    connect(m_ejectTimer, &QTimer::timeout, this, [this]() {
        beltStop();
        servo(QStringLiteral("NEUTRAL"));
        m_phase = PhIdle;
        maybeFinish();
    });
}

// ── helpers ─────────────────────────────────────────────────────────────────
void RecycleSession::setLast(const QString &str)
{
    if (m_lastEvent == str) return;
    m_lastEvent = str;
    emit lastEventChanged();
}

void RecycleSession::beltFwd()  { emit sendCommand(QStringLiteral("BELT:FWD"));  }
void RecycleSession::beltStop() { m_phaseTimer->stop(); emit sendCommand(QStringLiteral("BELT:STOP")); }
void RecycleSession::beltRev()  { emit sendCommand(QStringLiteral("BELT:REV"));  }
void RecycleSession::servo(const QString &which) { emit sendCommand(QStringLiteral("SERVO:") + which); }

void RecycleSession::armPhaseTimeout(int ms) { m_phaseTimer->start(ms); }

// ── lifecycle ─────────────────────────────────────────────────────────────────
void RecycleSession::start()        // QML calls this when entering the recycle UI
{
    startSession();
    setLast(tr("Insert a bottle or can"));
}

void RecycleSession::startSession()
{
    m_smallBottles = m_largeBottles = m_cans = m_rejected = 0;
    m_pendingLargeBottle = false;
    m_finishRequested = false;
    m_phase = PhIdle;
    emit countsChanged();
    if (!m_active) {
        m_active = true;
        emit activeChanged();            // → recycle camera light on (relay 1)
    }
    Logger::audit("Recycle", "Session started");
}

void RecycleSession::endSession()
{
    beltStop();
    servo(QStringLiteral("NEUTRAL"));
    m_phaseTimer->stop();
    m_ejectTimer->stop();
    m_phase = PhIdle;
    if (m_active) {
        m_active = false;
        emit activeChanged();            // → recycle camera light off
    }
    Logger::audit("Recycle", "Session finished",
                  { {"smallBottles", m_smallBottles}, {"largeBottles", m_largeBottles},
                    {"cans", m_cans}, {"rejected", m_rejected}, {"points", totalPoints()} });
    emit sessionEnded();
}

int RecycleSession::finish()        // QML "Finish" button
{
    // Honour Finish immediately when idle; otherwise let the in-flight item
    // finish dropping first (maybeFinish() ends it once we return to Idle).
    m_finishRequested = true;
    const int total = totalPoints();
    if (m_phase == PhIdle) endSession();
    else setLast(tr("Finishing — one moment…"));
    return total;
}

void RecycleSession::maybeFinish()
{
    if (m_finishRequested && m_phase == PhIdle) endSession();
}

// ── camera verdict → sort ─────────────────────────────────────────────────────
// cls is the classifier's raw sub-class: small_bottle | large_bottle | can |
// reject. Decides bottle/can/reject, applies bin-full rules, drives the servo
// and the belt toward the right drop sensor.
void RecycleSession::onCameraVerdict(const QString &cls)
{
    if (m_phase != PhAtCamera) return;          // stale verdict — ignore
    const QString c = cls.toLower();

    if (c.contains("bottle")) {
        if (plasticBinFull()) {                 // basket full → reject
            ++m_rejected; emit countsChanged();
            emit itemRejected(tr("Bottle basket is full"));
            setLast(tr("Rejected — bottle basket full"));
            beltRev(); m_phase = PhEjecting; m_ejectTimer->start(kEjectMs);
            return;
        }
        m_pendingLargeBottle = c.contains("large");
        setLast(tr("Bottle — sorting…"));
        servo(QStringLiteral("BOTTLE"));
        beltFwd(); m_phase = PhSortBottle; armPhaseTimeout(kPhaseTimeoutMs);
    } else if (c.contains("can")) {
        if (aluBinFull()) {
            ++m_rejected; emit countsChanged();
            emit itemRejected(tr("Can basket is full"));
            setLast(tr("Rejected — can basket full"));
            beltRev(); m_phase = PhEjecting; m_ejectTimer->start(kEjectMs);
            return;
        }
        setLast(tr("Can — sorting…"));
        servo(QStringLiteral("CAN"));
        beltFwd(); m_phase = PhSortCan; armPhaseTimeout(kPhaseTimeoutMs);
    } else {
        // Not a recyclable / low confidence → eject.
        ++m_rejected; emit countsChanged();
        m_pendingLargeBottle = false;
        emit itemRejected(tr("Not an empty bottle or can"));
        setLast(tr("Rejected — not recyclable"));
        beltRev(); m_phase = PhEjecting; m_ejectTimer->start(kEjectMs);
    }
}

void RecycleSession::creditBottle()
{
    const int pts = m_pendingLargeBottle ? m_largePts : m_smallPts;
    if (m_pendingLargeBottle) ++m_largeBottles; else ++m_smallBottles;
    m_pendingLargeBottle = false;
    emit countsChanged();
    if (m_plasticBin < kPlasticCap) ++m_plasticBin;
    QSettings().setValue("recycle/plasticBinCount", m_plasticBin);
    emit binChanged();
    emit itemAccepted(QStringLiteral("bottle"), pts);
    setLast(tr("Bottle accepted  +%1").arg(pts));
}

void RecycleSession::creditCan()
{
    ++m_cans; emit countsChanged();
    if (m_aluBin < kAluCap) ++m_aluBin;
    QSettings().setValue("recycle/aluBinCount", m_aluBin);
    emit binChanged();
    emit itemAccepted(QStringLiteral("can"), m_canPts);
    setLast(tr("Can accepted  +%1").arg(m_canPts));
}

// ── IR-driven state machine ───────────────────────────────────────────────────
void RecycleSession::onIrEdge(int sensor, bool blocked)
{
    if (!blocked) return;          // act on the "object arrived" edge only

    switch (sensor) {
    case 1:                        // inlet — an item entered the lane
        if (m_phase == PhIdle && !m_finishRequested) {
            if (!m_active) startSession();
            emit recyclePageRequested();   // open the counter page
            emit itemEntered();
            setLast(tr("Item detected — moving in…"));
            beltFwd();
            m_phase = PhMoving;
            armPhaseTimeout(kPhaseTimeoutMs);
        }
        break;
    case 2:                        // mid-lane — just passing through
        if (m_phase == PhMoving) setLast(tr("Moving to the scanner…"));
        break;
    case 3:                        // camera/stop position
        if (m_phase == PhMoving) {
            beltStop();
            m_phase = PhAtCamera;
            setLast(tr("Scanning…"));
            emit cameraRequested();        // → RecycleClassifier (3 s, ≥0.70)
            armPhaseTimeout(kPhaseTimeoutMs);
        }
        break;
    case 4:                        // bottle dropped into the plastic bin
        if (m_phase == PhSortBottle) {
            beltStop();
            servo(QStringLiteral("NEUTRAL"));
            creditBottle();
            m_phase = PhIdle;
            maybeFinish();
        }
        break;
    case 5:                        // can dropped into the aluminium bin
        if (m_phase == PhSortCan) {
            beltStop();
            servo(QStringLiteral("NEUTRAL"));
            creditCan();
            m_phase = PhIdle;
            maybeFinish();
        }
        break;
    default: break;
    }
}

// ── serial in: STM32 IR edge events ───────────────────────────────────────────
void RecycleSession::onSerialLine(const QString &line)
{
    // STM32 pushes "EVT,IR,<1..5>,<1|0>" (1 = blocked/arrived, 0 = cleared).
    if (!line.startsWith("EVT,IR,")) return;
    const QStringList p = line.split(',');
    if (p.size() < 4) return;
    bool okN = false, okV = false;
    const int  sensor  = p.at(2).toInt(&okN);
    const int  value   = p.at(3).toInt(&okV);
    if (!okN || !okV || sensor < 1 || sensor > 5) return;
    onIrEdge(sensor, value != 0);
}

// ── misc QML-facing setters (unchanged) ───────────────────────────────────────
void RecycleSession::sendVerdict(const QString &v)
{
    // Manual/admin verdict override — feed it through the same path the camera
    // uses so the belt + servo react identically.
    onCameraVerdict(v);
}

void RecycleSession::setBaskets(bool bottleFull, bool canFull)
{
    // Bin-full is now decided here from the cumulative counts; this stays for
    // QML/admin compatibility but no longer talks to the Arduino.
    Q_UNUSED(bottleFull); Q_UNUSED(canFull);
}

void RecycleSession::setSmallBottlePoints(int p)
{
    if (p < 0 || p == m_smallPts) return;
    m_smallPts = p;
    QSettings().setValue("recycle/smallBottlePoints", p);
    emit configChanged();
    emit countsChanged();
}

void RecycleSession::setLargeBottlePoints(int p)
{
    if (p < 0 || p == m_largePts) return;
    m_largePts = p;
    QSettings().setValue("recycle/largeBottlePoints", p);
    emit configChanged();
    emit countsChanged();
}

void RecycleSession::setCanPoints(int p)
{
    if (p < 0 || p == m_canPts) return;
    m_canPts = p;
    QSettings().setValue("recycle/canPoints", p);
    emit configChanged();
    emit countsChanged();
}

void RecycleSession::emptyAluBin()
{
    m_aluBin = 0;
    QSettings().setValue("recycle/aluBinCount", 0);
    emit binChanged();
    Logger::audit("Recycle", "Aluminium bin emptied");
}

void RecycleSession::emptyPlasticBin()
{
    m_plasticBin = 0;
    QSettings().setValue("recycle/plasticBinCount", 0);
    emit binChanged();
    Logger::audit("Recycle", "Plastic bin emptied");
}
