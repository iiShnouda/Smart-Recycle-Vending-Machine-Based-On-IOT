#ifndef UPDATE_CHECKER_H
#define UPDATE_CHECKER_H

#include <QObject>
#include <QQmlEngine>
#include <QDateTime>
#include <QString>
#include <QTimer>

class QNetworkAccessManager;
class QNetworkReply;
class QProcess;

/**
 * UpdateChecker — knows whether a newer version of ReWinGo is available
 * on the GitHub Releases page, and exposes that fact to QML.
 *
 * The current version is baked into the binary at build time via the
 * REWINGO_VERSION compile definition (set in CMakeLists.txt from
 * `project(... VERSION X.Y.Z ...)`). The latest version is fetched from
 *
 *     GET https://api.github.com/repos/<owner>/<repo>/releases/latest
 *
 * which returns JSON like { "tag_name": "v0.2.0", "name": "...",
 * "body": "...", "html_url": "...", "assets": [...] }. We strip the
 * leading 'v' and do a simple semver compare.
 *
 * Auto-check policy:
 *   - Once at app start (~5 s after Serial_Connection comes up, so we
 *     don't fight USB enumeration for CPU)
 *   - Once every 6 hours after that
 *   - On demand when the admin taps "Check for updates" in the About
 *     page (Q_INVOKABLE checkNow())
 *
 * Failures (no network, rate-limited, repo private) are silent — the
 * properties just keep their previous values. No nag dialog.
 *
 * QML usage:
 *
 *     // banner on AdminMainPage
 *     visible: UpdateInfo.updateAvailable
 *     Text { text: qsTr("v%1 → v%2 available")
 *                       .arg(UpdateInfo.currentVersion)
 *                       .arg(UpdateInfo.latestVersion) }
 */
class UpdateChecker : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(UpdateInfo)
    QML_SINGLETON

    Q_PROPERTY(QString   currentVersion  READ currentVersion  CONSTANT)
    Q_PROPERTY(QString   latestVersion   READ latestVersion   NOTIFY latestVersionChanged)
    Q_PROPERTY(QString   releaseNotes    READ releaseNotes    NOTIFY latestVersionChanged)
    Q_PROPERTY(QString   releaseUrl      READ releaseUrl      NOTIFY latestVersionChanged)
    Q_PROPERTY(QDateTime lastCheckedAt   READ lastCheckedAt   NOTIFY lastCheckedAtChanged)
    Q_PROPERTY(bool      updateAvailable READ updateAvailable NOTIFY latestVersionChanged)
    Q_PROPERTY(bool      busy            READ busy            NOTIFY busyChanged)

public:
    explicit UpdateChecker(QObject *parent = nullptr);
    static UpdateChecker *create(QQmlEngine *, QJSEngine *) { return s_instance; }
    static UpdateChecker *s_instance;

    /** Point this at your GitHub repo. Defaults to the placeholder that
     *  README.md uses. Override before the first check (in main / app
     *  init) once you've set up the real public repo.                   */
    void configure(const QString &ownerSlashRepo);

    /** Start the auto-check timer (6 h interval) and run an initial
     *  check after `firstDelayMs` (so we don't compete with serial
     *  enumeration on boot). Safe to call once.                          */
    void start(int firstDelayMs = 5000);

    QString   currentVersion()   const { return m_currentVersion; }
    QString   latestVersion()    const { return m_latestVersion;  }
    QString   releaseNotes()     const { return m_releaseNotes;   }
    QString   releaseUrl()       const { return m_releaseUrl;     }
    QDateTime lastCheckedAt()    const { return m_lastCheckedAt;  }
    bool      updateAvailable()  const;
    bool      busy()             const { return m_busy; }

public slots:
    /** Trigger an immediate check. Safe to spam — concurrent calls
     *  collapse to one outstanding request.                              */
    Q_INVOKABLE void checkNow();

    /** Kick off the full self-update flow:
     *    1. Download the .deb asset from the latest GitHub Release
     *    2. dpkg -i on it (via sudoers helper — see packaging/sudoers.d/)
     *    3. Re-exec rewingo so the new binary is running
     *  Progress is reported via downloadProgress + installStarted /
     *  installFinished signals. Errors via installFailed.                */
    Q_INVOKABLE void downloadAndInstall();

signals:
    void latestVersionChanged();
    void lastCheckedAtChanged();
    void busyChanged();
    /** Fired once when a NEW (not yet seen) update appears. The admin
     *  banner listens for this; QSettings remembers the last version
     *  the admin acknowledged.                                          */
    void newUpdateDetected(const QString &version, const QString &notes);

    // ── Install-flow events for the UI ──────────────────────────────────
    void downloadProgress(qint64 received, qint64 total);
    void installStarted();
    /** message is empty on success, non-empty on failure. */
    void installFinished(const QString &message);
    void installFailed(const QString &reason);

private:
    void setBusy(bool busy);
    void applyReleaseJson(const QByteArray &body);
    int  compareSemver(const QString &a, const QString &b) const;

    QNetworkAccessManager *m_net = nullptr;
    QTimer                 m_timer;
    QString                m_repo;            // "owner/name"
    QString                m_currentVersion;  // from REWINGO_VERSION
    QString                m_latestVersion;
    QString                m_releaseNotes;
    QString                m_releaseUrl;
    QString                m_assetDownloadUrl;  // .deb asset URL from latest release
    QDateTime              m_lastCheckedAt;
    bool                   m_busy        = false;
    bool                   m_haveInFlight = false;
    QNetworkReply         *m_reply       = nullptr;  // (legacy QNAM; unused now)
    QProcess              *m_checkProc   = nullptr;  // curl check subprocess
    QDateTime              m_inFlightSince;          // watchdog: when it started

    void onAssetDownloaded(const QString &localPath);
    void launchHelperAndExit(const QString &localPath);
};

#endif // UPDATE_CHECKER_H
