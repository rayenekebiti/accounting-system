#include "StartupDiagnostics.h"
#include "LicenseManager.h"
#include "BackupScheduler.h"
#include "UpdateManager.h"
#include "storage/StorageService.h"
#include "storage/AuditJournal.h"
#include "storage/CompatibilityManifest.h"
#include <QDir>
#include <QFileInfo>
#include <QStorageInfo>
#include <QDateTime>

namespace startupdiag {

Health collect(StorageService& storage, const LicenseManager* lic,
               const BackupScheduler* backups, const UpdateManager* updates)
{
    Health h;

    // ── Storage / compatibility / snapshot / verification (read-only engine reads) ──
    h.storageOpen = storage.isInitialized();
    if (h.storageOpen) {
        h.dataDir = QString::fromStdString(storage.dataDir());
        for (const QFileInfo& fi : QDir(h.dataDir).entryInfoList(QDir::Files)) h.dbBytes += fi.size();
        h.eventCount   = storage.auditEventCount();
        h.currentSeq   = storage.audit().lastSeq();
        h.snapshotSeq  = storage.audit().ledgerSnapshotSeq();
        h.snapshotValid = h.snapshotSeq > 0;
        h.trialBalanceZero = storage.audit().trialBalanceTotal() == 0;
        h.compatStatus = QString::fromLatin1(compat::toString(storage.compatibilityStatus()));
        const GovernanceVersions g = storage.governanceVersions();
        h.governance = QStringLiteral("schema %1 · replay %2 · posting %3 · snapshot %4")
            .arg(g.schema).arg(g.replay).arg(g.postingPolicy).arg(g.snapshot);
        h.diskFreeBytes = QStorageInfo(h.dataDir).bytesAvailable();
    }

    // ── License ──
    if (lic) {
        const lic::Status s = lic->status();
        h.licenseState   = QString::fromLatin1(lic::stateName(s.state));
        h.licenseEdition = QString::fromLatin1(lic::editionName(s.edition));
        h.licenseValid   = s.valid;
        h.licenseDaysRemaining = s.daysRemaining;
    }

    // ── Backups ──
    if (backups) {
        h.lastBackupEpoch = backups->lastBackupEpoch();
        h.restorePoints   = backups->restorePoints().size();
        if (h.lastBackupEpoch > 0)
            h.backupAgeHours = (QDateTime::currentSecsSinceEpoch() - h.lastBackupEpoch) / 3600;
    }

    // ── Update ──
    if (updates) {
        h.updateStaged    = updates->isStaged();
        h.updateAvailable = updates->state() == UpdateManager::State::Available;
        if (h.updateStaged) h.stagedVersion = updates->stagedVersion();
    }

    // ── Overall (no red flags) ──
    const bool diskOk = h.diskFreeBytes < 0 || h.diskFreeBytes > (50LL << 20);
    h.ok = h.storageOpen && h.trialBalanceZero && h.licenseValid
           && h.compatStatus.contains("compatible") && diskOk;
    h.summary = h.ok ? QStringLiteral("All systems healthy")
                     : QStringLiteral("Attention needed — review the report");
    return h;
}

static QString ageText(qint64 hours)
{
    if (hours < 0) return QStringLiteral("never");
    if (hours < 1) return QStringLiteral("< 1h ago");
    if (hours < 48) return QString("%1h ago").arg(hours);
    return QString("%1d ago").arg(hours / 24);
}

QString format(const Health& h)
{
    auto yn = [](bool b){ return b ? "ok" : "CHECK"; };
    QString r;
    r += "Occountant — Startup Health\n===========================\n";
    r += QString("overall:       %1\n").arg(h.summary);
    r += QString("storage:       %1 (db %2 KB, events %3, seq %4)\n")
            .arg(h.storageOpen ? "open" : "CLOSED").arg(h.dbBytes / 1024).arg(h.eventCount).arg(h.currentSeq);
    r += QString("compatibility: %1  [%2]\n").arg(h.compatStatus, yn(h.compatStatus.contains("compatible")));
    r += QString("governance:    %1\n").arg(h.governance);
    r += QString("snapshot:      %1\n").arg(h.snapshotValid ? QString("valid (seq %1)").arg(h.snapshotSeq) : QStringLiteral("none"));
    r += QString("verification:  trial balance %1  [%2]\n").arg(h.trialBalanceZero ? "balanced" : "OUT OF BALANCE", yn(h.trialBalanceZero));
    r += QString("license:       %1 (%2)  valid=%3  daysLeft=%4\n")
            .arg(h.licenseState, h.licenseEdition).arg(h.licenseValid ? "yes" : "no").arg(h.licenseDaysRemaining);
    r += QString("backup:        %1  (%2 restore point(s))\n").arg(ageText(h.backupAgeHours)).arg(h.restorePoints);
    r += QString("update:        %1\n").arg(h.updateStaged ? ("staged " + h.stagedVersion)
                                          : h.updateAvailable ? QStringLiteral("available") : QStringLiteral("up to date"));
    r += QString("disk free:     %1 MB\n").arg(h.diskFreeBytes < 0 ? -1 : (h.diskFreeBytes >> 20));
    return r;
}

} // namespace startupdiag
