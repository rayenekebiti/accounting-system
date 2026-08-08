#ifndef QUICK_FUZZ_H
#define QUICK_FUZZ_H

#include <QString>

// Deterministic Robustness, Fuzzing & Adversarial Validation harness. Proves the completed
// engine rejects-or-recovers under hostile inputs — never silent corruption / UB / replay
// divergence / a crash without a diagnostic. Everything is seeded (reproducible).
//
// Modes (ACCT_FUZZ=<mode>, ACCT_DATA_DIR=<scratch dir>):
//   suite       — structure-aware byte-mutation fuzzers over every on-disk boundary +
//                 property-based invariants over randomized valid histories. Returns the
//                 failure count (0 = robust). Depth via ACCT_FUZZ_ITERS / ACCT_FUZZ_SEED.
//   faultwrite  — build valid state, then arm ACCT_FAULT_ARM at the target persistence write
//                 (cross-process fault-injection writer). Exits 0 (our error path caught it).
//   faultverify — reopen and prove recovery (reconcile / truncation / rebuild / fallback).
int runFuzz(const QString& mode, const QString& dataDir);

#endif // QUICK_FUZZ_H
