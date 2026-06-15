#include "../include/barcode_scanner.h"
#include "../include/logger.h"

#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QJsonDocument>
#include <QJsonObject>

BarcodeScanner *BarcodeScanner::s_instance = nullptr;

BarcodeScanner::BarcodeScanner(QObject *parent) : QObject(parent)
{
    if (!s_instance) s_instance = this;
    m_timeout.setSingleShot(true);
    m_timeout.setInterval(12000);   // camera open + ~6s watch + headroom
    connect(&m_timeout, &QTimer::timeout, this, [this] {
        Logger::warn("Barcode", "Scan timed out");
        cancel();
        finishOnce(QString(), QString());   // → nothingFound
    });
}

BarcodeScanner::~BarcodeScanner()
{
    cancel();
    if (s_instance == this) s_instance = nullptr;
}

bool BarcodeScanner::isRunning() const
{
    return m_proc && m_proc->state() != QProcess::NotRunning;
}

QString BarcodeScanner::resolvePython() const
{
    QSettings s;
    const QString v = s.value("recycle/pythonExe").toString();
    if (!v.isEmpty()) return v;
#ifdef Q_OS_WIN
    return QStringLiteral("python");
#else
    const QString venv = QDir::homePath() + "/recycle_venv/bin/python";
    if (QFileInfo::exists(venv)) return venv;
    return QStringLiteral("/usr/bin/python3");
#endif
}

QString BarcodeScanner::resolveScript() const
{
    QSettings s;
    const QString v = s.value("recycle/barcodeScript").toString();
    if (!v.isEmpty()) return v;
#ifdef Q_OS_WIN
    return QDir::currentPath() + "/recycle/barcode_scan.py";
#else
    return QDir::homePath() + "/barcode_scan.py";
#endif
}

void BarcodeScanner::scan()
{
    if (isRunning()) {
        Logger::warn("Barcode", "Scan already running — ignored");
        return;
    }
    const QString python = resolvePython();
    const QString script = resolveScript();
    if (!QFileInfo::exists(script)) {
        emit failed(tr("Scanner script missing at %1").arg(script));
        return;
    }

    m_buf.clear();
    m_done = false;

    m_proc = new QProcess(this);
    m_proc->setProgram(python);
    m_proc->setArguments({ "-u", script });
    m_proc->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_proc.data(), &QProcess::readyReadStandardOutput,
            this, &BarcodeScanner::onStdout);
    connect(m_proc.data(), &QProcess::finished,
            this, &BarcodeScanner::onFinished);
    connect(m_proc.data(), &QProcess::errorOccurred,
            this, &BarcodeScanner::onErrorOccurred);

    Logger::audit("Barcode", "Launching scanner",
                  { {"python", python}, {"script", script} });
    m_proc->start();
    m_timeout.start();
    emit runningChanged();
}

void BarcodeScanner::cancel()
{
    m_timeout.stop();
    if (!m_proc || m_proc->state() == QProcess::NotRunning) return;
    m_proc->kill();
    m_proc->waitForFinished(400);
}

void BarcodeScanner::finishOnce(const QString &code, const QString &err)
{
    if (m_done) return;
    m_done = true;
    m_timeout.stop();
    if (!err.isEmpty())       emit failed(err);
    else if (!code.isEmpty()) emit scanned(code);
    else                      emit nothingFound();
}

void BarcodeScanner::onStdout()
{
    if (!m_proc) return;
    m_buf += QString::fromUtf8(m_proc->readAllStandardOutput());

    int nl = 0;
    while ((nl = m_buf.indexOf('\n')) >= 0) {
        const QString line = m_buf.left(nl).trimmed();
        m_buf.remove(0, nl + 1);
        if (line.isEmpty()) continue;

        const QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8());
        if (!doc.isObject()) { Logger::info("Barcode", line); continue; }
        const QJsonObject obj = doc.object();
        const QString ev = obj.value("e").toString();

        if (ev == "barcode") {
            const QString code = obj.value("code").toString();
            if (!code.isEmpty()) finishOnce(code, QString());
            else                 finishOnce(QString(), QString());  // none
        } else if (ev == "none") {
            finishOnce(QString(), QString());
        } else if (ev == "error") {
            Logger::warn("Barcode", "Scanner error",
                         { {"msg", obj.value("msg").toString()} });
        } else {
            Logger::info("Barcode", line);
        }
    }
}

void BarcodeScanner::onFinished(int exitCode, QProcess::ExitStatus /*status*/)
{
    if (m_proc) {
        m_buf += QString::fromUtf8(m_proc->readAll());
        onStdout();
    }
    if (!m_done) {
        Logger::warn("Barcode", "Scanner exited with no result",
                     { {"rc", exitCode} });
        finishOnce(QString(), QString());
    }
    if (m_proc) { m_proc->deleteLater(); m_proc = nullptr; }
    emit runningChanged();
}

void BarcodeScanner::onErrorOccurred(QProcess::ProcessError err)
{
    Logger::warn("Barcode", QString("QProcess error %1").arg(int(err)));
    if (err == QProcess::FailedToStart)
        finishOnce(QString(), tr("Could not start the scanner"));
}
