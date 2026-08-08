#include "SettingsViewModel.h"
#include "storage/StorageService.h"
#include "storage/AuditJournal.h"
#include "app/AppInfo.h"
#include <QSettings>

SettingsViewModel::SettingsViewModel(QObject* parent)
    : QObject(parent)
{
    QSettings s;
    companyName_    = s.value(QStringLiteral("company/name")).toString();
    companyAddress_ = s.value(QStringLiteral("company/address")).toString();
    companyTaxId_   = s.value(QStringLiteral("company/taxId")).toString();
    companyEmail_   = s.value(QStringLiteral("company/email")).toString();
    dateFormat_     = s.value(QStringLiteral("general/dateFormat"), QStringLiteral("yyyy-MM-dd")).toString();
    currencySymbol_ = s.value(QStringLiteral("general/currency"), QStringLiteral("$")).toString();
    // Default the tracked channel to the channel THIS build was cut for; normalise any stale value.
    updateChannel_  = appinfo::channelName(appinfo::parseChannel(
        s.value(QStringLiteral("update/channel"), appinfo::channel()).toString(),
        appinfo::buildChannel()));
}

QStringList SettingsViewModel::availableChannels() const { return appinfo::allChannels(); }
QString     SettingsViewModel::buildChannel()      const { return appinfo::channel(); }
QString     SettingsViewModel::channelLabel(const QString& id) const
{ return appinfo::channelDisplay(appinfo::parseChannel(id)); }

void SettingsViewModel::setUpdateChannel(const QString& v)
{
    const QString norm = appinfo::channelName(appinfo::parseChannel(v, appinfo::buildChannel()));
    if (updateChannel_ == norm) return;
    updateChannel_ = norm;
    QSettings().setValue(QStringLiteral("update/channel"), norm);
    emit updateChannelChanged(norm);
}

void SettingsViewModel::setCompanyName(const QString& v)
{ if (companyName_ == v) return; companyName_ = v; QSettings().setValue(QStringLiteral("company/name"), v); emit companyChanged(); }
void SettingsViewModel::setCompanyAddress(const QString& v)
{ if (companyAddress_ == v) return; companyAddress_ = v; QSettings().setValue(QStringLiteral("company/address"), v); emit companyChanged(); }
void SettingsViewModel::setCompanyTaxId(const QString& v)
{ if (companyTaxId_ == v) return; companyTaxId_ = v; QSettings().setValue(QStringLiteral("company/taxId"), v); emit companyChanged(); }
void SettingsViewModel::setCompanyEmail(const QString& v)
{ if (companyEmail_ == v) return; companyEmail_ = v; QSettings().setValue(QStringLiteral("company/email"), v); emit companyChanged(); }
void SettingsViewModel::setDateFormat(const QString& v)
{ if (dateFormat_ == v) return; dateFormat_ = v; QSettings().setValue(QStringLiteral("general/dateFormat"), v); emit generalChanged(); }
void SettingsViewModel::setCurrencySymbol(const QString& v)
{ if (currencySymbol_ == v) return; currencySymbol_ = v; QSettings().setValue(QStringLiteral("general/currency"), v); emit generalChanged(); }

void SettingsViewModel::captureStartupRecovery()
{
    if (!StorageService::instance().isInitialized()) return;
    auto& storage = StorageService::instance();
    recoveryOccurred_ = (storage.auditReconciled() > 0) || storage.auditTornTail();
    if (recoveryOccurred_) {
        // Re-verify the projection against authoritative history — non-destructive.
        const auto vr = storage.verifyAuditProjection();
        recoveryVerified_ = vr.ok;
        recoverySeq_      = vr.seq;
    }
}

// Formatted on read (not at capture time) so it uses the translator active when the dialog is
// shown — captureStartupRecovery() runs before LocaleController installs the translator.
QString SettingsViewModel::recoveryDetail() const
{
    return recoveryVerified_
        ? tr("Your accounting data was verified against its authoritative history and no "
             "inconsistencies were found.")
        : tr("Automatic verification found an inconsistency (live projection ≠ history at "
             "seq %1). Do not continue — restore your most recent backup.").arg(recoverySeq_);
}
