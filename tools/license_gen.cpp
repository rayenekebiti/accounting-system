// license_gen — Occountant vendor license generator (DEVELOPER-ONLY).
//
// Signs a customer license with the vendor Ed25519 PRIVATE key and prints a human-friendly activation
// key the customer pastes into Occountant (Settings → About). It is a separate console tool, built
// ONLY when -DACCT_DEV_SIGNING=ON (the vendor machine); it is never part of the shipped installer and
// the release build (ACCT_DEV_SIGNING=OFF) contains no private key. NEVER distribute this tool or the
// private key. See docs/license-generation.md.
//
// Usage:
//   license_gen --name "Acme Ltd" --plan business --features export,priority-support \
//               --issued 2026-08-01 --expires 2027-08-01
//   license_gen --name "Jane Doe" --plan personal --expires perpetual

#include "app/LicenseManager.h"
#include "app/LicenseTypes.h"
#include "app/Signature.h"

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDate>
#include <QDateTime>
#include <QTimeZone>
#include <QTextStream>

static qint64 epochFromDate(const QString& s, bool endOfDay = false)
{
    const QDate d = QDate::fromString(s.trimmed(), Qt::ISODate);
    if (!d.isValid()) return -1;
    const QDateTime dt = endOfDay ? d.endOfDay(QTimeZone::utc()) : d.startOfDay(QTimeZone::utc());
    return dt.toSecsSinceEpoch();
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("occountant-license-gen"));
    QTextStream out(stdout), err(stderr);

    QCommandLineParser p;
    p.setApplicationDescription(
        "Occountant vendor license generator (developer-only). Signs a customer license with the\n"
        "vendor Ed25519 private key. Never distribute this tool or the private key to customers.");
    p.addHelpOption();
    const QCommandLineOption oName   (QStringLiteral("name"),     QStringLiteral("Customer / company name (required)."), QStringLiteral("name"));
    const QCommandLineOption oPlan   (QStringLiteral("plan"),     QStringLiteral("personal | business | trial (default business)."), QStringLiteral("plan"), QStringLiteral("business"));
    const QCommandLineOption oFeat   (QStringLiteral("features"), QStringLiteral("Comma-separated enabled feature keys."), QStringLiteral("list"), QString());
    const QCommandLineOption oIssued (QStringLiteral("issued"),   QStringLiteral("Issue date YYYY-MM-DD (default: today)."), QStringLiteral("date"), QString());
    const QCommandLineOption oExpires(QStringLiteral("expires"),  QStringLiteral("Expiry YYYY-MM-DD, or 'perpetual' (default)."), QStringLiteral("date"), QStringLiteral("perpetual"));
    const QCommandLineOption oId     (QStringLiteral("id"),       QStringLiteral("License id (default: random UUID)."), QStringLiteral("id"), QString());
    p.addOptions({oName, oPlan, oFeat, oIssued, oExpires, oId});
    p.process(app);

    if (!p.isSet(oName) || p.value(oName).trimmed().isEmpty()) {
        err << "error: --name is required\n"; return 2;
    }
    const QString name = p.value(oName).trimmed();

    lic::Edition ed;
    const QString plan = p.value(oPlan).trimmed().toLower();
    if      (plan == QLatin1String("personal")) ed = lic::Edition::Personal;
    else if (plan == QLatin1String("business")) ed = lic::Edition::Business;
    else if (plan == QLatin1String("trial"))    ed = lic::Edition::Trial;
    else { err << "error: --plan must be personal | business | trial\n"; return 2; }

    QStringList features;
    if (p.isSet(oFeat))
        for (const QString& f : p.value(oFeat).split(QLatin1Char(','), Qt::SkipEmptyParts))
            features << f.trimmed();

    const qint64 iat = p.isSet(oIssued) ? epochFromDate(p.value(oIssued))
                                        : QDateTime::currentSecsSinceEpoch();
    if (iat < 0) { err << "error: --issued must be YYYY-MM-DD\n"; return 2; }

    qint64 exp = 0;   // perpetual
    const QString expS = p.value(oExpires).trimmed().toLower();
    if (!expS.isEmpty() && expS != QLatin1String("perpetual")) {
        exp = epochFromDate(expS, /*endOfDay*/ true);
        if (exp < 0) { err << "error: --expires must be YYYY-MM-DD or 'perpetual'\n"; return 2; }
    }

    const QString token = LicenseManager::mintVendorLicense(ed, name, features, iat, exp, p.value(oId));
    if (token.isEmpty()) {
        err << "error: no signing key available. Build the generator with -DACCT_DEV_SIGNING=ON on the\n"
               "       vendor machine (the release build intentionally has no private key).\n";
        return 1;
    }

    // Diagnostics to stderr; the activation key alone to stdout (easy to pipe/copy).
    err << "# Occountant license — signing key " << sig::updateKeyFingerprint() << "\n"
        << "# name="   << name
        << "  plan="   << plan
        << "  features=" << (features.isEmpty() ? QStringLiteral("(none)") : features.join(QLatin1Char('+')))
        << "  expires=" << (exp == 0 ? QStringLiteral("perpetual")
                                     : QDateTime::fromSecsSinceEpoch(exp, QTimeZone::utc()).date().toString(Qt::ISODate))
        << "\n# Give the customer the line below (Settings → About → paste license key):\n";
    out << "OCCLIC-" << token << "\n";
    return 0;
}
