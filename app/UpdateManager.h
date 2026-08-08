#ifndef APP_UPDATE_MANAGER_H
#define APP_UPDATE_MANAGER_H

#include <QObject>
#include <QString>

// Production updater above the app. Lifecycle: check -> notify -> download -> verify signature ->
// stage -> apply on the NEXT restart. It NEVER auto-installs while running and NEVER touches the
// live database — a staged update is a self-contained installer payload placed under
// <stageDir>/.pending-update/, applied by a launcher before the app (and StorageService) start.
//
// The "source" is deliberately a local path so the whole flow is deterministic and network-free
// in tests; production wires the same interface to an HTTPS manifest fetch (documented gap). The
// staging is crash-consistent: an interrupted download/stage leaves either a COMPLETE bundle
// (payload + signed marker whose signature matches) or nothing usable, and the startup path
// cleans any incomplete bundle.
struct UpdateInfo {
    QString   version;
    long long versionCode = 0;
    QString   notes;
    QString   payloadName;
    qint64    size = 0;
    QString   channel = QStringLiteral("stable");   // channel this build was cut for (default = stable)
    bool      valid = false;
};

class UpdateManager : public QObject
{
    Q_OBJECT
public:
    enum class State { Idle, Checking, UpToDate, Available, Downloading, Staged, Error };
    enum class ApplyResult { None, Applied, Recovered, Failed };

    UpdateManager(QString stageDir, QString sourceDir, long long currentVersionCode,
                  QObject* parent = nullptr);

    // The release channel the USER is tracking (persisted setting; defaults to this build's
    // channel). check() only offers a build whose channel is same-or-more-stable than this
    // (appinfo::channelVisibleTo). Changing it re-evaluates on the next check().
    void       setChannel(const QString& channel);
    QString    channel() const { return userChannel_; }

    bool       check();               // read the manifest; set Available/UpToDate
    bool       hasUpdate() const { return state_ == State::Available || state_ == State::Staged; }
    UpdateInfo available() const { return info_; }
    State      state() const { return state_; }
    QString    errorText() const { return error_; }

    bool    downloadAndStage();       // fetch payload, verify signature, stage atomically
    bool    isStaged() const;
    QString stagedVersion() const;
    bool    rollbackStaged();         // discard a staged update BEFORE restart

    // Called once at STARTUP, before StorageService opens. Applies a complete staged bundle
    // (hands the installer off in production) and clears it; cleans an incomplete/interrupted
    // bundle. Never touches the data directory. Deterministic + side-effect-scoped to stageDir.
    static ApplyResult applyPendingAtStartup(const QString& stageDir, long long currentVersionCode);

    static QString pendingDir(const QString& stageDir) { return stageDir + "/.pending-update"; }

signals:
    void stateChanged();

private:
    void setState(State s) { state_ = s; emit stateChanged(); }

    QString    stageDir_;
    QString    sourceDir_;
    long long  current_ = 0;
    QString    userChannel_;   // set in the ctor from appinfo::channel()
    State      state_ = State::Idle;
    QString    error_;
    UpdateInfo info_;
};

#endif // APP_UPDATE_MANAGER_H
