#include "../include/recycle_session.h"
#include "../include/logger.h"

#include <QSettings>

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
}

void RecycleSession::setLast(const QString &str)
{
    if (m_lastEvent == str) return;
    m_lastEvent = str;
    emit lastEventChanged();
}

void RecycleSession::start()
{
    m_smallBottles = m_largeBottles = m_cans = m_rejected = 0;
    m_pendingLargeBottle = false;
    emit countsChanged();
    setLast(tr("Insert a bottle or can"));
    m_active = true;
    emit activeChanged();
    emit sendCommand(QStringLiteral("RECYCLE 1"));   // arm the STM32 lane
    Logger::audit("Recycle", "Session started");
}

int RecycleSession::finish()
{
    m_active = false;
    emit activeChanged();
    emit sendCommand(QStringLiteral("RECYCLE 0"));   // lights + camera off
    const int total = totalPoints();
    Logger::audit("Recycle", "Session finished",
                  { {"smallBottles", m_smallBottles}, {"largeBottles", m_largeBottles},
                    {"cans", m_cans}, {"rejected", m_rejected}, {"points", total} });
    return total;
}

void RecycleSession::sendVerdict(const QString &v)
{
    emit sendCommand(QStringLiteral("VERDICT ") + v.toUpper());
}

void RecycleSession::onCameraVerdict(const QString &cls)
{
    // cls is the classifier's raw sub-class. Remember the bottle size so we
    // can award 1 (small) vs 2 (large) points when the item actually drops,
    // then hand the STM32 the BOTTLE/CAN/REJECT verdict it sorts on.
    const QString c = cls.toLower();
    if (c.contains("bottle")) {
        m_pendingLargeBottle = c.contains("large");
        sendVerdict(QStringLiteral("BOTTLE"));
    } else if (c.contains("can")) {
        sendVerdict(QStringLiteral("CAN"));
    } else {
        sendVerdict(QStringLiteral("REJECT"));
    }
}

void RecycleSession::setBaskets(bool bottleFull, bool canFull)
{
    emit sendCommand(QStringLiteral("BASKETS %1 %2")
                         .arg(bottleFull ? 1 : 0).arg(canFull ? 1 : 0));
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

void RecycleSession::onSerialLine(const QString &line)
{
    if (!line.startsWith("EVT,")) return;
    if (!m_active && !line.startsWith("EVT,READY")) return;

    if (line.startsWith("EVT,ENTRY")) {
        setLast(tr("Item detected — moving in…"));
        emit itemEntered();          // IR1 tripped → QML moves to the counter
    } else if (line.startsWith("EVT,CAMERA")) {
        setLast(tr("Scanning…"));
        emit cameraRequested();
    } else if (line.startsWith("EVT,DROPPED,BOTTLE")) {
        // Small vs large bottle scoring comes from the remembered camera verdict.
        const int pts = m_pendingLargeBottle ? m_largePts : m_smallPts;
        if (m_pendingLargeBottle) ++m_largeBottles; else ++m_smallBottles;
        m_pendingLargeBottle = false;
        emit countsChanged();
        if (m_plasticBin < kPlasticCap) ++m_plasticBin;   // → plastic bin fills
        QSettings().setValue("recycle/plasticBinCount", m_plasticBin);
        emit binChanged();
        emit itemAccepted(QStringLiteral("bottle"), pts);
        setLast(tr("Bottle accepted  +%1").arg(pts));
    } else if (line.startsWith("EVT,DROPPED,CAN")) {
        ++m_cans; emit countsChanged();
        if (m_aluBin < kAluCap) ++m_aluBin;               // → aluminium bin fills
        QSettings().setValue("recycle/aluBinCount", m_aluBin);
        emit binChanged();
        emit itemAccepted(QStringLiteral("can"), m_canPts);
        setLast(tr("Can accepted  +%1").arg(m_canPts));
    } else if (line.startsWith("EVT,REJECTED")) {
        ++m_rejected; emit countsChanged();
        m_pendingLargeBottle = false;
        QString why = line.section(',', 2, 2);
        QString human =
            why == "BOTTLE_FULL" ? tr("Bottle basket is full")
          : why == "CAN_FULL"    ? tr("Can basket is full")
                                 : tr("Not an empty bottle or can");
        emit itemRejected(human);
        setLast(tr("Rejected — ") + human);
    }
}
