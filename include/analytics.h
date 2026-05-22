#ifndef ANALYTICS_H
#define ANALYTICS_H

#include <QObject>
#include <QQmlEngine>
#include <QVariantList>

/**
 * Analytics — read-only KPIs computed from the local SQLite DB.
 * Recomputes on demand (refresh()) — cheap because SQLite is in-process.
 */
class Analytics : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(Analytics)
    QML_SINGLETON

    Q_PROPERTY(int recyclesToday    READ recyclesToday    NOTIFY changed)
    Q_PROPERTY(int vendingsToday    READ vendingsToday    NOTIFY changed)
    Q_PROPERTY(int totalUsers       READ totalUsers       NOTIFY changed)
    Q_PROPERTY(int pointsSpentToday READ pointsSpentToday NOTIFY changed)
    Q_PROPERTY(QVariantList recent  READ recent           NOTIFY changed)

public:
    explicit Analytics(QObject *parent = nullptr);
    static Analytics *create(QQmlEngine *, QJSEngine *) { return s_instance; }
    static Analytics *s_instance;

    int recyclesToday()    const { return m_recyclesToday; }
    int vendingsToday()    const { return m_vendingsToday; }
    int totalUsers()       const { return m_totalUsers; }
    int pointsSpentToday() const { return m_pointsSpentToday; }
    QVariantList recent()  const { return m_recent; }

public slots:
    Q_INVOKABLE void refresh();

signals:
    void changed();

private:
    int m_recyclesToday    = 0;
    int m_vendingsToday    = 0;
    int m_totalUsers       = 0;
    int m_pointsSpentToday = 0;
    QVariantList m_recent;
};

#endif // ANALYTICS_H
