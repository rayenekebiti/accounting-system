#include "PlatformController.h"
#include "app/AppInfo.h"
#include "app/LicenseManager.h"
#include "app/BackupScheduler.h"
#include "app/UpdateManager.h"
#include "app/StartupDiagnostics.h"
#include "storage/StorageService.h"

PlatformController::PlatformController(LicenseManager* lic, BackupScheduler* backups,
                                      UpdateManager* updates, QObject* parent)
    : QObject(parent), lic_(lic), backups_(backups), updates_(updates)
{
    if (lic_)     connect(lic_,     &LicenseManager::statusChanged, this, &PlatformController::licenseChanged);
    if (updates_) connect(updates_, &UpdateManager::stateChanged,   this, &PlatformController::updateChanged);
    refreshHealth();
}

QString PlatformController::appVersion() const { return appinfo::version(); }
QString PlatformController::buildId()    const { return appinfo::buildId(); }
QString PlatformController::channel()    const { return appinfo::channel(); }

QString PlatformController::licenseState() const
{ return lic_ ? QString::fromLatin1(lic::stateName(lic_->status().state)) : QStringLiteral("Unknown"); }
QString PlatformController::licenseEdition() const
{ return lic_ ? QString::fromLatin1(lic::editionName(lic_->status().edition)) : QString(); }
bool PlatformController::licenseValid() const   { return lic_ && lic_->status().valid; }
bool PlatformController::licenseInGrace() const { return lic_ && lic_->status().inGrace; }
int  PlatformController::licenseDaysRemaining() const { return lic_ ? int(lic_->status().daysRemaining) : 0; }
QString PlatformController::licenseDetail() const { return lic_ ? lic_->status().detail : QString(); }

QString PlatformController::updateState() const
{
    if (!updates_) return QStringLiteral("Idle");
    switch (updates_->state()) {
    case UpdateManager::State::Idle:        return QStringLiteral("Idle");
    case UpdateManager::State::Checking:    return QStringLiteral("Checking");
    case UpdateManager::State::UpToDate:    return QStringLiteral("UpToDate");
    case UpdateManager::State::Available:   return QStringLiteral("Available");
    case UpdateManager::State::Downloading: return QStringLiteral("Downloading");
    case UpdateManager::State::Staged:      return QStringLiteral("Staged");
    case UpdateManager::State::Error:       return QStringLiteral("Error");
    }
    return QStringLiteral("Idle");
}
bool PlatformController::updateAvailable() const { return updates_ && updates_->state() == UpdateManager::State::Available; }
bool PlatformController::updateStaged() const    { return updates_ && updates_->isStaged(); }
QString PlatformController::availableVersion() const { return updates_ ? updates_->available().version : QString(); }

bool PlatformController::activateLicense(const QString& token)
{
    const bool ok = lic_ && lic_->activate(token);
    refreshHealth();
    return ok;
}

void PlatformController::checkForUpdates()
{
    if (!updates_) return;
    if (!updates_->check())
        updateStatus_ = QStringLiteral("Could not check for updates: %1").arg(updates_->errorText());
    else if (updates_->state() == UpdateManager::State::Available)
        updateStatus_ = QStringLiteral("Update %1 is available.").arg(updates_->available().version);
    else if (updates_->isStaged())
        updateStatus_ = QStringLiteral("Update %1 is staged — restart to install.").arg(updates_->stagedVersion());
    else
        updateStatus_ = QStringLiteral("You are up to date.");
    emit updateChanged();
}

bool PlatformController::downloadUpdate()
{
    if (!updates_) return false;
    const bool ok = updates_->downloadAndStage();
    updateStatus_ = ok ? QStringLiteral("Update staged — restart to install.")
                       : QStringLiteral("Download failed: %1").arg(updates_->errorText());
    emit updateChanged();
    return ok;
}

bool PlatformController::rollbackUpdate()
{
    const bool ok = updates_ && updates_->rollbackStaged();
    if (ok) updateStatus_ = QStringLiteral("Staged update cancelled.");
    emit updateChanged();
    return ok;
}

void PlatformController::refreshHealth()
{
    const startupdiag::Health h =
        startupdiag::collect(StorageService::instance(), lic_, backups_, updates_);
    healthReport_ = startupdiag::format(h);
    healthOk_ = h.ok;
    emit healthChanged();
}
