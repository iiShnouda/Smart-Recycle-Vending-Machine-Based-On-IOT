#include "../include/update_checker.h"
#include "../include/logger.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QUrl>

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

    // 6-hour periodic check. Tied to single-shot semantics in start()
    // so the first fire happens after the boot delay, not immediately.
    m_timer.setSingleShot(false);
    m_timer.setInterval(6 * 60 * 60 * 1000);
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

    const bool changed = (tag != m_latestVersion);
    m_latestVersion = tag;
    m_releaseNotes  = notes;
    m_releaseUrl    = url;

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
