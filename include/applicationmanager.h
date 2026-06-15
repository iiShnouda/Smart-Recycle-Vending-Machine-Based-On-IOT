#ifndef APPLICATIONMANAGER_H
#define APPLICATIONMANAGER_H

#include <QObject>
#include <QString>
#include <QQmlEngine>
#include <QDebug>
#include <QThread>
#include "translationmanager.h"

class Serial_Connection;
class ReedMonitor;
class Database;
class YoloRunner;
class MqttClient;
class MachineLink;
class InventoryScanner;
class RecycleClassifier;

/**
 * ApplicationManager — single instance, exposed to QML two ways:
 *   1. Context property "appManager" (legacy access, used by existing pages)
 *   2. QML singleton "AppManager"   (modern, requires `import Recycle_Vending_Machine_LCD`)
 *
 * Owns:
 *   - TranslationManager    (UI language switching)
 *   - Serial_Connection     (lives on its own QThread; talks to STM32)
 *
 * Bridges QML calls to the serial worker via QMetaObject::invokeMethod
 * (queued connection) so the GUI never blocks.
 */
class ApplicationManager : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(AppManager)
    QML_SINGLETON

    Q_PROPERTY(QString currentLanguage READ getCurrentLanguage NOTIFY languageChanged)
    Q_PROPERTY(bool    presence        READ presence WRITE setPresence NOTIFY presenceChanged)
    Q_PROPERTY(QString kioskId         READ kioskId    CONSTANT)
    Q_PROPERTY(QString kioskName       READ kioskName  NOTIFY kioskNameChanged)

public:
    explicit ApplicationManager(QObject *parent = nullptr);
    ~ApplicationManager() override;   // stops the serial worker thread cleanly

    // QML_SINGLETON requires a static create() returning the instance to QML.
    // We hand back the single instance owned by main.cpp.
    static ApplicationManager *create(QQmlEngine *, QJSEngine *) {
        return s_instance;
    }
    static ApplicationManager *s_instance;

    void initialize();
    void setQmlEngine(QQmlEngine *engine);
    QString getCurrentLanguage() const;
    bool    presence() const { return m_presence; }

    /** This kiosk's unique ID — generated once at first run, persisted
     *  forever. Every Mongo write tags itself with this so multiple
     *  kiosks don't trample each other's data. */
    QString kioskId()    const { return m_kioskId; }
    QString kioskName()  const { return m_kioskName; }

public slots:
    void selectLanguage(const QString &languageCode);
    void setPresence(bool present);
    Q_INVOKABLE void reportTouchWake();

    // ---- Serial bridge (callable from QML) ----
    /** Send a command to the STM32.
     *  Omit timeoutMs (or pass 0/-1) to use the default 500 ms ACK window.
     *  For slow operations (motors, weighing) pass a larger timeoutMs.
     *  All STM32-side logic — stepper rotations, servo angles, load-cell
     *  reads — is invoked through this single channel. */
    Q_INVOKABLE void sendSerial(const QString &command, int timeoutMs = -1);

    // ---- Admin gate (callable from QML) ----
    /** Manually trigger the admin gate — used for desktop testing. */
    Q_INVOKABLE void devTriggerAdmin();

    /** Re-arm reed monitor after admin logs out. */
    Q_INVOKABLE void rearmReed();

    Q_INVOKABLE void setKioskName(const QString &name);

    // ---- Dispense fault inspection (called by AdminFaultsPage) ----
    /** Newest faults first; limit defaults to 100.                       */
    Q_INVOKABLE QVariantList dispenseFaults(int limit = 100);
    /** Per-slot fault summary (count + last-ts).                         */
    Q_INVOKABLE QVariantList dispenseFaultsBySlot();
    /** Wipe the fault log (admin-only action).                           */
    Q_INVOKABLE int          clearDispenseFaults();

    // ---- Inventory / restock history (called by AdminInventoryPage) ---
    Q_INVOKABLE QVariantList restockEvents(int limit = 100);
    Q_INVOKABLE QVariantList latestRestockBySlot();
    /** Force the inventory scanner to take a fresh sample right now. */
    Q_INVOKABLE void         rescanInventoryNow();

signals:
    void kioskNameChanged();
    void languageChanged(const QString &newLanguage);
    void languageChangeFailed(const QString &reason);
    void presenceChanged(bool present);
    void touchWakeRequested();

    // ---- Re-emitted from Serial_Connection so QML can react ----
    void serialConnected();
    void serialDisconnected();
    void serialReply(const QString &reply);                                  // every line
    void serialCommandSucceeded(const QString &command, const QString &reply);
    void serialCommandFailed   (const QString &command, const QString &reason);
    void serialError(const QString &message);

    /** Reed sensor / admin lifecycle */
    void adminRequested();    // magnet removed — QML should push AdminGatePage

private slots:
    void onTranslationLoaded(const QString &languageCode);
    void onTranslationFailed(const QString &reason);

    // Internal serial bridge handlers (re-emit + log)
    void onSerialConnected();
    void onSerialDisconnected();
    void onSerialReply(const QString &reply);
    void onSerialCommandSucceeded(const QString &command, const QString &reply);
    void onSerialCommandFailed   (const QString &command, const QString &reason);
    void onSerialError           (const QString &message);

private:
    TranslationManager *m_translationManager = nullptr;
    QQmlEngine         *m_qmlEngine          = nullptr;
    bool                m_presence           = true;

    // Serial worker on its own thread.
    QThread           *m_serialThread = nullptr;
    Serial_Connection *m_serial       = nullptr;

    // Cabinet LEDs (relays 1+2): on with any activity, off after 5 min idle.
    class QTimer      *m_ledTimer     = nullptr;
    bool               m_ledsOn       = false;
    void               setCabinetLeds(bool on);

    // Reed switch monitor (on GUI thread — polling at 20 Hz is cheap).
    ReedMonitor       *m_reed         = nullptr;

    // Database (SQLite + remote sync). Lives on GUI thread.
    Database          *m_database     = nullptr;

    // YOLO models (face + recycle)
    YoloRunner        *m_faceYolo     = nullptr;
    YoloRunner        *m_recycleYolo  = nullptr;

    // Recycle camera "brain" — headless YOLO sidecar that answers EVT,CAMERA
    RecycleClassifier *m_recycleClassifier = nullptr;

    // MongoDB Atlas Data API client
    MqttClient        *m_mqtt         = nullptr;
    MachineLink       *m_machineLink  = nullptr;

    // Inventory scanner — polls HX711 bank, keeps ProductsModel in sync
    InventoryScanner  *m_scanner      = nullptr;

    // Kiosk identity (persisted via QSettings on first run)
    QString            m_kioskId;
    QString            m_kioskName;
};

#endif // APPLICATIONMANAGER_H
