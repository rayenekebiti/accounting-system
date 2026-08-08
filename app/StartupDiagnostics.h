#ifndef APP_STARTUP_DIAGNOSTICS_H
#define APP_STARTUP_DIAGNOSTICS_H

#include <QString>
#include <cstdint>

class StorageService;
class LicenseManager;
class BackupScheduler;
class UpdateManager;

// The single startup health view. Aggregates read-only signals from the engine (storage /
// compatibility / snapshot / verification) and the commercial layer (license / backup age /
// pending update / disk) into ONE report + ONE struct the UI can bind. Reads only; it never
// mutates the engine or any manager.
namespace startupdiag {

struct Health {
    // storage
    bool     storageOpen     = false;
    QString  dataDir;
    qint64   dbBytes         = 0;
    quint64  eventCount      = 0;
    quint64  currentSeq      = 0;
    // compatibility / governance
    QString  compatStatus    = QStringLiteral("unknown");
    QString  governance;
    // snapshot
    quint64  snapshotSeq     = 0;
    bool     snapshotValid   = false;
    // verification
    bool     trialBalanceZero = false;
    // license
    QString  licenseState    = QStringLiteral("Unknown");
    QString  licenseEdition;
    bool     licenseValid    = false;
    qint64   licenseDaysRemaining = 0;
    // backup
    qint64   lastBackupEpoch = 0;
    qint64   backupAgeHours  = -1;   // -1 = never
    int      restorePoints   = 0;
    // update
    bool     updateAvailable = false;
    bool     updateStaged    = false;
    QString  stagedVersion;
    // disk
    qint64   diskFreeBytes   = -1;
    // overall
    bool     ok              = false;   // no red flags
    QString  summary;
};

// Managers may be null (e.g. headless contexts); those sections are reported as unknown/absent.
Health collect(StorageService& storage, const LicenseManager* lic,
               const BackupScheduler* backups, const UpdateManager* updates);

QString format(const Health& h);   // one-screen plaintext report

} // namespace startupdiag

#endif // APP_STARTUP_DIAGNOSTICS_H
