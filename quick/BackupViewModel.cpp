#include "BackupViewModel.h"
#include "storage/StorageService.h"
#include "storage/EventLog.h"
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QDateTime>
#include <QSettings>

static QString humanBytes(qint64 b)
{
    if (b < 1024) return QString("%1 B").arg(b);
    double v = b / 1024.0; const char* u = "KB";
    if (v >= 1024) { v /= 1024.0; u = "MB"; }
    if (v >= 1024) { v /= 1024.0; u = "GB"; }
    return QString("%1 %2").arg(v, 0, 'f', 1).arg(u);
}

// Copy the data files (top-level only — skip the backups/ + scratch dirs) from src → dst.
static bool copyDataFiles(const QString& src, const QString& dst, qint64* bytes = nullptr)
{
    QDir().mkpath(dst);
    QDir s(src);
    for (const QFileInfo& fi : s.entryInfoList(QDir::Files)) {
        const QString target = dst + "/" + fi.fileName();
        QFile::remove(target);
        if (!QFile::copy(fi.absoluteFilePath(), target)) return false;
        if (bytes) *bytes += fi.size();
    }
    return true;
}

BackupViewModel::BackupViewModel(QObject* parent) : QObject(parent) { rebuild(); }

QString BackupViewModel::dataDir() const
{
    return StorageService::instance().isInitialized()
        ? QString::fromStdString(StorageService::instance().dataDir()) : QString();
}

QString BackupViewModel::lastBackupText() const
{
    return backups_.isEmpty() ? tr("No backups yet")
                              : backups_.first().toMap().value(QStringLiteral("dateText")).toString();
}

void BackupViewModel::rebuild()
{
    backups_.clear();
    const QString dir = dataDir();
    if (!dir.isEmpty()) {
        qint64 live = 0;
        for (const QFileInfo& fi : QDir(dir).entryInfoList(QDir::Files)) live += fi.size();
        estimatedSize_ = humanBytes(live);

        QDir b(dir + "/backups");
        if (b.exists()) {
            for (const QFileInfo& fi : b.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Time)) {
                qint64 sz = 0;
                for (const QFileInfo& f : QDir(fi.absoluteFilePath()).entryInfoList(QDir::Files)) sz += f.size();
                QVariantMap m;
                m.insert(QStringLiteral("name"),     fi.fileName());
                m.insert(QStringLiteral("dateText"), fi.lastModified().toString(QStringLiteral("yyyy-MM-dd HH:mm")));
                m.insert(QStringLiteral("sizeText"), humanBytes(sz));
                backups_.append(m);
            }
        }
    }
    emit changed();
}

void BackupViewModel::refresh() { rebuild(); }

bool BackupViewModel::backupNow()
{
    const QString dir = dataDir();
    if (dir.isEmpty()) { statusText_ = tr("No open data set to back up."); emit statusChanged(); return false; }
    busy_ = true; emit busyChanged();
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_HH-mm-ss"));
    const QString dst = dir + "/backups/" + stamp;
    qint64 bytes = 0;
    const bool ok = copyDataFiles(dir, dst, &bytes);
    busy_ = false; emit busyChanged();
    if (!ok) {
        QDir(dst).removeRecursively();   // never leave a half-written backup
        statusText_ = tr("Backup failed — check that the folder is writable and the disk is not full.");
    } else {
        statusText_ = tr("Backed up %1 successfully.").arg(humanBytes(bytes));
    }
    emit statusChanged();
    rebuild();
    return ok;
}

bool BackupViewModel::verify(const QString& name)
{
    const QString dir = dataDir();
    const QString logPath = dir + "/backups/" + name + "/audit.log";
    if (!QFileInfo::exists(logPath)) {
        statusText_ = tr("This backup has no authoritative history to verify."); emit statusChanged(); return false;
    }
    busy_ = true; emit busyChanged();
    bool ok = true;
    try {
        EventLog log(logPath.toStdString());   // opening validates the committed history (CRC + gap-free seq)
        (void)log.lastSeq();
    } catch (const std::exception&) { ok = false; }
    busy_ = false; emit busyChanged();
    statusText_ = ok ? tr("Backup verified — its history is intact and readable.")
                     : tr("Backup verification FAILED — this backup is corrupt; keep an earlier one.");
    emit statusChanged();
    return ok;
}

bool BackupViewModel::restore(const QString& name)
{
    const QString dir = dataDir();
    const QString srcDir = dir + "/backups/" + name;
    if (!QFileInfo::exists(srcDir)) { statusText_ = tr("That backup no longer exists."); emit statusChanged(); return false; }

    // DATA SAFETY: restore is destructive — the staged set replaces the LIVE books on next start.
    // Never overwrite good data with a backup we have not proven is readable. A corrupt or
    // history-less backup would otherwise destroy the current books AND fail to open. Verify first;
    // if it fails, refuse and leave the live data untouched (nothing is staged).
    const QString logPath = srcDir + "/audit.log";
    if (!QFileInfo::exists(logPath)) {
        statusText_ = tr("Restore refused — this backup has no authoritative history (audit.log). "
                         "Your current data is untouched.");
        emit statusChanged();
        return false;
    }
    try {
        EventLog log(logPath.toStdString());   // opening validates the committed history (CRC + gap-free seq)
        (void)log.lastSeq();
    } catch (const std::exception&) {
        statusText_ = tr("Restore refused — this backup is corrupt and cannot be opened. "
                         "Your current data is untouched. Choose an earlier backup.");
        emit statusChanged();
        return false;
    }

    busy_ = true; emit busyChanged();
    // Stage the restore next to the data files; it is applied on the next startup BEFORE the store
    // opens (the live files are locked while the app runs). Safe + crash-consistent.
    const QString stage = dir + "/.pending-restore";
    QDir(stage).removeRecursively();
    const bool ok = copyDataFiles(srcDir, stage);
    busy_ = false; emit busyChanged();
    if (!ok) {
        QDir(stage).removeRecursively();
        statusText_ = tr("Could not stage the restore — check disk space and permissions.");
        emit statusChanged();
        return false;
    }
    QSettings().setValue(QStringLiteral("restore/pending"), name);
    restartRequired_ = true;
    statusText_ = tr("Restore staged. Close and reopen Occountant to complete it.");
    emit statusChanged();
    return true;
}
