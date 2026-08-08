#ifndef QUICK_BENCH_H
#define QUICK_BENCH_H

// Performance benchmark harness. Two modes (env-driven, isolated ACCT_DATA_DIR):
//   ACCT_BENCH_SEED=<N>  bulk-seed N invoices + N/5 customers, then quit.
//   ACCT_BENCH=1         measure load/aggregate/refresh/filter/editor/memory,
//                        print a metrics table, then quit.
// Numbers are real (QElapsedTimer); memory is the process working set (psapi).

#include <cstddef>

class QQmlApplicationEngine;
class InvoiceListModel;
class CustomerListModel;
class InvoicesViewModel;
class InvoiceEditorViewModel;

void benchSeed(int n);

int runBenchmark(QQmlApplicationEngine& engine,
                 InvoiceListModel& invoiceModel,
                 CustomerListModel& customerModel,
                 InvoicesViewModel& invoicesVm,
                 InvoiceEditorViewModel& invoiceEditor,
                 double startupInitMs,
                 double startupEngineMs);

#endif // QUICK_BENCH_H
