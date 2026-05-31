#include "../include/face_rec_sidecar.h"
#include "../include/logger.h"

#include <QFileInfo>
#include <QSettings>
#include <QJsonDocument>
#include <QJsonObject>
#include <QBuffer>
#include <QtEndian>

FaceRecSidecar *FaceRecSidecar::s_instance = nullptr;

FaceRecSidecar::FaceRecSidecar(QObject *parent) : QObject(parent)
{
    if (!s_instance) s_instance = this;
}

FaceRecSidecar::~FaceRecSidecar()
{
    // Make sure the Python child isn't still draining when the singleton
    // dies (kiosk shutdown) — otherwise Qt logs "QProcess: Destroyed
    // while process is still running" and the OS keeps a zombie.
    cancel();
    if (s_instance == this) s_instance = nullptr;
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
    m_stage.clear();
    m_blinkCount    = 0;
    m_blinkRequired = 2;
    emit stageChanged();
    setStatus(tr("Look at the camera…"));
    // -u = unbuffered I/O so JSON events surface immediately and our
    // length-prefixed frame writes don't get held in Python's buffer.
    // sidecar_identify.py reads frames from stdin and streams JSON events.
    startSidecar({ "-u", "-m", "scripts.sidecar_identify" });
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
    if (!m_proc || m_proc->state() == QProcess::NotRunning) return;

    Logger::info("FaceRec", "Cancel — closing sidecar");
    // 1) Send the EOS sentinel (length 0) so the sidecar's read_frame()
    //    loop exits cleanly. closeWriteChannel() also closes the pipe.
    const quint32 eos = 0;
    m_proc->write(reinterpret_cast<const char *>(&eos), 4);
    m_proc->closeWriteChannel();
    // 2) Give it a beat to exit on its own.
    if (!m_proc->waitForFinished(400)) {
        // 3) Force-kill if it didn't.
        m_proc->kill();
        m_proc->waitForFinished(400);
    }
}

void FaceRecSidecar::onStdout()
{
    if (!m_proc) return;
    m_stdoutBuf += QString::fromUtf8(m_proc->readAllStandardOutput());

    // sidecar_identify.py emits one JSON object per line. Drain whole
    // lines; any trailing fragment stays buffered for the next read.
    int nl = 0;
    while ((nl = m_stdoutBuf.indexOf('\n')) >= 0) {
        const QString line = m_stdoutBuf.left(nl).trimmed();
        m_stdoutBuf.remove(0, nl + 1);
        if (line.isEmpty()) continue;

        const QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8());
        if (!doc.isObject()) {
            // Non-JSON line — usually a Python warning. Log it and move on.
            Logger::info("FaceRec", line);
            continue;
        }
        const QJsonObject obj = doc.object();
        const QString     ev  = obj.value("e").toString();

        if (ev == "stage") {
            m_stage         = obj.value("stage").toString();
            m_blinkCount    = obj.value("count").toInt(m_blinkCount);
            m_blinkRequired = obj.value("required").toInt(m_blinkRequired);
            emit stageChanged();

            // Human-readable status mirroring the stage for the page footer.
            if      (m_stage == "BLINK")
                setStatus(tr("Blink twice  (%1/%2)").arg(m_blinkCount).arg(m_blinkRequired));
            else if (m_stage == "TURN_RIGHT") setStatus(tr("Turn your head RIGHT"));
            else if (m_stage == "TURN_LEFT")  setStatus(tr("Turn your head LEFT"));
            else if (m_stage == "RECOGNIZE")  setStatus(tr("Hold still — recognising…"));
        }
        else if (ev == "identified") {
            emit identified(obj.value("name").toString(),
                            obj.value("score").toDouble());
        }
        else if (ev == "unknown") {
            emit unknown(obj.value("score").toDouble());
        }
        else if (ev == "error") {
            emit failed(obj.value("msg").toString());
        }
        else {
            Logger::info("FaceRec", line);
        }
    }
}

void FaceRecSidecar::onFinished(int exitCode, QProcess::ExitStatus /*status*/)
{
    // Drain anything still in the pipe.
    if (m_proc) {
        m_stdoutBuf += QString::fromUtf8(m_proc->readAll());
        onStdout();   // re-parse whatever's left
    }
    Logger::info("FaceRec", QString("sidecar exit rc=%1").arg(exitCode));

    // If the process died without ever emitting identified/unknown/error,
    // surface a generic failure so the UI doesn't hang on "running".
    if (exitCode != 0 && m_currentMode == QStringLiteral("identify"))
        emit failed(tr("Python exited with code %1").arg(exitCode));

    m_stage.clear();
    emit stageChanged();
    setStatus(QString());

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

// ── Frame pump ──────────────────────────────────────────────────────────
//
// Wire protocol: [uint32 BE byte length] [JPEG bytes]
//                A length of 0 signals end-of-stream → Python exits cleanly.
// JPEG quality 75 keeps each frame ~25 KB so we sustain 10-15 fps easily
// over a stdin pipe without back-pressure stalls.

void FaceRecSidecar::feedFrame(const QImage &img)
{
    if (!m_proc || m_proc->state() != QProcess::Running) return;
    if (img.isNull()) return;

    // Encode the QImage as JPEG into a memory buffer.
    QByteArray jpeg;
    {
        QBuffer buf(&jpeg);
        buf.open(QIODevice::WriteOnly);
        // Downscale very large camera frames; ArcFace + MediaPipe don't
        // need more than ~720p, and smaller frames cut JPEG + IPC cost.
        const QImage src = (img.width() > 960)
            ? img.scaledToWidth(640, Qt::SmoothTransformation)
            : img;
        if (!src.save(&buf, "JPEG", /*quality*/ 75)) return;
    }

    const quint32 n  = quint32(jpeg.size());
    const quint32 be = qToBigEndian(n);
    m_proc->write(reinterpret_cast<const char *>(&be), 4);
    m_proc->write(jpeg);
}
