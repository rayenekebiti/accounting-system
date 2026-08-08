#include "ExportService.h"

#include "storage/StorageService.h"
#include "storage/AuditJournal.h"
#include "core/Invoice.h"
#include "core/InvoiceLine.h"
#include "core/Customer.h"

#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QSettings>
#include <QString>
#include <QPdfWriter>
#include <QTextDocument>
#include <QPainter>
#include <QPageSize>
#include <QPageLayout>
#include <QMarginsF>
#include <algorithm>
#include <vector>

namespace {

// Exact cents → "[-]D.DD" (no float rounding).
QString money2(int64_t cents)
{
    const bool neg = cents < 0;
    const long long a = neg ? -static_cast<long long>(cents) : static_cast<long long>(cents);
    return QString("%1%2.%3").arg(neg ? "-" : "")
        .arg(a / 100).arg(QString::number(a % 100).rightJustified(2, QLatin1Char('0')));
}

// RFC-4180 field escaping: wrap in quotes and double any embedded quotes.
QString csv(const QString& s)
{
    QString v = s; v.replace('"', QStringLiteral("\"\""));
    return '"' + v + '"';
}

QString invoiceStatusName(int s)
{
    switch (s) {
    case INVOICE_DRAFT:   return QStringLiteral("Draft");
    case INVOICE_POSTED:  return QStringLiteral("Posted");
    case INVOICE_PAID:    return QStringLiteral("Paid");
    case INVOICE_OVERDUE: return QStringLiteral("Overdue");
    case INVOICE_VOID:    return QStringLiteral("Void");
    default:              return QStringLiteral("Unknown");
    }
}

QString accountTypeName(uint8_t t)
{
    switch (t) {
    case AuditJournal::Asset:     return QStringLiteral("Asset");
    case AuditJournal::Liability: return QStringLiteral("Liability");
    case AuditJournal::Equity:    return QStringLiteral("Equity");
    case AuditJournal::Income:    return QStringLiteral("Income");
    case AuditJournal::Expense:   return QStringLiteral("Expense");
    default:                      return QStringLiteral("Other");
    }
}

bool writeAll(const QString& path, const QString& text)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) return false;
    QTextStream ts(&f);
    ts << text;
    f.close();
    return f.error() == QFile::NoError;
}

// HTML text escaping.
QString htmlEsc(const QString& s)
{
    QString v = s;
    v.replace('&', QStringLiteral("&amp;")).replace('<', QStringLiteral("&lt;"))
     .replace('>', QStringLiteral("&gt;")).replace('"', QStringLiteral("&quot;"));
    return v;
}

// Localised invoice-document labels (EN/FR/AR). Kept as an explicit table (not tr()) so a document
// can be generated in the CUSTOMER's language independent of the running UI language.
struct InvLabels {
    QString invoice, billTo, number, issueDate, dueDate, status,
            desc, qty, unit, taxPct, amount, subtotal, taxTotal, total, paid, balance;
    bool rtl = false;
};

InvLabels labelsFor(const QString& lang)
{
    if (lang == QLatin1String("fr"))
        return { QStringLiteral("FACTURE"), QStringLiteral("Facturé à"), QStringLiteral("Facture n°"),
                 QStringLiteral("Date d'émission"), QStringLiteral("Échéance"), QStringLiteral("Statut"),
                 QStringLiteral("Description"), QStringLiteral("Qté"), QStringLiteral("Prix unitaire"),
                 QStringLiteral("TVA %"), QStringLiteral("Montant"), QStringLiteral("Sous-total"),
                 QStringLiteral("TVA"), QStringLiteral("Total"), QStringLiteral("Montant payé"),
                 QStringLiteral("Solde dû"), false };
    if (lang == QLatin1String("ar"))
        return { QString::fromUtf8("فاتورة"), QString::fromUtf8("إلى"), QString::fromUtf8("رقم الفاتورة"),
                 QString::fromUtf8("تاريخ الإصدار"), QString::fromUtf8("تاريخ الاستحقاق"), QString::fromUtf8("الحالة"),
                 QString::fromUtf8("الوصف"), QString::fromUtf8("الكمية"), QString::fromUtf8("سعر الوحدة"),
                 QString::fromUtf8("الضريبة %"), QString::fromUtf8("المبلغ"), QString::fromUtf8("المجموع الفرعي"),
                 QString::fromUtf8("الضريبة"), QString::fromUtf8("الإجمالي"), QString::fromUtf8("المبلغ المدفوع"),
                 QString::fromUtf8("الرصيد المستحق"), true };
    return { QStringLiteral("INVOICE"), QStringLiteral("Bill To"), QStringLiteral("Invoice No."),
             QStringLiteral("Issue Date"), QStringLiteral("Due Date"), QStringLiteral("Status"),
             QStringLiteral("Description"), QStringLiteral("Qty"), QStringLiteral("Unit Price"),
             QStringLiteral("Tax %"), QStringLiteral("Amount"), QStringLiteral("Subtotal"),
             QStringLiteral("Tax"), QStringLiteral("Total"), QStringLiteral("Amount Paid"),
             QStringLiteral("Balance Due"), false };
}

QString statusNameLang(int s, const QString& lang)
{
    static const char* en[] = { "Draft", "Posted", "Paid", "Overdue", "Void" };
    if (lang == QLatin1String("fr")) {
        static const char* fr[] = { "Brouillon", "Émise", "Payée", "En retard", "Annulée" };
        if (s >= 0 && s <= 4) return QString::fromUtf8(fr[s]);
    } else if (lang == QLatin1String("ar")) {
        static const char* ar[] = { "مسودة", "صادرة", "مدفوعة", "متأخرة", "ملغاة" };
        if (s >= 0 && s <= 4) return QString::fromUtf8(ar[s]);
    }
    return (s >= 0 && s <= 4) ? QString::fromUtf8(en[s]) : QStringLiteral("Unknown");
}

// Company identity header lines — read from the same QSettings the Settings screen writes.
QString companyHeader()
{
    QSettings s;
    const QString name = s.value(QStringLiteral("company/name")).toString();
    const QString addr = s.value(QStringLiteral("company/address")).toString();
    const QString tax  = s.value(QStringLiteral("company/taxId")).toString();
    const QString cur  = s.value(QStringLiteral("general/currency"), QStringLiteral("$")).toString();
    QString h;
    h += "Company,"  + csv(name) + "\n";
    h += "Address,"  + csv(addr) + "\n";
    h += "Tax ID,"   + csv(tax)  + "\n";
    h += "Currency," + csv(cur)  + "\n";
    return h;
}

} // namespace

namespace exportsvc {

bool exportInvoiceCsv(uint32_t invoiceId, const QString& path)
{
    auto& storage = StorageService::instance();
    if (!storage.isInitialized()) return false;

    const Invoice inv = storage.invoices().load(invoiceId);
    const Customer cust = storage.customers().load(inv.getCustomerId());
    const int64_t settled     = storage.audit().settledFor(invoiceId);
    const int64_t outstanding = storage.audit().outstandingFor(invoiceId);

    QString out;
    out += "Occountant Invoice\n";
    out += companyHeader();
    out += "\n";
    out += "Invoice Number," + csv(QString::fromUtf8(inv.getInvoiceNumber())) + "\n";
    out += "Customer,"       + csv(QString::fromUtf8(cust.getName())) + "\n";
    out += "Issue Date,"     + csv(QString::fromStdString(inv.getIssueDate().toString())) + "\n";
    out += "Due Date,"       + csv(QString::fromStdString(inv.getDueDate().toString())) + "\n";
    out += "Status,"         + csv(invoiceStatusName(inv.getStatus())) + "\n";
    out += "\n";
    out += "Description,Quantity,Unit Price,Tax %,Line Total\n";
    for (const InvoiceLine& l : storage.invoiceLines().findByInvoice(invoiceId)) {
        if (l.getIsDeleted()) continue;
        out += csv(QString::fromUtf8(l.getDescription())) + ","
             + QString::number(l.getQuantity(), 'f', 3) + ","
             + money2(l.getUnitPrice().cents()) + ","
             + QString::number(l.getTaxRatePermille() / 10.0, 'f', 1) + ","
             + money2(l.getLineTotal().cents()) + "\n";
    }
    out += "\n";
    out += "Subtotal,"     + money2(inv.getSubtotal().cents())  + "\n";
    out += "Tax,"          + money2(inv.getTaxAmount().cents()) + "\n";
    out += "Total,"        + money2(inv.getTotal().cents())     + "\n";
    out += "Amount Paid,"  + money2(settled)                    + "\n";
    out += "Balance Due,"  + money2(outstanding)                + "\n";
    return writeAll(path, out);
}

bool exportTrialBalanceCsv(const QString& path)
{
    auto& storage = StorageService::instance();
    if (!storage.isInitialized()) return false;
    auto& aj = storage.audit();

    QString out;
    out += "Trial Balance\n";
    out += companyHeader();
    out += "\n";
    out += "Account,Type,Balance\n";
    for (const auto& a : aj.listAccounts())
        out += csv(QString::fromStdString(a.name)) + "," + accountTypeName(a.type) + ","
             + money2(a.balanceCents) + "\n";
    out += "Total,," + money2(aj.trialBalanceTotal()) + "\n";
    return writeAll(path, out);
}

bool exportIncomeStatementCsv(const QString& path)
{
    auto& storage = StorageService::instance();
    if (!storage.isInitialized()) return false;
    auto& aj = storage.audit();
    const auto is = aj.incomeStatementAt(aj.lastSeq());

    QString out;
    out += "Income Statement\n";
    out += companyHeader();
    out += "\n";
    out += "Income,"     + money2(is.income)    + "\n";
    out += "Expenses,"   + money2(is.expense)   + "\n";
    out += "Net Income," + money2(is.netIncome) + "\n";
    return writeAll(path, out);
}

bool exportTaxSummaryCsv(const QString& path)
{
    auto& storage = StorageService::instance();
    if (!storage.isInitialized()) return false;
    auto& aj = storage.audit();
    const auto t = aj.taxSummaryAt(aj.lastSeq());

    QString out;
    out += "Tax Summary\n";
    out += companyHeader();
    out += "\n";
    out += "Output Tax (collected on sales),"      + money2(t.collected)   + "\n";
    out += "Input Tax (recoverable on purchases)," + money2(t.recoverable) + "\n";
    out += "Net Tax Payable,"                       + money2(t.netPayable)  + "\n";
    return writeAll(path, out);
}

// ── Professional invoice document ──────────────────────────────────────────────────────────────
QString invoiceHtml(uint32_t invoiceId, const QString& lang)
{
    auto& storage = StorageService::instance();
    if (!storage.isInitialized()) return QString();

    const Invoice inv = storage.invoices().load(invoiceId);
    const Customer cust = storage.customers().load(inv.getCustomerId());
    const int64_t settled     = storage.audit().settledFor(invoiceId);
    const int64_t outstanding = storage.audit().outstandingFor(invoiceId);
    const InvLabels L = labelsFor(lang);

    QSettings s;
    const QString cName = s.value(QStringLiteral("company/name")).toString();
    const QString cAddr = s.value(QStringLiteral("company/address")).toString();
    const QString cTax  = s.value(QStringLiteral("company/taxId")).toString();
    const QString cur   = s.value(QStringLiteral("general/currency"), QStringLiteral("$")).toString();
    auto amt = [&](int64_t c) { return htmlEsc(cur + money2(c)); };

    const QString dir   = L.rtl ? QStringLiteral("rtl") : QStringLiteral("ltr");
    const QString align = L.rtl ? QStringLiteral("right") : QStringLiteral("left");
    const QString opp   = L.rtl ? QStringLiteral("left") : QStringLiteral("right");

    QString h;
    h += "<html dir=\"" + dir + "\"><body style=\"font-family:sans-serif;color:#1a1a1a;\">";
    // Company banner + INVOICE title
    h += "<table width=\"100%\"><tr>";
    h += "<td style=\"text-align:" + align + ";vertical-align:top;\">";
    h += "<div style=\"font-size:18pt;font-weight:bold;\">" + htmlEsc(cName) + "</div>";
    if (!cAddr.isEmpty()) h += "<div>" + htmlEsc(cAddr) + "</div>";
    if (!cTax.isEmpty())  h += "<div>" + htmlEsc(cTax) + "</div>";
    h += "</td>";
    h += "<td style=\"text-align:" + opp + ";vertical-align:top;\">";
    h += "<div style=\"font-size:22pt;font-weight:bold;color:#2b6cb0;\">" + htmlEsc(L.invoice) + "</div>";
    h += "<div>" + htmlEsc(L.number) + ": " + htmlEsc(QString::fromUtf8(inv.getInvoiceNumber())) + "</div>";
    h += "<div>" + htmlEsc(L.issueDate) + ": " + htmlEsc(QString::fromStdString(inv.getIssueDate().toString())) + "</div>";
    h += "<div>" + htmlEsc(L.dueDate) + ": " + htmlEsc(QString::fromStdString(inv.getDueDate().toString())) + "</div>";
    h += "<div>" + htmlEsc(L.status) + ": " + htmlEsc(statusNameLang(inv.getStatus(), lang)) + "</div>";
    h += "</td></tr></table><hr/>";
    // Bill-to
    h += "<div style=\"text-align:" + align + ";margin:8px 0;\"><b>" + htmlEsc(L.billTo) + ":</b> "
       + htmlEsc(QString::fromUtf8(cust.getName()));
    if (cust.getName() && QString::fromUtf8(cust.getEmail()).length())
        h += " &lt;" + htmlEsc(QString::fromUtf8(cust.getEmail())) + "&gt;";
    h += "</div>";
    // Line items
    h += "<table width=\"100%\" border=\"1\" cellspacing=\"0\" cellpadding=\"4\" style=\"border-collapse:collapse;\">";
    h += "<tr style=\"background:#edf2f7;\">"
         "<th align=\"" + align + "\">" + htmlEsc(L.desc) + "</th>"
         "<th align=\"" + opp + "\">" + htmlEsc(L.qty) + "</th>"
         "<th align=\"" + opp + "\">" + htmlEsc(L.unit) + "</th>"
         "<th align=\"" + opp + "\">" + htmlEsc(L.taxPct) + "</th>"
         "<th align=\"" + opp + "\">" + htmlEsc(L.amount) + "</th></tr>";
    for (const InvoiceLine& l : storage.invoiceLines().findByInvoice(invoiceId)) {
        if (l.getIsDeleted()) continue;
        h += "<tr><td align=\"" + align + "\">" + htmlEsc(QString::fromUtf8(l.getDescription())) + "</td>"
             "<td align=\"" + opp + "\">" + QString::number(l.getQuantity(), 'f', 2) + "</td>"
             "<td align=\"" + opp + "\">" + amt(l.getUnitPrice().cents()) + "</td>"
             "<td align=\"" + opp + "\">" + QString::number(l.getTaxRatePermille() / 10.0, 'f', 1) + "</td>"
             "<td align=\"" + opp + "\">" + amt(l.getLineTotal().cents()) + "</td></tr>";
    }
    h += "</table>";
    // Totals block
    auto row = [&](const QString& label, const QString& value, bool strong) {
        return "<tr><td align=\"" + opp + "\" style=\"padding:2px 8px;" + (strong ? "font-weight:bold;" : "") + "\">"
             + htmlEsc(label) + "</td><td align=\"" + opp + "\" style=\"padding:2px 8px;" + (strong ? "font-weight:bold;" : "")
             + "\">" + value + "</td></tr>";
    };
    h += "<table align=\"" + opp + "\" style=\"margin-top:10px;\">";
    h += row(L.subtotal, amt(inv.getSubtotal().cents()), false);
    h += row(L.taxTotal, amt(inv.getTaxAmount().cents()), false);
    h += row(L.total,    amt(inv.getTotal().cents()),    true);
    h += row(L.paid,     amt(settled),                   false);
    h += row(L.balance,  amt(outstanding),               true);
    h += "</table>";
    h += "</body></html>";
    return h;
}

bool exportInvoiceHtml(uint32_t invoiceId, const QString& lang, const QString& path)
{
    const QString h = invoiceHtml(invoiceId, lang);
    return !h.isEmpty() && writeAll(path, h);
}

bool exportInvoicePdf(uint32_t invoiceId, const QString& lang, const QString& path)
{
    const QString html = invoiceHtml(invoiceId, lang);
    if (html.isEmpty()) return false;

    QPdfWriter writer(path);
    writer.setPageSize(QPageSize(QPageSize::A4));
    writer.setPageMargins(QMarginsF(15, 15, 15, 15), QPageLayout::Millimeter);

    QTextDocument doc;
    if (lang == QLatin1String("ar")) {
        QTextOption opt(Qt::AlignRight);
        opt.setTextDirection(Qt::RightToLeft);
        doc.setDefaultTextOption(opt);
    }
    doc.setHtml(html);
    doc.setPageSize(writer.pageLayout().paintRectPixels(writer.resolution()).size());

    QPainter painter(&writer);
    if (!painter.isActive()) return false;
    doc.drawContents(&painter);
    painter.end();
    return QFileInfo(path).size() > 0;
}

// ── Customer communication ──────────────────────────────────────────────────────────────────────
bool exportCustomerStatementCsv(uint32_t customerId, const QString& path)
{
    auto& storage = StorageService::instance();
    if (!storage.isInitialized()) return false;
    const Customer cust = storage.customers().load(customerId);

    struct Row { QString date; QString ref; int64_t charge; int64_t payment; };
    std::vector<Row> rows;
    for (const Invoice& inv : storage.invoices().findByCustomer(customerId)) {
        if (inv.getIsDeleted()) continue;
        const auto st = inv.getStatus();
        if (st == INVOICE_POSTED || st == INVOICE_OVERDUE || st == INVOICE_PAID)
            rows.push_back({ QString::fromStdString(inv.getIssueDate().toString()),
                             QString::fromUtf8(inv.getInvoiceNumber()), inv.getTotal().cents(), 0 });
    }
    for (const auto& p : storage.audit().listPayments()) {
        if (p.customerId != customerId) continue;
        rows.push_back({ QString::fromStdString(p.date.toString()),
                         QStringLiteral("Payment #%1").arg(p.id), 0, p.amountCents });
    }
    // ISO dates sort lexicographically = chronologically.
    std::stable_sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) { return a.date < b.date; });

    QString out;
    out += "Customer Statement\n";
    out += companyHeader();
    out += "Customer," + csv(QString::fromUtf8(cust.getName())) + "\n";
    out += "\n";
    out += "Date,Reference,Charge,Payment,Balance\n";
    int64_t bal = 0;
    for (const Row& r : rows) {
        bal += r.charge - r.payment;
        out += csv(r.date) + "," + csv(r.ref) + ","
             + (r.charge ? money2(r.charge) : QString()) + ","
             + (r.payment ? money2(r.payment) : QString()) + ","
             + money2(bal) + "\n";
    }
    out += "Closing Balance,,,," + money2(bal) + "\n";
    return writeAll(path, out);
}

bool exportOutstandingSummaryCsv(const QString& path)
{
    auto& storage = StorageService::instance();
    if (!storage.isInitialized()) return false;

    const auto agg = storage.computeCustomerAggregates();
    QString out;
    out += "Outstanding Balances\n";
    out += companyHeader();
    out += "\n";
    out += "Customer,Outstanding,At Risk\n";
    int64_t total = 0;
    for (const Customer& c : storage.customers().loadAll()) {
        if (c.getIsDeleted()) continue;
        auto it = agg.find(c.getId());
        const int64_t bal = it == agg.end() ? 0 : it->second.balance.cents();
        const bool risk = it != agg.end() && it->second.hasOverdue;
        total += bal;
        out += csv(QString::fromUtf8(c.getName())) + "," + money2(bal) + ","
             + (risk ? "yes" : "no") + "\n";
    }
    out += "Total Outstanding," + money2(total) + ",\n";
    return writeAll(path, out);
}

} // namespace exportsvc
