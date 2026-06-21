#ifndef RECYCLE_SESSION_H
#define RECYCLE_SESSION_H

#include <QObject>
#include <QQmlEngine>
#include <QString>

/*
 * RecycleSession — owns the live recycle sequence AND the running tally.
 *
 * The KIOSK runs the sequence (the STM32 reads the 5 IR sensors and pushes
 * EVT,IR,<1..5>,<1|0> edge events; the Arduino is a dumb belt+servo actuator).
 * Sensor roles:  IR1 inlet · IR2 mid · IR3 camera/stop · IR4 bottle-drop ·
 *                IR5 can-drop.
 *
 * Flow (all driven from onSerialLine receiving the STM32's EVT,IR events):
 *   IR1 ↑  → open the recycle counter page, belt FWD (continuous)   [Moving]
 *   IR3 ↑  → belt STOP, run the camera (3 s, ≥0.70 conf)            [AtCamera]
 *   verdict bottle → servo BOTTLE, belt FWD until IR4 ↑ drops it    [SortBottle]
 *   verdict can    → servo CAN,    belt FWD until IR5 ↑ drops it    [SortCan]
 *   verdict reject → belt REV to eject for a moment                 [Ejecting]
 *   …then back to Idle for the next item, until the user taps Finish.
 *
 * Belt/servo are issued via sendCommand() (routed to the Arduino):
 *   BELT:FWD · BELT:REV · BELT:STOP · SERVO:BOTTLE · SERVO:CAN · SERVO:NEUTRAL
 *
 * Points: small bottle = 1, large bottle = 2, can = 2 (1 pt = 0.4 EGP). The
 * small-vs-large split comes from the camera verdict (remembered until drop).
 * Bin-full is decided here from the cumulative counts (no more Arduino BASKETS).
 *
 * Point values are admin-configurable and persisted in QSettings. Exposed to
 * QML as a singleton.
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
    /* Reset BOTH bin progress bars to empty (can + bottle). */
    Q_INVOKABLE void resetBins();

    /* Open the live camera-detection test window (recycle_test.py / recycle.sh)
     * on the Pi's screen so an operator can SEE what the camera classifies.
     * Use only when idle (it grabs the CSI camera). Close it with Q. */
    Q_INVOKABLE void openCameraTest();

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
    void itemEntered();                                  /* IR1 tripped */
    void cameraRequested();                              /* IR3 → classify now */
    void sendCommand(const QString &cmd);                /* → Arduino (belt/servo) */
    /** IR1 tripped from idle → QML should push the recycle counter page. */
    void recyclePageRequested();
    /** Session ended (Finish, or auto-end) → QML may pop back to home. */
    void sessionEnded();

private:
    // ── Sequence state machine ──
    enum Phase {
        PhIdle,        // armed, waiting for an item at IR1
        PhMoving,      // belt FWD, item travelling IR1 → IR3
        PhAtCamera,    // belt stopped at IR3, classifying
        PhSortBottle,  // servo BOTTLE, belt FWD until IR4 drop
        PhSortCan,     // servo CAN,    belt FWD until IR5 drop
        PhEjecting     // belt REV, ejecting a rejected item until IR2 sees it
    };
    void setLast(const QString &s);
    void onIrEdge(int sensor, bool blocked);   // sensor 1..5
    void startSession();                       // arm + reset (auto on IR1)
    void endSession();                         // disarm + stop everything
    void maybeFinish();                        // honour a pending Finish at Idle
    void armPhaseTimeout(int ms);              // safety: never run the belt forever
    void creditBottle();
    void creditCan();
    void beltFwd();
    void beltStop();
    void beltRev();
    void servo(const QString &which);

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
    bool    m_finishRequested = false; // user tapped Finish; end at next Idle
    Phase   m_phase    = PhIdle;
    class QTimer *m_phaseTimer = nullptr;  // belt-run safety watchdog
    QString m_lastEvent;

    // Safety: max time the belt may run in ONE phase before the watchdog stops
    // it and resets (covers a jam or a sensor that never trips). 20 s per the
    // user's spec — applies to every phase including the reject eject.
    static constexpr int kPhaseTimeoutMs = 20000;

    // Persisted cumulative bin fill + fixed capacities.
    int     m_aluBin     = 0;       // cans in the aluminium bin
    int     m_plasticBin = 0;       // bottles in the plastic bin
    static constexpr int kAluCap     = 140;
    static constexpr int kPlasticCap = 50;
};

#endif // RECYCLE_SESSION_H
