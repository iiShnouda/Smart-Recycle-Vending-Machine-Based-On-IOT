#ifndef RECYCLE_SESSION_H
#define RECYCLE_SESSION_H

#include <QObject>
#include <QQmlEngine>
#include <QString>

/*
 * RecycleSession — the live tally for one recycle session.
 *
 * Driven by the STM32's EVT lines (via ApplicationManager's serialReply):
 *   EVT,ENTRY              → "item detected"
 *   EVT,CAMERA             → ask the camera to classify (cameraRequested)
 *   EVT,DROPPED,BOTTLE|CAN → accepted: bump count + points
 *   EVT,REJECTED,<why>     → rejected: bump reject count + reason
 *
 * Point values (bottle/can) are configurable from the admin panel and
 * persisted in QSettings. Exposed to QML as a singleton.
 */
class RecycleSession : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(RecycleSession)
    QML_SINGLETON

    Q_PROPERTY(int  bottles      READ bottles      NOTIFY countsChanged)
    Q_PROPERTY(int  cans         READ cans         NOTIFY countsChanged)
    Q_PROPERTY(int  rejected     READ rejected     NOTIFY countsChanged)
    Q_PROPERTY(int  totalPoints  READ totalPoints  NOTIFY countsChanged)
    Q_PROPERTY(int  bottlePoints READ bottlePoints WRITE setBottlePoints NOTIFY configChanged)
    Q_PROPERTY(int  canPoints    READ canPoints    WRITE setCanPoints    NOTIFY configChanged)
    Q_PROPERTY(bool active       READ active       NOTIFY activeChanged)
    Q_PROPERTY(QString lastEvent READ lastEvent    NOTIFY lastEventChanged)

public:
    explicit RecycleSession(QObject *parent = nullptr);
    static RecycleSession *create(QQmlEngine *, QJSEngine *) { return s_instance; }
    static RecycleSession *s_instance;

    int  bottles()      const { return m_bottles;  }
    int  cans()         const { return m_cans;     }
    int  rejected()     const { return m_rejected; }
    int  totalPoints()  const { return m_bottles * m_bottlePts + m_cans * m_canPts; }
    int  bottlePoints() const { return m_bottlePts; }
    int  canPoints()    const { return m_canPts;   }
    bool active()       const { return m_active;   }
    QString lastEvent() const { return m_lastEvent; }

public slots:
    Q_INVOKABLE void start();                       /* reset + arm the lane */
    Q_INVOKABLE int  finish();                      /* disarm; returns total */
    Q_INVOKABLE void sendVerdict(const QString &v); /* "BOTTLE"|"CAN"|"REJECT" */
    Q_INVOKABLE void setBaskets(bool bottleFull, bool canFull);

    void setBottlePoints(int p);
    void setCanPoints(int p);

    /* Fed every serial line by ApplicationManager. */
    void onSerialLine(const QString &line);

signals:
    void countsChanged();
    void configChanged();
    void activeChanged();
    void lastEventChanged();

    void itemAccepted(const QString &type, int points);  /* drives the coin pop */
    void itemRejected(const QString &reason);
    void cameraRequested();                              /* EVT,CAMERA */
    void sendCommand(const QString &cmd);                /* → serial out */

private:
    void setLast(const QString &s);

    int     m_bottles  = 0;
    int     m_cans     = 0;
    int     m_rejected = 0;
    int     m_bottlePts = 1;
    int     m_canPts    = 2;
    bool    m_active   = false;
    QString m_lastEvent;
};

#endif // RECYCLE_SESSION_H
