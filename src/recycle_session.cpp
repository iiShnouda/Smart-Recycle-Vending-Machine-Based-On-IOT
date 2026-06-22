#include "../include/recycle_session.h"
#include "../include/logger.h"

#include <QSettings>
#include <QDir>
#include <QFile>
#include <QProcess>

RecycleSession *RecycleSession::s_instance = nullptr;

RecycleSession::RecycleSession(QObject *parent) : QObject(parent)
{
    if (!s_instance) s_instance = this;

    QSettings s;
    m_smallPts = s.value("recycle/smallBottlePoints", 1).toInt();
    m_largePts = s.value("recycle/largeBottlePoints", 2).toInt();
    m_canPts   = s.value("recycle/canPoints",         2).toInt();
    m_pointEGP = s.value("recycle/pointValueEGP",   0.4).toDouble();
    m_aluBin     = s.value("recycle/aluBinCount",     0).toInt();
    m_plasticBin = s.value("recycle/plasticBinCount", 0).toInt();
}

void RecycleSession::setLast(const QString &str)
{
    if (m_lastEvent == str) return;
    m_lastEvent = str;
    emit lastEventChanged();
}

// ── lifecycle ─────────────────────────────────────────────────────────────────
void RecycleSession::start()
{
    m_smallBottles = m_largeBottles = m_cans = m_rejected = 0;
    m_pendingLargeBottle = false;
    emit countsChanged();
    setLast(tr("Insert a bottle or can"));
    if (!m_active) { m_active = true; emit activeChanged(); }   // → recycle light on
    emit sendCommand(QStringLiteral("RECYCLE 1"));              // arm the Arduino lane
    Logger::audit("Recycle", "Session started");
}

int RecycleSession::finish()
{
    if (m_active) { m_active = false; emit activeChanged(); }   // → recycle light off
    emit sendCommand(QStringLiteral("RECYCLE 0"));              // disarm the lane
    const int total = totalPoints();
    Logger::audit("Recycle", "Session finished",
                  { {"smallBottles", m_smallBottles}, {"largeBottles", m_largeBottles},
                    {"cans", m_cans}, {"rejected", m_rejected}, {"points", total} });
    emit sessionEnded();
    return total;
}

// ── camera verdict → tell the Arduino how to sort ─────────────────────────────
void RecycleSession::sendVerdict(const QString &v)
{
    emit sendCommand(QStringLiteral("VERDICT ") + v.toUpper());
}

void RecycleSession::onCameraVerdict(const QString &cls)
{
    // Remember the bottle size so we award 1 (small) vs 2 (large) when it drops,
    // then hand the Arduino the BOTTLE/CAN/REJECT verdict it sorts on. Bin-full
    // is decided here: a full bin → REJECT so the item is handed back.
    const QString c = cls.toLower();
    if (c.contains("bottle")) {
        if (plasticBinFull()) { setLast(tr("Bottle bin full")); sendVerdict("REJECT"); return; }
        m_pendingLargeBottle = c.contains("large");
        setLast(tr("Bottle — sorting…"));
        sendVerdict(QStringLiteral("BOTTLE"));
    } else if (c.contains("can")) {
        if (aluBinFull()) { setLast(tr("Can bin full")); sendVerdict("REJECT"); return; }
        setLast(tr("Can — sorting…"));
        sendVerdict(QStringLiteral("CAN"));
    } else {
        m_pendingLargeBottle = false;
        setLast(tr("Not recyclable"));
        sendVerdict(QStringLiteral("REJECT"));
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

// ── serial in: the Arduino's EVT lines ────────────────────────────────────────
void RecycleSession::onSerialLine(const QString &line)
{
    if (!line.startsWith("EVT,")) return;
    // Accept EVT,READY even before "active" so a re-arm is visible; otherwise
    // only react while a session is live.
    if (!m_active && !line.startsWith("EVT,READY")) return;

    if (line.startsWith("EVT,ENTRY")) {
        setLast(tr("Item detected — moving in…"));
        emit recyclePageRequested();        // open the counter page (from home)
        emit itemEntered();                 // waiting page → push the counter
    } else if (line.startsWith("EVT,CAMERA")) {
        setLast(tr("Scanning…"));
        emit cameraRequested();             // → RecycleClassifier (window, ≥0.70)
    } else if (line.startsWith("EVT,DROPPED,BOTTLE")) {
        creditBottle();
    } else if (line.startsWith("EVT,DROPPED,CAN")) {
        creditCan();
    } else if (line.startsWith("EVT,REJECTED")) {
        ++m_rejected; emit countsChanged();
        m_pendingLargeBottle = false;
        const QString why = line.section(',', 2, 2);
        const QString human =
            why == "BOTTLE_FULL" ? tr("Bottle bin is full")
          : why == "CAN_FULL"    ? tr("Can bin is full")
          : why == "TIMEOUT"     ? tr("Didn't reach the bin — try again")
                                 : tr("Not an empty bottle or can");
        emit itemRejected(human);
        setLast(tr("Rejected — ") + human);
    }
}

// ── misc QML-facing setters ───────────────────────────────────────────────────
void RecycleSession::setBaskets(bool bottleFull, bool canFull)
{
    // Bin-full is decided kiosk-side from the counts now; kept for QML compat.
    Q_UNUSED(bottleFull); Q_UNUSED(canFull);
}

void RecycleSession::setSmallBottlePoints(int p)
{
    if (p < 0 || p == m_smallPts) return;
    m_smallPts = p; QSettings().setValue("recycle/smallBottlePoints", p);
    emit configChanged(); emit countsChanged();
}
void RecycleSession::setLargeBottlePoints(int p)
{
    if (p < 0 || p == m_largePts) return;
    m_largePts = p; QSettings().setValue("recycle/largeBottlePoints", p);
    emit configChanged(); emit countsChanged();
}
void RecycleSession::setCanPoints(int p)
{
    if (p < 0 || p == m_canPts) return;
    m_canPts = p; QSettings().setValue("recycle/canPoints", p);
    emit configChanged(); emit countsChanged();
}

void RecycleSession::emptyAluBin()
{
    m_aluBin = 0; QSettings().setValue("recycle/aluBinCount", 0);
    emit binChanged(); Logger::audit("Recycle", "Aluminium bin emptied");
}
void RecycleSession::emptyPlasticBin()
{
    m_plasticBin = 0; QSettings().setValue("recycle/plasticBinCount", 0);
    emit binChanged(); Logger::audit("Recycle", "Plastic bin emptied");
}

void RecycleSession::resetBins()
{
    m_aluBin = 0; m_plasticBin = 0;
    QSettings s;
    s.setValue("recycle/aluBinCount", 0);
    s.setValue("recycle/plasticBinCount", 0);
    emit binChanged();
    Logger::audit("Recycle", "Both bins reset (can + bottle)");
}

void RecycleSession::openCameraTest()
{
    const QString home   = QDir::homePath();
    const QString venvPy = home + "/recycle_venv/bin/python";
    const QString python = QFile::exists(venvPy) ? venvPy : QStringLiteral("python3");
    const QString script = home + "/recycle_test.py";
    const bool ok = QProcess::startDetached(python, { script });
    Logger::audit("Recycle", ok ? "Camera test window launched"
                                : "Camera test launch FAILED",
                  { {"python", python}, {"script", script} });
}
