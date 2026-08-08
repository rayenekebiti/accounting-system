#include "hostile_accept.h"

#include "InvoiceEditorViewModel.h"
#include "InvoiceDraftLinesModel.h"
#include "CustomerEditorViewModel.h"
#include "PaymentEditorViewModel.h"
#include "PaymentAllocationViewModel.h"
#include "ExpenseEditorViewModel.h"
#include "PeriodCloseViewModel.h"

#include "storage/StorageService.h"
#include "storage/AuditJournal.h"
#include "core/Expense.h"   // EXPENSE_PAY_CASH / EXPENSE_PAY_CREDIT

#include <QString>
#include <cstdio>
#include <cstdint>

// ─────────────────────────────────────────────────────────────────────────────
// Hostile accounting-correctness audit — drives the SHIPPING ViewModel workflow
// path (the same objects QML binds to) and asserts cross-subsystem invariants.
// ─────────────────────────────────────────────────────────────────────────────
namespace {

int g_findings = 0;     // violated correctness invariants (confirmed bugs)
int g_holds    = 0;     // invariants that held
int g_regress  = 0;     // baseline sanity that unexpectedly failed (harness/setup problem)

// An accounting invariant that MUST hold. If it does not, that is a confirmed finding.
void invariant(bool holds, const char* id, const char* desc)
{
    if (holds) { ++g_holds; std::fprintf(stderr, "    hold      %-6s %s\n", id, desc); }
    else       { ++g_findings; std::fprintf(stderr, ">>> FINDING   %-6s %s\n", id, desc); }
}

// A baseline the harness relies on (e.g. "the invoice actually posted"). Failure here means the
// scenario didn't set up, not an accounting bug — flagged separately so findings stay meaningful.
void baseline(bool ok, const char* desc)
{
    if (ok) { ++g_holds; std::fprintf(stderr, "    setup ok   %s\n", desc); }
    else    { ++g_regress; std::fprintf(stderr, "!!! SETUP FAIL %s\n", desc); }
}

// int64 cents → "D.DD" text, exactly what the QML fields feed the editor VMs.
QString cents(long long c)
{
    const char sign = c < 0 ? '-' : '+';
    (void)sign;
    long long a = c < 0 ? -c : c;
    return QString("%1.%2").arg(a / 100).arg(QString::number(a % 100).rightJustified(2, '0'));
}

int64_t ledger(const char* accountName)
{
    auto& aj = StorageService::instance().audit();
    const int id = aj.accountIdByName(accountName);
    return id < 0 ? 0 : aj.balanceFor(static_cast<uint32_t>(id));
}

uint32_t customerIdByName(const QString& name)
{
    for (const auto& c : StorageService::instance().customers().loadAll())
        if (QString::fromUtf8(c.getName()) == name) return c.getId();
    return 0xFFFFFFFFu;
}

int64_t customerScreenBalanceCents(uint32_t customerId)
{
    // Exactly what CustomerListModel / the Customers screen shows (computeCustomerAggregates()).
    auto agg = StorageService::instance().computeCustomerAggregates();
    auto it = agg.find(customerId);
    return it == agg.end() ? 0 : it->second.balance.cents();
}

// Drive the invoice editor exactly like accept.cpp / the QML form: one line, qty 1, given unit
// price and tax %. Returns the new invoice's stable id (or MAX on failure).
uint32_t createPostedInvoice(uint32_t customerId, const QString& number,
                             long long unitCents, int taxPercent)
{
    auto& storage = StorageService::instance();
    InvoiceEditorViewModel ie;
    ie.beginNew();
    ie.setInvoiceNumber(number);
    ie.setCustomerId(static_cast<int>(customerId));
    ie.setIssueDate(QStringLiteral("2026-03-10"));
    ie.setDueDate(QStringLiteral("2026-04-10"));
    InvoiceDraftLinesModel* lines = ie.lines();
    QMetaObject::invokeMethod(lines, "setCell", Q_ARG(int, 0),
        Q_ARG(QString, QStringLiteral("description")), Q_ARG(QString, QStringLiteral("Service")));
    QMetaObject::invokeMethod(lines, "setCell", Q_ARG(int, 0),
        Q_ARG(QString, QStringLiteral("qtyText")), Q_ARG(QString, QStringLiteral("1")));
    QMetaObject::invokeMethod(lines, "setCell", Q_ARG(int, 0),
        Q_ARG(QString, QStringLiteral("unitPriceText")), Q_ARG(QString, cents(unitCents)));
    QMetaObject::invokeMethod(lines, "setCell", Q_ARG(int, 0),
        Q_ARG(QString, QStringLiteral("taxText")), Q_ARG(QString, QString::number(taxPercent)));
    ie.setStatus(1);   // POSTED → recognises revenue on commit
    if (!ie.commit()) return 0xFFFFFFFFu;
    return static_cast<uint32_t>(storage.invoices().findIdByNumber(number.toStdString().c_str()));
}

// ── Workflow 1+2: customer payment + allocation must reach the ledger ─────────
// Real path: PaymentEditorViewModel.commit() → allocate via PaymentAllocationViewModel. A
// received-and-applied payment is economically "Dr Cash / Cr AR". We assert that AND that the
// three places a user reads a balance (ledger AR, settlement outstanding, Customers screen) agree.
void auditPayments()
{
    std::fprintf(stderr, "== hostile: customer payment → ledger ==\n");
    auto& storage = StorageService::instance();
    auto& aj = storage.audit();

    CustomerEditorViewModel ce;
    ce.beginNew();
    ce.setName(QStringLiteral("Payment Ledger Co"));
    ce.setEmail(QStringLiteral("pay@hostile.example"));
    baseline(ce.commit(), "customer created via editor VM");
    const uint32_t cust = customerIdByName(QStringLiteral("Payment Ledger Co"));
    baseline(cust != 0xFFFFFFFFu, "customer id resolved");

    const uint32_t inv = createPostedInvoice(cust, QStringLiteral("HST-PAY-1"), 10000, 0);   // $100, no VAT
    baseline(inv != 0xFFFFFFFFu, "posted invoice created via editor VM");

    const int64_t arAfterInvoice = ledger("Accounts Receivable");
    baseline(arAfterInvoice == 10000, "baseline: invoice debited AR by $100 (ledger reflects invoicing)");
    baseline(aj.outstandingFor(inv) == 10000, "baseline: settlement outstanding is $100");

    // Record a $100 payment and allocate it in full — through the SAME VMs QML uses.
    PaymentEditorViewModel pe;
    pe.beginNew();
    pe.setCustomerId(static_cast<int>(cust));
    pe.setDate(QStringLiteral("2026-04-01"));
    pe.setAmount(cents(10000));
    baseline(pe.commit(), "payment recorded via editor VM");
    const int pid = pe.lastPaymentId();

    PaymentAllocationViewModel pa;
    pa.beginFor(pid);
    baseline(pa.allocate(static_cast<int>(inv), cents(10000)), "payment allocated to the invoice via VM");

    // Settlement (the Payments screen) correctly shows the invoice cleared.
    baseline(aj.outstandingFor(inv) == 0, "baseline: settlement now shows $0 outstanding");

    const int64_t cash = ledger("Cash");
    const int64_t ar    = ledger("Accounts Receivable");

    // INVARIANTS a real accountant demands after a customer pays a $100 invoice in full:
    invariant(cash == 10000, "C1",
        "customer payment must DEBIT Cash by $100 in the ledger");
    invariant(ar == 0, "C1",
        "customer payment must CREDIT Accounts Receivable to $0 in the ledger");

    // The three balances a user can read must agree. Settlement says $0; the ledger and the
    // Customers screen must too.
    const int64_t screen = customerScreenBalanceCents(cust);
    invariant(ar == aj.outstandingFor(inv), "C2",
        "ledger AR must equal settlement outstanding for the customer");
    invariant(screen == aj.outstandingFor(inv), "C2",
        "Customers-screen balance must equal settlement outstanding (payment reflected)");

    // The tell-tale of 'balanced but wrong': the trial balance is still perfectly zero.
    invariant(aj.trialBalanceTotal() == 0, "TB",
        "trial balance is zero (holds even while the above are wrong — the blind spot)");
    // ^ This one HOLDS today; it is here to demonstrate that trial-balance-zero certifies nothing
    //   about C1/C2. It is an invariant that (correctly) holds; it is not a finding.
}

// ── Workflow 8: expense correction that changes the funding method ────────────
// Real path: ExpenseEditorViewModel — create a CASH expense, then beginEdit + switch the payment
// method to CREDIT. The ledger credit side must move from Cash to Accounts Payable. The delta-only
// posting model does not re-point it, so the ledger keeps crediting Cash.
void auditExpenseMethodChange()
{
    std::fprintf(stderr, "== hostile: expense correction changes funding method ==\n");
    auto& storage = StorageService::instance();
    auto& aj = storage.audit();

    const int64_t cash0 = ledger("Cash");
    const int64_t ap0   = ledger("Accounts Payable");

    ExpenseEditorViewModel xe;
    xe.beginNew();
    xe.setDate(QStringLiteral("2026-05-02"));
    xe.setAmount(cents(10000));           // $100 net, no tax
    xe.setCategory(0);                    // Office
    xe.setPaymentMethod(EXPENSE_PAY_CASH);
    baseline(xe.commit(), "cash expense created via editor VM");
    const uint32_t exp = storage.expenses().loadAll().empty()
        ? 0xFFFFFFFFu : storage.expenses().loadAll().back().getId();
    baseline(exp != 0xFFFFFFFFu, "expense id resolved");
    baseline(ledger("Cash") == cash0 - 10000, "baseline: cash expense credited Cash by $100");

    // Correct it: SAME amount, but now it's a CREDIT purchase (owed to a supplier).
    xe.beginEdit(static_cast<int>(exp));
    xe.setPaymentMethod(EXPENSE_PAY_CREDIT);
    baseline(xe.commit(), "expense corrected to CREDIT via editor VM");

    const int64_t cash = ledger("Cash");
    const int64_t ap   = ledger("Accounts Payable");

    // After re-classifying a $100 cash spend as a $100 credit purchase, the economics are:
    // Cash unaffected overall (the cash outflow never happened), Accounts Payable owes $100.
    invariant(cash == cash0, "H-EXP",
        "re-classifying cash→credit must UNWIND the Cash credit (Cash back to pre-expense)");
    invariant(ap == ap0 - 10000, "H-EXP",
        "re-classifying cash→credit must CREDIT Accounts Payable by $100");
    invariant(aj.trialBalanceTotal() == 0, "TB",
        "trial balance is zero (holds even though the funding side is misposted)");
}

// ── Workflow 8b: expense correction that changes BOTH amount and method ───────
void auditExpenseAmountAndMethodChange()
{
    std::fprintf(stderr, "== hostile: expense correction changes amount AND method ==\n");
    auto& storage = StorageService::instance();
    auto& aj = storage.audit();

    const int64_t cash0 = ledger("Cash");
    const int64_t ap0   = ledger("Accounts Payable");
    const int64_t exp0  = ledger("Expenses");

    ExpenseEditorViewModel xe;
    xe.beginNew();
    xe.setDate(QStringLiteral("2026-05-03"));
    xe.setAmount(cents(10000));           // $100 cash
    xe.setCategory(0);
    xe.setPaymentMethod(EXPENSE_PAY_CASH);
    baseline(xe.commit(), "cash expense created via editor VM");
    const uint32_t exp = storage.expenses().loadAll().back().getId();

    xe.beginEdit(static_cast<int>(exp));
    xe.setPaymentMethod(EXPENSE_PAY_CREDIT);
    xe.setAmount(cents(15000));           // now a $150 credit purchase
    baseline(xe.commit(), "expense corrected to $150 CREDIT via editor VM");

    // Economically this is a $150 credit purchase: Expenses +$150, AP owes $150, Cash untouched.
    invariant(ledger("Expenses") == exp0 + 15000, "H-EXP",
        "corrected expense must recognise $150 of expense");
    invariant(ledger("Cash") == cash0, "H-EXP",
        "amount+method change must UNWIND the original $100 Cash credit");
    invariant(ledger("Accounts Payable") == ap0 - 15000, "H-EXP",
        "amount+method change must leave $150 owed on Accounts Payable");
    invariant(aj.trialBalanceTotal() == 0, "TB", "trial balance is zero");
}

// ── Workflow 12: period closing must be reachable AND must reject modifications ───
// Real path: PeriodCloseViewModel (the new UI wiring onto the existing engine capability) freezes a
// period; then editor corrections/voids whose date falls inside it must be refused.
void auditPeriodClose()
{
    std::fprintf(stderr, "== hostile: period close rejects modifications ==\n");
    auto& storage = StorageService::instance();

    CustomerEditorViewModel ce;
    ce.beginNew();
    ce.setName(QStringLiteral("Period Test Co"));
    baseline(ce.commit(), "customer created via editor VM");
    const uint32_t cust = customerIdByName(QStringLiteral("Period Test Co"));

    const uint32_t inv = createPostedInvoice(cust, QStringLiteral("HST-PER-1"), 20000, 0);   // dated 2026-03-10
    baseline(inv != 0xFFFFFFFFu, "in-period invoice created via editor VM");

    // Create a cash expense INSIDE the period-to-be-closed (allowed now; frozen later).
    ExpenseEditorViewModel xe;
    xe.beginNew();
    xe.setDate(QStringLiteral("2026-03-20"));
    xe.setAmount(cents(5000));
    xe.setCategory(0);
    xe.setPaymentMethod(EXPENSE_PAY_CASH);
    baseline(xe.commit(), "in-period expense created via editor VM");
    const uint32_t exp = storage.expenses().loadAll().back().getId();

    // Freeze 2026-Q1 through the (previously unreachable) UI capability.
    PeriodCloseViewModel pv;
    baseline(pv.closePeriod(QStringLiteral("2026-Q1"),
                            QStringLiteral("2026-01-01"), QStringLiteral("2026-03-31")),
             "period 2026-Q1 closed via the period-close VM");
    invariant(pv.closedCount() >= 1, "M-PERIOD",
        "period closing is reachable and recorded (closedCount >= 1)");
    invariant(pv.isDateInClosedPeriod(QStringLiteral("2026-03-10")), "M-PERIOD",
        "the invoice's issue date is inside the closed period");

    // A correction to an invoice frozen in the closed period must be REFUSED.
    InvoiceEditorViewModel ie;
    ie.beginEdit(static_cast<int>(inv));
    ie.setDueDate(QStringLiteral("2026-05-15"));   // attempt a change
    invariant(!ie.commit(), "M-PERIOD",
        "editing an invoice in a closed period is refused");

    // Voiding an expense frozen in the closed period must be REFUSED.
    invariant(!xe.voidExpense(static_cast<int>(exp)), "M-PERIOD",
        "voiding an expense in a closed period is refused");

    invariant(storage.audit().trialBalanceTotal() == 0, "TB", "trial balance is zero");
}

} // namespace

int runHostileAudit(const QString& scenario)
{
    if (!StorageService::instance().isInitialized()) {
        std::fprintf(stderr, "hostile: storage not initialised\n");
        return 1;
    }
    const QString s = scenario.trimmed().toLower();
    g_findings = g_holds = g_regress = 0;

    std::fprintf(stderr,
        "\n===== HOSTILE ACCOUNTING AUDIT (drives the real ViewModel workflow path) =====\n");

    if (s == QLatin1String("payments") || s == QLatin1String("all"))
        auditPayments();
    if (s == QLatin1String("expenses") || s == QLatin1String("all")) {
        auditExpenseMethodChange();
        auditExpenseAmountAndMethodChange();
    }
    if (s == QLatin1String("period") || s == QLatin1String("all"))
        auditPeriodClose();
    if (s != QLatin1String("payments") && s != QLatin1String("expenses")
        && s != QLatin1String("period") && s != QLatin1String("all")) {
        std::fprintf(stderr, "hostile: unknown scenario '%s' (payments|expenses|period|all)\n",
                     s.toUtf8().constData());
        return 1;
    }

    std::fprintf(stderr,
        "\n===== SUMMARY: %d invariant(s) held, %d CONFIRMED FINDING(s), %d setup failure(s) =====\n",
        g_holds, g_findings, g_regress);
    std::fprintf(stderr,
        "A non-zero FINDING count is expected on the current build; it must reach 0 once the\n"
        "payment→ledger and expense-funding-side gaps are fixed. Setup failures must always be 0.\n\n");

    // Return findings + setup failures so CI can gate: 0 == books are economically correct.
    return g_findings + g_regress;
}
