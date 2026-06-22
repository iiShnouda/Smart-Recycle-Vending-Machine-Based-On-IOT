#ifndef RECYCLE_SESSION_H
#define RECYCLE_SESSION_H

#include <QObject>
#include <QQmlEngine>
#include <QString>

/*
 * RecycleSession — the live tally for one recycle session.
 *
 * The ARDUINO owns the lane (5 IR sensors + belt + sorting servo) and runs the
 * whole sequence; the kiosk only does the camera + the counter UI. The kiosk is
 * driven by the Arduino's EVT lines (fed in via ApplicationManager →
 * onSerialLine):
 *   EVT,ENTRY              → IR1: item entered → open the counter page
 *   EVT,CAMERA             → item is under the cam → run the camera, reply VERDICT
 *   EVT,DROPPED,BOTTLE|CAN → IR2/IR3 confirmed the drop → count it + bump points
 *   EVT,REJECTED[,why]     → ejected → bump the reject count
 * And the kiosk sends back:
 *   RECYCLE 1 / RECYCLE 0          (arm / disarm the lane)
 *   VERDICT BOTTLE|CAN|REJECT      (the camera's decision)
 *
 * Points: small bottle = 1, large bottle = 2, can = 2 (1 pt = 0.4 EGP). The
 * small-vs-large split comes from the camera verdict (remembered until the drop).
 * Bin-full is decided here from the cumulative counts. Exposed to QML as a
 * singleton.
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
    Q_INVOKABLE void start();                       /* arm the Arduino lane (RECYCLE 1) */
    Q_INVOKABLE int  finish();                      /* disarm (RECYCLE 0); returns total */
    Q_INVOKABLE void setCounts(int bottles, int cans);
    Q_INVOKABLE void sendVerdict(const QString &v); /* manual override: BOTTLE|CAN|REJECT */
    Q_INVOKABLE void setBaskets(bool bottleFull, bool canFull);
    Q_INVOKABLE void emptyAluBin();
    Q_INVOKABLE void emptyPlasticBin();
    /* Reset BOTH bin progress bars to empty (can + bottle). */
    Q_INVOKABLE void resetBins();
    /* Open the live camera-detection test window (recycle_test.py) on the Pi. */
    Q_INVOKABLE void openCameraTest();

    /* The camera classifier's raw sub-class: small_bottle | large_bottle | can |
     * reject. Remembers the bottle size for scoring, then sends the
     * BOTTLE/CAN/REJECT verdict to the Arduino. */
    void onCameraVerdict(const QString &cls);

    void setSmallBottlePoints(int p);
    void setLargeBottlePoints(int p);
    void setCanPoints(int p);

    /* Fed every serial line from the Arduino by ApplicationManager. */
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
    void cameraRequested();                              /* EVT,CAMERA → classify */
    void sendCommand(const QString &cmd);                /* → Arduino (RECYCLE/VERDICT) */
    /** EVT,ENTRY → QML should push the recycle counter page. */
    void recyclePageRequested();
    /** Finish tapped → QML may pop back to home. */
    void sessionEnded();

private:
    void setLast(const QString &s);
    void creditBottle();
    void creditCan();

    int     m_smallBottles = 0;
    int     m_largeBottles = 0;
    int     m_cans     = 0;
    int     m_rejected = 0;
    int     m_smallPts = 1;
    int     m_largePts = 2;
    int     m_canPts   = 2;
    double  m_pointEGP = 0.4;               // 1 point = 0.4 EGP
    bool    m_pendingLargeBottle = false;   // last camera verdict was a large bottle?
    bool    m_active   = false;
    QString m_lastEvent;

    // Persisted cumulative bin fill + fixed capacities.
    int     m_aluBin     = 0;       // cans in the aluminium bin
    int     m_plasticBin = 0;       // bottles in the plastic bin
    static constexpr int kAluCap     = 140;
    static constexpr int kPlasticCap = 50;
};

#endif // RECYCLE_SESSION_H
