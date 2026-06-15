#ifndef BARCODE_SCANNER_H
#define BARCODE_SCANNER_H

#include <QObject>
#include <QQmlEngine>
#include <QProcess>
#include <QPointer>
#include <QTimer>

/*
 * BarcodeScanner — "show the camera a product, get its barcode".
 *
 * Admin flow: tap "Scan product" → we launch a headless Python sidecar
 * (barcode_scan.py) that opens the camera and decodes a 1D barcode / QR,
 * then emits scanned(code). QML hands the code to OffClient.lookupBarcode()
 * to fetch name + image + weight, and shows a popup that suggests a slot and
 * lets the admin set the points.
 *
 * Exposed to QML as the singleton `BarcodeScanner`.
 *
 * Paths (override via QSettings):
 *   recycle/pythonExe       python interpreter (Pi: ~/recycle_venv/bin/python)
 *   recycle/barcodeScript   barcode_scan.py    (Pi: ~/barcode_scan.py)
 */
class BarcodeScanner : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(BarcodeScanner)
    QML_SINGLETON
    Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged)
public:
    explicit BarcodeScanner(QObject *parent = nullptr);
    static BarcodeScanner *create(QQmlEngine *, QJSEngine *) { return s_instance; }
    static BarcodeScanner *s_instance;
    ~BarcodeScanner() override;

    bool isRunning() const;

public slots:
    Q_INVOKABLE void scan();    // launch the sidecar
    Q_INVOKABLE void cancel();

signals:
    void scanned(const QString &code);   // a barcode was decoded
    void nothingFound();                 // timed out with no code
    void failed(const QString &msg);
    void runningChanged();

private slots:
    void onStdout();
    void onFinished(int exitCode, QProcess::ExitStatus status);
    void onErrorOccurred(QProcess::ProcessError err);

private:
    QString resolvePython() const;
    QString resolveScript() const;
    void    finishOnce(const QString &code, const QString &err);

    QPointer<QProcess> m_proc;
    QString            m_buf;
    QTimer             m_timeout;
    bool               m_done = false;
};

#endif // BARCODE_SCANNER_H
