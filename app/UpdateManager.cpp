#include "UpdateManager.h"
#include "Signature.h"
#include "Logging.h"
#include "AppInfo.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

namespace {
QByteArray readAll(const QString& p)
{
    QFile f(p);
    if (!f.open(QIODevice::ReadOnly)) return {};
    const QByteArray b = f.readAll();
    f.close();
    return b;
}
bool writeAll(const QString& p, const QByteArray& b)
{
    QDir().mkpath(QFileInfo(p).absolutePath());
    QFile f(p);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    const bool ok = f.write(b) == b.size();
    f.close();
    return ok;
}
// A manifest describes an available build + a detached signature over the payload FILE bytes.
UpdateInfo parseManifest(const QByteArray& json)
{
    UpdateInfo u;
    const QJsonDocument d = QJsonDocument::fromJson(json);
    if (!d.isObject()) return u;
    const QJsonObject o = d.object();
    u.version     = o.value("version").toString();
    u.versionCode = static_cast<long long>(o.value("versionCode").toDouble());
    u.notes       = o.value("notes").toString();
    u.payloadName = o.value("payload").toString();
    u.size        = static_cast<qint64>(o.value("size").toDouble());
    // A manifest without a channel is treated as "stable" — the most conservative, so it stays
    // visible on every channel and old manifests keep working.
    u.channel     = o.value("channel").toString(QStringLiteral("stable"));
    u.valid       = !u.version.isEmpty() && !u.payloadName.isEmpty();
    return u;
}
} // namespace

UpdateManager::UpdateManager(QString stageDir, QString sourceDir, long long currentVersionCode,
                             QObject* parent)
    : QObject(parent), stageDir_(std::move(stageDir)), sourceDir_(std::move(sourceDir)),
      current_(currentVersionCode), userChannel_(appinfo::channel())
{}

void UpdateManager::setChannel(const QString& channel)
{
    userChannel_ = appinfo::channelName(appinfo::parseChannel(channel, appinfo::buildChannel()));
}

bool UpdateManager::check()
{
    setState(State::Checking);
    const QByteArray mj = readAll(sourceDir_ + "/manifest.json");
    if (mj.isEmpty()) { error_ = "no update source reachable"; setState(State::Error); return false; }
    info_ = parseManifest(mj);
    if (!info_.valid)  { error_ = "malformed update manifest"; setState(State::Error); return false; }
    // Channel gate: a build is only offered when it is same-or-more-stable than the channel the
    // user is tracking (a Stable user never sees a Beta build; a Beta user sees Beta + Stable).
    if (!appinfo::channelVisibleTo(info_.channel, userChannel_)) {
        prodlog::info("update", QString("newer build %1 is on the '%2' channel; user tracks '%3' — not offered")
                                    .arg(info_.version, info_.channel, userChannel_));
        setState(State::UpToDate);
        return true;
    }
    if (info_.versionCode > current_) {
        prodlog::info("update", "update available: " + info_.version);
        setState(State::Available);
        return true;
    }
    setState(State::UpToDate);
    return true;
}

bool UpdateManager::downloadAndStage()
{
    if (state_ != State::Available) return false;
    setState(State::Downloading);

    // 1) "Download" the payload + its manifest to a temp area (a partial/interrupted copy is
    //    contained here and never becomes a usable bundle).
    const QString tmp = stageDir_ + "/.update.tmp";
    QDir(tmp).removeRecursively();
    QDir().mkpath(tmp);
    const QByteArray payload  = readAll(sourceDir_ + "/" + info_.payloadName);
    const QByteArray manifest = readAll(sourceDir_ + "/manifest.json");
    if (payload.isEmpty() || !writeAll(tmp + "/" + info_.payloadName, payload)
                          || !writeAll(tmp + "/manifest.json", manifest)) {
        QDir(tmp).removeRecursively();
        error_ = "download failed"; setState(State::Error); return false;
    }

    // 2) Verify the signature over the DOWNLOADED bytes. A tampered/truncated payload fails here
    //    and is discarded — never staged.
    const QByteArray sigHex = QJsonDocument::fromJson(manifest).object().value("sig").toString().toLatin1();
    const QByteArray got = readAll(tmp + "/" + info_.payloadName);
    if (sigHex.isEmpty() || !sig::verifyDetached(got, sigHex)) {
        QDir(tmp).removeRecursively();
        error_ = "signature verification failed"; setState(State::Error);
        prodlog::error("update", "staging rejected: signature verification failed");
        return false;
    }

    // 3) Atomically promote to the pending bundle: a WRITE-LAST marker means an interrupted
    //    promote can never look complete. (The payload lands first; the signed marker last.)
    const QString pend = pendingDir(stageDir_);
    QDir(pend).removeRecursively();
    QDir().mkpath(pend);
    if (!writeAll(pend + "/" + info_.payloadName, got)) {
        QDir(pend).removeRecursively(); QDir(tmp).removeRecursively();
        error_ = "stage failed"; setState(State::Error); return false;
    }
    writeAll(pend + "/manifest.json", manifest);   // marker written LAST
    QDir(tmp).removeRecursively();
    prodlog::info("update", "staged update " + info_.version + " — will apply on next restart");
    setState(State::Staged);
    return true;
}

bool UpdateManager::isStaged() const
{
    return QFileInfo::exists(pendingDir(stageDir_) + "/manifest.json");
}

QString UpdateManager::stagedVersion() const
{
    const QByteArray mj = readAll(pendingDir(stageDir_) + "/manifest.json");
    return parseManifest(mj).version;
}

bool UpdateManager::rollbackStaged()
{
    if (!isStaged()) return false;
    // Honor the removal result: if the staged bundle can't be deleted (e.g. a locked file), report
    // failure rather than claiming success — otherwise a bundle we "rolled back" could still be
    // applied on the next start. The caller/UI can then surface an honest error.
    if (!QDir(pendingDir(stageDir_)).removeRecursively()) {
        error_ = "could not cancel the staged update";
        prodlog::error("update", "rollback failed: staged bundle could not be removed");
        return false;
    }
    prodlog::info("update", "staged update rolled back by user");
    setState(State::Available);
    return true;
}

UpdateManager::ApplyResult
UpdateManager::applyPendingAtStartup(const QString& stageDir, long long /*currentVersionCode*/)
{
    // Always clear any temp download debris first.
    QDir(stageDir + "/.update.tmp").removeRecursively();

    const QString pend = pendingDir(stageDir);
    if (!QDir(pend).exists()) return ApplyResult::None;

    const QByteArray manifest = readAll(pend + "/manifest.json");
    const UpdateInfo u = parseManifest(manifest);
    const QByteArray sigHex = QJsonDocument::fromJson(manifest).object().value("sig").toString().toLatin1();

    // A COMPLETE bundle has the marker, the payload, and a signature that matches the payload.
    const QByteArray payload = u.valid ? readAll(pend + "/" + u.payloadName) : QByteArray();
    const bool complete = u.valid && !payload.isEmpty() && !sigHex.isEmpty()
                          && sig::verifyDetached(payload, sigHex);

    if (!complete) {
        // Interrupted / tampered staging — clean it and continue normally. The DB is untouched.
        QDir(pend).removeRecursively();
        prodlog::warning("update", "incomplete/invalid staged update discarded on startup");
        return ApplyResult::Recovered;
    }

    // Production: launch the staged installer (QProcess payload with an /update flag) and quit;
    // the installer replaces the binary and relaunches. In v1 we verify + record + clear staging
    // so the app proceeds on the current build (the installer hand-off is the documented step).
    prodlog::info("update", "applying staged update " + u.version + " at startup");
    QDir(pend).removeRecursively();
    return ApplyResult::Applied;
}
