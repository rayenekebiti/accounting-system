#ifndef QUICK_ACCEPT_H
#define QUICK_ACCEPT_H

#include <QString>

// Acceptance suite (Phase C4): drive a realistic small-business persona end-to-end through the
// REAL editor ViewModels + commercial managers — company setup, customers, suppliers, expenses,
// invoices (with VAT), payments + allocation, VAT summary, reports, backup, restore verification,
// license, update staging, and deterministic-replay verification — against a fresh, isolated data
// directory (ACCT_DATA_DIR). Returns the number of failed assertions (0 = the business completed
// its whole lifecycle successfully). Headless; exits before the UI. Changes no accounting semantics.
//
// `persona` selects one of: cafe | retail | freelancer | consultant | repair | clinic (or "all").
int runAcceptance(const QString& persona);

#endif // QUICK_ACCEPT_H
