#include "accept.h"

#include "InvoiceEditorViewModel.h"
#include "InvoiceDraftLinesModel.h"
#include "CustomerEditorViewModel.h"
#include "SupplierEditorViewModel.h"
#include "PaymentEditorViewModel.h"
#include "PaymentAllocationViewModel.h"
#include "ExpenseEditorViewModel.h"
#include "TaxSummaryViewModel.h"
#include "TrialBalanceModel.h"
#include "SettingsViewModel.h"

#include "storage/StorageService.h"
#include "storage/AuditJournal.h"
#include "storage/ExpenseRepository.h"

#include "app/Signature.h"
#include "app/LicenseManager.h"
#include "app/UpdateManager.h"
#include "app/BackupScheduler.h"
#include "app/AppInfo.h"

#include <QSettings>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonObject>
#include <QJsonDocument>
#include <QVariantMap>
#include <cstdio>

namespace {

int g_pass = 0, g_fail = 0;
void ok(bool cond, const char* name)
{
    (cond ? g_pass : g_fail)++;
    std::fprintf(stderr, "    %s  %s\n", cond ? "ok  " : "FAIL", name);
}

// int64 cents → "D.DD" for the editor VMs (which parse text like the QML fields do).
QString cents(long long c) {
    return QString("%1.%2").arg(c / 100).arg(QString::number(c % 100).rightJustified(2, '0'));
}

constexpr qint64 T0 = 1735689600;   // 2025-01-01T00:00:00Z — fixed clock

// A realistic small business. The workflow is identical across personas; the DATA differs
// (counts, amounts, VAT rate, single- vs multi-line invoices) so the same lifecycle is proven for
// cash-only exempt shops, VAT retailers, high-value service invoicing, and multi-line jobs.
struct Persona {
    const char* key;
    const char* display;
    int  customers, suppliers, expenses;
    long long expenseCents;
    int  invoices;
    long long lineUnitCents;
    int  taxPercent;    // VAT rate on invoice lines (0 = exempt / no VAT)
    bool multiLine;     // two lines per invoice (e.g. parts + labour)
};

const Persona kPersonas[] = {
    { "cafe",       "Small café (cash, VAT-exempt)",       3, 2,  8,  1250, 10,    800,  0, false },
    { "retail",     "Retail shop (20% VAT)",               5, 3,  6,  5000, 12,   2000, 20, false },
    { "freelancer", "Freelancer (high-value services)",    2, 1,  4,  3000,  3, 150000, 20, false },
    { "consultant", "Consultant (retainers + VAT)",        4, 2,  5,  8000,  6, 100000, 20, false },
    { "repair",     "Repair shop (parts + labour)",        6, 4, 10,  2500,  8,   4000, 10, true  },
    { "clinic",     "Medical clinic (exempt services)",    8, 3, 12,  6000, 15,   9000,  0, false },
};

// makeUpdateSource — a signed local update the persona's updater can stage (mirrors c2test).
void makeUpdateSource(const QString& srcDir, long long versionCode, const QString& version)
{
    QDir().mkpath(srcDir);
    const QByteArray payload(2048, 'U');
    { QFile f(srcDir + "/Occountant-setup.bin");
      if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) { f.write(payload); f.close(); } }
    QJsonObject o;
    o["version"] = version; o["versionCode"] = double(versionCode);
    o["payload"] = "Occountant-setup.bin"; o["size"] = double(payload.size());
    o["channel"] = "stable"; o["sig"] = QString::fromLatin1(sig::signDetached(payload)); o["notes"] = "acceptance";
    QFile m(srcDir + "/manifest.json");
    if (m.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        m.write(QJsonDocument(o).toJson(QJsonDocument::Compact)); m.close();
    }
}

int runOne(const Persona& p, const QString& dataDir)
{
    g_pass = 0; g_fail = 0;
    std::fprintf(stderr, "== acceptance: %s ==\n", p.display);

    auto& storage = StorageService::instance();
    auto& aj = storage.audit();
    const auto clock = []{ return T0; };
    const int linesPer = p.multiLine ? 2 : 1;

    // ── 1. Company creation — fresh empty books + company identity (isolated prefs) ──
    // Keep the company name in an ini under the persona's dir so no two personas (or the real
    // user) share a QSettings location.
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, dataDir + "/config");
    ok(storage.isInitialized() && aj.trialBalanceTotal() == 0,
       "company created: empty books open, trial balance 0");
    {
        SettingsViewModel sv;
        sv.setCompanyName(QString::fromLatin1(p.display));
        sv.setCurrencySymbol(QStringLiteral("$"));
        ok(sv.companyName() == QString::fromLatin1(p.display), "company identity saved");
    }

    // ── 2. Customers ──────────────────────────────────────────────────────────
    {
        CustomerEditorViewModel ce;
        for (int i = 0; i < p.customers; ++i) {
            ce.beginNew();
            ce.setName(QStringLiteral("%1 Customer %2").arg(p.key).arg(i + 1));
            ce.setEmail(QStringLiteral("cust%1@%2.example").arg(i + 1).arg(p.key));
            if (!ce.commit()) { ok(false, "customer commit"); return g_fail; }
        }
        ok((int)storage.customers().loadAll().size() == p.customers, "all customers persisted");
    }
    const auto custs = storage.customers().loadAll();
    const uint32_t firstCust = custs.empty() ? 0u : custs.front().getId();

    // ── 3. Suppliers ──────────────────────────────────────────────────────────
    {
        SupplierEditorViewModel se;
        for (int i = 0; i < p.suppliers; ++i) {
            se.beginNew();
            se.setName(QStringLiteral("%1 Supplier %2").arg(p.key).arg(i + 1));
            if (!se.commit()) { ok(false, "supplier commit"); return g_fail; }
        }
        ok((int)storage.suppliers().loadAll().size() == p.suppliers, "all suppliers persisted");
    }

    // ── 4. Expenses (event-authored: Dr Expenses / Cr Cash) ────────────────────
    {
        ExpenseEditorViewModel xe;
        for (int i = 0; i < p.expenses; ++i) {
            xe.beginNew();
            xe.setDate(QStringLiteral("2026-03-%1").arg((i % 27) + 1, 2, 10, QLatin1Char('0')));
            xe.setAmount(cents(p.expenseCents));
            xe.setCategory(0);        // Office
            xe.setPaymentMethod(0);   // Cash
            if (!xe.commit()) { ok(false, "expense commit"); return g_fail; }
        }
        ok((int)storage.expenses().count() == p.expenses, "all expenses posted");
        ok(aj.incomeStatementAt(aj.lastSeq()).expense == p.expenses * p.expenseCents,
           "income statement recognises expense total");
        ok(aj.trialBalanceTotal() == 0, "trial balance 0 after expenses");
    }

    // ── 5. Invoices with VAT (event-authored revenue posting) ──────────────────
    {
        InvoiceEditorViewModel ie;
        for (int i = 0; i < p.invoices; ++i) {
            ie.beginNew();
            ie.setCustomerId((int)firstCust);
            ie.setIssueDate(QStringLiteral("2026-03-10"));
            ie.setDueDate(QStringLiteral("2026-04-10"));
            InvoiceDraftLinesModel* lines = ie.lines();
            for (int L = 0; L < linesPer; ++L) {
                if (L > 0) QMetaObject::invokeMethod(lines, "addBlankLine");
                QMetaObject::invokeMethod(lines, "setCell", Q_ARG(int, L),
                    Q_ARG(QString, QStringLiteral("description")), Q_ARG(QString, L == 0 ? QStringLiteral("Parts") : QStringLiteral("Labour")));
                QMetaObject::invokeMethod(lines, "setCell", Q_ARG(int, L),
                    Q_ARG(QString, QStringLiteral("qtyText")), Q_ARG(QString, QStringLiteral("1")));
                QMetaObject::invokeMethod(lines, "setCell", Q_ARG(int, L),
                    Q_ARG(QString, QStringLiteral("unitPriceText")), Q_ARG(QString, cents(p.lineUnitCents)));
                QMetaObject::invokeMethod(lines, "setCell", Q_ARG(int, L),
                    Q_ARG(QString, QStringLiteral("taxText")), Q_ARG(QString, QString::number(p.taxPercent)));
            }
            ie.setStatus(1);   // POSTED → recognises revenue on commit
            if (!ie.valid()) { ok(false, "invoice not valid before commit"); return g_fail; }
            if (!ie.commit()) { ok(false, "invoice commit"); return g_fail; }
        }
        ok((int)storage.invoices().loadAll().size() == p.invoices, "all invoices posted");
        const long long expectedNet = (long long)p.invoices * linesPer * p.lineUnitCents;
        ok(aj.incomeStatementAt(aj.lastSeq()).income == expectedNet,
           "income statement recognises net revenue (ex-VAT)");
        ok(aj.trialBalanceTotal() == 0, "trial balance 0 after invoicing");
    }

    // ── 6. VAT summary — output tax present iff the persona charges VAT ─────────
    {
        TaxSummaryViewModel tv; tv.refresh();
        const bool hasVat = tv.collectedText() != QStringLiteral("$0.00");
        ok(hasVat == (p.taxPercent > 0),
           p.taxPercent > 0 ? "VAT summary reports collected output tax"
                            : "VAT-exempt persona reports zero output tax");
    }

    // ── 7. Payments + allocation ───────────────────────────────────────────────
    {
        const auto invs = storage.invoices().loadAll();
        ok(!invs.empty(), "invoices available for settlement");
        if (invs.empty()) return g_fail;
        const uint32_t firstInv = invs.front().getId();
        const long long before = aj.outstandingFor(firstInv);

        PaymentEditorViewModel pe;
        pe.beginNew(); pe.setCustomerId((int)firstCust);
        pe.setDate(QStringLiteral("2026-04-01")); pe.setAmount(cents(p.lineUnitCents));
        ok(pe.commit(), "payment recorded");
        const int pid = pe.lastPaymentId();

        PaymentAllocationViewModel pa; pa.beginFor(pid);
        ok(pa.allocate((int)firstInv, cents(p.lineUnitCents)), "payment allocated to an invoice");
        ok(aj.outstandingFor(firstInv) == before - p.lineUnitCents,
           "allocation reduced the invoice's outstanding balance");
        ok(aj.trialBalanceTotal() == 0, "trial balance 0 after settlement");
    }

    // ── 8. Reports — the read-only projections mirror the engine ───────────────
    {
        TrialBalanceModel tb; tb.refresh();
        ok(tb.balanced() && aj.trialBalanceTotal() == 0, "trial balance report balances");
    }

    // ── 9. Backup + restore verification ───────────────────────────────────────
    {
        BackupScheduler sch(dataDir, BackupPolicy{24, 3, 90}, clock);
        const QString name = sch.runNow(true);
        ok(!name.isEmpty(), "backup restore point created");
        ok(sch.verify(name), "backup verifies (authoritative log intact)");
    }

    // ── 10. License — a fresh install issues a usable trial ────────────────────
    {
        LicenseManager lm(dataDir + "/lic", clock);
        lm.initialize();
        ok(lm.isUsable(), "license usable (trial issued on first run)");
    }

    // ── 11. Update staging — signed, verified, stageable, rollback-safe ────────
    {
        const QString src = dataDir + "/upd/source";
        const QString stg = dataDir + "/upd/stage";
        makeUpdateSource(src, appinfo::versionCode() + 1000, QStringLiteral("9.9.9"));
        UpdateManager um(stg, src, appinfo::versionCode());
        um.setChannel(QStringLiteral("stable"));
        ok(um.check() && um.hasUpdate(), "updater sees the signed update");
        ok(um.downloadAndStage(), "update verifies + stages");
        ok(um.rollbackStaged(), "staged update rolls back cleanly");
    }

    // ── 12. Verification — deterministic replay + projection equivalence ───────
    {
        const std::string d = storage.dataDir();
        CustomerRepository vc(d + "/.acc_c.dat"); SupplierRepository vs(d + "/.acc_s.dat");
        InvoiceRepository  vi(d + "/.acc_i.dat"); InvoiceLineRepository vl(d + "/.acc_l.dat");
        ExpenseRepository  ve(d + "/.acc_e.dat");
        const auto va = aj.verifyAll(vc, vs, vi, vl, &ve);
        ok(va.ok && va.expensesOk, "verifyAll: books replay byte-identically from history");
        ok(storage.verifyAuditProjection().ok, "live projection == authoritative history");
    }

    std::fprintf(stderr, "== %s: %d passed, %d failed ==\n", p.key, g_pass, g_fail);
    return g_fail;
}

} // namespace

int runAcceptance(const QString& persona)
{
    if (!StorageService::instance().isInitialized()) {
        std::fprintf(stderr, "acceptance: storage not initialised\n");
        return 1;
    }
    const QString key = persona.trimmed().toLower();
    const QString dataDir = QString::fromStdString(StorageService::instance().dataDir());
    for (const Persona& p : kPersonas)
        if (key == QLatin1String(p.key))
            return runOne(p, dataDir);
    std::fprintf(stderr, "acceptance: unknown persona '%s' (cafe|retail|freelancer|consultant|repair|clinic)\n",
                 key.toUtf8().constData());
    return 1;
}
