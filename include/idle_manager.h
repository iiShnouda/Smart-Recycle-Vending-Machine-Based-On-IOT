#ifndef IDLE_MANAGER_H
#define IDLE_MANAGER_H

#include <QObject>
#include <QQmlEngine>
#include <QTimer>

/**
 * IdleManager — single source of truth for "user has gone away".
 *
 * Every page touches it on:
 *   - Component.onCompleted              (just entered the page)
 *   - StackView.onActivated              (came back to this page)
 *   - any TapHandler / sensor event      (user did something)
 *
 * One global QTimer counts down. When it fires, the QML root pops back
 * to the start page. Admin panels call disable() to opt out of the
 * timeout, then enable() when they leave.
 *
 * QML: `Idle.touch()` to reset, `Idle.disable()` for admin pages.
 */
class IdleManager : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(Idle)
    QML_SINGLETON

    Q_PROPERTY(int  timeoutMs READ timeoutMs WRITE setTimeoutMs NOTIFY timeoutMsChanged)
    Q_PROPERTY(bool enabled   READ enabled   NOTIFY enabledChanged)

public:
    explicit IdleManager(QObject *parent = nullptr);
    static IdleManager *create(QQmlEngine *, QJSEngine *) { return s_instance; }
    static IdleManager *s_instance;

    int  timeoutMs() const { return m_timeoutMs; }
    void setTimeoutMs(int ms);
    bool enabled()   const { return m_enabled; }

public slots:
    /** Restart the countdown. Call from every page on activity. */
    Q_INVOKABLE void touch();

    /** Opt out — admin panel calls this on entry. */
    Q_INVOKABLE void disable();

    /** Opt back in — admin panel calls this on exit. */
    Q_INVOKABLE void enable();

signals:
    void timedOut();              // main QML hooks this to pop to sleep
    void touched();               // any user activity (drives the LED timer)
    void timeoutMsChanged();
    void enabledChanged();

private:
    QTimer m_timer;
    int    m_timeoutMs = 60000;   // 60 s default
    bool   m_enabled   = true;
};

#endif // IDLE_MANAGER_H
