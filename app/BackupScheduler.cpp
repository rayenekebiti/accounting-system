#include "BackupScheduler.h"
#include "Logging.h"
#include "storage/EventLog.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QTimer>
#include <algorithm>

namespace {
// Copy the top-level data files (skip the backups/ + scratch dirs) from src -> dst.
bool copyDataFiles(const QString& src, const QString& dst, qint64* bytes = nullptr)
{
    QDir().mkpath(dst);
    for (const QFileInfo& fi : QDir(src).entryInfoList(QDir::Files)) {
        const QString target = dst + "/" + fi.fileName();
        QFile::remove(target);
        if (!QFile::copy(fi.absoluteFilePath(), target)) return false;
        if (bytes) *bytes += fi.size();
    }
    return true;
}
} // namespace

BackupScheduler::BackupScheduler(QString dataDir, BackupPolicy policy, Clock clock, QObject* parent)
    : QObject(parent), dataDir_(std::move(dataDir)), policy_(policy),
      clock_(clock ? std::move(clock) : Clock([]{ return QDateTime::currentSecsSinceEpoch(); }))
{}

void BackupScheduler::start()
{
    // Hourly wake-up that backs up if due, then prunes. Cheap; never blocks the UI.
    auto* t = new QTimer(this);
    connect(t, &QTimer::timeout, this, [this] {
        if (isDue()) { runNow(true); prune(); }
    });
    t->start(60 * 60 * 1000);
    if (isDue()) { runNow(true); prune(); }   // catch up on launch
}

void BackupScheduler::stop() { /* timers are children; destroyed with this */ }

qint64 BackupScheduler::epochOf(const QString& dirName, qint64 mtimeFallback)
{
    if (dirName.startsWith("backup-")) {
        bool ok = false;
        const qint64 e = dirName.mid(7).toLongLong(&ok);
        if (ok) return e;
    }
    return mtimeFallback;   // manual backups (timestamp-named) fall back to mtime
}

qint64 BackupScheduler::lastBackupEpoch() const
{
    qint64 newest = 0;
    for (const RestorePoint& rp : restorePoints()) newest = std::max(newest, rp.epoch);
    return newest;
}

bool BackupScheduler::isDue() const
{
    const qint64 last = lastBackupEpoch();
    if (last == 0) return true;   // never backed up
    return (now() - last) >= qint64(policy_.intervalHours) * 3600;
}

QString BackupScheduler::runNow(bool verifyAfter)
{
    const QString name = QString("backup-%1").arg(now());
    const QString dst = backupsDir() + "/" + name;
    QDir(dst).removeRecursively();   // idempotent for a same-second re-run
    qint64 bytes = 0;
    if (!copyDataFiles(dataDir_, dst, &bytes)) {
        QDir(dst).removeRecursively();
        prodlog::error("backup", "scheduled backup failed (copy error) — folder writable / disk full?");
        return {};
    }
    bool ok = true;
    if (verifyAfter) ok = verify(name);
    prodlog::info("backup", QString("created restore point %1 (%2 bytes, verified=%3)")
        .arg(name).arg(bytes).arg(ok ? "yes" : "no"));
    emit backupsChanged();
    return name;
}

bool BackupScheduler::verify(const QString& name) const
{
    const QString log = backupsDir() + "/" + name + "/audit.log";
    if (!QFileInfo::exists(log)) return true;   // no authoritative history to check
    try {
        EventLog el(log.toStdString());          // opening validates CRC + gap-free seq
        (void)el.lastSeq();
        return true;
    } catch (const std::exception&) {
        prodlog::warning("backup", "restore point failed verification: " + name);
        return false;
    }
}

QVector<RestorePoint> BackupScheduler::restorePoints() const
{
    QVector<RestorePoint> out;
    QDir b(backupsDir());
    if (!b.exists()) return out;
    for (const QFileInfo& fi : b.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Time)) {
        RestorePoint rp;
        rp.name  = fi.fileName();
        rp.epoch = epochOf(fi.fileName(), fi.lastModified().toSecsSinceEpoch());
        qint64 sz = 0;
        for (const QFileInfo& f : QDir(fi.absoluteFilePath()).entryInfoList(QDir::Files)) sz += f.size();
        rp.sizeBytes = sz;
        rp.verified  = QFileInfo::exists(fi.absoluteFilePath() + "/audit.log");
        out.push_back(rp);
    }
    std::sort(out.begin(), out.end(), [](const RestorePoint& a, const RestorePoint& b){ return a.epoch > b.epoch; });
    return out;
}

int BackupScheduler::prune()
{
    QVector<RestorePoint> pts = restorePoints();   // newest first
    const qint64 cutoff = now() - qint64(policy_.maxAgeDays) * 86400;
    int removed = 0;
    for (int i = 0; i < pts.size(); ++i) {
        const bool overCount = i >= policy_.keep;
        const bool tooOld    = pts[i].epoch < cutoff;
        if (overCount || tooOld) {
            if (QDir(backupsDir() + "/" + pts[i].name).removeRecursively()) ++removed;
        }
    }
    if (removed > 0) {
        prodlog::info("backup", QString("retention removed %1 old restore point(s)").arg(removed));
        emit backupsChanged();
    }
    return removed;
}
