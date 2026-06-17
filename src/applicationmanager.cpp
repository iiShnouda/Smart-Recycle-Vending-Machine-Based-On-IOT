#include "../include/applicationmanager.h"
#include "../include/Serial_Connection.h"
#include "../include/recycle_session.h"
#include "../include/reed_monitor.h"
#include "../include/database.h"
#include "../include/logger.h"
#include "../include/admin_auth.h"
#include "../include/products_model.h"
#include "../include/diagnostics_runner.h"
#include "../include/yolo_runner.h"
#include "../include/recycle_classifier.h"
#include "../include/barcode_scanner.h"
#include "../include/face_preview_feeder.h"
#include "../include/face_service.h"
#include "../include/face_rec_sidecar.h"
#include "../include/analytics.h"
#include "../include/logs_viewer.h"
#include "../include/mqtt_client.h"
#include "../include/machine_link.h"
#include "../include/idle_manager.h"
#include <QTimer>
#include "../include/product_image_catalog.h"
#include "../include/inventory_scanner.h"
#include "../include/product_catalog.h"
#include "../include/off_client.h"
#include "../include/update_checker.h"

#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QNetworkInterface>
#include <QHostAddress>
#include <QSettings>
#include <QUuid>

// Static instance pointer used by the QML_SINGLETON create() callback.
ApplicationManager *ApplicationManager::s_instance = nullptr;

ApplicationManager::ApplicationManager(QObject *parent)
    : QObject(parent)
{
    // Remember ourselves so QML_SINGLETON::create() can hand us back.
    if (!s_instance) s_instance = this;
}

ApplicationManager::~ApplicationManager()
{
    // Shut down the serial worker thread cleanly so we don't get
    // "QThread: Destroyed while thread is still running" on exit.
    if (m_serialThread) {
        if (m_serial) {
            // Ask the worker to release the port (queued, runs on its thread).
            QMetaObject::invokeMethod(m_serial, "stop", Qt::BlockingQueuedConnection);
        }
        m_serialThread->quit();      // ask the event loop to exit
        m_serialThread->wait(2000);  // join (max 2 s)
    }
    if (s_instance == this) s_instance = nullptr;
}

// =========================================================================
// initialize() — set up translation manager + start the serial worker thread
// =========================================================================
void ApplicationManager::initialize()
{
    qInfo() << "========================================";
    qInfo() << "[INIT] Initializing ApplicationManager...";

    // ── Create every QML_SINGLETON instance up-front ───────────────────
    // The `s_instance` pattern means QML's singleton lookup returns whatever
    // we put there. If we don't create them BEFORE the first QML access,
    // calls like `Idle.disable()` may silently no-op. So do it here, in
    // order, before any QML page has a chance to load.
    new IdleManager(this);
    new AdminAuth(this);
    new FaceService(this);
    new FaceRecSidecar(this);
    // Admin gate authenticates with the SAME face recogniser, then checks the
    // matched face's role in faces.db (only "admin" unlocks).
    if (AdminAuth::s_instance && FaceRecSidecar::s_instance)
        AdminAuth::s_instance->bindFace(FaceRecSidecar::s_instance);
    new BarcodeScanner(this);     // QML singleton: admin "scan a product" flow
    new FacePreviewFeeder(this);  // QML singleton: live face preview (data URL)
    new ProductsModel(this);
    new ProductCatalog(this);
    new OpenFoodFactsClient(this);
    new UpdateChecker(this);
    new DiagnosticsRunner(this);
    new Analytics(this);
    new LogsViewer(this);
    new ProductImageCatalog(this);

    // ── Kiosk identity (persisted via QSettings) ────────────────────────
    // Each physical kiosk has its own ID so its products / transactions
    // in MongoDB don't conflict with other kiosks. Generated once on the
    // very first launch, then never changes.
    {
        QSettings s;
        m_kioskId   = s.value("kiosk/id").toString();
        m_kioskName = s.value("kiosk/name",
                              QStringLiteral("ReWinGo Kiosk")).toString();
        if (m_kioskId.isEmpty()) {
            m_kioskId = QUuid::createUuid().toString(QUuid::WithoutBraces);
            s.setValue("kiosk/id", m_kioskId);
            qInfo() << "[INIT] New kiosk_id assigned:" << m_kioskId;
        } else {
            qInfo() << "[INIT] Kiosk ID:" << m_kioskId;
        }
    }

    // ── Translations ────────────────────────────────────────────────────
    m_translationManager = new TranslationManager(this);

    connect(m_translationManager, &TranslationManager::translationLoaded,
            this, &ApplicationManager::onTranslationLoaded);
    connect(m_translationManager, &TranslationManager::translationFailed,
            this, &ApplicationManager::onTranslationFailed);

    m_translationManager->loadTranslation("en");

    // ── Serial worker thread ────────────────────────────────────────────
    // The Serial_Connection object is constructed on this (GUI) thread,
    // then handed off to a dedicated worker thread. After moveToThread(),
    // every signal/slot it processes runs on that worker.
    m_serialThread = new QThread(this);
    m_serial       = new Serial_Connection();         // no parent — required for moveToThread
    m_serial->moveToThread(m_serialThread);

    // Auto-cleanup when the thread quits.
    connect(m_serialThread, &QThread::finished,
            m_serial,       &QObject::deleteLater);

    // Bridge worker signals → our internal handlers (and on to QML).
    connect(m_serial, &Serial_Connection::connected,
            this,     &ApplicationManager::onSerialConnected);
    connect(m_serial, &Serial_Connection::disconnected,
            this,     &ApplicationManager::onSerialDisconnected);
    connect(m_serial, &Serial_Connection::replyReceived,
            this,     &ApplicationManager::onSerialReply);
    connect(m_serial, &Serial_Connection::commandSucceeded,
            this,     &ApplicationManager::onSerialCommandSucceeded);
    connect(m_serial, &Serial_Connection::commandFailed,
            this,     &ApplicationManager::onSerialCommandFailed);
    connect(m_serial, &Serial_Connection::errorOccurred,
            this,     &ApplicationManager::onSerialError);

    m_serialThread->start();

    // Cross-thread call: tells the worker to start listening for the STM32.
    // "AUTO" → auto-detect by USB VID:PID (0x0483:0x5740 by default).
    QMetaObject::invokeMethod(m_serial, "start", Qt::QueuedConnection,
                              Q_ARG(QString, "AUTO"),
                              Q_ARG(int,     115200));

    // ── Logger ──────────────────────────────────────────────────────────
    {
        const QString dataDir = QStandardPaths::writableLocation(
            QStandardPaths::AppDataLocation);
        QDir().mkpath(dataDir + "/logs");
        Logger::instance()->start(dataDir + "/logs", 1*1024*1024, 5);
        Logger::info("App", "Boot");
    }

    // ── Database ────────────────────────────────────────────────────────
    {
        const QString dataDir = QStandardPaths::writableLocation(
            QStandardPaths::AppDataLocation);
        QDir().mkpath(dataDir);
        m_database = new Database(this);
        m_database->setKioskId(m_kioskId);
        m_database->open(dataDir + "/rewingo.db");
        //m_database->setRemoteSync("https://your.api/audit", "API_KEY");

        // Funnel audit events from Logger → DB → remote sync
        connect(Logger::instance(), &Logger::auditEvent,
                m_database, &Database::onAuditEvent);

        // Tell ProductsModel about the DB so it can populate from it.
        if (ProductsModel::s_instance)
            ProductsModel::s_instance->setDatabase(m_database);

        // Shared product catalog (across kiosks via Mongo). The singleton
        // is created automatically by the QML engine via QML_SINGLETON;
        // we only need to feed it the DB handle.
        if (ProductCatalog::s_instance)
            ProductCatalog::s_instance->setDatabase(m_database);
    }

    // ── Reed switch monitor ────────────────────────────────────────────
    // The reed switch lives on the STM32, not Pi GPIO. We kept the
    // ReedMonitor class around as a dev fallback (triggerForDev()) but no
    // longer wire it to a real GPIO line — the STM32 pushes DOOR:OPEN /
    // DOOR:CLOSED on the USB-CDC stream and we listen for those below.
    m_reed = new ReedMonitor(this);
    connect(m_reed, &ReedMonitor::adminRequested,
            this,   &ApplicationManager::adminRequested);
    // m_reed->start(22, true);   // ← DISABLED: reed is on STM32, not Pi

    // ── Inventory scanner ──────────────────────────────────────────────
    // Created AFTER products model + database are wired so the boot scan
    // can land into a ready model. The scanner auto-runs an initial scan
    // when Serial_Connection emits `connected`.
    m_scanner = new InventoryScanner(this, this);
    m_scanner->start();

    // STM32 reed events. The firmware pushes "DOOR:OPEN" when the magnet
    // leaves and "DOOR:CLOSED" when it returns. We listen via serialReply
    // (every line, ack or not) so admins can:
    //   - OPEN  → enter admin mode (same effect as ReedMonitor::adminRequested)
    //   - CLOSED → trigger an inventory rescan tagged "admin"
    connect(this, &ApplicationManager::serialReply, this,
            [this](const QString &line) {
        if (line.startsWith("DOOR:OPEN")) {
            Logger::audit("Reed", "Admin door opened (STM32)");
            emit adminRequested();
        } else if (line.startsWith("DOOR:CLOSED")) {
            Logger::info("Inventory", "Admin door closed (STM32) — rescan");
            if (m_scanner) m_scanner->rescanNow("admin");
        }
    });

    // ── Recycle session ────────────────────────────────────────────────
    // The recycle counter (RecycleSession) is driven by the STM32's EVT
    // lines and sends RECYCLE/VERDICT/BASKETS back out over serial.
    new RecycleSession(this);
    if (RecycleSession::s_instance) {
        connect(this, &ApplicationManager::serialReply,
                RecycleSession::s_instance, &RecycleSession::onSerialLine);
        connect(RecycleSession::s_instance, &RecycleSession::sendCommand,
                this, [this](const QString &cmd){ sendSerial(cmd, 0); });

        // Camera "brain": EVT,CAMERA → run the headless recycle classifier
        // (CSI cam + YOLO) → send VERDICT BOTTLE|CAN|REJECT back to the STM32.
        m_recycleClassifier = new RecycleClassifier(this);
        connect(RecycleSession::s_instance, &RecycleSession::cameraRequested,
                m_recycleClassifier, &RecycleClassifier::classify);
        connect(m_recycleClassifier, &RecycleClassifier::verdict,
                RecycleSession::s_instance, &RecycleSession::onCameraVerdict);
    }

    // ── Cabinet LEDs (relays 1+2) ───────────────────────────────────────
    // Lit whenever someone's using the machine; off after 5 minutes with
    // no touch. Any activity (IdleManager::touched) relights them and
    // restarts the countdown.
    m_ledTimer = new QTimer(this);
    m_ledTimer->setSingleShot(true);
    m_ledTimer->setInterval(5 * 60 * 1000);          // 5 minutes
    connect(m_ledTimer, &QTimer::timeout, this, [this]() { setCabinetLeds(false); });
    if (IdleManager::s_instance) {
        connect(IdleManager::s_instance, &IdleManager::touched, this, [this]() {
            if (!m_ledsOn) setCabinetLeds(true);
            m_ledTimer->start();                     // restart 5-min countdown
        });
    }
    setCabinetLeds(true);                            // on at boot
    m_ledTimer->start();

    // ── YOLO models + face service ─────────────────────────────────────
    {
        const QString dataDir = QStandardPaths::writableLocation(
            QStandardPaths::AppDataLocation);
        m_faceYolo = new YoloRunner(this);
        m_faceYolo->loadModel(dataDir + "/models/face.onnx",
                              dataDir + "/models/face.names",
                              640, 640);

        m_recycleYolo = new YoloRunner(this);
        m_recycleYolo->loadModel(dataDir + "/models/recycle.onnx",
                                 dataDir + "/models/recycle.names",
                                 640, 640);

        if (FaceService::s_instance) {
            FaceService::s_instance->setRunner(m_faceYolo);
            FaceService::s_instance->setDatabase(m_database);
        }
    }

    // ── MQTT client ────────────────────────────────────────────────────
    //
    // The kiosk publishes telemetry over MQTT directly — no Flask backend,
    // no MongoDB driver baked in. A subscriber on the cloud / your phone /
    // a dashboard listens on rewingo/<kiosk-id>/# for everything.
    //
    // Broker host + credentials live in /etc/rewingo/.env so they can be
    // changed without rebuilding. Keys we read (all optional):
    //   MQTT_HOST     mosquitto broker hostname              (no default)
    //   MQTT_PORT     port; 8883 for TLS, 1883 for plain     (default 8883)
    //   MQTT_USERNAME (no default)
    //   MQTT_PASSWORD (no default)
    //   MQTT_TLS      "1"/"0" — toggle TLS                   (default "1")
    //
    // If MQTT_HOST is empty the kiosk runs offline-only — SQLite still
    // captures every event locally; nothing leaves the box.
    m_mqtt = new MqttClient(this);
    if (m_database) m_database->setMqttClient(m_mqtt);

    QString mqttHost, mqttUser, mqttPass;
    int     mqttPort = 8883;
    bool    mqttTls  = true;
    {
        // Broker host + creds live in a .env file. Linux: /etc/rewingo/.env.
        // Windows (dev/testing): %APPDATA%/<org>/.env, then C:/rewingo/.env.
        QString envPath = QStringLiteral("/etc/rewingo/.env");
#ifdef Q_OS_WIN
        const QString winCfg =
            QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
            + QStringLiteral("/.env");
        if (QFile::exists(winCfg))
            envPath = winCfg;
        else if (QFile::exists(QStringLiteral("C:/rewingo/.env")))
            envPath = QStringLiteral("C:/rewingo/.env");
        else
            envPath = winCfg;                    // for the log line below
#endif
        QFile env(envPath);
        if (env.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qInfo() << "[INIT] MQTT config from" << envPath;
            while (!env.atEnd()) {
                QString line = QString::fromUtf8(env.readLine()).trimmed();
                if (line.isEmpty() || line.startsWith('#')) continue;
                const int eq = line.indexOf('=');
                if (eq < 0) continue;
                const QString key = line.left(eq).trimmed();
                QString       val = line.mid(eq + 1).trimmed();
                if (val.startsWith('"') && val.endsWith('"'))
                    val = val.mid(1, val.size() - 2);
                if      (key == "MQTT_HOST")     mqttHost = val;
                else if (key == "MQTT_PORT")     mqttPort = val.toInt();
                else if (key == "MQTT_USERNAME") mqttUser = val;
                else if (key == "MQTT_PASSWORD") mqttPass = val;
                else if (key == "MQTT_TLS")      mqttTls  = (val != "0");
            }
        }
    }

    if (mqttHost.isEmpty()) {
        qWarning() << "[INIT] MQTT_HOST not set in /etc/rewingo/.env — "
                      "running offline (telemetry won't reach the cloud).";
    } else {
        m_mqtt->configure(mqttHost, mqttPort, mqttUser, mqttPass,
                          m_kioskId, mqttTls);
        m_mqtt->setTopicBase(QStringLiteral("rewingo/") + m_kioskId);
        m_mqtt->connectToBroker();
    }

    // ── Machine link (QR sign-in) ───────────────────────────────────────
    // Fixed machine QR ("REWINGO:<kioskId>"). The linked user arrives over
    // MQTT on rewingo/<kioskId>/login (published by the backend after the
    // phone app scans). machineId == kiosk id. Configured AFTER MqttClient
    // so it can hook onto MqttClient::s_instance.
    m_machineLink = new MachineLink(this);
    m_machineLink->configure(m_kioskId);

    // ── Update checker ─────────────────────────────────────────────────
    // Owner/name as it appears in the GitHub URL — bake yours in here,
    // OR override at runtime via QSettings("updater/repo", "owner/name").
    if (UpdateChecker::s_instance) {
        QSettings s;
        const QString repo =
            s.value("updater/repo",
                    QStringLiteral("iiShnouda/Smart-Recycle-Vending-Machine-Based-On-IOT")).toString();
        UpdateChecker::s_instance->configure(repo);
        // Delay the first hit by 5 s so it doesn't share CPU with USB
        // enumeration. After that, every 6 h.
        UpdateChecker::s_instance->start(5000);
    }

    qInfo() << "[INIT] ApplicationManager initialized successfully";
    qInfo() << "========================================";
}

void ApplicationManager::setQmlEngine(QQmlEngine *engine)
{
    m_qmlEngine = engine;
}

// =========================================================================
// Language switching
// =========================================================================

QString ApplicationManager::getCurrentLanguage() const
{
    return m_translationManager ? m_translationManager->currentLanguage() : "en";
}

void ApplicationManager::selectLanguage(const QString &languageCode)
{
    if (!m_translationManager) {
        emit languageChangeFailed("Translation manager not initialized");
        return;
    }
    m_translationManager->loadTranslation(languageCode);
}

void ApplicationManager::onTranslationLoaded(const QString &languageCode)
{
    if (m_qmlEngine) {
        // setUiLanguage updates the locale used by qsTr; retranslate() walks
        // every bound qsTr() expression and re-evaluates it. We call both —
        // in Qt 6.4 setUiLanguage alone doesn't always rebind existing pages.
        m_qmlEngine->setUiLanguage(languageCode);
        m_qmlEngine->retranslate();
    }
    emit languageChanged(languageCode);
}

void ApplicationManager::onTranslationFailed(const QString &reason)
{
    emit languageChangeFailed(reason);
}

// =========================================================================
// Presence (idle/wake state — set by sensor or touch)
// =========================================================================

void ApplicationManager::setPresence(bool present)
{
    if (m_presence == present) return;             // nothing changed → skip
    m_presence = present;
    emit presenceChanged(m_presence);
}

void ApplicationManager::reportTouchWake()
{
    emit touchWakeRequested();
}

// =========================================================================
// Serial bridge — Q_INVOKABLE callable from QML
// =========================================================================

void ApplicationManager::sendSerial(const QString &command, int timeoutMs)
{
    if (!m_serial) return;
    // Negative or zero → tell Serial_Connection to use its default (500 ms).
    if (timeoutMs <= 0) timeoutMs = Serial_Connection::kDefaultAckMs;

    // Pause inventory polling whenever a DISPENSE is in flight so the
    // scanner doesn't race the dispense task on the HX711 bus. The
    // dispense itself does a weigh-before + weigh-after internally —
    // we'll resume + rescan once the reply lands.
    if (m_scanner && command.startsWith("DISPENSE:")) {
        m_scanner->pauseFor("dispense");
    }

    // Cross-thread: posts the call to the serial worker's event loop.
    QMetaObject::invokeMethod(m_serial, "sendCommand", Qt::QueuedConnection,
                              Q_ARG(QString, command),
                              Q_ARG(int,     timeoutMs));
}

void ApplicationManager::setCabinetLeds(bool on)
{
    if (m_ledsOn == on) return;
    m_ledsOn = on;
    // Relay 1 = vending light, Relay 2 = bottom LED (the STM32 switches
    // them via the 2N2222 driver). Fire-and-forget; no-op if no STM32.
    sendSerial(QStringLiteral("RELAY 1 %1").arg(on ? 1 : 0), 0);
    sendSerial(QStringLiteral("RELAY 2 %1").arg(on ? 1 : 0), 0);
    Logger::info("LED", on ? "Cabinet LEDs on" : "Cabinet LEDs off (idle)");
}

void ApplicationManager::devTriggerAdmin()
{
    if (m_reed) m_reed->triggerForDev();
}

void ApplicationManager::rearmReed()
{
    if (m_reed) m_reed->rearm();
}

void ApplicationManager::setKioskName(const QString &name)
{
    if (name.isEmpty() || name == m_kioskName) return;
    m_kioskName = name;
    QSettings().setValue("kiosk/name", name);
    emit kioskNameChanged();
}

QString ApplicationManager::localIp() const
{
    // First non-loopback, running IPv4 address — what the phone uses to reach
    // the kiosk-backend on this Pi. Prefer wlan/eth over virtual interfaces.
    const auto ifaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &iface : ifaces) {
        const auto flags = iface.flags();
        if (!(flags & QNetworkInterface::IsUp) ||
            !(flags & QNetworkInterface::IsRunning) ||
            (flags & QNetworkInterface::IsLoopBack))
            continue;
        for (const QNetworkAddressEntry &entry : iface.addressEntries()) {
            const QHostAddress ip = entry.ip();
            if (ip.protocol() == QAbstractSocket::IPv4Protocol &&
                !ip.isLoopback())
                return ip.toString();
        }
    }
    return QStringLiteral("unavailable");
}

// =========================================================================
// Serial bridge — re-emit signals so QML can listen
// =========================================================================

void ApplicationManager::onSerialConnected()
{
    qInfo() << "[App] STM32 connected";
    emit serialConnected();
}

void ApplicationManager::onSerialDisconnected()
{
    qWarning() << "[App] STM32 disconnected";
    emit serialDisconnected();
}

void ApplicationManager::onSerialReply(const QString &reply)
{
    qDebug() << "[App] STM32 ->" << reply;
    emit serialReply(reply);
}

void ApplicationManager::onSerialCommandSucceeded(const QString &command,
                                                  const QString &reply)
{
    qInfo() << "[App] cmd OK :" << command << "<-" << reply;
    emit serialCommandSucceeded(command, reply);

    // Dispense finished cleanly → let the scanner take a fresh sample so the
    // model reflects the just-dropped item. Source tag stays "dispense".
    if (m_scanner && command.startsWith("DISPENSE:")) {
        m_scanner->resume();
    }

    if (DiagnosticsRunner::s_instance)
        DiagnosticsRunner::s_instance->onCommandSucceeded(command, reply);
}

void ApplicationManager::onSerialCommandFailed(const QString &command,
                                               const QString &reason)
{
    qWarning() << "[App] cmd FAIL:" << command << "(" << reason << ")";
    emit serialCommandFailed(command, reason);

    // Persist DISPENSE failures so the admin can audit them after the fact.
    // Reason format from the STM32:
    //   "Error DISPENSE:<slot>:<code>:before=<g>:after=<g>:drop=<g>:index=<n>"
    // The code is one of STALL / NO_DROP / STEP_LOSS / TIMEOUT / BUSY. If
    // we got here from a serial-layer timeout the reason is just "timeout"
    // and the parsing below leaves the weights at 0 — still useful.
    if (m_database && command.startsWith("DISPENSE:")) {
        const int slot = command.section(':', 1, 1).toInt();
        QString reasonCode = "TIMEOUT";
        int before = 0, after = 0, drop = 0, idx = 0;

        const QStringList parts = reason.split(':');
        if (parts.size() >= 3 && parts[0].startsWith("Error")) {
            reasonCode = parts[2];
            for (const QString &p : parts) {
                if      (p.startsWith("before=")) before = p.mid(7).toInt();
                else if (p.startsWith("after="))  after  = p.mid(6).toInt();
                else if (p.startsWith("drop="))   drop   = p.mid(5).toInt();
                else if (p.startsWith("index="))  idx    = p.mid(6).toInt();
            }
        }
        m_database->recordDispenseFault(slot, reasonCode,
                                        before, after, drop, idx);
    }

    // Dispense failed too — scanner needs to come back up either way.
    if (m_scanner && command.startsWith("DISPENSE:")) {
        m_scanner->resume();
    }

    if (DiagnosticsRunner::s_instance)
        DiagnosticsRunner::s_instance->onCommandFailed(command, reason);
}

QVariantList ApplicationManager::dispenseFaults(int limit)
{
    return m_database ? m_database->listDispenseFaults(limit) : QVariantList{};
}

QVariantList ApplicationManager::dispenseFaultsBySlot()
{
    return m_database ? m_database->faultsBySlot() : QVariantList{};
}

int ApplicationManager::clearDispenseFaults()
{
    return m_database ? m_database->clearDispenseFaults() : 0;
}

QVariantList ApplicationManager::restockEvents(int limit)
{
    return m_database ? m_database->listRestockEvents(limit) : QVariantList{};
}

QVariantList ApplicationManager::latestRestockBySlot()
{
    return m_database ? m_database->latestRestockBySlot() : QVariantList{};
}

void ApplicationManager::rescanInventoryNow()
{
    if (m_scanner) m_scanner->rescanNow("manual");
}

void ApplicationManager::onSerialError(const QString &message)
{
    qWarning() << "[App] serial error:" << message;
    emit serialError(message);
}
