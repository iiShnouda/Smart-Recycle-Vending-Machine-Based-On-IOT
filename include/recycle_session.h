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
 * Points (per item type):
 *   small bottle = 1,  large bottle = 2,  can = 2.   (1 point = 0.4 EGP)
 * The STM32 only sorts BOTTLE vs CAN (two baskets); the small-vs-large
 * distinction comes from the CAMERA verdict, so we remember the last
 * classified bottle size and award the right points when it drops.
 *
 * Point values are admin-configurable and persisted in QSettings. Exposed
 * to QML as a singleton.
 */
class RecycleSession : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(RecycleSession)
    QML_SINGLETON

    Q_PROPERTY(int  bottles       READ bottles       NOTIFY countsChanged)
    Q_PROPERTY(int  smallBottles  READ smallBottles  NOTIFY countsChanged)
    Q_PROPERTY(int  largeBottles  READ largeBottles  NOTIFY countsChanged)
    Q_PROPERTY(int  cans          READ cans          NOTIFY countsChanged)
    Q_PROPERTY(int  rejected      READ rejected      NOTIFY countsChanged)
    Q_PROPERTY(int  totalPoints   READ totalPoints   NOTIFY countsChanged)
    Q_PROPERTY(int  smallBottlePoints READ smallBottlePoints WRITE setSmallBottlePoints NOTIFY configChanged)
    Q_PROPERTY(int  largeBottlePoints READ largeBottlePoints WRITE setLargeBottlePoints NOTIFY configChanged)
    Q_PROPERTY(int  canPoints     READ canPoints     WRITE setCanPoints     NOTIFY configChanged)
    Q_PROPERTY(double pointValueEGP READ pointValueEGP NOTIFY configChanged)
    Q_PROPERTY(bool active        READ active        NOTIFY activeChanged)
    Q_PROPERTY(QString lastEvent  READ lastEvent     NOTIFY lastEventChanged)
    // Physical bin fill — CUMULATIVE across sessions, persisted. The aluminium
    // (can) bin holds 140; the plastic (bottle) bin holds 50. Progress bars on
    // the recycle page fill from these as the camera accepts items.
    Q_PROPERTY(int  aluBinCount     READ aluBinCount     NOTIFY binChanged)
    Q_PROPERTY(int  plasticBinCount READ plasticBinCount NOTIFY binChanged)
    Q_PROPERTY(int  aluBinCap       READ aluBinCap       CONSTANT)
    Q_PROPERTY(int  plasticBinCap   READ plasticBinCap   CONSTANT)
    Q_PROPERTY(bool aluBinFull      READ aluBinFull      NOTIFY binChanged)
    Q_PROPERTY(bool plasticBinFull  READ plasticBinFull  NOTIFY binChanged)

public:
    explicit RecycleSession(QObject *parent = nullptr);
    static RecycleSession *create(QQmlEngine *, QJSEngine *) { return s_instance; }
    static RecycleSession *s_instance;

    int  bottles()      const { return m_smallBottles + m_largeBottles; }
    int  smallBottles() const { return m_smallBottles; }
    int  largeBottles() const { return m_largeBottles; }
    int  cans()         const { return m_cans;     }
    int  rejected()     const { return m_rejected; }
    int  totalPoints()  const { return m_smallBottles * m_smallPts
                                     + m_largeBottles * m_largePts
                                     + m_cans         * m_canPts;   }
    int  smallBottlePoints() const { return m_smallPts; }
    int  largeBottlePoints() const { return m_largePts; }
    int  canPoints()         const { return m_canPts;   }
    double pointValueEGP()   const { return m_pointEGP; }
    bool active()       const { return m_active;   }
    QString lastEvent() const { return m_lastEvent; }
    int  aluBinCount()     const { return m_aluBin; }
    int  plasticBinCount() const { return m_plasticBin; }
    int  aluBinCap()       const { return kAluCap; }
    int  plasticBinCap()   const { return kPlasticCap; }
    bool aluBinFull()      const { return m_aluBin >= kAluCap; }
    bool plasticBinFull()  const { return m_plasticBin >= kPlasticCap; }

public slots:
    Q_INVOKABLE void start();                       /* reset + arm the lane */
    Q_INVOKABLE int  finish();                      /* disarm; returns total */
    Q_INVOKABLE void sendVerdict(const QString &v); /* "BOTTLE"|"CAN"|"REJECT" */
    Q_INVOKABLE void setBaskets(bool bottleFull, bool canFull);
    /* Admin emptied a physical bin → reset its fill counter to 0. */
    Q_INVOKABLE void emptyAluBin();
    Q_INVOKABLE void emptyPlasticBin();

    /* The camera classifier's raw sub-class: "small_bottle" | "large_bottle"
     * | "can" | "reject". Remembers the bottle size for scoring, then sends
     * the BOTTLE/CAN/REJECT verdict the STM32 understands. */
    void onCameraVerdict(const QString &cls);

    void setSmallBottlePoints(int p);
    void setLargeBottlePoints(int p);
    void setCanPoints(int p);

    /* Fed every serial line by ApplicationManager. */
    void onSerialLine(const QString &line);

signals:
    void countsChanged();
    void configChanged();
    void activeChanged();
    void lastEventChanged();
    void binChanged();

    void itemAccepted(const QString &type, int points);  /* drives the coin pop */
    void itemRejected(const QString &reason);
    void itemEntered();                                  /* EVT,ENTRY (IR1) */
    void cameraRequested();                              /* EVT,CAMERA */
    void sendCommand(const QString &cmd);                /* → serial out */

private:
    void setLast(const QString &s);

    int     m_smallBottles = 0;
    int     m_largeBottles = 0;
    int     m_cans     = 0;
    int     m_rejected = 0;
    int     m_smallPts = 1;
    int     m_largePts = 2;
    int     m_canPts   = 2;
    double  m_pointEGP = 0.4;          // 1 point = 0.4 EGP
    bool    m_pendingLargeBottle = false;  // last camera verdict was large?
    bool    m_active   = false;
    QString m_lastEvent;

    // Persisted cumulative bin fill + fixed capacities.
    int     m_aluBin     = 0;       // cans in the aluminium bin
    int     m_plasticBin = 0;       // bottles in the plastic bin
    static constexpr int kAluCap     = 140;
    static constexpr int kPlasticCap = 50;
};

#endif // RECYCLE_SESSION_H
