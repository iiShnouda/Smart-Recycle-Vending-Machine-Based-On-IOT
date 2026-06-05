#include "../include/recycle_session.h"
#include "../include/logger.h"

#include <QSettings>

RecycleSession *RecycleSession::s_instance = nullptr;

RecycleSession::RecycleSession(QObject *parent) : QObject(parent)
{
    if (!s_instance) s_instance = this;

    // Point values are admin-configurable + persisted.
    QSettings s;
    m_bottlePts = s.value("recycle/bottlePoints", 1).toInt();
    m_canPts    = s.value("recycle/canPoints",    2).toInt();
}

void RecycleSession::setLast(const QString &str)
{
    if (m_lastEvent == str) return;
    m_lastEvent = str;
    emit lastEventChanged();
}

void RecycleSession::start()
{
    m_bottles = m_cans = m_rejected = 0;
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
                  { {"bottles", m_bottles}, {"cans", m_cans},
                    {"rejected", m_rejected}, {"points", total} });
    return total;
}

void RecycleSession::sendVerdict(const QString &v)
{
    emit sendCommand(QStringLiteral("VERDICT ") + v.toUpper());
}

void RecycleSession::setBaskets(bool bottleFull, bool canFull)
{
    emit sendCommand(QStringLiteral("BASKETS %1 %2")
                         .arg(bottleFull ? 1 : 0).arg(canFull ? 1 : 0));
}

void RecycleSession::setBottlePoints(int p)
{
    if (p < 0 || p == m_bottlePts) return;
    m_bottlePts = p;
    QSettings().setValue("recycle/bottlePoints", p);
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

void RecycleSession::onSerialLine(const QString &line)
{
    if (!line.startsWith("EVT,")) return;
    if (!m_active && !line.startsWith("EVT,READY")) return;

    if (line.startsWith("EVT,ENTRY")) {
        setLast(tr("Item detected — moving in…"));
    } else if (line.startsWith("EVT,CAMERA")) {
        setLast(tr("Scanning…"));
        emit cameraRequested();
    } else if (line.startsWith("EVT,DROPPED,BOTTLE")) {
        ++m_bottles; emit countsChanged();
        emit itemAccepted(QStringLiteral("bottle"), m_bottlePts);
        setLast(tr("Bottle accepted  +%1").arg(m_bottlePts));
    } else if (line.startsWith("EVT,DROPPED,CAN")) {
        ++m_cans; emit countsChanged();
        emit itemAccepted(QStringLiteral("can"), m_canPts);
        setLast(tr("Can accepted  +%1").arg(m_canPts));
    } else if (line.startsWith("EVT,REJECTED")) {
        ++m_rejected; emit countsChanged();
        QString why = line.section(',', 2, 2);
        QString human =
            why == "BOTTLE_FULL" ? tr("Bottle basket is full")
          : why == "CAN_FULL"    ? tr("Can basket is full")
                                 : tr("Not an empty bottle or can");
        emit itemRejected(human);
        setLast(tr("Rejected — ") + human);
    }
}
