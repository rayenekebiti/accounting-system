#ifndef APP_LICENSE_MANAGER_H
#define APP_LICENSE_MANAGER_H

#include "LicenseTypes.h"
#include <QObject>
#include <QString>
#include <functional>

// Offline-first licensing above StorageService. The signed license token is validated LOCALLY
// (no network, ever), the result is cached deterministically for fast startup, a grace window
// absorbs a just-expired license, and any tamper (bad signature / corrupt cache) is detected and
// never trusted. The engine has no knowledge of this class.
//
// Determinism: all time comes from an injectable clock (epoch seconds), so every state
// transition is reproducible in tests with zero wall-clock or network dependence.
class LicenseManager : public QObject
{
    Q_OBJECT
public:
    using Clock = std::function<qint64()>;   // epoch seconds

    static constexpr int kTrialDays = 30;
    static constexpr int kGraceDays = 7;      // after expiry, still usable but flagged

    // `storeDir` holds license.key + license.cache (machine-global config dir in production;
    // a scratch dir in tests). Clock defaults to the system clock.
    explicit LicenseManager(QString storeDir, Clock clock = {}, QObject* parent = nullptr);

    // Load + validate. On first run with no token, a Trial is issued (self-signed in v1) and
    // persisted. Safe to call once at startup; deterministic given the clock.
    void initialize();

    // Install a user-supplied license token (from "Enter license key"). Validates + persists on
    // success; returns whether it was accepted. Never throws.
    bool activate(const QString& token);

    lic::Status status() const { return status_; }
    bool        isUsable() const { return status_.valid; }
    // Whether the active license enables a named feature (case-sensitive vendor key).
    bool        hasFeature(const QString& key) const { return status_.features.contains(key); }

    // Mint a SELF-SIGNED (HMAC) token. Used by the first-run trial and legacy fixtures. HMAC tokens
    // are LOCAL-TRUST: the app can mint them offline, but a hs256 token is only ever accepted for the
    // TRIAL edition (a hs256 token claiming Personal/Business is rejected as a forgery — see
    // parseToken). Not a boundary against a determined attacker; only the trial relies on it.
    static QString mintToken(lic::Edition edition, const QString& issuedTo,
                             qint64 issuedAtEpoch, qint64 expiresAtEpoch,
                             const QString& licenseId = QString());

    // Mint a VENDOR license: asymmetric Ed25519, domain-tagged so it cannot be confused with an
    // update signature. Only works where the private key is compiled in (-DACCT_DEV_SIGNING: the
    // generator + dev/test builds); a release build returns an empty (unusable) token. This is the
    // ONLY way to grant a Personal/Business edition — the shipped app can verify it but never forge it.
    static QString mintVendorLicense(lic::Edition edition, const QString& issuedTo,
                                     const QStringList& features,
                                     qint64 issuedAtEpoch, qint64 expiresAtEpoch,
                                     const QString& licenseId = QString());

signals:
    void statusChanged();

private:
    QString keyPath()   const { return storeDir_ + "/license.key"; }
    QString cachePath() const { return storeDir_ + "/license.cache"; }
    qint64  now() const { return clock_(); }

    bool         parseToken(const QString& token, lic::Status& out, QString& why) const;
    lic::Status  deriveState(lic::Status parsed) const;   // apply expiry + grace vs now()
    void         writeCache(const lic::Status& s) const;
    bool         readCache(lic::Status& out) const;       // false if absent/corrupt (tamper)
    void         issueTrial();                            // first-run self-signed trial
    void         setStatus(const lic::Status& s);

    QString     storeDir_;
    Clock       clock_;
    lic::Status status_;
};

#endif // APP_LICENSE_MANAGER_H
