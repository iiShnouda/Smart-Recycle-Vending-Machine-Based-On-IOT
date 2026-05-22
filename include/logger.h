#ifndef LOGGER_H
#define LOGGER_H

#include <QObject>
#include <QString>
#include <QFile>
#include <QHash>
#include <QMutex>
#include <QDateTime>

/**
 * Logger — file + console with rate-limiting and rotation.
 *
 * Features:
 *   - 4 levels: INFO, WARN, ERROR, AUDIT
 *   - Rate limiter: identical messages within RateWindowMs are coalesced
 *     ("[Serial] connecting..." spam becomes "[Serial] connecting... (×42)")
 *   - Rotates at MaxBytesPerFile; keeps last MaxFiles archives
 *   - Audit events (transactions, admin actions) are flagged separately
 *     for upload to the remote DB
 *
 * Usage:
 *   Logger::info("Recycle", "User started session", { {"user_id", "abc"} });
 *   Logger::audit("Vending", "Bought item", { {"product","Cola"}, {"points",50} });
 */
class Logger : public QObject {
    Q_OBJECT
public:
    enum Level { INFO, WARN, ERROR, AUDIT };
    Q_ENUM(Level)

    static Logger *instance();

    /** Open log directory + start. Idempotent. */
    void start(const QString &logDir, qint64 maxBytesPerFile = 1*1024*1024,
               int maxFiles = 5);

    /** Stop and flush. */
    void stop();

    /** Convenience static methods — go through the singleton. */
    static void info (const QString &category, const QString &msg,
                      const QVariantMap &meta = {});
    static void warn (const QString &category, const QString &msg,
                      const QVariantMap &meta = {});
    static void error(const QString &category, const QString &msg,
                      const QVariantMap &meta = {});
    static void audit(const QString &category, const QString &msg,
                      const QVariantMap &meta = {});

signals:
    /** Emitted for every AUDIT event so the database layer can persist + sync. */
    void auditEvent(const QString &category, const QString &message,
                    const QVariantMap &meta, const QDateTime &when);

private:
    explicit Logger(QObject *parent = nullptr);
    void writeLine(Level lvl, const QString &category, const QString &msg,
                   const QVariantMap &meta);
    void rotateIfNeeded();
    QString formatLine(Level lvl, const QString &category, const QString &msg,
                       const QVariantMap &meta, const QDateTime &when) const;

    static constexpr int kRateWindowMs = 2000;  // coalesce identical msgs within 2 s

    QFile      m_file;
    QString    m_logDir;
    qint64     m_maxBytes  = 1*1024*1024;
    int        m_maxFiles  = 5;
    QMutex     m_mutex;

    // Rate limiter state
    struct LastSeen { qint64 atMs; int count; };
    QHash<QString, LastSeen> m_lastSeen;
};

#endif // LOGGER_H
