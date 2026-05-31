#include "../include/face_rec_sidecar.h"
#include "../include/logger.h"

#include <QFileInfo>
#include <QRegularExpression>
#include <QSettings>

FaceRecSidecar *FaceRecSidecar::s_instance = nullptr;

FaceRecSidecar::FaceRecSidecar(QObject *parent) : QObject(parent)
{
    if (!s_instance) s_instance = this;
}

bool FaceRecSidecar::isRunning() const
{
    return m_proc && m_proc->state() != QProcess::NotRunning;
}

// ── Path resolution ──────────────────────────────────────────────────────
//
// Defaults match the user's existing FaceRec_project layout on Windows.
// Override on the Pi (or any other machine) via QSettings keys:
//     faceRec/pythonExe   absolute path to the python interpreter
//     faceRec/projectDir  absolute path to the FaceRec_project root

QString FaceRecSidecar::resolvePython() const
{
    QSettings s;
    const QString v = s.value("faceRec/pythonExe").toString();
    if (!v.isEmpty()) return v;
#ifdef Q_OS_WIN
    // Fresh local venv (created with the user's Python 3.11; the original
    // fr_env was built on a different machine and its launcher hardcoded
    // a Python path that doesn't exist here, so the kiosk could never
    // start it). To rebuild from scratch:
    //   cd FaceRec_project
    //   python -m venv .venv
    //   .venv\Scripts\pip install "mediapipe==0.10.14" opencv-python onnxruntime numpy
    return QStringLiteral(
        "C:/Users/Shnou/Downloads/FaceRec_project/.venv/Scripts/python.exe");
#else
    // On the Pi: assume a venv at /opt/face_rec/.venv exists with the
    // FaceRec_project's requirements installed. Falls back to system python3.
    if (QFileInfo::exists("/opt/face_rec/.venv/bin/python"))
        return QStringLiteral("/opt/face_rec/.venv/bin/python");
    return QStringLiteral("/usr/bin/python3");
#endif
}

QString FaceRecSidecar::resolveProjectDir() const
{
    QSettings s;
    const QString v = s.value("faceRec/projectDir").toString();
    if (!v.isEmpty()) return v;
#ifdef Q_OS_WIN
    return QStringLiteral("C:/Users/Shnou/Downloads/FaceRec_project");
#else
    return QStringLiteral("/opt/face_rec");
#endif
}

void FaceRecSidecar::setStatus(const QString &s)
{
    if (m_status == s) return;
    m_status = s;
    emit statusChanged();
}

// ── Process lifecycle ────────────────────────────────────────────────────

void FaceRecSidecar::startSidecar(const QStringList &args)
{
    const QString python = resolvePython();
    const QString dir    = resolveProjectDir();

    if (!QFileInfo::exists(python)) {
        emit failed(tr("Python interpreter not found at %1").arg(python));
        return;
    }
    if (!QFileInfo::exists(dir + "/scripts")) {
        emit failed(tr("FaceRec_project not found at %1").arg(dir));
        return;
    }

    m_stdoutBuf.clear();

    m_proc = new QProcess(this);
    m_proc->setProgram(python);
    m_proc->setArguments(args);                  // e.g. -u -m scripts.recognize_user
    m_proc->setWorkingDirectory(dir);
    m_proc->setProcessChannelMode(QProcess::MergedChannels);

    connect(m_proc.data(), &QProcess::readyReadStandardOutput,
            this, &FaceRecSidecar::onStdout);
    connect(m_proc.data(), &QProcess::finished,
            this, &FaceRecSidecar::onFinished);
    connect(m_proc.data(), &QProcess::errorOccurred,
            this, &FaceRecSidecar::onErrorOccurred);

    Logger::audit("FaceRec", "Launching sidecar",
                  { {"python", python}, {"dir", dir},
                    {"args",   args.join(' ')} });

    m_proc->start();
    emit runningChanged();
}

void FaceRecSidecar::identify()
{
    if (isRunning()) {
        Logger::warn("FaceRec", "Already running — ignoring identify");
        return;
    }
    m_currentMode = QStringLiteral("identify");
    setStatus(tr("Starting camera + liveness check…"));
    // -u = unbuffered stdout (so we see progress lines as Python emits them)
    startSidecar({ "-u", "-m", "scripts.recognize_user" });
}

void FaceRecSidecar::enroll(const QString &name)
{
    if (isRunning()) return;
    m_currentMode = QStringLiteral("enroll");
    m_pendingName = name;
    setStatus(tr("Enrolling %1…").arg(name));
    startSidecar({ "-u", "-m", "scripts.enroll_user" });
    // enroll_user.py reads the user's name from stdin
    if (m_proc) m_proc->write((name + "\n").toUtf8());
}

void FaceRecSidecar::cancel()
{
    if (m_proc && m_proc->state() != QProcess::NotRunning) {
        Logger::info("FaceRec", "Cancel — killing sidecar");
        m_proc->kill();
    }
}

void FaceRecSidecar::onStdout()
{
    if (!m_proc) return;
    m_stdoutBuf += QString::fromUtf8(m_proc->readAllStandardOutput());

    // Drain whole lines for live status updates. The buffer keeps any
    // trailing partial line for the final-result parse in onFinished.
    int nl = 0;
    while ((nl = m_stdoutBuf.indexOf('\n')) >= 0) {
        const QString line = m_stdoutBuf.left(nl).trimmed();
        m_stdoutBuf.remove(0, nl + 1);
        if (line.isEmpty()) continue;

        Logger::info("FaceRec", line);
        if (line.contains("Liveness"))         setStatus(tr("Liveness — blink + turn your head"));
        else if (line.contains("Recognition")) setStatus(tr("Look at the camera…"));
        else if (line.contains("Enrollment"))  setStatus(tr("Capturing face…"));
    }
}

void FaceRecSidecar::onFinished(int exitCode, QProcess::ExitStatus /*status*/)
{
    if (m_proc) m_stdoutBuf += QString::fromUtf8(m_proc->readAll());
    Logger::info("FaceRec", QString("sidecar exit rc=%1").arg(exitCode));
    setStatus(QString());

    // recognize_user.py prints one of:
    //   ✅ IDENTIFIED: <name>  score=0.xxx
    //   ❌ UNKNOWN  best=<name>  score=0.xxx (threshold=0.55)
    // enroll_user.py exits 0 on success after "Enrollment complete".
    if (m_currentMode == QStringLiteral("identify")) {
        QRegularExpression idRe (QStringLiteral("IDENTIFIED:\\s*(\\S+)\\s+score=([0-9.]+)"));
        QRegularExpression unkRe(QStringLiteral("UNKNOWN\\s+best=\\S+\\s+score=([0-9.]+)"));
        const auto idM  = idRe.match(m_stdoutBuf);
        const auto unkM = unkRe.match(m_stdoutBuf);
        if (idM.hasMatch())
            emit identified(idM.captured(1), idM.captured(2).toDouble());
        else if (unkM.hasMatch())
            emit unknown(unkM.captured(1).toDouble());
        else if (exitCode != 0)
            emit failed(tr("Python exited with code %1").arg(exitCode));
        else
            emit failed(tr("No recognition result"));
    } else if (m_currentMode == QStringLiteral("enroll")) {
        if (exitCode == 0 && m_stdoutBuf.contains("Enrollment", Qt::CaseInsensitive))
            emit enrolled(m_pendingName);
        else
            emit failed(tr("Enrollment failed (rc=%1)").arg(exitCode));
    }

    if (m_proc) {
        m_proc->deleteLater();
        m_proc = nullptr;
    }
    emit runningChanged();
}

void FaceRecSidecar::onErrorOccurred(QProcess::ProcessError err)
{
    Logger::warn("FaceRec", QString("QProcess error %1").arg(int(err)));
    if (err == QProcess::FailedToStart)
        emit failed(tr("Could not start the Python interpreter — check faceRec/pythonExe"));
}
