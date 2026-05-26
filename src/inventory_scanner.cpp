#include "../include/inventory_scanner.h"
#include "../include/applicationmanager.h"
#include "../include/products_model.h"
#include "../include/logger.h"

InventoryScanner::InventoryScanner(ApplicationManager *app, QObject *parent)
    : QObject(parent), m_app(app)
{
    m_timer.setInterval(kPollIntervalMs);
    connect(&m_timer, &QTimer::timeout, this, &InventoryScanner::onTick);

    // serialReply carries the actual payload (parsed for the raw values).
    connect(m_app, &ApplicationManager::serialReply,
            this,  &InventoryScanner::onSerialReply);

    // Use the command-resolution signals to flip m_pending back to false.
    // If we relied on the reply line alone, an "Error WEIGH_ALL:..." reply
    // (or a serial-layer timeout) would leave us wedged.
    connect(m_app, &ApplicationManager::serialCommandSucceeded,
            this,  [this](const QString &cmd, const QString &) {
        if (cmd == QLatin1String("WEIGH_ALL")) m_pending = false;
    });
    connect(m_app, &ApplicationManager::serialCommandFailed,
            this,  [this](const QString &cmd, const QString &reason) {
        if (cmd == QLatin1String("WEIGH_ALL")) {
            Logger::warn("Inventory", "Scan failed", { {"reason", reason} });
            m_pending = false;
        }
    });

    // Boot scan: the moment the STM32 says hello, we sweep the bank. That's
    // how we catch any restock that happened while the machine was off.
    connect(m_app, &ApplicationManager::serialConnected, this, [this]() {
        Logger::info("Inventory", "Serial up — initial bank scan");
        rescanNow("boot");
    });
}

void InventoryScanner::start()
{
    if (m_timer.isActive()) return;
    m_timer.start();
    Logger::info("Inventory",
                 QString("Scanner started (every %1 ms)").arg(kPollIntervalMs));
}

void InventoryScanner::stop()
{
    m_timer.stop();
    m_paused  = false;
    m_pending = false;
}

void InventoryScanner::pauseFor(const QString &source)
{
    m_paused = true;
    // Remember why we paused so the auto-rescan after resume gets a useful
    // source tag (typically "dispense").
    m_pendingSource = source;
}

void InventoryScanner::resume()
{
    if (!m_paused) return;
    m_paused = false;
    // One scan right away so the model reflects whatever the dispense did.
    rescanNow(m_pendingSource.isEmpty() ? "scan" : m_pendingSource);
    m_pendingSource.clear();
}

void InventoryScanner::rescanNow(const QString &source)
{
    if (m_paused)  return;         // honour the pause
    if (m_pending) return;         // one in flight already
    m_pendingSource = source;
    sendScan();
}

void InventoryScanner::onTick()
{
    if (m_paused || m_pending) return;
    m_pendingSource = "scan";
    sendScan();
}

void InventoryScanner::sendScan()
{
    m_pending = true;
    // 1.5 s ack window — well over the HX711 bank's ~100 ms read time, but
    // not so long that a missing reply blocks the next tick forever.
    m_app->sendSerial("WEIGH_ALL", kAckTimeoutMs);
}

void InventoryScanner::onSerialReply(const QString &reply)
{
    // STM32 emits "Done WEIGH_ALL:r0,r1,...,r7". The "Done " prefix is what
    // lets Serial_Connection mark the pending command resolved; we strip it
    // here to get to the payload. Errors come in as "Error WEIGH_ALL:..."
    // and we just let Serial_Connection's failure path log them.
    static const QString kPrefix = QStringLiteral("Done WEIGH_ALL:");
    if (!reply.startsWith(kPrefix)) return;

    const QString payload = reply.mid(kPrefix.size());
    const QStringList parts = payload.split(',', Qt::SkipEmptyParts);
    if (parts.size() != 8) {
        Logger::warn("Inventory", "Malformed WEIGH_ALL reply",
                     { {"reply", reply}, {"count", parts.size()} });
        return;     // m_pending cleared by serialCommandSucceeded handler
    }

    if (!ProductsModel::s_instance) return;

    // Push each cell into the model. ingestRawReading handles count math +
    // restock_events logging.
    const QString source = m_pendingSource.isEmpty() ? QStringLiteral("scan")
                                                     : m_pendingSource;
    for (int slot = 1; slot <= 8; ++slot) {
        bool ok = false;
        const int raw = parts.at(slot - 1).toInt(&ok);
        if (!ok) continue;
        ProductsModel::s_instance->ingestRawReading(slot, raw, source);
    }
    m_pendingSource.clear();
    // m_pending is cleared by the commandSucceeded handler installed in ctor.
}
