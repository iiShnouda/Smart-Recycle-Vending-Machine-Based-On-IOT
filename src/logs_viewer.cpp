#include "../include/logs_viewer.h"
#include "../include/logger.h"
#include "../include/database.h"

#include <QFile>
#include <QStandardPaths>
#include <QDir>
#include <QTextStream>
#include <QSqlQuery>
#include <QSqlDatabase>

LogsViewer *LogsViewer::s_instance = nullptr;

LogsViewer::LogsViewer(QObject *parent) : QObject(parent)
{
    if (!s_instance) s_instance = this;
}

void LogsViewer::refresh()
{
    m_lines.clear();
    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QFile f(dataDir + "/logs/rewingo.log");
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit changed();
        return;
    }
    // Tail: read last ~200 lines
    QStringList all;
    QTextStream s(&f);
    while (!s.atEnd()) all << s.readLine();
    const qsizetype start = std::max(qsizetype(0), all.size() - 200);
    for (qsizetype i = start; i < all.size(); ++i) m_lines << all[i];
    emit changed();
}

int LogsViewer::deleteOlderThan(int days)
{
    // 1) Delete old transactions in DB
    Database *db = nullptr;            // we don't keep a back-pointer; use shared name
    // Hacky direct exec: get our SQLite handle by name.
    QSqlDatabase d = QSqlDatabase::database("rewingo");
    if (!d.isOpen()) return -1;

    const QString cutoff = QDateTime::currentDateTimeUtc()
                            .addDays(-days)
                            .toString(Qt::ISODateWithMs);
    QSqlQuery q(d);
    q.prepare("DELETE FROM transactions WHERE ts < ?");
    q.addBindValue(cutoff);
    if (!q.exec()) return -1;
    const int n = q.numRowsAffected();

    // 2) Also drop synced audit_log rows older than cutoff
    QSqlQuery q2(d);
    q2.prepare("DELETE FROM audit_log WHERE ts < ? AND synced=1");
    q2.addBindValue(cutoff);
    q2.exec();

    Logger::audit("Admin", "Old data deleted",
                  { {"days", days}, {"tx_deleted", n} });
    refresh();
    return n;
}

void LogsViewer::wipeAll()
{
    // 1) Clear DB tables (keep schema)
    QSqlDatabase d = QSqlDatabase::database("rewingo");
    if (d.isOpen()) {
        QSqlQuery q(d);
        q.exec("DELETE FROM transactions");
        q.exec("DELETE FROM audit_log");
        // We deliberately KEEP users + products so the machine still works.
    }

    // 2) Truncate log files
    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir d2(dataDir + "/logs");
    for (const auto &fn : d2.entryList({ "rewingo.log*" }, QDir::Files)) {
        QFile::remove(d2.filePath(fn));
    }

    Logger::audit("Admin", "Local data wiped (manual)");
    refresh();
}
