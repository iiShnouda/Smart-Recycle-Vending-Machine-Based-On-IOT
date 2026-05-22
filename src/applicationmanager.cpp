#include "../include/applicationmanager.h"
#include "src/Serial_Connection.h"
#include "../include/reed_monitor.h"
#include "../include/database.h"
#include "../include/logger.h"
#include "../include/admin_auth.h"
#include "../include/products_model.h"
#include "../include/diagnostics_runner.h"
#include "../include/yolo_runner.h"
#include "../include/face_service.h"
#include "../include/analytics.h"
#include "../include/logs_viewer.h"
#include "../include/mongo_client.h"
#include "../include/idle_manager.h"
#include "../include/product_image_catalog.h"

#include <QStandardPaths>
#include <QDir>
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
    new ProductsModel(this);
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
    }

    // ── Reed switch monitor ────────────────────────────────────────────
    m_reed = new ReedMonitor(this);
    connect(m_reed, &ReedMonitor::adminRequested,
            this,   &ApplicationManager::adminRequested);
    // Reed pin (BCM GPIO 22 by default — change to match your wiring).
    m_reed->start(22, true);

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

    // ── MongoDB Atlas Data API client ─────────────────────────────────
    m_mongo = new MongoClient(this);
    if (m_database) m_database->setMongoClient(m_mongo);
    // TODO: replace with your real Atlas values:
    //   1. Atlas → "Data API" → enable, get the endpoint URL
    //   2. Atlas → "API Keys" → create one with rwManageBuiltin role
     m_mongo->configure(
    "https://data.mongodb-api.com/app/data-XXXXX/endpoint/data/v1",
    "al-sbP47gMCPkSSrT1QvBejTR_gqCH8cAWyVHL8ypPb-1m",
    "Cluster0",
    "rewingo");

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
        // Triggers retranslation in QQmlApplicationEngine — every qsTr() re-runs.
        m_qmlEngine->setUiLanguage(languageCode);
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
    // Cross-thread: posts the call to the serial worker's event loop.
    QMetaObject::invokeMethod(m_serial, "sendCommand", Qt::QueuedConnection,
                              Q_ARG(QString, command),
                              Q_ARG(int,     timeoutMs));
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
    if (DiagnosticsRunner::s_instance)
        DiagnosticsRunner::s_instance->onCommandSucceeded(command, reply);
}

void ApplicationManager::onSerialCommandFailed(const QString &command,
                                               const QString &reason)
{
    qWarning() << "[App] cmd FAIL:" << command << "(" << reason << ")";
    emit serialCommandFailed(command, reason);
    if (DiagnosticsRunner::s_instance)
        DiagnosticsRunner::s_instance->onCommandFailed(command, reason);
}

void ApplicationManager::onSerialError(const QString &message)
{
    qWarning() << "[App] serial error:" << message;
    emit serialError(message);
}
