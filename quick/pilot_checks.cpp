#include "pilot_checks.h"

#include "CustomerEditorViewModel.h"
#include "InvoiceEditorViewModel.h"
#include "InvoiceDraftLinesModel.h"
#include "SettingsViewModel.h"
#include "BackupViewModel.h"
#include "ExportViewModel.h"
#include "ExportService.h"
#include "OnboardingViewModel.h"
#include "DiagnosticsViewModel.h"
#include "PeriodCloseViewModel.h"

#include "storage/AuditJournal.h"

#include "storage/StorageService.h"

#include <QString>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <cstdio>

namespace {

int g_pass = 0, g_fail = 0;
void ok(bool cond, const char* name)
{
    (cond ? g_pass : g_fail)++;
    std::fprintf(stderr, "    %s  %s\n", cond ? "ok  " : "FAIL", name);
}

QString cents(long long c)
{
    return QString("%1.%2").arg(c / 100).arg(QString::number(c % 100).rightJustified(2, QLatin1Char('0')));
}

QString readAll(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return QString();
    const QString s = QString::fromUtf8(f.readAll());
    f.close();
    return s;
}

uint32_t customerIdByName(const QString& name)
{
    for (const auto& c : StorageService::instance().customers().loadAll())
        if (QString::fromUtf8(c.getName()) == name) return c.getId();
    return 0xFFFFFFFFu;
}

// Newest backup directory name under <dataDir>/backups (by modification time).
QString newestBackup(const QString& dataDir)
{
    QDir b(dataDir + "/backups");
    const auto entries = b.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Time);
    return entries.isEmpty() ? QString() : entries.first().fileName();
}

// Build a tiny business through the real editor VMs so there is something to back up / export.
uint32_t seedBusiness()
{
    CustomerEditorViewModel ce;
    ce.beginNew();
    ce.setName(QStringLiteral("Pilot Co"));
    ce.setEmail(QStringLiteral("owner@pilot.example"));
    ce.commit();

    uint32_t cust = 0;
    for (const auto& c : StorageService::instance().customers().loadAll())
        if (QString::fromUtf8(c.getName()) == QStringLiteral("Pilot Co")) cust = c.getId();

    InvoiceEditorViewModel ie;
    ie.beginNew();
    ie.setInvoiceNumber(QStringLiteral("PILOT-1"));
    ie.setCustomerId(static_cast<int>(cust));
    ie.setIssueDate(QStringLiteral("2026-03-10"));
    ie.setDueDate(QStringLiteral("2026-04-10"));
    InvoiceDraftLinesModel* lines = ie.lines();
    QMetaObject::invokeMethod(lines, "setCell", Q_ARG(int, 0),
        Q_ARG(QString, QStringLiteral("description")), Q_ARG(QString, QStringLiteral("Consulting")));
    QMetaObject::invokeMethod(lines, "setCell", Q_ARG(int, 0),
        Q_ARG(QString, QStringLiteral("qtyText")), Q_ARG(QString, QStringLiteral("1")));
    QMetaObject::invokeMethod(lines, "setCell", Q_ARG(int, 0),
        Q_ARG(QString, QStringLiteral("unitPriceText")), Q_ARG(QString, cents(10000)));
    QMetaObject::invokeMethod(lines, "setCell", Q_ARG(int, 0),
        Q_ARG(QString, QStringLiteral("taxText")), Q_ARG(QString, QStringLiteral("20")));
    ie.setStatus(1);   // POSTED
    ie.commit();
    return static_cast<uint32_t>(StorageService::instance().invoices().findIdByNumber("PILOT-1"));
}

// ── Data safety: a corrupt / history-less backup must NOT be able to overwrite live books ──────
void checkDataSafety(const QString& dataDir)
{
    std::fprintf(stderr, "== pilot: data safety (backup / restore) ==\n");
    BackupViewModel bk;

    ok(bk.backupNow(), "backupNow() creates a restore point");
    const QString good = newestBackup(dataDir);
    ok(!good.isEmpty(), "a backup directory exists");
    ok(bk.verify(good), "verify(): a fresh backup's history is intact");

    // A good backup can be staged for restore.
    ok(bk.restore(good), "restore(): a VERIFIED backup is accepted (staged)");
    ok(QDir(dataDir + "/.pending-restore").exists(), "restore staged the verified backup");
    // Undo the staging so this headless run leaves no pending restore behind.
    QDir(dataDir + "/.pending-restore").removeRecursively();
    QSettings().remove(QStringLiteral("restore/pending"));

    // Corrupt the backup's authoritative log (flip a byte inside the first committed frame).
    const QString badLog = dataDir + "/backups/" + good + "/audit.log";
    { QFile f(badLog);
      if (f.open(QIODevice::ReadWrite)) {
          f.seek(40); char b = 0; f.read(&b, 1); f.seek(40); char c = static_cast<char>(b ^ 0xFF); f.write(&c, 1); f.close(); } }

    ok(!bk.verify(good), "verify(): a corrupted backup is detected as unreadable");
    ok(!bk.restore(good), "restore(): a CORRUPT backup is REFUSED (not staged)");
    ok(!QDir(dataDir + "/.pending-restore").exists(), "corrupt restore staged nothing");

    // A history-less backup (no audit.log) must also be refused.
    ok(bk.backupNow(), "second backup created");
    const QString noHist = newestBackup(dataDir);
    QFile::remove(dataDir + "/backups/" + noHist + "/audit.log");
    ok(!bk.restore(noHist), "restore(): a backup with no history is REFUSED");
    ok(!QDir(dataDir + "/.pending-restore").exists(), "history-less restore staged nothing");

    // Live books are untouched by the refused restores.
    bool custIntact = false;
    for (const auto& c : StorageService::instance().customers().loadAll())
        if (QString::fromUtf8(c.getName()) == QStringLiteral("Pilot Co")) custIntact = true;
    ok(custIntact, "live data is untouched after the refused restores");
}

// ── Deliverability: invoices + the core reports export to correct, well-formed CSV ─────────────
void checkExport(const QString& dataDir, uint32_t invoiceId)
{
    std::fprintf(stderr, "== pilot: export / share (CSV) ==\n");

    // Company identity is written by the Settings screen; the exports must carry it.
    { SettingsViewModel sv; sv.setCompanyName(QStringLiteral("Pilot Co Ltd")); }

    ExportViewModel ex;

    const QString invPath = ex.exportInvoice(static_cast<int>(invoiceId));
    ok(!invPath.isEmpty() && QFileInfo::exists(invPath), "invoice exported to a file");
    const QString inv = readAll(invPath);
    ok(inv.contains(QStringLiteral("PILOT-1")), "invoice CSV carries the invoice number");
    ok(inv.contains(QStringLiteral("Pilot Co Ltd")), "invoice CSV carries the company identity");
    ok(inv.contains(QStringLiteral("Total,120.00")), "invoice CSV total is net+VAT ($100 + 20%)");
    ok(inv.contains(QStringLiteral("Balance Due,120.00")), "invoice CSV shows the outstanding balance");

    const QString tbPath = ex.exportTrialBalance();
    const QString tb = readAll(tbPath);
    ok(!tb.isEmpty() && tb.contains(QStringLiteral("Trial Balance")), "trial balance exported");
    ok(tb.contains(QStringLiteral("Accounts Receivable")), "trial balance lists ledger accounts");
    ok(tb.contains(QStringLiteral("Total,,0.00")), "trial balance totals to zero (balanced)");

    const QString isPath = ex.exportIncomeStatement();
    const QString is = readAll(isPath);
    ok(is.contains(QStringLiteral("Net Income,")), "income statement (P&L) exported");
    ok(is.contains(QStringLiteral("Income,100.00")), "income statement recognises net revenue");

    const QString txPath = ex.exportTaxSummary();
    const QString tx = readAll(txPath);
    ok(tx.contains(QStringLiteral("Net Tax Payable,")), "tax summary (VAT) exported");
    ok(tx.contains(QStringLiteral("20.00")), "tax summary reports the $20 output VAT");

    (void)dataDir;
}

// ── C8: first-run onboarding writes ONLY settings — never accounting events ─────────────────────
void checkOnboarding(const QString& dataDir)
{
    std::fprintf(stderr, "== pilot: first-run onboarding (settings, not events) ==\n");
    (void)dataDir;
    auto& aj = StorageService::instance().audit();
    const uint64_t seqBefore = aj.lastSeq();

    OnboardingViewModel ob;
    ob.setBusinessName(QStringLiteral("Rivierre Joinery"));
    ob.setAddress(QStringLiteral("12 Rue des Artisans, Lyon"));
    ob.setTaxNumber(QStringLiteral("FR-99887766"));
    ob.setCurrency(QStringLiteral("€"));
    ob.setFiscalYearStart(QStringLiteral("04-01"));
    ob.setLanguage(QStringLiteral("fr"));
    ok(ob.canComplete(), "onboarding is completable once a business name is set");
    ok(ob.commit(), "onboarding commit() succeeds");

    // The profile is persisted as PREFERENCES.
    QSettings s;
    ok(s.value(QStringLiteral("company/name")).toString() == QStringLiteral("Rivierre Joinery"),
       "business name persisted to settings");
    ok(s.value(QStringLiteral("company/fiscalYearStart")).toString() == QStringLiteral("04-01"),
       "fiscal-year start persisted to settings");
    ok(s.value(QStringLiteral("ui/language")).toString() == QStringLiteral("fr"),
       "default language persisted to settings");
    ok(s.value(QStringLiteral("onboarding/completed")).toBool(), "onboarding marked complete");

    // The critical invariant: onboarding created NO accounting events.
    ok(aj.lastSeq() == seqBefore, "onboarding authored NO accounting events (settings ≠ history)");
    // Once complete on a non-empty store, it is no longer 'needed'.
    ok(!ob.needed(), "onboarding not re-triggered after completion");
}

// ── C8: professional invoice documents (EN/FR/AR, RTL) derived from engine data ─────────────────
void checkInvoiceDocuments(const QString& dataDir, uint32_t invoiceId)
{
    std::fprintf(stderr, "== pilot: invoice documents (PDF/HTML, EN/FR/AR) ==\n");
    ExportViewModel ex;
    { SettingsViewModel sv; sv.setCompanyName(QStringLiteral("Pilot Co Ltd")); }

    // HTML is the single render source both HTML + PDF use, so we assert its content per language.
    const QString enHtml = exportsvc::invoiceHtml(invoiceId, QStringLiteral("en"));
    ok(enHtml.contains(QStringLiteral("INVOICE")) && enHtml.contains(QStringLiteral("PILOT-1")),
       "EN invoice document has title + number");
    ok(enHtml.contains(QStringLiteral("Pilot Co Ltd")), "invoice document carries company identity");
    ok(enHtml.contains(QStringLiteral("120.00")), "invoice document total derives from engine (net+VAT)");

    const QString frHtml = exportsvc::invoiceHtml(invoiceId, QStringLiteral("fr"));
    ok(frHtml.contains(QString::fromUtf8("FACTURE")), "FR invoice document is localized");

    const QString arHtml = exportsvc::invoiceHtml(invoiceId, QStringLiteral("ar"));
    ok(arHtml.contains(QStringLiteral("dir=\"rtl\"")), "AR invoice document is RTL");
    ok(arHtml.contains(QString::fromUtf8("فاتورة")), "AR invoice document is localized (Arabic)");

    // PDF export produces a real PDF file.
    const QString pdf = ex.exportInvoicePdf(static_cast<int>(invoiceId), QStringLiteral("en"));
    ok(!pdf.isEmpty() && QFileInfo::exists(pdf), "invoice PDF written");
    { QFile f(pdf); f.open(QIODevice::ReadOnly); const QByteArray head = f.read(5); f.close();
      ok(head.startsWith("%PDF"), "invoice PDF has a valid PDF header"); }
    const QString pdfAr = ex.exportInvoicePdf(static_cast<int>(invoiceId), QStringLiteral("ar"));
    ok(!pdfAr.isEmpty() && QFileInfo::exists(pdfAr), "Arabic (RTL) invoice PDF written");
    (void)dataDir;
}

// ── C8: customer communication (statement + outstanding summary) ────────────────────────────────
void checkCustomerComms(uint32_t customerId)
{
    std::fprintf(stderr, "== pilot: customer communication (statement / outstanding) ==\n");
    ExportViewModel ex;

    const QString st = ex.exportCustomerStatement(static_cast<int>(customerId));
    const QString stTxt = readAll(st);
    ok(!stTxt.isEmpty() && stTxt.contains(QStringLiteral("Customer Statement")), "customer statement exported");
    ok(stTxt.contains(QStringLiteral("Closing Balance")), "statement has a running closing balance");
    ok(stTxt.contains(QStringLiteral("PILOT-1")), "statement lists the invoice as a charge");

    const QString os = ex.exportOutstandingSummary();
    const QString osTxt = readAll(os);
    ok(!osTxt.isEmpty() && osTxt.contains(QStringLiteral("Outstanding Balances")), "outstanding summary exported");
    ok(osTxt.contains(QStringLiteral("Total Outstanding")), "outstanding summary has a grand total");
}

// ── C8: trust dashboard signals are read-only projections (author no events) ────────────────────
void checkTrustDashboard()
{
    std::fprintf(stderr, "== pilot: trust dashboard (read-only projections) ==\n");
    auto& aj = StorageService::instance().audit();
    const uint64_t seqBefore = aj.lastSeq();

    DiagnosticsViewModel dv;
    dv.refresh();
    ok(dv.trialBalanceOk(), "trust: trial balance reported balanced");
    ok(!dv.eventCount().isEmpty(), "trust: event count surfaced");
    ok(!dv.lastBackup().isEmpty(), "trust: last-backup status surfaced");
    // Reading trust projections must not mutate history.
    ok(aj.lastSeq() == seqBefore, "trust dashboard is read-only (no events authored)");
}

// ── C9: period close is drivable from the UI VM (freeze a filed month/quarter) ─────────────────
// The engine capability + its refusal of in-period edits is proven by ACCT_HOSTILE=period; this
// asserts the on-screen path (periodVm) that the new Ledger → Periods form binds to.
void checkPeriodClose()
{
    std::fprintf(stderr, "== pilot: period close (freeze filed periods via VM) ==\n");
    auto& aj = StorageService::instance().audit();
    PeriodCloseViewModel pv;

    const int      closedBefore = pv.closedCount();
    const uint64_t seqBefore    = aj.lastSeq();

    // Bad input is refused and authors nothing.
    ok(!pv.closePeriod(QString(), QStringLiteral("2026-01-01"), QStringLiteral("2026-01-31")),
       "closePeriod refuses an empty label");
    ok(!pv.closePeriod(QStringLiteral("Bad"), QStringLiteral("2026-13-99"), QStringLiteral("2026-01-31")),
       "closePeriod refuses an invalid date");
    ok(aj.lastSeq() == seqBefore, "rejected closes authored NO events");

    // A valid close freezes the period AND is an authoritative append-only event.
    ok(pv.closePeriod(QStringLiteral("2026-01"), QStringLiteral("2026-01-01"), QStringLiteral("2026-01-31")),
       "closePeriod(2026-01) succeeds via the VM");
    ok(pv.closedCount() == closedBefore + 1, "closedCount incremented after close");
    ok(aj.lastSeq() > seqBefore, "closing a period IS an authoritative event (unlike settings)");
    ok(pv.isDateInClosedPeriod(QStringLiteral("2026-01-15")), "a date inside the period reads as closed");
    ok(!pv.isDateInClosedPeriod(QStringLiteral("2026-02-15")), "a date outside the period reads as open");

    // Reopen unfreezes it (append-only); the membership test flips back.
    ok(pv.reopenPeriod(QStringLiteral("2026-01")), "reopenPeriod(2026-01) succeeds via the VM");
    ok(pv.closedCount() == closedBefore, "closedCount back to baseline after reopen");
    ok(!pv.isDateInClosedPeriod(QStringLiteral("2026-01-15")), "the date reads as open again after reopen");
}

// ── C10: user-reachable support (stable non-PII id + one-click diagnostics bundle) ─────────────
// The pilot support process depends on a user being able to (a) quote a stable id and (b) produce a
// diagnostics bundle from the UI. Drives the real DiagnosticsViewModel path end-to-end.
void checkSupport(const QString& dataDir)
{
    std::fprintf(stderr, "== pilot: support (id + diagnostics bundle, no accounting data) ==\n");
    auto& aj = StorageService::instance().audit();
    const uint64_t seqBefore = aj.lastSeq();

    DiagnosticsViewModel dv;

    const QString id1 = dv.supportId();
    ok(id1.startsWith(QStringLiteral("OCC-")), "support id is issued in the OCC-XXXX-XXXX form");
    ok(dv.supportId() == id1, "support id is stable across reads (persisted, not regenerated)");

    const QString zip = dv.exportSupportBundle();
    ok(!zip.isEmpty() && QFileInfo::exists(zip), "diagnostics bundle is produced from the VM");

    QByteArray raw;
    { QFile f(zip); if (f.open(QIODevice::ReadOnly)) { raw = f.readAll(); f.close(); } }
    ok(raw.startsWith(QByteArrayLiteral("PK\x03\x04")), "bundle is a valid zip");
    ok(raw.contains(id1.toUtf8()), "bundle records the support id for correlation");
    // The support surface authors NOTHING in the ledger (it is read-only diagnostics).
    ok(aj.lastSeq() == seqBefore, "producing a support bundle authors NO accounting events");
    (void)dataDir;
}

} // namespace

int runPilotChecks(const QString& scenario)
{
    if (!StorageService::instance().isInitialized()) {
        std::fprintf(stderr, "pilot: storage not initialised\n");
        return 1;
    }
    const QString dataDir = QString::fromStdString(StorageService::instance().dataDir());
    // Isolate QSettings (company identity, restore/pending) to this run's data dir.
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, dataDir + "/config");

    g_pass = g_fail = 0;
    std::fprintf(stderr, "\n===== PILOT READINESS CHECKS =====\n");

    const uint32_t inv  = seedBusiness();
    const uint32_t cust = customerIdByName(QStringLiteral("Pilot Co"));

    const QString s = scenario.trimmed().toLower();
    bool known = false;
    if (s == QLatin1String("safety")     || s == QLatin1String("all")) { checkDataSafety(dataDir); known = true; }
    if (s == QLatin1String("export")     || s == QLatin1String("all")) { checkExport(dataDir, inv); known = true; }
    if (s == QLatin1String("onboarding") || s == QLatin1String("all")) { checkOnboarding(dataDir); known = true; }
    if (s == QLatin1String("documents")  || s == QLatin1String("all")) { checkInvoiceDocuments(dataDir, inv); known = true; }
    if (s == QLatin1String("comms")      || s == QLatin1String("all")) { checkCustomerComms(cust); known = true; }
    if (s == QLatin1String("trust")      || s == QLatin1String("all")) { checkTrustDashboard(); known = true; }
    if (s == QLatin1String("periods")    || s == QLatin1String("all")) { checkPeriodClose(); known = true; }
    if (s == QLatin1String("support")    || s == QLatin1String("all")) { checkSupport(dataDir); known = true; }
    if (!known) {
        std::fprintf(stderr, "pilot: unknown scenario '%s' "
                     "(safety|export|onboarding|documents|comms|trust|periods|support|all)\n", s.toUtf8().constData());
        return 1;
    }

    std::fprintf(stderr, "\n===== PILOT SUMMARY: %d passed, %d failed =====\n\n", g_pass, g_fail);
    return g_fail;
}
