#include "../include/update_checker.h"
#include "../include/logger.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSettings>
#include <QUrl>
#include <QFile>
#include <QDir>
#include <QProcess>
#include <QCoreApplication>
#include <QStandardPaths>

// Set in CMakeLists.txt from `project(... VERSION X.Y.Z ...)`. If the
// build flag is missing we fall back to "0.0.0" — the comparison then
// always reports an update available, which is also a useful loud signal
// that the build flag dropped.
#ifndef REWINGO_VERSION
#define REWINGO_VERSION "0.0.0"
#endif

UpdateChecker *UpdateChecker::s_instance = nullptr;

UpdateChecker::UpdateChecker(QObject *parent) : QObject(parent)
{
    if (!s_instance) s_instance = this;

    m_net = new QNetworkAccessManager(this);
    m_currentVersion = QStringLiteral(REWINGO_VERSION);

    // Periodic check every 30 minutes (was 6 h — felt "not automatic").
    // The first fire still happens after the boot delay in start(), then
    // every 30 min, so a freshly-published release is picked up within
    // half an hour without anyone tapping "Check for updates".
    m_timer.setSingleShot(false);
    m_timer.setInterval(30 * 60 * 1000);
    connect(&m_timer, &QTimer::timeout, this, &UpdateChecker::checkNow);
}

void UpdateChecker::configure(const QString &ownerSlashRepo)
{
    m_repo = ownerSlashRepo;
}

void UpdateChecker::start(int firstDelayMs)
{
    if (m_repo.isEmpty()) {
        Logger::warn("Updater", "No repo configured — auto-check disabled");
        return;
    }
    QTimer::singleShot(firstDelayMs, this, &UpdateChecker::checkNow);
    m_timer.start();
    Logger::info("Updater",
                 QString("Auto-check armed (first in %1 s, then every 6 h)")
                     .arg(firstDelayMs / 1000));
}

bool UpdateChecker::updateAvailable() const
{
    if (m_latestVersion.isEmpty()) return false;
    return compareSemver(m_latestVersion, m_currentVersion) > 0;
}

void UpdateChecker::checkNow()
{
    if (m_repo.isEmpty() || m_haveInFlight) return;
    m_haveInFlight = true;
    setBusy(true);

    const QUrl url(QString("https://api.github.com/repos/%1/releases/latest")
                       .arg(m_repo));
    QNetworkRequest req(url);
    // GitHub API requires a User-Agent header. The Accept header pins
    // the v3 API response shape so a future GitHub schema change won't
    // silently break us.
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  "ReWinGo-Kiosk/1.0");
    req.setRawHeader("Accept", "application/vnd.github+json");
    req.setRawHeader("X-GitHub-Api-Version", "2022-11-28");

    QNetworkReply *reply = m_net->get(req);
    connect(reply, &QNetworkReply::finished,
            this,  &UpdateChecker::onReplyFinished);
}

void UpdateChecker::onReplyFinished()
{
    auto *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply) { setBusy(false); m_haveInFlight = false; return; }
    reply->deleteLater();

    m_lastCheckedAt = QDateTime::currentDateTime();
    emit lastCheckedAtChanged();

    if (reply->error() != QNetworkReply::NoError) {
        Logger::warn("Updater", "Check failed",
                     { {"err",   reply->errorString()},
                       {"http",  reply->attribute(
                           QNetworkRequest::HttpStatusCodeAttribute)} });
        setBusy(false); m_haveInFlight = false;
        return;
    }

    const QByteArray body = reply->readAll();
    QJsonParseError perr;
    const QJsonDocument doc = QJsonDocument::fromJson(body, &perr);
    if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
        Logger::warn("Updater", "Bad JSON from GitHub");
        setBusy(false); m_haveInFlight = false;
        return;
    }

    const QJsonObject obj = doc.object();
    // tag_name is the canonical version. GitHub conventions are "v1.2.3";
    // strip the leading 'v' for semver compare.
    QString tag = obj.value("tag_name").toString();
    if (tag.startsWith('v') || tag.startsWith('V')) tag = tag.mid(1);

    const QString notes = obj.value("body").toString();
    const QString url   = obj.value("html_url").toString();

    // Look for an arm64 .deb asset in the release. The download URL is
    // what we'll fetch when the admin presses "Install update".
    QString assetUrl;
    const QJsonArray assets = obj.value("assets").toArray();
    for (const QJsonValue &av : assets) {
        const QJsonObject a    = av.toObject();
        const QString     name = a.value("name").toString();
        if (name.endsWith("_arm64.deb") || name.endsWith(".deb")) {
            assetUrl = a.value("browser_download_url").toString();
            break;
        }
    }

    const bool changed = (tag != m_latestVersion);
    m_latestVersion    = tag;
    m_releaseNotes     = notes;
    m_releaseUrl       = url;
    m_assetDownloadUrl = assetUrl;

    if (changed) emit latestVersionChanged();

    setBusy(false); m_haveInFlight = false;

    // If a NEW version (one we haven't told the admin about before)
    // is available, fire the signal. We remember the last version we
    // surfaced via QSettings so re-checks don't re-fire the toast.
    if (updateAvailable()) {
        QSettings s;
        const QString lastSeen =
            s.value("updater/lastSeenVersion").toString();
        if (lastSeen != m_latestVersion) {
            s.setValue("updater/lastSeenVersion", m_latestVersion);
            emit newUpdateDetected(m_latestVersion, m_releaseNotes);
            Logger::audit("Updater", "New version detected",
                          { {"current", m_currentVersion},
                            {"latest",  m_latestVersion} });
        }
    }
}

void UpdateChecker::setBusy(bool busy)
{
    if (m_busy == busy) return;
    m_busy = busy;
    emit busyChanged();
}

// ─── Self-update flow ─────────────────────────────────────────────────
//
// The admin clicks "Install update" on AdminAboutPage. We:
//   1. GET the .deb asset URL we stashed during checkNow() into /tmp.
//   2. Hand it off to the rewingo-update-helper script (installed by the
//      .deb itself in /usr/local/bin) which:
//        - waits a second for our process to exit
//        - `sudo dpkg -i /tmp/rewingo_*.deb`
//        - relaunches /usr/local/bin/rewingo
//   3. Exit ourselves so dpkg can replace our binary cleanly.
//
// The sudoers entry installed by `postinst` allows dpkg -i on rewingo's
// own package only — see packaging/sudoers.d/rewingo.

void UpdateChecker::downloadAndInstall()
{
    // Always log the entry so a tap that reaches C++ is visible in the log,
    // even if we bail below. (Empty helper log + no /tmp .deb was the symptom
    // of bailing here silently.)
    Logger::audit("Updater",
                  QString("downloadAndInstall tapped: asset='%1' repo='%2' latest='%3'")
                      .arg(m_assetDownloadUrl, m_repo, m_latestVersion));

    QString url = m_assetDownloadUrl;

    // Fallback: if the GitHub API response didn't yield an asset URL (race,
    // empty parse, or the user tapped before a check completed), construct
    // the canonical Release asset URL from the repo + version. The CI names
    // every asset rewingo_<version>_arm64.deb, so this is deterministic.
    if (url.isEmpty() && !m_repo.isEmpty() && !m_latestVersion.isEmpty()) {
        url = QString("https://github.com/%1/releases/download/v%2/"
                      "rewingo_%2_arm64.deb")
                  .arg(m_repo, m_latestVersion);
        Logger::warn("Updater",
                     QString("asset URL was empty — using fallback %1").arg(url));
    }

    if (url.isEmpty()) {
        Logger::warn("Updater", "downloadAndInstall: no URL and no repo/version");
        emit installFailed(tr("No .deb asset found in the latest release."));
        return;
    }

    const QString tmpDir = QStandardPaths::writableLocation(
                               QStandardPaths::TempLocation);
    QDir().mkpath(tmpDir);
    const QString localPath =
        tmpDir + "/rewingo_" + m_latestVersion + "_arm64.deb";

    Logger::audit("Updater",
                  QString("Downloading %1 → %2").arg(url, localPath));

    QNetworkRequest req((QUrl(url)));
    req.setHeader(QNetworkRequest::UserAgentHeader, "ReWinGo-Kiosk/1.0");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply *reply = m_net->get(req);

    connect(reply, &QNetworkReply::downloadProgress,
            this,  &UpdateChecker::downloadProgress);

    connect(reply, &QNetworkReply::finished, this,
            [this, reply, localPath]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit installFailed(tr("Download failed: %1")
                                   .arg(reply->errorString()));
            return;
        }
        QFile f(localPath);
        if (!f.open(QIODevice::WriteOnly)) {
            emit installFailed(tr("Could not open %1 for writing")
                                   .arg(localPath));
            return;
        }
        f.write(reply->readAll());
        f.close();
        onAssetDownloaded(localPath);
    });
}

void UpdateChecker::onAssetDownloaded(const QString &localPath)
{
    emit installStarted();
    Logger::audit("Updater",
                  QString("Download complete, launching helper: %1")
                      .arg(localPath));
    launchHelperAndExit(localPath);
}

void UpdateChecker::launchHelperAndExit(const QString &localPath)
{
    // The helper script lives at /usr/local/bin/rewingo-update-helper.
    // Spawned detached so it survives our exit. The script runs `dpkg -i`
    // then relaunches /usr/local/bin/rewingo.
    const QString helper = QStringLiteral("/usr/local/bin/rewingo-update-helper");
    const QStringList args { localPath };

    qint64 pid = 0;
    const bool ok =
        QProcess::startDetached(helper, args, QDir::tempPath(), &pid);
    if (!ok) {
        emit installFailed(tr("Could not launch update helper: %1")
                               .arg(helper));
        return;
    }
    Logger::audit("Updater",
                  QString("Helper launched (pid=%1); exiting.").arg(pid));
    emit installFinished(QString());

    // Give the helper a moment to spin up, then exit so dpkg can replace
    // the running binary. The helper sleeps 1s before dpkg so this race
    // is safe.
    QTimer::singleShot(500, []() {
        QCoreApplication::quit();
    });
}

int UpdateChecker::compareSemver(const QString &a, const QString &b) const
{
    // Compare X.Y.Z numerically component-by-component. Any non-numeric
    // suffix (e.g. "1.0.0-rc1") sorts before its plain counterpart.
    const QStringList ap = a.split('.', Qt::SkipEmptyParts);
    const QStringList bp = b.split('.', Qt::SkipEmptyParts);
    const int n = qMax(ap.size(), bp.size());
    for (int i = 0; i < n; ++i) {
        // Strip prerelease tags from each component when comparing
        // numerically — "0-rc1" becomes 0, then we tie-break on the
        // string at the end if they're otherwise equal.
        const QString aRaw = i < ap.size() ? ap.at(i) : "0";
        const QString bRaw = i < bp.size() ? bp.at(i) : "0";
        const int aNum = aRaw.section('-', 0, 0).toInt();
        const int bNum = bRaw.section('-', 0, 0).toInt();
        if (aNum != bNum) return aNum > bNum ? 1 : -1;
    }
    // All numeric components equal — sort by full string so "1.0.0" >
    // "1.0.0-rc1" (the rc has the suffix).
    return a.compare(b);
}
