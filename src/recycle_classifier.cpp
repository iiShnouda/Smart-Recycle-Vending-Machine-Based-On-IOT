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
    m_timeout.setInterval(600000);
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

bool RecycleClassifier::isRunning() const
{
    return m_proc && m_proc->state() == QProcess::Running && !m_emitted;
}

void RecycleClassifier::classify()
{
    if (isRunning()) {
        Logger::warn("Recycle", "Classify requested while still scanning — ignored");
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

    if (!m_proc || m_proc->state() != QProcess::Running) {
        if (m_proc) { m_proc->deleteLater(); }
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

        Logger::audit("Recycle", "Launching continuous classifier",
                      { {"python", python}, {"script", script} });
        m_proc->start();
        if (!m_proc->waitForStarted(3000)) {
            Logger::error("Recycle", "Classifier failed to start — REJECT");
            emitVerdictOnce(QStringLiteral("REJECT"));
            return;
        }
    }

    m_buf.clear();
    m_emitted = false;
    
    // Trigger the scan!
    m_proc->write("SCAN\n");
    m_timeout.start();
}

void RecycleClassifier::cancel()
{
    m_timeout.stop();
    m_emitted = true; // Stop waiting
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
            // Emit the raw sub-class so RecycleSession can score small (1) vs
            // large (2) bottles. cls is small_bottle | large_bottle | can.
            const QString v   = obj.value("verdict").toString().toLower();
            const QString cls = obj.value("cls").toString();
            const double conf = obj.value("conf").toDouble();
            
            Logger::audit("Recycle", "Classifier Result", {
                {"verdict", v},
                {"class", cls},
                {"confidence", conf}
            });

            if (v == "bottle")
                emitVerdictOnce(cls.isEmpty() ? QStringLiteral("small_bottle") : cls);
            else if (v == "can")
                emitVerdictOnce(QStringLiteral("can"));
            else
                emitVerdictOnce(QStringLiteral("reject"));
        } else if (ev == "error") {
            Logger::warn("Recycle", "Classifier error",
                         { {"msg", obj.value("msg").toString()} });
        } else {
            Logger::info("Recycle", line);
        }
    }
}

void RecycleClassifier::onFinished(int exitCode, QProcess::ExitStatus status)
{
    Q_UNUSED(status);
    Logger::info("Recycle", QString("Classifier python process exited (code %1)").arg(exitCode));
    if (!m_emitted) {
        Logger::warn("Recycle", "Classifier exited while scanning — REJECT",
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
