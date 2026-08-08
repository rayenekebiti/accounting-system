#include "early_access.h"

#include "EarlyAccessViewModel.h"
#include "SupportCenterViewModel.h"
#include "CustomerEditorViewModel.h"
#include "app/SupportId.h"
#include "app/AppInfo.h"
#include "storage/StorageService.h"
#include "storage/AuditJournal.h"

#include <QSettings>
#include <QTranslator>
#include <QFile>
#include <QFileInfo>
#include <cstdio>

namespace {
int g_pass = 0, g_fail = 0;
void ok(bool c, const char* n) { (c ? g_pass : g_fail)++; std::fprintf(stderr, "    %s  %s\n", c ? "ok  " : "FAIL", n); }
}

int runEarlyAccessTests(const QString& dataDir)
{
    if (!StorageService::instance().isInitialized()) {
        std::fprintf(stderr, "early-access: storage not initialised\n");
        return 1;
    }
    // Isolate QSettings to this run's data dir — never touch the machine registry, and start clean.
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, dataDir + "/config");
    { QSettings s; s.remove(QStringLiteral("earlyAccess")); s.sync(); }

    g_pass = g_fail = 0;
    std::fprintf(stderr, "\n===== EARLY ACCESS / SUPPORT CENTER CHECKS =====\n");

    // ── 1. Welcome-notice state machine: shows once, never repeats after acknowledgement ──
    std::fprintf(stderr, "== early access: welcome-notice state machine ==\n");
    {
        EarlyAccessViewModel ea;
        ok(ea.shouldShow(), "first launch shows the notice");
        ea.acknowledge();
        ok(!ea.shouldShow(), "Continue dismisses it (no repeat this version)");
    }
    { EarlyAccessViewModel ea2; ok(!ea2.shouldShow(), "acknowledgement persists across restart (no repeat)"); }

    // Simulate a MAJOR version update (last-acknowledged major < current) → notice returns.
    { QSettings s; s.setValue(QStringLiteral("earlyAccess/acknowledgedMajor"), appinfo::kVersionMajor - 1); s.sync(); }
    {
        EarlyAccessViewModel ea3;
        ok(ea3.shouldShow(), "a major-version update re-shows the notice");
        ea3.remindLater();
        ok(ea3.shouldShow(), "remind-me-later keeps it for next launch (no state change)");
        ea3.dontShowAgain();
        ok(!ea3.shouldShow(), "don't-show-again suppresses it");
    }
    { EarlyAccessViewModel ea4; ok(!ea4.shouldShow(), "suppression persists across restart"); }

    // ── 2. Support ID is stable + well-formed ──
    std::fprintf(stderr, "== early access: support id ==\n");
    const QString id1 = supportid::get();
    ok(id1.startsWith(QStringLiteral("OCC-")), "support id issued in OCC-XXXX-XXXX form");
    ok(supportid::get() == id1, "support id stable across reads (persisted)");

    // ── 3. Support flows author NO accounting events; replay-equivalence unchanged ──
    std::fprintf(stderr, "== early access: support flows author nothing ==\n");
    auto& aj = StorageService::instance().audit();

    // Seed a distinctive customer as a leak canary (this DOES author an event — the baseline moves).
    CustomerEditorViewModel ce;
    ce.beginNew();
    ce.setName(QStringLiteral("LEAKCANARY_Client_ZZ"));
    ce.setEmail(QStringLiteral("canary@example.com"));
    ce.commit();
    const uint64_t seqBaseline = aj.lastSeq();
    const auto repBefore = StorageService::instance().compatibilityReport(/*runValidation*/ true);
    ok(repBefore.replayValidated, "replay-equivalence holds before support flows");

    SupportCenterViewModel sc;
    const QString ticketId = sc.submitReport(QStringLiteral("Invoice"), QStringLiteral("Important"),
                                             QStringLiteral("Something looked off"),
                                             QStringLiteral("Expected X"), QStringLiteral("Steps"), true);
    ok(ticketId.startsWith(QStringLiteral("TCK-")), "problem report stored as a local ticket");
    ok(aj.lastSeq() == seqBaseline, "filing a support ticket authors NO accounting events");

    const QString bundle = sc.exportDiagnostics();
    ok(!bundle.isEmpty() && QFileInfo::exists(bundle), "diagnostics bundle produced from the VM");
    ok(aj.lastSeq() == seqBaseline, "exporting diagnostics authors NO accounting events");

    // Reward framework: mark valuable → records eligibility; still no events, no auto-discount.
    sc.markValuable(ticketId);
    ok(!sc.rewardEligibility(ticketId).isEmpty(), "marking feedback valuable records a reward eligibility note");
    ok(aj.lastSeq() == seqBaseline, "reward eligibility record authors NO accounting events");

    const auto repAfter = StorageService::instance().compatibilityReport(/*runValidation*/ true);
    ok(repAfter.replayValidated && repAfter.guaranteesSatisfied(),
       "replay-equivalence still holds after all support flows");
    ok(repAfter.trialBalanceZero, "trial balance still balanced after support flows");

    // ── 4. The diagnostics bundle carries NO accounting data ──
    std::fprintf(stderr, "== early access: bundle carries no accounting data ==\n");
    QByteArray z;
    { QFile f(bundle); if (f.open(QIODevice::ReadOnly)) { z = f.readAll(); f.close(); } }
    ok(z.startsWith(QByteArrayLiteral("PK\x03\x04")), "bundle is a valid zip");
    ok(!z.contains("LEAKCANARY_Client_ZZ"), "bundle does NOT contain customer data (leak canary absent)");
    ok(!z.contains("customers.dat") && !z.contains("audit.log"), "bundle does NOT contain accounting data-file names");
    ok(z.contains(id1.toUtf8()), "bundle records the support id (for correlation)");

    // ── 5. Translation works: the Arabic catalog loads and an Early Access string is localized ──
    std::fprintf(stderr, "== early access: translation ==\n");
    {
        QTranslator tr;
        const bool loaded = tr.load(QStringLiteral("app_ar"), QStringLiteral(":/i18n"));
        ok(loaded, "Arabic catalog (app_ar.qm) loads from resources");
        const QString t = tr.translate("EarlyAccessDialog", "Continue");
        ok(!t.isEmpty() && t != QLatin1String("Continue"), "an Early Access string is translated to Arabic");
    }

    std::fprintf(stderr, "\n===== EARLY ACCESS SUMMARY: %d passed, %d failed =====\n\n", g_pass, g_fail);
    return g_fail;
}
