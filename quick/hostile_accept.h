#ifndef QUICK_HOSTILE_ACCEPT_H
#define QUICK_HOSTILE_ACCEPT_H

#include <QString>

// Hostile accounting-correctness suite (Phase C6). Unlike accept.cpp — which drives the same
// ViewModel workflows but only asserts what the engine already guarantees (trial-balance == 0,
// settlement outstanding) — this suite drives the REAL editor ViewModels and then asserts the
// CROSS-SUBSYSTEM accounting invariants that a real accountant would demand:
//
//   • a customer payment moves Cash and Accounts Receivable in the LEDGER (not just settlement);
//   • the Customers-screen balance, the ledger AR, and the settlement outstanding AGREE;
//   • an expense correction that changes the funding method re-points the ledger credit side.
//
// Each invariant encodes CORRECT behaviour. A violated invariant is a CONFIRMED FINDING against
// the current build, printed as such. The return value is the number of confirmed findings, so a
// future fix drives it to 0 (today it is deliberately non-zero — that is the point of the audit).
//
// Adds NO features and changes NO accounting semantics — it only observes the existing workflow
// path through the shipping ViewModels. Headless; requires StorageService already initialized on a
// fresh ACCT_DATA_DIR (mirrors runAcceptance). `scenario` = payments | expenses | all.
int runHostileAudit(const QString& scenario);

#endif // QUICK_HOSTILE_ACCEPT_H
