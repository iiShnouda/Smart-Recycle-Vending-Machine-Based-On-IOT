#include "../include/analytics.h"

#include <QSqlQuery>
#include <QSqlDatabase>
#include <QDateTime>
#include <QVariantMap>

Analytics *Analytics::s_instance = nullptr;

Analytics::Analytics(QObject *parent) : QObject(parent)
{
    if (!s_instance) s_instance = this;
}

static int scalarInt(const QString &sql, const QVariantList &binds = {})
{
    QSqlQuery q(QSqlDatabase::database("rewingo"));
    q.prepare(sql);
    for (const auto &b : binds) q.addBindValue(b);
    if (q.exec() && q.next()) return q.value(0).toInt();
    return 0;
}

void Analytics::refresh()
{
    if (!QSqlDatabase::database("rewingo").isOpen()) return;
    const QString today = QDateTime::currentDateTime().toString("yyyy-MM-dd");
    const QString likeToday = today + "%";

    m_recyclesToday = scalarInt(
        "SELECT COUNT(*) FROM transactions WHERE kind='recycle' AND ts LIKE ?",
        { likeToday });
    m_vendingsToday = scalarInt(
        "SELECT COUNT(*) FROM transactions WHERE kind='vending' AND ts LIKE ?",
        { likeToday });
    m_pointsSpentToday = scalarInt(
        "SELECT IFNULL(SUM(-amount),0) FROM transactions "
        "WHERE kind='vending' AND ts LIKE ?",
        { likeToday });
    m_totalUsers = scalarInt("SELECT COUNT(*) FROM users");

    // Recent 100 transactions for the table view.
    m_recent.clear();
    QSqlQuery q(QSqlDatabase::database("rewingo"));
    if (q.exec("SELECT ts,kind,user_id,slot,amount FROM transactions "
               "ORDER BY id DESC LIMIT 100")) {
        while (q.next()) {
            QVariantMap row;
            row["ts"]      = q.value(0).toString();
            row["kind"]    = q.value(1).toString();
            row["user_id"] = q.value(2).toString();
            row["slot"]    = q.value(3).toInt();
            row["amount"]  = q.value(4).toInt();
            m_recent.append(row);
        }
    }
    emit changed();
}
