#ifndef LOGS_VIEWER_H
#define LOGS_VIEWER_H

#include <QObject>
#include <QQmlEngine>
#include <QStringList>

/**
 * LogsViewer — reads recent log lines + lets admin delete old data.
 */
class LogsViewer : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(LogsViewer)
    QML_SINGLETON

    Q_PROPERTY(QStringList lines READ lines NOTIFY changed)

public:
    explicit LogsViewer(QObject *parent = nullptr);
    static LogsViewer *create(QQmlEngine *, QJSEngine *) { return s_instance; }
    static LogsViewer *s_instance;

    QStringList lines() const { return m_lines; }

public slots:
    Q_INVOKABLE void refresh();
    Q_INVOKABLE int  deleteOlderThan(int days);
    Q_INVOKABLE void wipeAll();

signals:
    void changed();

private:
    QStringList m_lines;
};

#endif // LOGS_VIEWER_H
