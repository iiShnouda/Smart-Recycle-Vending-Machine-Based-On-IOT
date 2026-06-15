#include "../include/recycle_classifier.h"
#include "../include/logger.h"

#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QJsonDocument>
#include <QJsonObject>

RecycleClassifier::RecycleClassifier(QObject *parent) : QObject(parent)
{
    // Safety net: if the sidecar hangs (camera driver wedged, etc.) we still
    // owe the STM32 a verdict so the lane clears. Default to REJECT.
    m_timeout.setSingleShot(true);
    // Covers ONNX model load (~2-3s) + camera open (~1-2s) + the burst (~1.5s)
    // with headroom, so a slow-but-valid classification isn't pre-empted by a
    // spurious REJECT.
    m_timeout.setInterval(12000);
    connect(&m_timeout, &QTimer::timeout, this, [this] {
        Logger::warn("Recycle", "Classifier timed out — defaulting to REJECT");
        cancel();
        emitVerdictOnce(QStringLiteral("REJECT"));
    });
}

RecycleClassifier::~RecycleClassifier()
{
    cancel();
}

bool RecycleClassifier::isRunning() const
{
    return m_proc && m_proc->state() != QProcess::NotRunning;
}

QString RecycleClassifier::resolvePython() const
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

QString RecycleClassifier::resolveScript() const
{
    QSettings s;
    const QString v = s.value("recycle/scriptPath").toString();
    if (!v.isEmpty()) return v;
#ifdef Q_OS_WIN
    return QDir::currentPath() + "/recycle/recycle_classify.py";
#else
    return QDir::homePath() + "/recycle_classify.py";
#endif
}

void RecycleClassifier::classify()
{
    if (isRunning()) {
        Logger::warn("Recycle", "Classify requested while still running — ignored");
        return;
    }

    const QString python = resolvePython();
    const QString script = resolveScript();
    if (!QFileInfo::exists(script)) {
        Logger::error("Recycle", "Classifier script missing — REJECT",
                      { {"script", script} });
        emitVerdictOnce(QStringLiteral("REJECT"));
        return;
    }

    m_buf.clear();
    m_emitted = false;

    m_proc = new QProcess(this);
    m_proc->setProgram(python);
    m_proc->setArguments({ "-u", script });        // -u = unbuffered JSON
    m_proc->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_proc.data(), &QProcess::readyReadStandardOutput,
            this, &RecycleClassifier::onStdout);
    connect(m_proc.data(), &QProcess::finished,
            this, &RecycleClassifier::onFinished);
    connect(m_proc.data(), &QProcess::errorOccurred,
            this, &RecycleClassifier::onErrorOccurred);

    Logger::audit("Recycle", "Launching classifier",
                  { {"python", python}, {"script", script} });
    m_proc->start();
    m_timeout.start();
}

void RecycleClassifier::cancel()
{
    m_timeout.stop();
    if (!m_proc || m_proc->state() == QProcess::NotRunning) return;
    m_proc->kill();
    m_proc->waitForFinished(400);
}

void RecycleClassifier::emitVerdictOnce(const QString &v)
{
    if (m_emitted) return;
    m_emitted = true;
    m_timeout.stop();
    Logger::info("Recycle", "Verdict", { {"verdict", v} });
    emit verdict(v);
}

void RecycleClassifier::onStdout()
{
    if (!m_proc) return;
    m_buf += QString::fromUtf8(m_proc->readAllStandardOutput());

    int nl = 0;
    while ((nl = m_buf.indexOf('\n')) >= 0) {
        const QString line = m_buf.left(nl).trimmed();
        m_buf.remove(0, nl + 1);
        if (line.isEmpty()) continue;

        const QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8());
        if (!doc.isObject()) { Logger::info("Recycle", line); continue; }
        const QJsonObject obj = doc.object();
        const QString ev = obj.value("e").toString();

        if (ev == "verdict") {
            const QString v = obj.value("verdict").toString().toUpper();
            if (v == "BOTTLE" || v == "CAN")
                emitVerdictOnce(v);
            else
                emitVerdictOnce(QStringLiteral("REJECT"));
        } else if (ev == "error") {
            Logger::warn("Recycle", "Classifier error",
                         { {"msg", obj.value("msg").toString()} });
        } else {
            Logger::info("Recycle", line);
        }
    }
}

void RecycleClassifier::onFinished(int exitCode, QProcess::ExitStatus /*status*/)
{
    if (m_proc) {
        m_buf += QString::fromUtf8(m_proc->readAll());
        onStdout();   // re-parse any tail
    }
    // The sidecar always prints a verdict; if it died without one, clear the
    // lane safely.
    if (!m_emitted) {
        Logger::warn("Recycle", "Classifier exited with no verdict — REJECT",
                     { {"rc", exitCode} });
        emitVerdictOnce(QStringLiteral("REJECT"));
    }
    if (m_proc) { m_proc->deleteLater(); m_proc = nullptr; }
}

void RecycleClassifier::onErrorOccurred(QProcess::ProcessError err)
{
    Logger::warn("Recycle", QString("Classifier QProcess error %1").arg(int(err)));
    if (err == QProcess::FailedToStart)
        emitVerdictOnce(QStringLiteral("REJECT"));
}
