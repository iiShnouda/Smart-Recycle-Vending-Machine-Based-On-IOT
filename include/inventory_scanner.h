#ifndef INVENTORY_SCANNER_H
#define INVENTORY_SCANNER_H

#include <QObject>
#include <QTimer>
#include <QString>

class ApplicationManager;

/**
 * InventoryScanner — keeps ProductsModel in sync with what's physically
 * sitting on each of the 8 shelves.
 *
 * How it works
 * ────────────
 *   • Every kPollInterval ms we send "WEIGH_ALL" to the STM32.
 *   • The STM32 replies "WEIGH_ALL:<r0>,<r1>,...,<r7>" with raw HX711 counts.
 *   • For each slot we feed the raw value into ProductsModel.ingestRawReading,
 *     which uses the stored calibration (empty_shelf_raw + unit_weight_raw)
 *     to derive the item count.
 *   • When a count *changes* between scans, the model logs a row into
 *     restock_events with a `source` tag — that's how the admin sees who
 *     touched each shelf.
 *
 * Event-driven scans we trigger immediately (not waiting for the timer):
 *   • boot — when the STM32 first connects. Source = "boot". Catches any
 *            restock that happened while the kiosk was powered off.
 *   • door-closed — when the magnet reattaches (reed signals true). Source
 *            = "admin". Counts the items the admin just refilled / removed.
 *
 * We pause polling between sending DISPENSE and receiving its reply so we
 * don't race against the dispense task on the HX711 bus. Dispense triggers
 * an immediate scan when it finishes — source = "dispense".
 */
class InventoryScanner : public QObject {
    Q_OBJECT
public:
    explicit InventoryScanner(ApplicationManager *app, QObject *parent = nullptr);

    void start();
    void stop();

    /** Suspend polling but keep the source tag for the next scan. Used by
     *  the dispense flow to avoid bus contention.                         */
    void pauseFor(const QString &source);
    void resume();

    /** Force an immediate scan with a custom source tag. */
    Q_INVOKABLE void rescanNow(const QString &source = "manual");

    bool isScanning() const { return m_pending; }

private slots:
    void onTick();
    void onSerialReply(const QString &reply);

private:
    void sendScan();

    ApplicationManager *m_app = nullptr;
    QTimer  m_timer;
    bool    m_paused      = false;
    bool    m_pending     = false;          // a WEIGH_ALL is in flight
    QString m_pendingSource;                // source tag for the in-flight scan

    static constexpr int kPollIntervalMs = 3000;   // 3 s — gentle on the bus
    static constexpr int kAckTimeoutMs   = 1500;   // HX711 read ~100 ms + slack
};

#endif // INVENTORY_SCANNER_H
