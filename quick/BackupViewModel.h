#ifndef QUICK_BACKUP_VIEW_MODEL_H
#define QUICK_BACKUP_VIEW_MODEL_H

#include <QObject>
#include <QString>
#include <QVariantList>

// Production backup UX over the existing on-disk data directory. A backup is a plain COPY of the
// data folder into <dataDir>/backups/<timestamp>/ — the engine already guarantees deterministic
// recovery from those files. Never exposes internal file names to the user; a restore is staged
// and applied on the next startup (before the store opens), so it is safe against the live lock.
class BackupViewModel : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QVariantList backups     READ backups     NOTIFY changed)   // [{name,dateText,sizeText,estimatedText}]
    Q_PROPERTY(QString      estimatedSize READ estimatedSize NOTIFY changed)
    Q_PROPERTY(QString      lastBackupText READ lastBackupText NOTIFY changed)
    Q_PROPERTY(bool         busy         READ busy        NOTIFY busyChanged)
    Q_PROPERTY(QString      statusText   READ statusText  NOTIFY statusChanged)
    Q_PROPERTY(bool         restartRequired READ restartRequired NOTIFY statusChanged)

public:
    explicit BackupViewModel(QObject* parent = nullptr);

    QVariantList backups()       const { return backups_; }
    QString      estimatedSize() const { return estimatedSize_; }
    QString      lastBackupText() const;
    bool         busy()          const { return busy_; }
    QString      statusText()    const { return statusText_; }
    bool         restartRequired() const { return restartRequired_; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE bool backupNow();
    Q_INVOKABLE bool verify(const QString& name);   // integrity-check a backup's authoritative log
    Q_INVOKABLE bool restore(const QString& name);  // stage + require restart
    // Re-emit so QML re-reads lastBackupText/labels under a newly-installed translator (live switch).
    Q_INVOKABLE void retranslate() { emit changed(); }

signals:
    void changed();
    void busyChanged();
    void statusChanged();

private:
    QString dataDir() const;
    void    rebuild();

    QVariantList backups_;
    QString      estimatedSize_;
    bool         busy_ = false;
    QString      statusText_;
    bool         restartRequired_ = false;
};

#endif // QUICK_BACKUP_VIEW_MODEL_H
