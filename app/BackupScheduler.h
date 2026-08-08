#ifndef APP_BACKUP_SCHEDULER_H
#define APP_BACKUP_SCHEDULER_H

#include <QObject>
#include <QString>
#include <QVector>
#include <functional>

// Automatic backup lifecycle above StorageService: scheduling, retention, restore points,
// per-backup verification, and cleanup. A backup is a plain COPY of the data folder into
// <dataDir>/backups/backup-<epoch>/ (the engine already guarantees deterministic recovery from
// those files); this class NEVER writes into the live books — it only reads + copies them.
// All time comes from an injectable clock, so scheduling/retention are fully deterministic.
struct BackupPolicy {
    int intervalHours = 24;   // minimum spacing between automatic backups
    int keep          = 14;   // retain at most this many restore points
    int maxAgeDays    = 90;   // and drop anything older than this
};

struct RestorePoint {
    QString name;
    qint64  epoch     = 0;    // when it was taken
    qint64  sizeBytes = 0;
    bool    verified  = false;
};

class BackupScheduler : public QObject
{
    Q_OBJECT
public:
    using Clock = std::function<qint64()>;   // epoch seconds

    explicit BackupScheduler(QString dataDir, BackupPolicy policy = {},
                             Clock clock = {}, QObject* parent = nullptr);

    void start();                 // begin periodic due-checks (production); no-op in tests
    void stop();

    bool    isDue() const;        // now - lastBackupEpoch >= intervalHours
    QString runNow(bool verifyAfter = true);   // create a restore point; returns its name or ""
    bool    verify(const QString& name) const; // open the backup's authoritative log read-only
    int     prune();              // apply retention (keep + maxAge); returns #removed
    qint64  lastBackupEpoch() const;
    QVector<RestorePoint> restorePoints() const;   // newest first

signals:
    void backupsChanged();

private:
    QString backupsDir() const { return dataDir_ + "/backups"; }
    qint64  now() const { return clock_(); }
    static qint64 epochOf(const QString& dirName, qint64 mtimeFallback);

    QString      dataDir_;
    BackupPolicy policy_;
    Clock        clock_;
};

#endif // APP_BACKUP_SCHEDULER_H
