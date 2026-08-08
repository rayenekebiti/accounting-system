#ifndef QUICK_PTEST_H
#define QUICK_PTEST_H

#include <QString>

// Deterministic persistence + integrity test harness. Exercises the real
// BinaryRecordFile journal/recovery protocol and the accounting invariants in
// isolated temp dirs. Modes (ACCT_PTEST=<mode>, ACCT_DATA_DIR=<dir>):
//   suite       — full deterministic suite; returns failure count.
//   crashwrite  — append a sentinel (subject to ACCT_CRASH_POINT); for shell-
//                 orchestrated REAL crash→recover testing across processes.
//   crashverify — reopen and report whether the sentinel survived + was recovered.
// Returns the process exit code (0 = pass).

int runPersistenceTests(const QString& mode, const QString& dataDir);

#endif // QUICK_PTEST_H
