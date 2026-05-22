#include "../include/logger.h"

#include <QDir>
#include <QDateTime>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QElapsedTimer>

static QElapsedTimer s_clock;
static bool s_clockStarted = false;

Logger *Logger::instance()
{
    static Logger inst;
    return &inst;
}

Logger::Logger(QObject *parent) : QObject(parent)
{
    if (!s_clockStarted) { s_clock.start(); s_clockStarted = true; }
}

void Logger::start(const QString &logDir, qint64 maxBytesPerFile, int maxFiles)
{
    QMutexLocker lock(&m_mutex);
    if (m_file.isOpen()) return;

    QDir().mkpath(logDir);
    m_logDir   = logDir;
    m_maxBytes = maxBytesPerFile;
    m_maxFiles = maxFiles;

    m_file.setFileName(logDir + "/rewingo.log");
    m_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);

    qInstallMessageHandler(nullptr); // keep Qt default; we add to it, don't replace
}

void Logger::stop()
{
    QMutexLocker lock(&m_mutex);
    if (m_file.isOpen()) m_file.close();
}

QString Logger::formatLine(Level lvl, const QString &category,
                           const QString &msg, const QVariantMap &meta,
                           const QDateTime &when) const
{
    const char *lname = "INFO";
    switch (lvl) { case WARN:  lname = "WARN";  break;
                   case ERROR: lname = "ERROR"; break;
                   case AUDIT: lname = "AUDIT"; break;
                   default: break; }

    QString line = QStringLiteral("%1 [%2] %3: %4")
        .arg(when.toString(Qt::ISODateWithMs))
        .arg(lname)
        .arg(category, msg);

    if (!meta.isEmpty()) {
        QJsonObject obj;
        for (auto it = meta.constBegin(); it != meta.constEnd(); ++it)
            obj.insert(it.key(), QJsonValue::fromVariant(it.value()));
        line += " " + QString::fromUtf8(
            QJsonDocument(obj).toJson(QJsonDocument::Compact));
    }
    return line;
}

void Logger::rotateIfNeeded()
{
    if (!m_file.isOpen()) return;
    if (m_file.size() < m_maxBytes) return;

    m_file.close();

    // shift rewingo.log.N → rewingo.log.N+1 (drop oldest)
    for (int i = m_maxFiles; i >= 1; --i) {
        const QString src = QString("%1/rewingo.log.%2").arg(m_logDir).arg(i);
        const QString dst = QString("%1/rewingo.log.%2").arg(m_logDir).arg(i+1);
        if (i == m_maxFiles) QFile::remove(src);
        QFile::rename(src, dst);
    }
    QFile::rename(m_logDir + "/rewingo.log",
                  m_logDir + "/rewingo.log.1");

    m_file.setFileName(m_logDir + "/rewingo.log");
    m_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
}

void Logger::writeLine(Level lvl, const QString &category,
                       const QString &msg, const QVariantMap &meta)
{
    const QDateTime now = QDateTime::currentDateTime();
    const qint64 nowMs  = s_clock.elapsed();

    QMutexLocker lock(&m_mutex);

    // Rate limit by (category, msg). Audit messages always pass through.
    if (lvl != AUDIT) {
        const QString key = category + "\x1f" + msg;
        auto it = m_lastSeen.find(key);
        if (it != m_lastSeen.end() && (nowMs - it->atMs) < kRateWindowMs) {
            it->count++;
            return; // suppress
        }
        if (it != m_lastSeen.end() && it->count > 0) {
            // emit the coalesce summary
            const QString line = formatLine(lvl, category,
                QString("%1  (×%2 in last %3 ms)").arg(msg).arg(it->count + 1).arg(kRateWindowMs),
                meta, now);
            if (m_file.isOpen()) { m_file.write(line.toUtf8() + "\n"); m_file.flush(); }
            qInfo().noquote() << line;
            m_lastSeen[key] = { nowMs, 0 };
            rotateIfNeeded();
            return;
        }
        m_lastSeen[key] = { nowMs, 0 };
    }

    const QString line = formatLine(lvl, category, msg, meta, now);
    if (m_file.isOpen()) { m_file.write(line.toUtf8() + "\n"); m_file.flush(); }

    // Mirror to console for development
    switch (lvl) {
        case WARN:  qWarning().noquote()  << line; break;
        case ERROR: qCritical().noquote() << line; break;
        default:    qInfo().noquote()     << line; break;
    }

    if (lvl == AUDIT) emit auditEvent(category, msg, meta, now);

    rotateIfNeeded();
}

void Logger::info (const QString &c, const QString &m, const QVariantMap &md)
{ instance()->writeLine(INFO,  c, m, md); }
void Logger::warn (const QString &c, const QString &m, const QVariantMap &md)
{ instance()->writeLine(WARN,  c, m, md); }
void Logger::error(const QString &c, const QString &m, const QVariantMap &md)
{ instance()->writeLine(ERROR, c, m, md); }
void Logger::audit(const QString &c, const QString &m, const QVariantMap &md)
{ instance()->writeLine(AUDIT, c, m, md); }
