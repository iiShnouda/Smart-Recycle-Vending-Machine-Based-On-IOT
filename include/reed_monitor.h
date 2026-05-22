#ifndef REED_MONITOR_H
#define REED_MONITOR_H

#include <QObject>
#include <QTimer>
#include <QString>

/**
 * ReedMonitor — polls the door's reed switch on a Pi GPIO line.
 *
 * Logic:
 *   - Reed closed (magnet present) → door installed → kiosk running normally
 *   - Reed open   (magnet away)    → service mode  → emit adminRequested()
 *
 * Includes 200 ms debounce so a wobbling magnet doesn't trigger multiple times.
 *
 * Once admin mode is ENTERED, this class stops emitting until manually
 * re-armed (call rearm() after admin logs out). Per the spec: closing the
 * door back does NOT cancel admin mode — only admin's logout button does.
 *
 * Linux GPIO access:
 *   - Tries /sys/class/gpio (works on Pi 4 and most Pi 5 setups)
 *   - On Windows/macOS dev machines: simulation mode — call triggerForDev()
 *     manually for testing.
 */
class ReedMonitor : public QObject {
    Q_OBJECT
public:
    explicit ReedMonitor(QObject *parent = nullptr);
    ~ReedMonitor() override;

    /** gpioNumber: the BCM GPIO number on the Pi (e.g. 22 = "GPIO22").
     *  activeLow: true if your reed pulls LOW when magnet present. */
    bool start(int gpioNumber, bool activeLow = true);
    void stop();

    /** Tell the monitor admin is done — start firing adminRequested again
     *  on the next magnet-away event. */
    void rearm();

    bool isMagnetPresent() const { return m_magnetPresent; }

signals:
    /** Fired exactly once when magnet leaves and we're armed. */
    void adminRequested();

    void magnetStateChanged(bool present);

public slots:
    /** Dev hook — simulate magnet removed from a keypress in QML. */
    void triggerForDev();

private slots:
    void poll();
    void onDebounceTimeout();

private:
    bool exportGpio(int gpio);
    bool readGpio(int gpio, bool &outValue);

    QTimer  m_pollTimer;
    QTimer  m_debounceTimer;
    int     m_gpio          = -1;
    bool    m_activeLow     = true;
    bool    m_magnetPresent = true;     // assume installed at boot
    bool    m_armed         = true;
    bool    m_pendingState  = true;
};

#endif // REED_MONITOR_H
