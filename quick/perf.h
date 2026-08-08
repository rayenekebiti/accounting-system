#ifndef QUICK_PERF_H
#define QUICK_PERF_H

#include <QString>

// Deterministic Performance & Scalability Validation harness. Measures the EVENT-SOURCING
// engine (replay, snapshot, compat verify, reconstruction, ledger, statements) under synthetic
// books from 10k to 1M events. Evidence, not optimization — no production paths change.
//
// Modes (ACCT_PERF=<mode>, ACCT_DATA_DIR=<scratch dir>, ACCT_PERF_SEED=<seed>):
//   gen:<N>        — bulk-generate a valid synthetic history of ~N events into the data dir
//                    (fast: EventLog::appendAtomic batches → one fsync per batch). Exits.
//   measure:<runs> — open the generated dataset, run each op `runs` times (size-aware for the
//                    fsync-bound projection ops), report median/min/max/stddev + profiling.
int runPerf(const QString& mode, const QString& dataDir);

#endif // QUICK_PERF_H
