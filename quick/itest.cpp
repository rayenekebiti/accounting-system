#include "itest.h"

#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlExpression>
#include <QObject>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QDebug>
#include <cstdio>

#include "InvoiceEditorViewModel.h"
#include "InvoiceDraftLinesModel.h"
#include "CustomerEditorViewModel.h"
#include "SupplierEditorViewModel.h"
#include "PaymentEditorViewModel.h"
#include "PaymentAllocationViewModel.h"
#include "AccountsListModel.h"
#include "TrialBalanceModel.h"
#include "LedgerExplorerViewModel.h"
#include "ExpenseEditorViewModel.h"
#include "DiagnosticsViewModel.h"
#include "SettingsViewModel.h"
#include "BackupViewModel.h"
#include "ExportViewModel.h"
#include "LocaleController.h"
#include <QFile>
#include <QFileInfo>
#include "storage/StorageService.h"
#include "storage/AuditJournal.h"
#include "storage/ExpenseRepository.h"

// ── QML runtime-error capture (Phase 3) ───────────────────────────────────────
// A message handler collects QML-origin warnings/errors so any test that emits
// one (ReferenceError, TypeError, binding loop, unknown property, delegate/model
// scope error, …) fails. The benign MSYS translation-catalog warning is excluded.
static QStringList     g_qmlErrors;
static QtMessageHandler g_prev = nullptr;

static bool isQmlError(const QString& m)
{
    if (m.contains(QLatin1String("catalogs.json"))) return false; // env, not the app
    static const char* markers[] = {
        ".qml:", "ReferenceError", "TypeError", "Binding loop detected",
        "Unable to assign", "Cannot assign", "is not defined",
        "Cannot read property", "is not a function", "Unknown property",
        "Cannot read property", "non-existent"
    };
    for (const char* mk : markers)
        if (m.contains(QLatin1String(mk))) return true;
    return false;
}

static void msgHandler(QtMsgType t, const QMessageLogContext& c, const QString& m)
{
    if ((t == QtWarningMsg || t == QtCriticalMsg || t == QtFatalMsg) && isQmlError(m))
        g_qmlErrors << m;
    if (g_prev) g_prev(t, c, m);
    else if (t != QtDebugMsg) { std::fputs(m.toUtf8().constData(), stderr); std::fputc('\n', stderr); }
}

// Test reporting goes straight to stderr (NOT through qInfo/qWarning, which route
// through the message handler above — for capturing QML errors, not our output).
static void report(const QString& s)
{
    std::fputs(s.toUtf8().constData(), stderr);
    std::fputc('\n', stderr);
    std::fflush(stderr);
}

namespace {

struct Runner {
    QQmlApplicationEngine&   engine;
    InvoiceEditorViewModel&      ie;
    CustomerEditorViewModel&     ce;
    SupplierEditorViewModel&     se;
    PaymentEditorViewModel&      pe;
    PaymentAllocationViewModel&  pa;
    LocaleController&            lo;
    int pass = 0;
    int fail = 0;
    QString section;

    QObject* root() {
        return engine.rootObjects().isEmpty() ? nullptr : engine.rootObjects().first();
    }
    QObject* find(const char* name) {
        QObject* r = root();
        return r ? r->findChild<QObject*>(QString::fromLatin1(name)) : nullptr;
    }
    // Evaluate a QML expression in obj's own context (the real user path).
    QVariant qeval(QObject* obj, const QString& expr, bool* hadError = nullptr) {
        QQmlExpression x(qmlContext(obj), obj, expr);
        const QVariant v = x.evaluate();
        if (hadError) *hadError = x.hasError();
        return v;
    }
    void open(QObject* popup)  { QMetaObject::invokeMethod(popup, "open");  QCoreApplication::processEvents(); }
    void close(QObject* popup) { QMetaObject::invokeMethod(popup, "close"); QCoreApplication::processEvents(); }

    void ok(bool cond, const QString& name) {
        if (cond) ++pass; else ++fail;
        report(QStringLiteral("  %1 %2 — %3").arg(cond ? "ok  " : "FAIL", section, name));
    }
    // Assert no NEW QML error was emitted since `before`.
    void noErrorsSince(int before, const QString& name) {
        const bool clean = (g_qmlErrors.size() == before);
        if (!clean)
            for (int i = before; i < g_qmlErrors.size(); ++i)
                report(QStringLiteral("       qml-error: %1").arg(g_qmlErrors[i]));
        ok(clean, name);
    }

    // ── T1: full invoice creation through QML interaction paths ───────────────
    void invoiceCreation() {
        section = "invoice-creation";
        const int before = g_qmlErrors.size();
        ie.beginNew();
        QObject* ed = find("invoiceEditorRoot");
        ok(ed != nullptr, "editor instantiated");
        if (!ed) return;
        open(ed);

        QVariantList opts = ie.customerOptions();
        ok(!opts.isEmpty(), "customer options populated");
        const int cid = opts.isEmpty() ? 0 : opts.first().toMap().value(QStringLiteral("value")).toInt();

        ok(!ie.customerError().isEmpty(), "customer error present before selection");

        // Drive the REAL onActivated handler of the customer Select (not our own
        // expression) so this catches the QML FILE reverting to a broken idiom
        // like setCustomerId(v). Emit the Select's `activated(value)` signal.
        QObject* sel = find("invCustomerSelect");
        ok(sel != nullptr, "customer Select found (direct popup child)");
        if (sel) {
            QMetaObject::invokeMethod(sel, "activated", Q_ARG(int, cid));
            QCoreApplication::processEvents();
        }
        ok(ie.customerId() == cid, "selecting a customer propagated customerId to VM");
        ok(ie.customerError().isEmpty(), "customer validation cleared live after selection");

        qeval(ed, QStringLiteral("invoiceEditor.issueDate = '2026-01-15'"));
        qeval(ed, QStringLiteral("invoiceEditor.dueDate = '2026-02-15'"));
        qeval(ed, QStringLiteral("invoiceEditor.lines.setCell(0, 'description', 'Service')"));
        qeval(ed, QStringLiteral("invoiceEditor.lines.setCell(0, 'qtyText', '2')"));
        qeval(ed, QStringLiteral("invoiceEditor.lines.setCell(0, 'unitPriceText', '50')"));
        ok(ie.valid(), "invoice valid after fill");

        const int invBefore = (int) StorageService::instance().invoices().loadAll().size();
        // Post the invoice (INVOICE_POSTED = 1) so the commit recognises revenue and the
        // Full Domain Cutover ledger effect fires. Drive the real QML property write.
        qeval(ed, QStringLiteral("invoiceEditor.status = 1"));
        auto& aj = StorageService::instance().audit();
        const std::size_t entriesBefore = aj.entryCount();
        int savedCount = 0;
        auto conn = QObject::connect(&ie, &InvoiceEditorViewModel::saved, [&]{ ++savedCount; });
        const bool committed = ie.commit();
        QObject::disconnect(conn);
        ok(committed, "commit() succeeded");
        ok(savedCount == 1, "saved() emitted once");
        const int invAfter = (int) StorageService::instance().invoices().loadAll().size();
        ok(invAfter == invBefore + 1, "invoice persisted (count +1)");

        // Full domain cutover: the commit routed through the event log AND posted the
        // revenue effect to the ledger (Dr AR / Cr Revenue) — invoice → ledger → statements.
        ok(aj.entryCount() == entriesBefore + 1, "invoice commit posted one ledger entry (revenue)");
        ok(aj.trialBalanceTotal() == 0, "ledger still balances (trial balance 0) after invoice post");
        ok(aj.incomeStatementAt(aj.lastSeq()).income >= 10000,
           "income statement recognises the invoice revenue (2 x $50)");

        close(ed);
        noErrorsSince(before, "no QML errors during invoice creation");
    }

    // ── T2: customer create — live validation + persistence round-trip ────────
    void customerValidation() {
        section = "customer-validation";
        const int before = g_qmlErrors.size();
        ce.beginNew();
        QObject* ed = find("customerEditorRoot");
        ok(ed != nullptr, "editor instantiated");
        if (!ed) return;
        open(ed);

        ok(!ce.valid(), "empty customer invalid");
        ok(!ce.nameError().isEmpty(), "name-required error present");
        qeval(ed, QStringLiteral("customerEditor.name = 'Acme Ltd'")); // property WRITE
        ok(ce.nameError().isEmpty(), "name error clears after entry");

        qeval(ed, QStringLiteral("customerEditor.email = 'x@'"));
        ok(!ce.emailError().isEmpty(), "invalid-email error appears");
        qeval(ed, QStringLiteral("customerEditor.email = 'a@b.com'"));
        ok(ce.emailError().isEmpty(), "email error clears when fixed");
        ok(ce.valid(), "customer valid after fields set");

        const int custBefore = (int) StorageService::instance().customers().loadAll().size();
        const bool committed = ce.commit();
        ok(committed, "commit() succeeded");
        const auto all = StorageService::instance().customers().loadAll();
        ok((int) all.size() == custBefore + 1, "customer persisted (count +1)");

        // Reopen and verify persisted data round-trips.
        if (!all.empty()) {
            const int newId = (int) all.back().getId();
            ce.beginEdit(newId);
            ok(ce.name() == QStringLiteral("Acme Ltd"), "persisted name reloads correctly");
        }
        close(ed);
        noErrorsSince(before, "no QML errors during customer flow");
    }

    // ── T2b: supplier create — live validation + event-authored persistence ───
    void supplierValidation() {
        section = "supplier-validation";
        const int before = g_qmlErrors.size();
        se.beginNew();
        QObject* ed = find("supplierEditorRoot");
        ok(ed != nullptr, "supplier editor instantiated");
        if (!ed) return;
        open(ed);

        ok(!se.valid(), "empty supplier invalid");
        ok(!se.nameError().isEmpty(), "name-required error present");
        qeval(ed, QStringLiteral("supplierEditor.name = 'Globex Supply'"));   // property WRITE
        ok(se.nameError().isEmpty(), "name error clears after entry");

        qeval(ed, QStringLiteral("supplierEditor.email = 'x@'"));
        ok(!se.emailError().isEmpty(), "invalid-email error appears");
        qeval(ed, QStringLiteral("supplierEditor.email = 'ap@globex.example'"));
        ok(se.emailError().isEmpty(), "email error clears when fixed");
        ok(se.valid(), "supplier valid after fields set");

        auto& storage = StorageService::instance();
        const int supBefore    = (int) storage.suppliers().loadAll().size();
        const uint64_t seqBefore = storage.audit().lastSeq();   // event-authored check
        const bool committed = se.commit();
        ok(committed, "commit() succeeded");
        const auto all = storage.suppliers().loadAll();
        ok((int) all.size() == supBefore + 1, "supplier persisted (count +1)");
        // AUTHORITY: the write appended exactly one authoritative event (SupplierCreated) —
        // it routed through the event log, never a direct suppliers().save().
        ok(storage.audit().lastSeq() == seqBefore + 1,
           "supplier commit appended one authoritative event (event-authored, no repo bypass)");

        // Reopen and verify persisted data round-trips.
        if (!all.empty()) {
            const int newId = (int) all.back().getId();
            se.beginEdit(newId);
            ok(se.name() == QStringLiteral("Globex Supply"), "persisted supplier name reloads correctly");
        }
        close(ed);
        noErrorsSince(before, "no QML errors during supplier flow");
    }

    // ── T2c: payments & settlement — record / allocate / reverse (event-authored) ─
    void paymentsWorkflow() {
        section = "payments";
        const int before = g_qmlErrors.size();
        auto& storage = StorageService::instance();
        auto& aj = storage.audit();

        // Setup: a customer + two posted $100 invoices via the authoritative engine.
        Customer c; c.setName("Pay Co");
        aj.recordCustomerCreated(c, StorageService::now());
        const uint32_t cid = c.getId();
        auto mkInvoice = [&](const char* num) -> uint32_t {
            Invoice inv; inv.setInvoiceNumber(num); inv.setCustomerId(cid); inv.setStatus(INVOICE_POSTED);
            inv.setIssueDate(IsoDate::fromString("2026-08-01").value()); inv.setTotal(Money::fromCents(10000));
            InvoiceLineData d; d.description = "L"; d.quantityMilliunits = 1000; d.unitPrice = Money::fromCents(10000); d.taxRatePermille = 0;
            InvoiceLine ln(d); ln.recompute(); std::vector<InvoiceLine> lines = { ln };
            aj.recordInvoiceWithRevenue(inv, lines, false, 10000, /*tax*/ 0, inv.getIssueDate(), StorageService::now());
            return inv.getId();
        };
        const uint32_t invA = mkInvoice("PINV-A");
        const uint32_t invB = mkInvoice("PINV-B");
        ok(aj.outstandingFor(invA) == 10000 && aj.outstandingFor(invB) == 10000, "invoices start fully outstanding");

        // Record payment P1 ($100) — event-authored: the settlement fact AND its Dr Cash / Cr AR
        // ledger posting are ONE atomic group (two events), so the ledger reflects the receipt.
        uint64_t seq = aj.lastSeq();
        const int64_t cashBefore = aj.balanceFor((uint32_t)aj.accountIdByName("Cash"));
        pe.beginNew(); pe.setCustomerId((int)cid); pe.setDate(QStringLiteral("2026-08-05")); pe.setAmount(QStringLiteral("100.00"));
        ok(pe.valid(), "payment valid after fields set");
        ok(pe.commit() && aj.lastSeq() == seq + 2, "record payment appended the settlement event + its ledger posting");
        ok(aj.balanceFor((uint32_t)aj.accountIdByName("Cash")) == cashBefore + 10000, "payment debited Cash $100 in the ledger");
        const int p1 = pe.lastPaymentId();
        ok(aj.unallocatedFor((uint32_t)p1) == 10000, "new payment fully unallocated (credit)");

        // ONE payment → MANY invoices: $40 → invA (partial), $60 → invB (partial).
        pa.beginFor(p1);
        ok(pa.allocatableInvoices().size() == 2, "both open invoices allocatable");
        seq = aj.lastSeq();
        ok(pa.allocate((int)invA, QStringLiteral("40.00")) && aj.lastSeq() == seq + 1, "partial allocation is one event");
        ok(pa.allocate((int)invB, QStringLiteral("60.00")), "second allocation (same payment, other invoice)");
        ok(aj.outstandingFor(invA) == 6000 && aj.outstandingFor(invB) == 4000, "partial allocations reduce outstanding");
        ok(aj.unallocatedFor((uint32_t)p1) == 0, "P1 fully allocated across two invoices");

        // MANY payments → ONE invoice: P2 ($40) fully settles invB (60 + 40 = 100).
        pe.beginNew(); pe.setCustomerId((int)cid); pe.setDate(QStringLiteral("2026-08-06")); pe.setAmount(QStringLiteral("40.00"));
        ok(pe.commit(), "record payment P2");
        const int p2 = pe.lastPaymentId();
        pa.beginFor(p2);
        ok(pa.allocate((int)invB, QStringLiteral("40.00")), "P2 allocation to invB");
        ok(aj.outstandingFor(invB) == 0, "invB fully settled by two payments (outstanding 0)");

        // Allocation reversal restores outstanding (settlement history is append-only).
        const auto p1allocs = aj.allocationsForPayment((uint32_t)p1);
        ok(p1allocs.size() == 2, "P1 has two allocations recorded");
        uint32_t invAalloc = 0; for (const auto& a : p1allocs) if (a.invoiceId == invA) invAalloc = a.id;
        pa.beginFor(p1);
        ok(pa.reverse((int)invAalloc), "reverse the invA allocation");
        ok(aj.outstandingFor(invA) == 10000, "reversing restores invA outstanding to $100");
        ok(aj.unallocatedFor((uint32_t)p1) == 4000, "reversal returns $40 to unallocated (credit)");

        // Fully-unallocated payment = a pure credit.
        pe.beginNew(); pe.setCustomerId((int)cid); pe.setDate(QStringLiteral("2026-08-07")); pe.setAmount(QStringLiteral("25.00"));
        ok(pe.commit() && aj.unallocatedFor((uint32_t)pe.lastPaymentId()) == 2500, "fully-unallocated payment is a $25 credit");

        // Integrity: trial balance stays 0 and the full model still verifies (deterministic replay).
        ok(aj.trialBalanceTotal() == 0, "trial balance still 0 after settlement activity");
        const std::string d = storage.dataDir();
        CustomerRepository vc(d + "/.itv_c.dat"); SupplierRepository vs(d + "/.itv_s.dat");
        InvoiceRepository  vi(d + "/.itv_i.dat"); InvoiceLineRepository vl(d + "/.itv_l.dat");
        ok(aj.verifyAll(vc, vs, vi, vl).ok, "verifyAll holds — settlement replays deterministically, no repo bypass");

        noErrorsSince(before, "no QML errors during payments workflow");
    }

    // ── T3: language switching keeps editors functional ──────────────────────
    void languageSwitching() {
        section = "language-switching";
        const int before = g_qmlErrors.size();
        lo.setLanguage(QStringLiteral("ar"));
        ok(QGuiApplication::layoutDirection() == Qt::RightToLeft, "AR sets RTL layout direction");
        ok(QCoreApplication::translate("StatusBadge", "Paid") != QStringLiteral("Paid"),
           "AR translation active");

        // Editor still mutates correctly after a live switch (no stale bindings).
        ie.beginNew();
        QObject* ed = find("invoiceEditorRoot");
        bool e = false;
        qeval(ed, QStringLiteral("invoiceEditor.customerId = 0"), &e);
        ok(!e && ie.customerId() == 0, "editor functional after EN→AR switch");

        lo.setLanguage(QStringLiteral("fr"));
        ok(QGuiApplication::layoutDirection() == Qt::LeftToRight, "FR sets LTR layout direction");
        ok(QCoreApplication::translate("InvoicesScreen", "New Invoice") != QStringLiteral("New Invoice"),
           "FR translation active");

        lo.setLanguage(QStringLiteral("en"));
        ok(QCoreApplication::translate("StatusBadge", "Paid") == QStringLiteral("Paid"),
           "EN restores source strings");
        noErrorsSince(before, "no QML errors / binding churn across EN↔FR↔AR");
    }

    // ── T4: line-item editing — delegate handler, setCell, totals, add/remove ─
    void lineItems() {
        section = "line-items";
        const int before = g_qmlErrors.size();
        ie.beginNew();
        QObject* ed = find("invoiceEditorRoot");
        ok(ed != nullptr, "editor instantiated");
        if (!ed) return;
        open(ed);

        qeval(ed, QStringLiteral("invoiceEditor.lines.setCell(0, 'unitPriceText', '50')"));
        qeval(ed, QStringLiteral("invoiceEditor.lines.setCell(0, 'taxText', '10')"));
        // setCell is the exact call the delegate's onEditingFinished makes — drive it
        // through the editor's QML context (the real binding path), then assert totals
        // recompute. (The FieldInput's own editingFinished signal can only be reached
        // once the window is exposed under exec(); covered by the screenshot harness.)
        qeval(ed, QStringLiteral("invoiceEditor.lines.setCell(0, 'qtyText', '3')"));
        ok(ie.subtotalText() == QStringLiteral("$150.00"), "setCell qty=3 → subtotal $150 (3 × $50)");
        ok(ie.totalText() == QStringLiteral("$165.00"), "total recomputes with 10% tax → $165");

        InvoiceDraftLinesModel* lines = ie.lines();
        const int c0 = lines->property("count").toInt();
        QMetaObject::invokeMethod(lines, "addBlankLine");
        ok(lines->property("count").toInt() == c0 + 1, "addBlankLine increments count");
        QMetaObject::invokeMethod(lines, "removeLine", Q_ARG(int, 1));
        ok(lines->property("count").toInt() == c0, "removeLine decrements count");
        ok(ie.subtotalText() == QStringLiteral("$150.00"), "totals stable after add/remove");

        close(ed);
        noErrorsSince(before, "no QML errors during line-item editing");
    }

    // ── T7: ledger explorer — read-only views mirror the engine exactly ───────
    void ledgerExplorer() {
        section = "ledger-explorer";
        const int before = g_qmlErrors.size();
        auto& aj = StorageService::instance().audit();

        // The engine already carries a chart of accounts + posted revenue entries (from
        // invoice creation + the payments workflow). The UI models must MIRROR it — nothing
        // is written, cached, or recomputed here.
        AccountsListModel am; am.refresh();
        ok(am.rowCount() == (int)aj.accountCount() && am.rowCount() >= 3,
           "AccountsListModel lists every ledger account");
        bool balMatch = true;
        for (const auto& r : am.rows())
            if (r.balanceCents != aj.balanceFor(r.id)) balMatch = false;
        ok(balMatch, "each account row balance == audit().balanceFor (derived)");

        const int revId = aj.accountIdByName("Revenue");
        ok(revId >= 0, "Revenue role account resolved by name");

        TrialBalanceModel tb; tb.refresh();
        ok(tb.accountCount() == am.rowCount(), "TrialBalanceModel covers every account");
        ok(tb.balanced() && tb.totalDebitText() == tb.totalCreditText(),
           "trial balance model balances (Σ debits == Σ credits)");
        ok(aj.trialBalanceTotal() == 0, "engine trial balance total is 0");

        LedgerExplorerViewModel lv; lv.refresh();
        ok(lv.entryCount() == (int)aj.entryCount() && aj.entryCount() >= 1,
           "LedgerExplorer lists every journal entry");
        lv.setAccountScope(revId);
        ok(lv.hasScope() && lv.filteredCount() == (int)aj.entriesForAccount((uint32_t)revId).size(),
           "scoping to Revenue lists exactly its entries");
        ok(lv.balanceAtText(revId, (int)aj.lastSeq()).length() > 0, "historical balanceAtText resolves");
        lv.clearScope();
        ok(!lv.hasScope(), "clearing scope restores the full journal");

        // Reversal lineage must surface through inspect() (both directions). Author a real
        // ledger reversal, then inspect the original and its negation.
        const uint32_t orig = aj.listJournalEntries().front().id;
        const std::size_t entriesBefore = aj.entryCount();
        const uint32_t revEntry = aj.reverseJournalEntry(orig, IsoDate::fromString("2026-08-10").value(),
                                                          StorageService::now());
        ok(aj.entryCount() == entriesBefore + 1, "reversal is append-only (one new entry)");
        ok(aj.trialBalanceTotal() == 0, "trial balance still 0 after the reversal");

        lv.refresh();
        lv.inspect((int)revEntry);
        const QVariantMap rev = lv.currentEntry();
        ok(rev.value("isReversal").toBool() && rev.value("reverses").toInt() == (int)orig,
           "inspect(reversal): flagged as reversal, points back at the original");
        ok(rev.value("balanced").toBool() && rev.value("postings").toList().size() >= 2,
           "the reversal entry is itself balanced");
        lv.inspect((int)orig);
        ok(lv.currentEntry().value("isReversed").toBool() &&
           lv.currentEntry().value("reversedBy").toInt() == (int)revEntry,
           "inspect(original): shows it was reversed by the negation");

        noErrorsSince(before, "no QML errors during ledger exploration");
    }

    // ── T8: expenses — drive the editor VM → event-authored posting, no repo bypass ──
    void expensesWorkflow() {
        section = "expenses";
        const int before = g_qmlErrors.size();
        auto& storage = StorageService::instance();
        auto& aj = storage.audit();

        const int expenseAcct = aj.accountIdByName("Expenses");
        const int cashAcct    = aj.accountIdByName("Cash");
        ok(expenseAcct >= 0 && cashAcct >= 0, "chart carries the Expenses + Cash accounts");

        const int64_t expBefore  = aj.balanceFor((uint32_t)expenseAcct);
        const int64_t cashBefore = aj.balanceFor((uint32_t)cashAcct);
        const std::size_t entriesBefore  = aj.entryCount();
        const std::size_t expensesBefore = storage.expenses().count();
        const int64_t incomeExpBefore = aj.incomeStatementAt(aj.lastSeq()).expense;

        // Drive the REAL editor VM (the user path): supplier optional, cash, $50, Office.
        ExpenseEditorViewModel xe;
        xe.beginNew();
        xe.setDate(QStringLiteral("2026-08-15"));
        xe.setAmount(QStringLiteral("50.00"));
        xe.setCategory(0);          // Office
        xe.setPaymentMethod(0);     // Cash
        ok(xe.valid(), "expense valid after fields set");

        const uint64_t seq = aj.lastSeq();
        ok(xe.commit(), "commit() authored the expense");
        ok(aj.lastSeq() == seq + 2, "commit is ONE atomic group (ExpenseCreated + JournalEntryPosted)");
        ok(storage.expenses().count() == expensesBefore + 1, "expense projected (count +1) — no repo bypass");
        ok(aj.entryCount() == entriesBefore + 1, "exactly one ledger entry posted");
        ok(aj.balanceFor((uint32_t)expenseAcct) == expBefore + 5000
             && aj.balanceFor((uint32_t)cashAcct) == cashBefore - 5000,
           "posted Dr Expenses $50 / Cr Cash $50");
        ok(aj.trialBalanceTotal() == 0, "trial balance still 0 after the expense");
        ok(aj.incomeStatementAt(aj.lastSeq()).expense == incomeExpBefore + 5000,
           "income statement recognises the expense");

        const int newId = xe.lastExpenseId();

        // Void through the VM → status VOID + compensating entry; expense account nets back.
        ok(xe.voidExpense(newId), "voidExpense() authored the void");
        ok(aj.isExpenseVoided((uint32_t)newId), "the expense is marked void");
        ok(aj.balanceFor((uint32_t)expenseAcct) == expBefore
             && aj.trialBalanceTotal() == 0,
           "void posts a compensating entry (Expenses back to prior; trial balance 0)");

        // Deterministic replay incl. the expense projection (no repo is a write authority).
        const std::string d = storage.dataDir();
        CustomerRepository vc(d + "/.ite_c.dat"); SupplierRepository vs(d + "/.ite_s.dat");
        InvoiceRepository  vi(d + "/.ite_i.dat"); InvoiceLineRepository vl(d + "/.ite_l.dat");
        ExpenseRepository  ve(d + "/.ite_e.dat");
        const auto va = aj.verifyAll(vc, vs, vi, vl, &ve);
        ok(va.ok && va.expensesOk, "verifyAll holds — expenses replay byte-identically, no repo bypass");

        // F3 (security / input hardening): the editor rejects non-finite / absurd amounts at the
        // UI boundary, before they reach Money's int64-cents conversion.
        xe.beginNew();
        xe.setAmount(QStringLiteral("1e13"));    // > the $1e12 cap
        ok(xe.amountError() == QStringLiteral("amountTooLarge"), "editor rejects an absurdly large amount");
        xe.setAmount(QStringLiteral("1e400"));   // parses to +inf (or fails to parse)
        ok(!xe.valid() && !xe.amountError().isEmpty(), "editor rejects a non-finite amount");
        xe.setAmount(QStringLiteral("50.00"));
        ok(xe.amountError().isEmpty(), "a normal amount is still accepted");

        noErrorsSince(before, "no QML errors during expenses workflow");
    }

    // ── T9: Settings & System — read-only over the engine; NEVER mutates the store ─
    void settingsSystem() {
        section = "settings-system";
        const int before = g_qmlErrors.size();
        auto& storage = StorageService::instance();

        // Capture authoritative state up front — reading diagnostics must not change it.
        const uint64_t seqBefore   = storage.audit().lastSeq();
        const uint64_t eventsBefore = storage.auditEventCount();

        // Diagnostics VM reads the engine directly (no context wiring needed).
        DiagnosticsViewModel dv;
        ok(!dv.eventCount().isEmpty() && dv.eventCount() != QStringLiteral("—"),
           "diagnostics surfaces a live event count");
        ok(dv.trialBalanceOk(), "diagnostics reports the trial balance balanced (== 0)");
        ok(!dv.databaseSize().isEmpty(), "diagnostics surfaces a database size");

        // The on-demand verification is the same non-destructive replay-equivalence check.
        dv.runVerification();
        ok(dv.projectionOk(), "diagnostics: live projection == history (verified)");
        ok(dv.replayOk(),     "diagnostics: replay-equivalence held");

        // The critical invariant: NOTHING on these screens wrote to the store.
        ok(storage.audit().lastSeq()   == seqBefore,   "diagnostics read did NOT advance the sequence");
        ok(storage.auditEventCount()   == eventsBefore, "diagnostics read did NOT append events");

        // Settings VM persists to QSettings only (independent of the accounting store).
        SettingsViewModel sv;
        sv.setCurrencySymbol(QStringLiteral("€"));
        ok(sv.currencySymbol() == QStringLiteral("€"), "settings currency preference round-trips");
        sv.setCurrencySymbol(QStringLiteral("$"));     // restore default
        ok(storage.auditEventCount() == eventsBefore, "a settings change did NOT touch the store");

        // Backup VM enumerates history without exposing internal file names.
        BackupViewModel bv;
        ok(!bv.estimatedSize().isEmpty(), "backup surfaces an estimated size");

        // Drive the real QML: switch through every Settings tab (they must all render clean).
        if (QObject* w = find("settingsWorkspace")) {
            for (const char* t : {"general", "company", "backup", "diagnostics", "about"}) {
                w->setProperty("activeTab", QString::fromLatin1(t));
                QCoreApplication::processEvents();
            }
            ok(true, "all Settings tabs switch without error");
        } else {
            ok(false, "settingsWorkspace found in the loaded UI");
        }

        noErrorsSince(before, "no QML errors during settings/diagnostics workflow");
    }

    // ── T10: Trust dashboard (read-only) + invoice document export, driven via QML ────
    void trustAndDocuments() {
        section = "trust-documents";
        const int before = g_qmlErrors.size();
        auto& storage = StorageService::instance();
        auto& aj = storage.audit();

        // Navigate to the Ledger workspace and switch to the new Trust tab through QML.
        if (QObject* w = find("ledgerWorkspace")) {
            const uint64_t seqBefore = aj.lastSeq();
            w->setProperty("activeTab", QStringLiteral("trust"));
            QCoreApplication::processEvents();
            ok(w->property("activeTab").toString() == QLatin1String("trust"), "Trust tab activates");
            // The dashboard is READ-ONLY: switching to it authored no events.
            ok(aj.lastSeq() == seqBefore, "Trust dashboard authored no accounting events");
            if (QObject* row = find("trustTrialBalance"))
                ok(row->property("ok").toBool(), "Trust dashboard shows trial balance balanced");
            else
                ok(false, "trustTrialBalance row found");
        } else {
            ok(false, "ledgerWorkspace found");
        }

        // Export a professional PDF for an existing invoice through the export VM the UI binds to.
        const auto invs = storage.invoices().loadAll();
        if (!invs.empty()) {
            ExportViewModel ex;
            const QString path = ex.exportInvoicePdf((int)invs.front().getId(), QStringLiteral("en"));
            ok(!path.isEmpty() && QFileInfo::exists(path), "invoice PDF exported via VM");
            QFile f(path);
            const bool opened = f.open(QIODevice::ReadOnly);
            ok(opened && f.read(5).startsWith("%PDF"), "exported invoice is a valid PDF");
            f.close();
        } else {
            ok(false, "an invoice exists to export");
        }

        noErrorsSince(before, "no QML errors during trust/documents workflow");
    }

    int run() {
        report(QStringLiteral("== interaction tests =="));
        invoiceCreation();
        customerValidation();
        supplierValidation();
        paymentsWorkflow();
        ledgerExplorer();
        expensesWorkflow();
        settingsSystem();
        trustAndDocuments();
        languageSwitching();
        lineItems();
        report(QStringLiteral("== %1 passed, %2 failed ==").arg(pass).arg(fail));
        return fail;
    }
};

} // namespace

int runInteractionTests(QQmlApplicationEngine& engine,
                        InvoiceEditorViewModel& invoiceEditor,
                        CustomerEditorViewModel& customerEditor,
                        SupplierEditorViewModel& supplierEditor,
                        PaymentEditorViewModel& paymentEditor,
                        PaymentAllocationViewModel& paymentAllocation,
                        LocaleController& locale)
{
    g_qmlErrors.clear();
    g_prev = qInstallMessageHandler(msgHandler);

    Runner r{engine, invoiceEditor, customerEditor, supplierEditor,
             paymentEditor, paymentAllocation, locale};
    int failures = r.run();

    // Any UNATTRIBUTED QML error that slipped outside a test window still fails CI.
    if (!g_qmlErrors.isEmpty()) {
        report(QStringLiteral("total captured QML errors: %1").arg(g_qmlErrors.size()));
        if (failures == 0) failures = g_qmlErrors.size();   // never let QML errors pass
    }

    qInstallMessageHandler(g_prev);
    return failures;
}
