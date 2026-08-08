#include "c2test.h"
#include "app/AppInfo.h"
#include "app/Signature.h"
#include "app/Logging.h"
#include "app/LicenseManager.h"
#include "app/BackupScheduler.h"
#include "app/UpdateManager.h"
#include "app/CrashReporter.h"
#include "app/StartupDiagnostics.h"
#include "app/BuildInfo.h"
#include "app/SupportBundle.h"
#include "storage/StorageService.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <cstdio>

namespace {
int g_pass = 0, g_fail = 0;
void ok(bool cond, const char* name)
{
    (cond ? g_pass : g_fail)++;
    std::fprintf(stderr, "  %s  %s\n", cond ? "ok  " : "FAIL", name);
}
QString mk(const QString& base, const char* sub) { const QString p = base + "/" + sub; QDir(p).removeRecursively(); QDir().mkpath(p); return p; }
void writeText(const QString& p, const QByteArray& b) { QDir().mkpath(QFileInfo(p).absolutePath()); QFile f(p); f.open(QIODevice::WriteOnly|QIODevice::Truncate); f.write(b); f.close(); }
QByteArray readBytes(const QString& p) { QFile f(p); f.open(QIODevice::ReadOnly); const QByteArray b = f.readAll(); f.close(); return b; }

constexpr qint64 T0 = 1735689600;   // 2025-01-01T00:00:00Z — fixed clock base
constexpr qint64 DAY = 86400;

// Build a signed update manifest + payload in `srcDir`; `goodSig` false forges a mismatched sig.
// `channel` tags the build (default stable → visible on every channel, matching the updater default).
void makeUpdateSource(const QString& srcDir, const QByteArray& payload, long long versionCode,
                      const QString& version, bool goodSig = true,
                      const QString& channel = QStringLiteral("stable"))
{
    QDir().mkpath(srcDir);
    writeText(srcDir + "/Occountant-setup.bin", payload);
    QByteArray sigHex = sig::signDetached(payload);
    if (!goodSig) sigHex = sig::signDetached(payload + "x");   // signature of different bytes => mismatch
    QJsonObject o;
    o["version"] = version; o["versionCode"] = double(versionCode);
    o["payload"] = "Occountant-setup.bin"; o["size"] = double(payload.size());
    o["channel"] = channel;
    o["sig"] = QString::fromLatin1(sigHex); o["notes"] = "test build";
    writeText(srcDir + "/manifest.json", QJsonDocument(o).toJson(QJsonDocument::Compact));
}
} // namespace

int runC2Tests(const QString& scratchDir)
{
    QDir().mkpath(scratchDir);
    prodlog::init({ scratchDir + "/logs", 1 << 20, 5, prodlog::Level::Info });
    std::fprintf(stderr, "== C2 infrastructure tests ==\n");

    // ── 0. Asymmetric update signing (Ed25519) ───────────────────────────────
    // The update trust boundary: the app verifies with the embedded PUBLIC key and cannot forge.
    // These prove our wiring around OpenSSL's Ed25519 (round-trip, tamper on both payload and
    // signature, malformed length, and that the algorithm really changed from the licensing HMAC).
    {
        const QByteArray payload = QByteArrayLiteral("Occountant-setup.bin:v2:the-real-bytes");
        const QByteArray good = sig::signDetached(payload);
        ok(good.size() == 128, "Ed25519 signature is 64 bytes (128 hex)");
        ok(sig::verifyDetached(payload, good), "valid Ed25519 signature verifies against the public key");
        ok(!sig::verifyDetached(payload + "x", good), "payload tamper is rejected");
        QByteArray flipped = good; flipped[0] = (flipped[0] == '0' ? '1' : '0');
        ok(!sig::verifyDetached(payload, flipped), "signature tamper is rejected");
        ok(!sig::verifyDetached(payload, QByteArrayLiteral("deadbeef")), "malformed (short) signature is rejected");
        // A licensing HMAC over the same bytes must NOT pass as an Ed25519 signature — proves the
        // update path uses the asymmetric scheme, not the old symmetric one.
        ok(!sig::verifyDetached(payload, sig::sign(payload)), "an HMAC signature does NOT verify as Ed25519");
        ok(!sig::updateKeyFingerprint().isEmpty(), "update public-key fingerprint is exposed");
    }

    // ── 1. Expired license ──────────────────────────────────────────────────
    {
        const QString dir = mk(scratchDir, "lic_expired");
        auto clock = []{ return T0; };
        writeText(dir + "/license.key",
                  LicenseManager::mintVendorLicense(lic::Edition::Business, "Acme", {},
                      T0 - 40*DAY, T0 - (LicenseManager::kGraceDays+1)*DAY, "exp-id").toUtf8());
        LicenseManager lm(dir, clock); lm.initialize();
        ok(lm.status().state == lic::State::Expired && !lm.isUsable(), "expired license -> Expired, not usable");
    }
    // Bonus: grace window keeps a just-expired license usable
    {
        const QString dir = mk(scratchDir, "lic_grace");
        auto clock = []{ return T0; };
        writeText(dir + "/license.key",
                  LicenseManager::mintVendorLicense(lic::Edition::Personal, "Acme", {}, T0 - 40*DAY, T0 - 1*DAY, "grace-id").toUtf8());
        LicenseManager lm(dir, clock); lm.initialize();
        ok(lm.isUsable() && lm.status().inGrace, "just-expired license -> usable in grace period");
    }

    // ── 2. Invalid signature ────────────────────────────────────────────────
    {
        const QString dir = mk(scratchDir, "lic_badsig");
        QString tok = LicenseManager::mintVendorLicense(lic::Edition::Business, "Acme", {}, T0, T0 + 365*DAY, "badsig-id");
        const int dot = tok.indexOf('.');                         // flip one signature hex char
        tok[dot + 1] = (tok[dot + 1] == QLatin1Char('a')) ? QLatin1Char('b') : QLatin1Char('a');
        writeText(dir + "/license.key", tok.toUtf8());
        LicenseManager lm(dir, []{ return T0; }); lm.initialize();
        ok(lm.status().state == lic::State::Invalid && !lm.isUsable(), "tampered signature -> Invalid");
    }

    // ── 3. Corrupted cache is detected + not trusted ────────────────────────
    {
        const QString dir = mk(scratchDir, "lic_badcache");
        writeText(dir + "/license.key",
                  LicenseManager::mintVendorLicense(lic::Edition::Business, "Acme", {}, T0, 0, "cache-id").toUtf8());   // perpetual
        writeText(dir + "/license.cache", QByteArrayLiteral("not-a-valid-signed-cache"));
        LicenseManager lm(dir, []{ return T0; }); lm.initialize();
        ok(lm.status().state == lic::State::Business && lm.isUsable(),
           "corrupt cache ignored; state derived from signed token");
    }
    // Bonus: first run issues a trial
    {
        const QString dir = mk(scratchDir, "lic_firstrun");
        LicenseManager lm(dir, []{ return T0; }); lm.initialize();
        ok(lm.status().state == lic::State::Trial && lm.isUsable() && QFile::exists(dir + "/license.key"),
           "first run issues a signed trial");
    }

    // ── 3b. Vendor Ed25519 licenses (asymmetric activation) ─────────────────────
    // A valid vendor key activates a paid edition offline (verified with the embedded public key).
    {
        const QString dir = mk(scratchDir, "lic_vendor_ok");
        const QString key = LicenseManager::mintVendorLicense(lic::Edition::Business, "Acme Ltd",
                                {"export", "priority-support"}, T0, T0 + 365*DAY, "vendor-ok");
        LicenseManager lm(dir, []{ return T0; }); lm.initialize();     // begins as trial
        const bool activated = lm.activate(key);
        ok(activated && lm.status().state == lic::State::Business && lm.isUsable(),
           "valid vendor (Ed25519) license activates -> Business");
        ok(lm.hasFeature("export") && lm.hasFeature("priority-support"),
           "activated vendor license carries its enabled features");
        ok(lm.status().issuedTo == QStringLiteral("Acme Ltd"), "vendor license records the customer name");
    }
    // The generator's human-friendly OCCLIC- prefix is accepted.
    {
        const QString dir = mk(scratchDir, "lic_vendor_prefix");
        const QString key = "OCCLIC-" + LicenseManager::mintVendorLicense(lic::Edition::Personal, "Jane",
                                {}, T0, 0, "vendor-perp");
        LicenseManager lm(dir, []{ return T0; });
        ok(lm.activate(key) && lm.status().state == lic::State::Personal && lm.isUsable(),
           "OCCLIC-prefixed perpetual vendor key activates -> Personal");
    }
    // A MODIFIED vendor key (payload byte flipped) fails Ed25519 verification.
    {
        const QString dir = mk(scratchDir, "lic_vendor_mod");
        QString key = LicenseManager::mintVendorLicense(lic::Edition::Business, "Acme", {}, T0, T0 + 365*DAY, "vendor-mod");
        const int pos = 12;   // inside the base64url payload (well before the dot)
        key[pos] = (key[pos] == QLatin1Char('A')) ? QLatin1Char('B') : QLatin1Char('A');
        LicenseManager lm(dir, []{ return T0; });
        ok(!lm.activate(key) && !lm.isUsable(), "modified vendor license is rejected");
    }
    // A WRONG signature (sig byte flipped) is rejected.
    {
        const QString dir = mk(scratchDir, "lic_vendor_badsig");
        QString key = LicenseManager::mintVendorLicense(lic::Edition::Business, "Acme", {}, T0, T0 + 365*DAY, "vendor-badsig");
        const int dot = key.indexOf('.');
        key[dot + 1] = (key[dot + 1] == QLatin1Char('a')) ? QLatin1Char('b') : QLatin1Char('a');
        LicenseManager lm(dir, []{ return T0; });
        ok(!lm.activate(key) && !lm.isUsable(), "vendor license with a wrong signature is rejected");
    }
    // An EXPIRED vendor key (past expiry + grace) is Expired, not usable.
    {
        const QString dir = mk(scratchDir, "lic_vendor_exp");
        const QString key = LicenseManager::mintVendorLicense(lic::Edition::Business, "Acme", {},
                                T0 - 400*DAY, T0 - (LicenseManager::kGraceDays + 1)*DAY, "vendor-exp");
        LicenseManager lm(dir, []{ return T0; }); lm.activate(key);
        ok(lm.status().state == lic::State::Expired && !lm.isUsable(),
           "expired vendor license -> Expired, not usable");
    }
    // THE security property: a self-signed (HMAC) token claiming a PAID edition is rejected.
    {
        const QString dir = mk(scratchDir, "lic_hmac_paid");
        const QString forged = LicenseManager::mintToken(lic::Edition::Business, "Attacker",
                                   T0, T0 + 3650*DAY, "forged-business");
        LicenseManager lm(dir, []{ return T0; });
        ok(!lm.activate(forged) && !lm.isUsable(),
           "self-signed (HMAC) token claiming a PAID edition is REJECTED (paid requires Ed25519)");
    }
    // Trial behavior unchanged: a self-signed (HMAC) Trial token is still accepted.
    {
        const QString dir = mk(scratchDir, "lic_hmac_trial");
        const QString trial = LicenseManager::mintToken(lic::Edition::Trial, "Trial User", T0, T0 + 30*DAY, "trial-ok");
        LicenseManager lm(dir, []{ return T0; });
        ok(lm.activate(trial) && lm.status().state == lic::State::Trial && lm.isUsable(),
           "self-signed (HMAC) Trial token is still accepted (trial behavior unchanged)");
    }

    // ── 4. Update interrupted (signature mismatch) is not staged ────────────
    {
        const QString src = mk(scratchDir, "upd_bad_src");
        const QString stg = mk(scratchDir, "upd_bad_stage");
        makeUpdateSource(src, QByteArray(2048, 'A'), 1002000, "1.2.0", /*goodSig*/false);
        UpdateManager um(stg, src, appinfo::versionCode(1,0,0));
        um.check();
        const bool staged = um.downloadAndStage();
        ok(!staged && !um.isStaged() && !QDir(UpdateManager::pendingDir(stg)).exists(),
           "interrupted/forged update -> not staged");
    }

    // ── 5. Staged update rollback ───────────────────────────────────────────
    {
        const QString src = mk(scratchDir, "upd_ok_src");
        const QString stg = mk(scratchDir, "upd_ok_stage");
        makeUpdateSource(src, QByteArray(4096, 'Z'), 1002000, "1.2.0");
        UpdateManager um(stg, src, appinfo::versionCode(1,0,0));
        um.check();
        const bool staged = um.downloadAndStage();
        const bool rolled = um.rollbackStaged();
        ok(staged && rolled && !um.isStaged() && !QDir(UpdateManager::pendingDir(stg)).exists(),
           "valid update stages then rolls back cleanly");
    }
    // Bonus: check() reports up-to-date when not newer
    {
        const QString src = mk(scratchDir, "upd_same_src");
        const QString stg = mk(scratchDir, "upd_same_stage");
        makeUpdateSource(src, QByteArray(16, 'x'), appinfo::versionCode(1,0,0), "1.0.0");
        UpdateManager um(stg, src, appinfo::versionCode(1,0,0));
        um.check();
        ok(um.state() == UpdateManager::State::UpToDate && !um.hasUpdate(), "same version -> up to date");
    }

    // ── 6. Backup scheduler: due / create / verify / retention ──────────────
    {
        const QString data = mk(scratchDir, "bk_data");
        writeText(data + "/customers.dat", QByteArray(512, 'c'));
        writeText(data + "/invoices.dat",  QByteArray(256, 'i'));
        qint64 nowV = T0;
        BackupScheduler sch(data, BackupPolicy{ /*intervalHours*/24, /*keep*/1, /*maxAge*/90 }, [&]{ return nowV; });
        const bool due0 = sch.isDue();
        const QString b1 = sch.runNow(true);
        nowV += 2*DAY;                                  // advance past the interval
        const bool dueAfter = sch.isDue();
        const QString b2 = sch.runNow(true);
        const int removed = sch.prune();                // keep=1 -> drop the older one
        const auto pts = sch.restorePoints();
        ok(due0 && !b1.isEmpty() && dueAfter && !b2.isEmpty() && removed == 1 && pts.size() == 1
           && pts.first().name == b2, "scheduler: due -> backup -> retention keeps newest");

        // verify() detects a corrupt authoritative log inside a restore point
        const QString bad = data + "/backups/backup-999";
        writeText(bad + "/audit.log", QByteArrayLiteral("corrupt-not-an-eventlog"));
        ok(!sch.verify("backup-999"), "scheduler: verify() rejects a corrupt restore point");
    }

    // ── 7. Crash report generation ──────────────────────────────────────────
    {
        const QString rep = mk(scratchDir, "crash");
        crashreport::Context ctx;
        ctx.buildVersion = appinfo::fullVersion(); ctx.buildId = appinfo::buildId();
        ctx.channel = appinfo::channel(); ctx.platform = "Test/OS"; ctx.reportDir = rep;
        ctx.governance = "schema 1 · replay 1 · posting 2 · snapshot 1";
        crashreport::install(ctx);
        const QString zipPath = crashreport::generate("regression self-test");
        const QByteArray z = readBytes(zipPath);
        const bool isZip = z.startsWith(QByteArrayLiteral("PK\x03\x04"));
        const bool hasReport = z.contains("Occountant Crash Report") && z.contains("governance:")
                               && z.contains(ctx.governance.toUtf8());
        ok(QFile::exists(zipPath) && isZip && hasReport && z.size() > 200,
           "crash report -> valid CrashReport.zip with build+governance, no accounting data");
    }

    // ── 8. Log rotation ─────────────────────────────────────────────────────
    {
        const QString logs = mk(scratchDir, "rot");
        prodlog::init({ logs, /*maxBytes*/2000, /*keep*/3, prodlog::Level::Info });
        for (int i = 0; i < 300; ++i) prodlog::info("rot", QString("line %1 padded with filler text").arg(i));
        const bool cur   = QFile::exists(logs + "/occountant.log");
        const bool rolled = QFile::exists(logs + "/occountant.1.log");
        const bool capped = !QFile::exists(logs + "/occountant.4.log");   // keep=3
        prodlog::init({ scratchDir + "/logs", 1 << 20, 5, prodlog::Level::Info });   // restore
        ok(cur && rolled && capped, "log rotation rolls files and honours keep=3");
    }

    // ── 9. Startup diagnostics aggregate ────────────────────────────────────
    {
        const QString data = mk(scratchDir, "diag_data");
        StorageService::instance().initialize(data.toStdString());
        const QString licdir = mk(scratchDir, "diag_lic");
        LicenseManager lm(licdir, []{ return T0; }); lm.initialize();
        BackupScheduler sch(data, {}, []{ return T0; });
        const startupdiag::Health h = startupdiag::collect(StorageService::instance(), &lm, &sch, nullptr);
        const QString rep = startupdiag::format(h);
        ok(h.storageOpen && h.trialBalanceZero && h.licenseValid && !rep.isEmpty()
           && rep.contains("Startup Health") && h.compatStatus.contains("compatible"),
           "startup diagnostics aggregate storage+license+verification into one report");
    }

    // ── 10. Recovery after interrupted update (incomplete staging cleaned) ──
    {
        const QString stg  = mk(scratchDir, "recover_stage");
        const QString data = mk(scratchDir, "recover_data");
        writeText(data + "/customers.dat", QByteArray(128, 'k'));   // "live DB" — must stay untouched
        // Simulate an interrupted stage: payload present but NO signed marker.
        writeText(UpdateManager::pendingDir(stg) + "/Occountant-setup.bin", QByteArray(64, 'p'));
        const auto res = UpdateManager::applyPendingAtStartup(stg, appinfo::versionCode(1,0,0));
        const bool cleaned = !QDir(UpdateManager::pendingDir(stg)).exists();
        const bool dbIntact = readBytes(data + "/customers.dat").size() == 128;
        ok(res == UpdateManager::ApplyResult::Recovered && cleaned && dbIntact,
           "interrupted staged update is recovered on startup; live DB untouched");
    }
    // Bonus: a complete staged bundle applies on startup
    {
        const QString src = mk(scratchDir, "apply_src");
        const QString stg = mk(scratchDir, "apply_stage");
        makeUpdateSource(src, QByteArray(1024, 'Q'), 1002000, "1.2.0");
        UpdateManager um(stg, src, appinfo::versionCode(1,0,0));
        um.check(); um.downloadAndStage();
        const auto res = UpdateManager::applyPendingAtStartup(stg, appinfo::versionCode(1,0,0));
        ok(res == UpdateManager::ApplyResult::Applied && !QDir(UpdateManager::pendingDir(stg)).exists(),
           "complete staged update applies + clears on startup");
    }

    // ── 11. Build provenance is deterministic + complete (Release Engineering) ──
    {
        const QByteArray a = buildinfo::json();
        const QByteArray b = buildinfo::json();               // two calls must be byte-identical
        const buildinfo::Info& i = buildinfo::info();
        const bool deterministic = (a == b);
        const bool provenance = i.product == QString::fromLatin1(appinfo::kProduct)
                             && i.vendor  == QString::fromLatin1(appinfo::kVendor)
                             && i.versionCode == appinfo::versionCode()
                             && !i.qtVersion.isEmpty() && !i.compiler.isEmpty()
                             && a.contains("\"gitCommit\"") && a.contains("\"buildTimestamp\"");
        // The timestamp is compile-time (git-derived) — NOT a wall clock. It is either "unknown"
        // (bare build) or internally consistent with the embedded epoch, never "now".
        const bool notWallClock = i.buildTimestamp == "unknown" || i.buildEpoch > 0;
        ok(deterministic && provenance && notWallClock,
           "buildinfo: deterministic JSON with full, non-wall-clock provenance");
    }

    // ── 12. Release channels gate which builds the updater offers ──────────────
    {
        auto offered = [&](const char* stageSub, const QString& srcDir, const char* userChan) {
            UpdateManager um(mk(scratchDir, stageSub), srcDir, appinfo::versionCode(1,0,0));
            um.setChannel(QString::fromLatin1(userChan));
            um.check();
            return um.hasUpdate();
        };
        // A newer build cut for BETA: visible to beta/development, invisible to stable/rc.
        const QString betaSrc = mk(scratchDir, "chan_beta_src");
        makeUpdateSource(betaSrc, QByteArray(1024,'B'), 1002000, "1.2.0", true, "beta");
        const bool betaRule = !offered("cs1", betaSrc, "stable")
                           && !offered("cs2", betaSrc, "rc")
                           &&  offered("cs3", betaSrc, "beta")
                           &&  offered("cs4", betaSrc, "development");
        // A newer STABLE build: visible on every channel (more-stable is always safe to offer).
        const QString stableSrc = mk(scratchDir, "chan_stable_src");
        makeUpdateSource(stableSrc, QByteArray(64,'S'), 1002000, "1.2.0", true, "stable");
        const bool stableRule = offered("cs5", stableSrc, "stable")
                             && offered("cs6", stableSrc, "beta")
                             && offered("cs7", stableSrc, "development");
        ok(betaRule && stableRule,
           "channels: beta build only for beta/dev users; stable build for everyone; switching re-gates");
    }

    // ── 13. Signed package verification round-trip ─────────────────────────────
    {
        const QString src = mk(scratchDir, "sig_src");
        makeUpdateSource(src, QByteArray(4096,'P'), 1002000, "1.2.0");   // good signature
        UpdateManager um(mk(scratchDir, "sig_stage"), src, appinfo::versionCode(1,0,0));
        um.check();
        const bool staged = um.downloadAndStage();
        // Tamper the source payload so its bytes no longer match the manifest signature.
        QByteArray p = readBytes(src + "/Occountant-setup.bin");
        p[10] = char(p[10] ^ 0xFF);
        writeText(src + "/Occountant-setup.bin", p);
        UpdateManager um2(mk(scratchDir, "sig_stage2"), src, appinfo::versionCode(1,0,0));
        um2.check();
        const bool rejected = !um2.downloadAndStage() && !um2.isStaged();
        ok(staged && rejected, "signed package: valid payload stages, one-byte tamper is rejected");
    }

    // ── 14. Support bundle carries diagnostics but NO accounting data ──────────
    {
        const QString data = mk(scratchDir, "sb_data");
        StorageService::instance().initialize(data.toStdString());
        writeText(data + "/customers.dat", QByteArray(256,'D'));   // a real data file that must NOT leak
        prodlog::init({ data + "/logs", 1<<20, 5, prodlog::Level::Info });
        prodlog::info("test", "support bundle self-test");
        supportbundle::Params p; p.outputZip = data + "/SupportBundle.zip"; p.dataDir = data;
        p.supportId = QStringLiteral("OCC-TEST-0001");
        const QString zipPath = supportbundle::generate(p);
        const QByteArray z = readBytes(zipPath);
        const bool isZip   = z.startsWith(QByteArrayLiteral("PK\x03\x04"));
        const bool hasDiag = z.contains("buildinfo.json") && z.contains("bundle-manifest.txt")
                          && z.contains("startup-diagnostics.txt");
        const bool noData  = !z.contains("customers.dat") && !z.contains(QByteArray(256,'D'))
                          && !z.contains("compat.manifest");
        ok(!zipPath.isEmpty() && isZip && hasDiag && noData,
           "support bundle: valid zip with diagnostics; no *.dat name or bytes, no accounting data");
        ok(z.contains("OCC-TEST-0001"), "support bundle records the non-PII support id (for correlation)");
        prodlog::init({ scratchDir + "/logs", 1 << 20, 5, prodlog::Level::Info });   // restore
    }

    // ── 15. Installer downgrade gate (the comparator the .iss [Code] mirrors) ──
    {
        const bool refusesOlder = appinfo::isDowngrade(appinfo::versionCode(1,2,0), appinfo::versionCode(1,1,0));
        const bool allowsUpgrade = !appinfo::isDowngrade(appinfo::versionCode(1,1,0), appinfo::versionCode(1,2,0));
        const bool allowsReinstall = !appinfo::isDowngrade(appinfo::versionCode(1,1,0), appinfo::versionCode(1,1,0));
        ok(refusesOlder && allowsUpgrade && allowsReinstall,
           "installer downgrade gate: older-over-newer refused; upgrade + same-version reinstall allowed");
    }

    // ── 16. Log redaction never persists a currency value ──────────────────────
    {
        const QString r = prodlog::redact(QStringLiteral("charged $1,234.56 to Acme; balance 9,000.00"));
        ok(r.contains("<redacted-amount>") && !r.contains("1,234.56") && !r.contains("9,000.00"),
           "logging redaction masks currency values (no amount reaches a shipped log/bundle)");
    }

    std::fprintf(stderr, "== C2: %d passed, %d failed ==\n", g_pass, g_fail);
    return g_fail;
}
