#ifndef QUICK_SETTINGS_VIEW_MODEL_H
#define QUICK_SETTINGS_VIEW_MODEL_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <cstdint>

// Production settings — user preferences persisted to QSettings (machine-global, independent of
// the accounting data). NEVER touches the event store / ledger. Also carries the startup-recovery
// status the crash-recovery UX reads.
class SettingsViewModel : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString companyName    READ companyName    WRITE setCompanyName    NOTIFY companyChanged)
    Q_PROPERTY(QString companyAddress READ companyAddress WRITE setCompanyAddress NOTIFY companyChanged)
    Q_PROPERTY(QString companyTaxId   READ companyTaxId   WRITE setCompanyTaxId   NOTIFY companyChanged)
    Q_PROPERTY(QString companyEmail   READ companyEmail   WRITE setCompanyEmail   NOTIFY companyChanged)
    Q_PROPERTY(QString dateFormat     READ dateFormat     WRITE setDateFormat     NOTIFY generalChanged)
    Q_PROPERTY(QString currencySymbol READ currencySymbol WRITE setCurrencySymbol NOTIFY generalChanged)

    // Release-channel selection (drives the updater). The channel ids are technical identifiers
    // (stable/rc/beta/development), not translatable UI chrome. Persisted to QSettings.
    Q_PROPERTY(QString     updateChannel     READ updateChannel WRITE setUpdateChannel NOTIFY updateChannelChanged)
    Q_PROPERTY(QStringList availableChannels READ availableChannels CONSTANT)
    Q_PROPERTY(QString     buildChannel      READ buildChannel      CONSTANT)

    // Startup crash-recovery status (read-only; the RecoveryDialog / RecoveryBlocker read these).
    Q_PROPERTY(bool recoveryOccurred READ recoveryOccurred CONSTANT)
    Q_PROPERTY(bool recoveryVerified READ recoveryVerified CONSTANT)
    // Detail text is formatted from state on read (not stored) so it uses the translator active
    // when the dialog is shown — captureStartupRecovery() runs before the translator is installed.
    Q_PROPERTY(QString recoveryDetail READ recoveryDetail NOTIFY recoveryChanged)

public:
    explicit SettingsViewModel(QObject* parent = nullptr);

    QString companyName()    const { return companyName_; }
    QString companyAddress() const { return companyAddress_; }
    QString companyTaxId()   const { return companyTaxId_; }
    QString companyEmail()   const { return companyEmail_; }
    QString dateFormat()     const { return dateFormat_; }
    QString currencySymbol() const { return currencySymbol_; }

    QString     updateChannel()     const { return updateChannel_; }
    QStringList availableChannels() const;
    QString     buildChannel()      const;
    void        setUpdateChannel(const QString& v);
    // Human label for a channel id (English technical labels; not localised UI chrome).
    Q_INVOKABLE QString channelLabel(const QString& id) const;

    bool    recoveryOccurred() const { return recoveryOccurred_; }
    bool    recoveryVerified() const { return recoveryVerified_; }
    QString recoveryDetail()   const;   // formatted from state → uses the active translator

    void setCompanyName(const QString& v);
    void setCompanyAddress(const QString& v);
    void setCompanyTaxId(const QString& v);
    void setCompanyEmail(const QString& v);
    void setDateFormat(const QString& v);
    void setCurrencySymbol(const QString& v);

    // Called once at startup (after the engine is initialised) to capture recovery status.
    void captureStartupRecovery();
    // Re-emit so the recovery dialog re-reads recoveryDetail under a newly-installed translator.
    Q_INVOKABLE void retranslate() { emit recoveryChanged(); }

signals:
    void companyChanged();
    void generalChanged();
    void recoveryChanged();
    // Carries the new channel id so the app can retarget the UpdateManager without a back-reference.
    void updateChannelChanged(const QString& channel);

private:
    QString  companyName_, companyAddress_, companyTaxId_, companyEmail_;
    QString  dateFormat_, currencySymbol_;
    QString  updateChannel_;
    bool     recoveryOccurred_ = false;
    bool     recoveryVerified_ = true;
    uint64_t recoverySeq_ = 0;
};

#endif // QUICK_SETTINGS_VIEW_MODEL_H
