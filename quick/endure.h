#ifndef QUICK_ENDURE_H
#define QUICK_ENDURE_H

// Long-session / keyboard-workflow endurance harness (ACCT_ENDURE=<cycles>).
//
// Drives the REAL view-model operations a sustained keyboard session triggers —
// open editor → fill → save, filter churn, language switching, periodic customer
// add — for N cycles, sampling working-set memory and the runtime-warning counters
// (diag) so memory growth and warning DRIFT over a long session are measured, not
// assumed. DeferredDelete events are drained before each sample (a known
// measurement artifact — see docs/performance.md). Quits when done.
//
// Boundary: this exercises the VM/persistence/i18n layers a keyboard session hits;
// literal key-event focus traversal needs an exposed window under exec() and is a
// separate manual/visual check.

class InvoiceEditorViewModel;
class CustomerEditorViewModel;
class InvoicesViewModel;
class LocaleController;

int runEndurance(InvoiceEditorViewModel& invoiceEditor,
                 CustomerEditorViewModel& customerEditor,
                 InvoicesViewModel& invoicesVm,
                 LocaleController& locale,
                 int cycles);

#endif // QUICK_ENDURE_H
