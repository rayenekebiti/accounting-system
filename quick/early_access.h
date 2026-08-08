#ifndef QUICK_EARLY_ACCESS_H
#define QUICK_EARLY_ACCESS_H

#include <QString>

// ACCT_EARLY_ACCESS gate. Verifies the Early Access notice state machine (no repeat), support-id
// stability, the privacy-safe diagnostics bundle (no accounting data), and the invariant that the
// Early Access / Support Center flows author NO accounting events and leave replay-equivalence
// intact. Headless; isolated QSettings + data dir. Returns the failed-assertion count (0 = pass).
int runEarlyAccessTests(const QString& dataDir);

#endif // QUICK_EARLY_ACCESS_H
