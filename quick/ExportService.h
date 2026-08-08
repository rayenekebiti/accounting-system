#ifndef QUICK_EXPORT_SERVICE_H
#define QUICK_EXPORT_SERVICE_H

#include <QString>
#include <cstdint>

// ExportService — deliver accounting information OUT of Occountant as portable CSV, so a pilot SMB
// can actually hand an invoice to a customer and give figures to an accountant/tax authority. This
// is NOT a new accounting feature: every number written here is READ from the authoritative engine
// (StorageService / AuditJournal) — nothing is computed or stored differently. Pure + headless
// (no QML), so it is unit-testable. Company identity is read from the same QSettings the Settings
// screen writes.
namespace exportsvc {

// Each writes a UTF-8 CSV to `path` (its parent directory must already exist). Returns true on
// success. Amounts are exact decimals derived from int64 cents (no float rounding).
bool exportInvoiceCsv(uint32_t invoiceId, const QString& path);   // a deliverable invoice document
bool exportTrialBalanceCsv(const QString& path);                  // every account + derived balance
bool exportIncomeStatementCsv(const QString& path);              // income / expense / net (P&L)
bool exportTaxSummaryCsv(const QString& path);                   // output/input/net tax (VAT return)

// ── Professional invoice document (EN/FR/AR, RTL-aware) ───────────────────────────────────────
// `lang` ∈ {en, fr, ar}; ar produces an RTL document. Every number is READ from the engine
// (invoice/line/settlement) — no calculation is duplicated here. invoiceHtml() is the single
// source both the HTML and PDF exports render, so they can never diverge.
QString invoiceHtml(uint32_t invoiceId, const QString& lang);
bool exportInvoiceHtml(uint32_t invoiceId, const QString& lang, const QString& path);
bool exportInvoicePdf(uint32_t invoiceId, const QString& lang, const QString& path);

// ── Customer communication ────────────────────────────────────────────────────────────────────
bool exportCustomerStatementCsv(uint32_t customerId, const QString& path);   // charges/payments/running balance
bool exportOutstandingSummaryCsv(const QString& path);                       // all customers + total outstanding

} // namespace exportsvc

#endif // QUICK_EXPORT_SERVICE_H
