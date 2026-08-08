#ifndef QUICK_PLATFORM_CONTROLLER_H
#define QUICK_PLATFORM_CONTROLLER_H

#include <QObject>
#include <QString>

class LicenseManager;
class BackupScheduler;
class UpdateManager;

// Read-mostly bridge that exposes the commercial (C2) layer to QML: license status, the one
// startup-health report, update availability, and product version. It owns no engine state and
// never writes to StorageService — it only reads the managers (which themselves live above the
// engine). Licensing/updates/health are surfaced to the About + Diagnostics screens.
class PlatformController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString appVersion   READ appVersion   CONSTANT)
    Q_PROPERTY(QString buildId      READ buildId      CONSTANT)
    Q_PROPERTY(QString channel      READ channel      CONSTANT)

    Q_PROPERTY(QString licenseState   READ licenseState   NOTIFY licenseChanged)
    Q_PROPERTY(QString licenseEdition READ licenseEdition NOTIFY licenseChanged)
    Q_PROPERTY(bool    licenseValid   READ licenseValid   NOTIFY licenseChanged)
    Q_PROPERTY(bool    licenseInGrace READ licenseInGrace NOTIFY licenseChanged)
    Q_PROPERTY(int     licenseDaysRemaining READ licenseDaysRemaining NOTIFY licenseChanged)
    Q_PROPERTY(QString licenseDetail  READ licenseDetail  NOTIFY licenseChanged)

    Q_PROPERTY(QString updateState    READ updateState    NOTIFY updateChanged)
    Q_PROPERTY(bool    updateAvailable READ updateAvailable NOTIFY updateChanged)
    Q_PROPERTY(bool    updateStaged   READ updateStaged   NOTIFY updateChanged)
    Q_PROPERTY(QString availableVersion READ availableVersion NOTIFY updateChanged)
    Q_PROPERTY(QString updateStatusText READ updateStatusText NOTIFY updateChanged)

    Q_PROPERTY(QString healthReport READ healthReport NOTIFY healthChanged)
    Q_PROPERTY(bool    healthOk     READ healthOk     NOTIFY healthChanged)

public:
    PlatformController(LicenseManager* lic, BackupScheduler* backups, UpdateManager* updates,
                       QObject* parent = nullptr);

    QString appVersion() const;
    QString buildId() const;
    QString channel() const;

    QString licenseState() const;
    QString licenseEdition() const;
    bool    licenseValid() const;
    bool    licenseInGrace() const;
    int     licenseDaysRemaining() const;
    QString licenseDetail() const;

    QString updateState() const;
    bool    updateAvailable() const;
    bool    updateStaged() const;
    QString availableVersion() const;
    QString updateStatusText() const { return updateStatus_; }

    QString healthReport() const { return healthReport_; }
    bool    healthOk() const { return healthOk_; }

    Q_INVOKABLE bool activateLicense(const QString& token);
    Q_INVOKABLE void checkForUpdates();
    Q_INVOKABLE bool downloadUpdate();
    Q_INVOKABLE bool rollbackUpdate();
    Q_INVOKABLE void refreshHealth();

signals:
    void licenseChanged();
    void updateChanged();
    void healthChanged();

private:
    LicenseManager*  lic_;
    BackupScheduler* backups_;
    UpdateManager*   updates_;
    QString          healthReport_;
    bool             healthOk_ = false;
    QString          updateStatus_;
};

#endif // QUICK_PLATFORM_CONTROLLER_H
