#ifndef QUICK_PILOT_CHECKS_H
#define QUICK_PILOT_CHECKS_H

#include <QString>

// Pilot-readiness regression suite (Phase C7). Drives the real ViewModels/services a pilot SMB
// depends on and asserts the two pilot blockers are fixed:
//   • data safety — a corrupt or history-less backup is REFUSED before it can overwrite live books;
//   • deliverability — invoices and the core reports export to well-formed CSV with the right totals.
// Returns the number of failed assertions (0 = pilot-ready on these axes). Headless; requires
// StorageService already initialized on a fresh ACCT_DATA_DIR. `scenario` = safety | export | all.
int runPilotChecks(const QString& scenario);

#endif // QUICK_PILOT_CHECKS_H
