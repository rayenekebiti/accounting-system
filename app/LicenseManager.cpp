#include "LicenseManager.h"
#include "Signature.h"
#include "Logging.h"
#include <QDir>
#include <QFile>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUuid>

namespace {
constexpr qint64 kDay = 86400;

// Prepended before Ed25519 signing/verification of a license so a license signature can NEVER be
// replayed as an update-payload signature (both reuse the vendor Ed25519 key). Never change it.
const QByteArray kLicenseDomain = QByteArrayLiteral("occ.lic.v2:");

// Build the base64url license payload shared by both minting paths. `alg` records how it is signed
// ("hs256" self-signed HMAC | "ed25519" vendor asymmetric); readers use it to pick the verifier.
QByteArray licensePayload(const char* alg, lic::Edition edition, const QString& issuedTo,
                          const QStringList& features, qint64 iat, qint64 exp, const QString& licenseId)
{
    QJsonObject o;
    o["v"]   = 2;
    o["alg"] = QString::fromLatin1(alg);
    o["ed"]  = static_cast<int>(edition);
    o["to"]  = issuedTo;
    o["id"]  = licenseId.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : licenseId;
    o["iat"] = static_cast<double>(iat);
    o["exp"] = static_cast<double>(exp);
    if (!features.isEmpty()) o["feat"] = QJsonArray::fromStringList(features);
    return QJsonDocument(o).toJson(QJsonDocument::Compact).toBase64(QByteArray::Base64UrlEncoding);
}

QString readFile(const QString& p)
{
    QFile f(p);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
    const QString s = QString::fromUtf8(f.readAll());
    f.close();
    return s.trimmed();
}
void writeFile(const QString& p, const QString& s)
{
    QDir().mkpath(QFileInfo(p).absolutePath());
    QFile f(p);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        f.write(s.toUtf8());
        f.close();
    }
}
} // namespace

LicenseManager::LicenseManager(QString storeDir, Clock clock, QObject* parent)
    : QObject(parent), storeDir_(std::move(storeDir)),
      clock_(clock ? std::move(clock) : Clock([]{ return QDateTime::currentSecsSinceEpoch(); }))
{}

QString LicenseManager::mintToken(lic::Edition edition, const QString& issuedTo,
                                  qint64 issuedAtEpoch, qint64 expiresAtEpoch,
                                  const QString& licenseId)
{
    // Self-signed HMAC ("hs256"). Accepted only for the Trial (see parseToken).
    const QByteArray payload = licensePayload("hs256", edition, issuedTo, {},
                                              issuedAtEpoch, expiresAtEpoch, licenseId);
    return QString::fromLatin1(payload + "." + sig::sign(payload));
}

QString LicenseManager::mintVendorLicense(lic::Edition edition, const QString& issuedTo,
                                          const QStringList& features,
                                          qint64 issuedAtEpoch, qint64 expiresAtEpoch,
                                          const QString& licenseId)
{
    // Vendor-signed Ed25519 ("ed25519"), domain-tagged. Grants any edition; unforgeable without the
    // private key. Returns empty when the private key is absent (release build) — cannot mint.
    const QByteArray payload = licensePayload("ed25519", edition, issuedTo, features,
                                              issuedAtEpoch, expiresAtEpoch, licenseId);
    const QByteArray signature = sig::signDetached(kLicenseDomain + payload);
    if (signature.isEmpty()) return {};
    return QString::fromLatin1(payload + "." + signature);
}

bool LicenseManager::parseToken(const QString& rawToken, lic::Status& out, QString& why) const
{
    // Accept an optional human-friendly "OCCLIC-" prefix on the activation key (the generator adds it).
    QString token = rawToken.trimmed();
    if (token.startsWith(QLatin1String("OCCLIC-"), Qt::CaseInsensitive)) token = token.mid(7);

    const int dot = token.lastIndexOf('.');
    if (dot <= 0) { why = QStringLiteral("malformed token"); return false; }
    const QByteArray payload   = token.left(dot).toLatin1();
    const QByteArray signature = token.mid(dot + 1).toLatin1();

    // Parse the (still-UNtrusted) payload first so we know which signature scheme to verify against.
    QJsonParseError perr{};
    const QJsonDocument doc = QJsonDocument::fromJson(
        QByteArray::fromBase64(payload, QByteArray::Base64UrlEncoding), &perr);
    if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
        why = QStringLiteral("unparseable payload"); return false;
    }
    const QJsonObject o = doc.object();
    const QString      alg = o.value("alg").toString(QStringLiteral("hs256"));  // v1 tokens: no alg → hs256
    const lic::Edition ed  = static_cast<lic::Edition>(o.value("ed").toInt());

    // Verify by scheme. Ed25519 (vendor) is asymmetric and may grant ANY edition. HMAC (self-signed)
    // is LOCAL-TRUST and is accepted ONLY for the Trial — a hs256 token claiming a paid edition is a
    // forgery, so paid editions require a vendor Ed25519 signature the app cannot mint.
    if (alg == QLatin1String("ed25519")) {
        if (!sig::verifyDetached(kLicenseDomain + payload, signature)) {
            why = QStringLiteral("signature mismatch"); return false;
        }
    } else {
        if (!sig::verify(payload, signature)) { why = QStringLiteral("signature mismatch"); return false; }
        if (ed != lic::Edition::Trial) {
            why = QStringLiteral("unsigned paid license (requires a vendor signature)");
            return false;
        }
    }

    out.edition        = ed;
    out.issuedTo       = o.value("to").toString();
    out.licenseId      = o.value("id").toString();
    out.issuedAtEpoch  = static_cast<qint64>(o.value("iat").toDouble());
    out.expiresAtEpoch = static_cast<qint64>(o.value("exp").toDouble());
    out.features.clear();
    const QJsonArray feats = o.value("feat").toArray();
    for (const QJsonValue& v : feats) out.features << v.toString();
    return true;
}

lic::Status LicenseManager::deriveState(lic::Status s) const
{
    const qint64 t = now();
    auto paidState = [](lic::Edition e) {
        return e == lic::Edition::Business ? lic::State::Business
             : e == lic::Edition::Personal ? lic::State::Personal
                                           : lic::State::Trial;
    };
    if (s.expiresAtEpoch == 0) {                 // perpetual (paid)
        s.state = paidState(s.edition);
        s.valid = true;
        s.daysRemaining = 0;
        s.detail = QStringLiteral("perpetual license");
        return s;
    }
    s.daysRemaining = (s.expiresAtEpoch - t) / kDay;
    if (t <= s.expiresAtEpoch) {
        s.state = paidState(s.edition);
        s.valid = true;
        s.inGrace = false;
    } else if (t <= s.expiresAtEpoch + qint64(kGraceDays) * kDay) {
        s.state = paidState(s.edition);          // still usable during grace
        s.valid = true;
        s.inGrace = true;
        s.detail = QStringLiteral("in grace period — renew to continue");
    } else {
        s.state = lic::State::Expired;
        s.valid = false;
        s.detail = QStringLiteral("license expired");
    }
    return s;
}

void LicenseManager::writeCache(const lic::Status& s) const
{
    QJsonObject o;
    o["state"]  = lic::stateName(s.state);
    o["ed"]     = static_cast<int>(s.edition);
    o["exp"]    = static_cast<double>(s.expiresAtEpoch);
    o["id"]     = s.licenseId;
    const QByteArray payload = QJsonDocument(o).toJson(QJsonDocument::Compact)
                                   .toBase64(QByteArray::Base64UrlEncoding);
    writeFile(cachePath(), QString::fromLatin1(payload + "." + sig::sign(payload)));
}

bool LicenseManager::readCache(lic::Status& /*out*/) const
{
    const QString token = readFile(cachePath());
    if (token.isEmpty()) return false;
    const int dot = token.lastIndexOf('.');
    if (dot <= 0) return false;
    const QByteArray payload = token.left(dot).toLatin1();
    const QByteArray signature = token.mid(dot + 1).toLatin1();
    return sig::verify(payload, signature);   // false => corrupt/tampered cache
}

void LicenseManager::issueTrial()
{
    const qint64 t = now();
    const QString token = mintToken(lic::Edition::Trial, QStringLiteral("Trial User"),
                                    t, t + qint64(kTrialDays) * kDay);
    writeFile(keyPath(), token);
    prodlog::info("license", "issued a new trial license");
}

void LicenseManager::setStatus(const lic::Status& s)
{
    status_ = s;
    emit statusChanged();
}

void LicenseManager::initialize()
{
    if (readFile(keyPath()).isEmpty())
        issueTrial();                              // first run

    lic::Status parsed;
    QString why;
    if (!parseToken(readFile(keyPath()), parsed, why)) {
        lic::Status bad;
        bad.state  = lic::State::Invalid;
        bad.valid  = false;
        bad.detail = why;
        prodlog::warning("license", "license invalid: " + why);
        setStatus(bad);
        return;
    }

    // Cache is advisory; the token is the source of truth. A corrupt/tampered cache is DETECTED
    // and never trusted — we log it and re-derive from the token.
    lic::Status ignored;
    if (QFile::exists(cachePath()) && !readCache(ignored))
        prodlog::warning("license", "license cache tampered/corrupt — re-validating from token");

    lic::Status s = deriveState(parsed);
    writeCache(s);
    prodlog::info("license", QStringLiteral("license=%1 edition=%2 grace=%3")
        .arg(lic::stateName(s.state), lic::editionName(s.edition), s.inGrace ? "yes" : "no"));
    setStatus(s);
}

bool LicenseManager::activate(const QString& token)
{
    lic::Status parsed;
    QString why;
    if (!parseToken(token.trimmed(), parsed, why)) {
        prodlog::warning("license", "activation rejected: " + why);
        return false;
    }
    writeFile(keyPath(), token.trimmed());
    QFile::remove(cachePath());
    initialize();
    return status_.valid;
}
